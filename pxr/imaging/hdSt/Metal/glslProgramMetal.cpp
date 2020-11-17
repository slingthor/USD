//
// Copyright 2016 Pixar
//
// Licensed under the Apache License, Version 2.0 (the "Apache License")
// with the following modification; you may not use this file except in
// compliance with the Apache License and the following modification to it:
// Section 6. Trademarks. is deleted and replaced with:
//
// 6. Trademarks. This License does not grant permission to use the trade
//    names, trademarks, service marks, or product names of the Licensor
//    and its affiliates, except as required to comply with Section 4(c) of
//    the License and to reproduce the content of the NOTICE file.
//
// You may obtain a copy of the Apache License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the Apache License with the above modification is
// distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the Apache License for the specific
// language governing permissions and limitations under the Apache License.
//
#include "pxr/imaging/glf/glew.h"
#include "pxr/imaging/mtlf/mtlDevice.h"

#include "pxr/imaging/mtlf/bindingMap.h"

#include "pxr/imaging/hio/glslfx.h"

#include "pxr/imaging/hdSt/debugCodes.h"
#include "pxr/imaging/hdSt/Metal/codeGenMSL.h"
#include "pxr/imaging/hdSt/Metal/glslProgramMetal.h"
#include "pxr/imaging/hdSt/package.h"
#include "pxr/imaging/hdSt/resourceRegistry.h"
#include "pxr/imaging/hdSt/surfaceShader.h"
#include "pxr/imaging/hdSt/textureResourceHandle.h"
#include "pxr/imaging/hdSt/textureResource.h"

#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/tokens.h"

#include "pxr/imaging/hgiMetal/shaderFunction.h"

#include "pxr/base/tf/diagnostic.h"

#include <string>
#include <fstream>
#include <iostream>

PXR_NAMESPACE_OPEN_SCOPE

static MTLPrimitiveType GetMetalPrimType(GLenum glPrimType) {
    MTLPrimitiveType primType;
    switch(glPrimType) {
        case GL_POINTS:
            primType = MTLPrimitiveTypePoint;
            break;
        case GL_LINE_STRIP:
            primType = MTLPrimitiveTypeLineStrip;
            break;
        case GL_LINES:
            primType = MTLPrimitiveTypeLine;
            break;
        case GL_TRIANGLE_STRIP:
            primType = MTLPrimitiveTypeTriangleStrip;
            break;
        case GL_TRIANGLES:
        // See comment in the draw function as to why we do this
        case GL_LINES_ADJACENCY:
        case GL_PATCHES:
            primType = MTLPrimitiveTypeTriangle;
            break;
        case GL_LINE_STRIP_ADJACENCY:
        case GL_LINE_LOOP:
            // MTL_FIXME - These do no not directly map but work OK for now.
			primType = MTLPrimitiveTypeLineStrip;
            break;
        case GL_TRIANGLE_FAN:
        case GL_TRIANGLE_STRIP_ADJACENCY:
        case GL_TRIANGLES_ADJACENCY:
        //case GL_PATCHES:
            primType = MTLPrimitiveTypePoint;
            TF_FATAL_CODING_ERROR("Not Implemented");
            break;
    }
    
    return primType;
}

MSL_ShaderBindingMapResults MSL_FindBinding(
        MSL_ShaderBindingMap const& bindings,
        TfToken const& name,
        int32_t level) {
    if (level < 0) {
        return bindings.equal_range(name.Hash());
    }
    
    TfToken nameToFind;
    if (level >= 10) {
        TF_FATAL_CODING_ERROR("Not Implemented");
    }
    
    char const *pN = name.GetText();
    char buf[256];
    char *pD = buf;
    
    while(*pN)
        *pD++ = *pN++;
    
    *pD++ = '_';
    *pD++ = '0' + level;
    *pD = 0;
    
    nameToFind = TfToken(buf, TfToken::Immortal);

    return bindings.equal_range(nameToFind.Hash());
}

MSL_ShaderBinding const* MSL_FindBinding(MSL_ShaderBindingMap const& bindings,
                                         TfToken const& name,
                                         uint bindingTypeMask,
                                         uint programStageMask,
                                         uint skipCount,
                                         int level)
{
    auto it_range = MSL_FindBinding(bindings, name, level);
    auto it = it_range.first;

    for(; it != it_range.second; ++it) {
        if( ((*it).second->_type & bindingTypeMask) == 0 ||
            ((*it).second->_stage & programStageMask) == 0 ||
            skipCount-- != 0)
            continue;
        return (*it).second;
    }
    return NULL;
}


HdStGLSLProgramMSL::HdStGLSLProgramMSL(
    TfToken const &role,
    HdStResourceRegistry *const registry)
    : HdStGLSLProgram(role, registry)
, _role(role)
, _valid(false)
, _buildTarget(kMSL_BuildTarget_Regular)
, _gsVertOutBufferSlot(-1), _gsPrimOutBufferSlot(-1), _gsVertOutStructSize(-1), _gsPrimOutStructSize(-1)
, _drawArgsSlot(-1), _indicesSlot(-1), _fragExtrasSlot(-1)
{
    _currentlySet = false;
    _reapplyIndexBuffer = false;
    _vertexFunction = 0;
    _fragmentFunction = 0;
    _computeFunction = 0;
    _computeGeometryFunction = 0;
}

HdStGLSLProgramMSL::~HdStGLSLProgramMSL()
{
    for(auto it = _bindingMap.begin(); it != _bindingMap.end(); ++it)
        delete (*it).second;
    _bindingMap.clear();
    
    if (_vertexFunction)
        [_vertexFunction release];
    if (_fragmentFunction)
        [_fragmentFunction release];
    if (_computeFunction)
        [_computeFunction release];
    if (_computeGeometryFunction)
        [_computeGeometryFunction release];
    
    Hgi *const hgi = _registry->GetHgi();

    if (_program) {
        for (HgiShaderFunctionHandle fn : _program->GetShaderFunctions()) {
            hgi->DestroyShaderFunction(&fn);
        }
        hgi->DestroyShaderProgram(&_program);
    }
}

#if defined(GENERATE_METAL_DEBUG_SOURCE_CODE)

NSUInteger dumpedFileCount = 0;

static std::mutex _DebugDumpSourceMutex;
const HdStGLSLProgram* previousProgram = 0;
NSUInteger totalPrograms = 0;

void DumpMetalSource(const HdStGLSLProgram* program, NSString *metalSrc, NSString *fileSuffix, NSString *compilerMessages)
{
    std::lock_guard<std::mutex> lock(_DebugDumpSourceMutex);

    if(program != previousProgram) {
        previousProgram = program;
        totalPrograms++;
    }

    NSFileManager *fileManager= [NSFileManager defaultManager];
    
    NSURL *applicationDocumentsDirectory = [[fileManager URLsForDirectory:NSDocumentDirectory inDomains:NSUserDomainMask] lastObject];
    
    NSString *srcDumpLocation = [applicationDocumentsDirectory.path stringByAppendingPathComponent:@"/HydraMetalSourceDumps"];
    
    if(![fileManager fileExistsAtPath:srcDumpLocation]) {
        if(![fileManager createDirectoryAtPath:srcDumpLocation withIntermediateDirectories:YES attributes:nil error:NULL]) {
            NSLog(@"Error: Create folder failed %@", srcDumpLocation);
            return;
        }
    }
    
    NSString *fileContents = [[NSString alloc] init];
    if(compilerMessages != nil)
    {
        fileContents = [fileContents stringByAppendingString:@"/* BEGIN COMPILER MESSAGES *\\\n"];
        fileContents = [fileContents stringByAppendingString:compilerMessages];
        fileContents = [fileContents stringByAppendingString:@"\\* END COMPILER MESSAGES*/\n"];
    }
    fileContents = [fileContents stringByAppendingString:metalSrc];
    
    NSString *fileName = [NSString stringWithFormat:@"HydraMetalSource_%lu_%lu_%@.metal", totalPrograms, dumpedFileCount++, fileSuffix];
    NSString *srcDumpFilePath = [srcDumpLocation stringByAppendingPathComponent:fileName];
    [fileContents writeToFile:srcDumpFilePath atomically:YES encoding:NSUTF8StringEncoding error:nil];
    NSLog(@"Dumping Metal Source to %@", srcDumpFilePath);
}

NSString *LoadPreviousMetalSource(const HdStGLSLProgram* program, NSString *metalSrc, NSString *fileSuffix)
{
    std::lock_guard<std::mutex> lock(_DebugDumpSourceMutex);

    NSUInteger programIndex = totalPrograms;
    if (program != previousProgram) {
        programIndex++;
    }
    NSString *fileName = [NSString stringWithFormat:@"HydraMetalSource_%lu_%lu_%@.metal", programIndex, dumpedFileCount, fileSuffix];
    
    NSFileManager *fileManager= [NSFileManager defaultManager];
    
    NSURL *applicationDocumentsDirectory = [[fileManager URLsForDirectory:NSDocumentDirectory inDomains:NSUserDomainMask] lastObject];
    
    NSString *srcDumpLocation = [applicationDocumentsDirectory.path stringByAppendingPathComponent:@"/HydraMetalSourceDumps"];
    
    NSError *error = NULL;
    
    NSString *srcDumpFilePath = [srcDumpLocation stringByAppendingPathComponent:fileName];
    NSString *fileContents = [NSString stringWithContentsOfFile:srcDumpFilePath
                                                       encoding:NSUTF8StringEncoding
                                                          error:&error];

    if (fileContents && !error) {
        NSLog(@"Loading shader from %@", srcDumpFilePath);
        return fileContents;
    }
    
    NSLog(@"Failed loading shader from %@", srcDumpFilePath);
    return metalSrc;
}
#else
#define DumpMetalSource(a, b, c, d)
#define LoadPreviousMetalSource(program, metalSrc, fileSuffix) metalSrc
#endif

bool
HdStGLSLProgramMSL::CompileShader(HgiShaderStage stage,
                              std::string const &shaderSourceOriginal)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    // early out for empty source.
    // this may not be an error, since glslfx gives empty string
    // for undefined shader stages (i.e. null geometry shader)
    if (shaderSourceOriginal.empty()) return false;
    
    const char *shaderType = NULL;
    switch (stage) {
        case HgiShaderStageTessellationControl:
        case HgiShaderStageTessellationEval:
            //TF_CODING_ERROR("Unsupported shader type on Metal %d\n", type);
            NSLog(@"Unsupported shader type on Metal %d\n", stage); //MTL_FIXME - remove the above error so it doesn't propogate all the way back but really we should never see these types of shaders
            DumpMetalSource(this, [NSString stringWithUTF8String:shaderSourceOriginal.c_str()], @"InvalidType", nil); //MTL_FIXME
            return true;
        default:
            break;
    }

    // create a shader, compile it
    NSError *error = NULL;
    MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();
    
    bool success = true;
    NSString *entryPoint = nil;
    switch (stage) {
    case HgiShaderStageVertex: shaderType = "VS"; entryPoint = @"vertexEntryPoint"; break;
    case HgiShaderStageFragment: shaderType = "FS"; entryPoint = @"fragmentEntryPoint"; break;
    case HgiShaderStageGeometry: shaderType = "Compute_GS"; entryPoint = @"computeEntryPoint"; break;
    case HgiShaderStageCompute: shaderType = "CS"; entryPoint = @"computeEntryPoint"; break;
    default: TF_FATAL_CODING_ERROR("Not allowed!");
    }
    
    if (TfDebug::IsEnabled(HDST_DUMP_SHADER_SOURCE)) {
        static std::mutex _mutex;
        std::lock_guard<std::mutex> lock(_mutex);

        std::cout   << "--------- " << shaderType << " ----------\n"
                    << shaderSourceOriginal
                    << "---------------------------\n"
                    << std::flush;
    }
    
    std::string filePostFix = shaderType;
    std::string shaderSource;
    
    // Metal Debug. Set this to true to overwrite the shaders being compiled from the dump
    // files of the last run. Useful for running experiements during debug.
    bool loadShadersFromDump = false;

    if (loadShadersFromDump) {
        shaderSource =
            [[NSString stringWithString:LoadPreviousMetalSource(this,
                                                                [NSString stringWithUTF8String:shaderSourceOriginal.c_str()],
                                                                [NSString stringWithUTF8String:filePostFix.c_str()])]
             cStringUsingEncoding:NSUTF8StringEncoding];
    }
    else {
        shaderSource = shaderSourceOriginal;
    }

    MTLCompileOptions *options = [[MTLCompileOptions alloc] init];
    options.fastMathEnabled = YES;
    options.languageVersion = MTLLanguageVersion2_1;
    options.preprocessorMacros = @{
        @"HD_MTL_VERTEXSHADER":(stage==HgiShaderStageVertex)?@1:@0,
        @"HD_MTL_COMPUTESHADER":(stage==HgiShaderStageGeometry || stage==HgiShaderStageCompute)?@1:@0,
        @"HD_MTL_FRAGMENTSHADER":(stage==HgiShaderStageFragment)?@1:@0,
    };

    id<MTLLibrary> library = [context->currentDevice newLibraryWithSource:@(shaderSource.c_str())
                                                  options:options
                                                    error:&error];
    
    // Load the function into the library
    id <MTLFunction> function = [library newFunctionWithName:entryPoint];
    if (!function) {
        NSString *err = [error localizedDescription];
        err = [err stringByReplacingOccurrencesOfString:@"error: use of undeclared identifier 'surfaceShader'" withString:@"redacted"];
        NSUInteger loc = [err rangeOfString:@"error: "].location;
        if (loc != NSNotFound) {
            // XXX:validation
            TF_WARN("Failed to compile shader (%s): \n%s",
                    shaderType, [err UTF8String]);
            filePostFix += "_Fail";
        }
        success = false;
    }
    
    if (stage == HgiShaderStageVertex) {
        _vertexFunction = function;
    } else if (stage == HgiShaderStageFragment) {
        _fragmentFunction = function;
    } else if (stage == HgiShaderStageCompute) {
        _computeFunction = function;
    } else if (stage == HgiShaderStageGeometry) {
        _computeGeometryFunction = function;
    }
    [library release];

    [options release];
    options = nil;
    
    Hgi *const hgi = _registry->GetHgi();

    // Create a shader, compile it
    HgiShaderFunctionDesc shaderFnDesc;
    shaderFnDesc.shaderCode = nullptr;
    shaderFnDesc.shaderStage = stage;
    HgiShaderFunctionHandle shaderFn = hgi->CreateShaderFunction(shaderFnDesc);
    
    HgiMetalShaderFunction* metalShaderFn =
        static_cast<HgiMetalShaderFunction*>(shaderFn.Get());
    metalShaderFn->SetShaderId(function);

    // Store the shader function in the program descriptor so it can be used
    // during Link time.
    _programDesc.shaderFunctions.push_back(shaderFn);

    //MTL_FIXME: Remove this debug line once done.
    DumpMetalSource(this, [NSString stringWithUTF8String:shaderSource.c_str()], [NSString stringWithUTF8String:filePostFix.c_str()], error != nil ? [error localizedDescription] : nil);
    
    return success;
}

bool
HdStGLSLProgramMSL::Link()
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    bool vertexFuncPresent = _vertexFunction != nil;
    bool fragmentFuncPresent = _fragmentFunction != nil;
    bool computeFuncPresent = _computeFunction != nil;
    bool computeGeometryFuncPresent = _computeGeometryFunction != nil;
    
    if (computeFuncPresent && (vertexFuncPresent ^ fragmentFuncPresent)) {
        TF_CODING_ERROR("A compute shader can't be set with a vertex shader or fragment shader also set.");
        return false;
    }
    
    if(_buildTarget == kMSL_BuildTarget_MVA_ComputeGS && !computeGeometryFuncPresent) {
        TF_CODING_ERROR("Missing Compute Geometry shader while linking.");
        return false;
    }

    id<MTLDevice> device = MtlfMetalContext::GetMetalContext()->currentDevice;

    // update the program resource allocation
    _valid = true;

    for(auto it = _bindingMap.begin(); it != _bindingMap.end(); ++it) {
        const MSL_ShaderBinding& binding = *(*it).second;
        if(binding._stage == kMSL_ProgramStage_Vertex || binding._stage == kMSL_ProgramStage_Compute) {
            if(binding._type == kMSL_BindingType_DrawArgs) _drawArgsSlot = binding._index;
            else if(binding._type == kMSL_BindingType_GSVertOutput) _gsVertOutBufferSlot = binding._index;
            else if(binding._type == kMSL_BindingType_GSPrimOutput) _gsPrimOutBufferSlot = binding._index;
            else if(binding._type == kMSL_BindingType_UniformBuffer &&
                    binding._name == "indices") _indicesSlot = binding._index;
        }
        else if(binding._stage == kMSL_ProgramStage_Fragment) {
            if(binding._type == kMSL_BindingType_FragExtras)
                _fragExtrasSlot = binding._index;
        }
    }
    
    Hgi *const hgi = _registry->GetHgi();

    // Create the shader program.
    if (_program) {
        hgi->DestroyShaderProgram(&_program);
    }
    _program = hgi->CreateShaderProgram(_programDesc);


    return true;
}

bool
HdStGLSLProgramMSL::GetProgramLinkStatus(std::string * reason) const
{
    return _valid;
}

bool
HdStGLSLProgramMSL::Validate() const
{
    return _valid;
}

void HdStGLSLProgramMSL::AssignUniformBindings(GarchBindingMapRefPtr bindingMap) const
{
    MtlfBindingMapRefPtr mtlfBindingMap(TfDynamic_cast<MtlfBindingMapRefPtr>(bindingMap));
    
    for (GarchBindingMap::UniformBindingMap::value_type& p : mtlfBindingMap->_uniformBindings) {
        auto it_range = _bindingMap.equal_range(p.first.Hash());
        for(auto it = it_range.first; it != it_range.second; ++it) {
            const MSL_ShaderBinding& binding = *(*it).second;
            if(binding._type != kMSL_BindingType_UniformBuffer)
                continue;
            MtlfBindingMap::MTLFBindingIndex mtlfIndex(
                binding._index, (uint32_t)binding._type, (uint32_t)binding._stage, true);
            p.second = mtlfIndex.asInt;
        }
    }
}

void HdStGLSLProgramMSL::AssignSamplerUnits(GarchBindingMapRefPtr bindingMap) const
{
    //Samplers really means OpenGL style samplers (ancient style) where a sampler is both a texture and an actual sampler.
    //For us this means a texture always needs to have an accompanying sampler that is bound to the same slot index.
    //This way when an index is returned it can be used for both.
    
    MtlfBindingMapRefPtr mtlfBindingMap(TfDynamic_cast<MtlfBindingMapRefPtr>(bindingMap));
    
    for (GarchBindingMap::SamplerBindingMap::value_type& p : mtlfBindingMap->_samplerBindings) {
        auto it_range = _bindingMap.equal_range(p.first.Hash());
        for(auto it = it_range.first; it != it_range.second; ++it) {
            const MSL_ShaderBinding& binding = *(*it).second;
            if(binding._type != kMSL_BindingType_Texture && binding._type != kMSL_BindingType_Sampler)
                continue;
            MtlfBindingMap::MTLFBindingIndex mtlfIndex(
                binding._index, (uint32_t)binding._type, (uint32_t)binding._stage, true);
            p.second = mtlfIndex.asInt;
        }
    }
}

void HdStGLSLProgramMSL::AddBinding(std::string const &name, int index,
    HdBinding const &binding, MSL_BindingType bindingType,
    MSL_ProgramStage programStage, int offsetWithinResource,
    int uniformBufferSize)
{
    _locationMap.insert(make_pair(name, index));
    MSL_ShaderBinding* newBinding = new MSL_ShaderBinding(
        bindingType, programStage, index, name, offsetWithinResource,
        uniformBufferSize, binding);
    _bindingMap.insert(std::make_pair(newBinding->_nameToken.Hash(), newBinding));
}

void HdStGLSLProgramMSL::UpdateUniformBinding(std::string const &name, int index)
{
    TfToken nameToken(name);
    auto it_range = _bindingMap.equal_range(nameToken.Hash());
    for(auto it = it_range.first; it != it_range.second; ++it) {
        MSL_ShaderBinding& binding = *(*it).second;
        if(binding._type != kMSL_BindingType_Uniform)
            continue;
        binding._index = index;
        return;
    }
    TF_FATAL_CODING_ERROR("Failed to find binding %s", name.c_str());
}

void HdStGLSLProgramMSL::AddCustomBindings(GarchBindingMapRefPtr bindingMap) const
{
    MtlfBindingMapRefPtr mtlfBindingMap(TfDynamic_cast<MtlfBindingMapRefPtr>(bindingMap));
    
    TF_FATAL_CODING_ERROR("Not Implemented");
}

void
HdStGLSLProgramMSL::BindTexture(
    const TfToken &name,
    id<MTLTexture> textureId,
    MTLTextureType typeHint) const
{
    std::string textureName("textureBind_" + name.GetString());
    TfToken textureNameToken(textureName, TfToken::Immortal);
   
    MSL_ShaderBinding const* const textureBinding = MSL_FindBinding(
        _bindingMap,
        textureNameToken,
        kMSL_BindingType_Texture,
        0xFFFFFFFF,
        0);

    if(!textureBinding) {
        TF_FATAL_CODING_ERROR("Could not bind a texture to the shader?!");
    }
     
    MtlfMetalContext::GetMetalContext()->SetTexture(
        textureBinding->_index,
        textureId,
        textureNameToken,
        textureBinding->_stage,
        typeHint);
}

void
HdStGLSLProgramMSL::BindSampler(
    const TfToken &name,
    id<MTLSamplerState> samplerId) const
{
    std::string samplerName("samplerBind_" + name.GetString());
    TfToken samplerNameToken(samplerName, TfToken::Immortal);

    MSL_ShaderBinding const* const samplerBinding = MSL_FindBinding(
        _bindingMap,
        samplerNameToken,
        kMSL_BindingType_Sampler,
        0xFFFFFFFF,
        0);

    if(!samplerBinding) {
        TF_FATAL_CODING_ERROR("Could not bind a sampler to the shader?!");
    }

    MtlfMetalContext::GetMetalContext()->SetSampler(
        samplerBinding->_index,
        samplerId,
        samplerNameToken,
        samplerBinding->_stage);
}

void
HdStGLSLProgramMSL::BindBuffer(
    const TfToken &name,
    id<MTLBuffer> bufferId,
    bool fragment) const
{
    std::string bufferName("bufferBind_" + name.GetString());
    TfToken bufferNameToken(bufferName, TfToken::Immortal);
   
    MSL_ShaderBinding const* const bufferBinding = MSL_FindBinding(
        _bindingMap,
        bufferNameToken,
        kMSL_BindingType_UniformBuffer,
        0xFFFFFFFF,
        0);

    if(!bufferBinding) {
        TF_FATAL_CODING_ERROR("Could not bind a buffer to the shader?!");
    }
 
    if (fragment) {
        MtlfMetalContext::GetMetalContext()->SetFragmentBuffer(
            bufferBinding->_index,
            bufferId,
            bufferNameToken);
    } else {
        MtlfMetalContext::GetMetalContext()->SetVertexBuffer(
            bufferBinding->_index,
            bufferId,
            bufferNameToken);
    }
}

void HdStGLSLProgramMSL::BindResources(HdStSurfaceShader* surfaceShader, HdSt_ResourceBinder const &binder) const
{
    // XXX: there's an issue where other shaders try to use textures.
    std::string textureName;
    std::string samplerName;

    TF_FOR_ALL(it, surfaceShader->GetTextureDescriptors()) {
        //When more types are added to the switch below, don't forget to update the mask too.
        textureName = "textureBind_" + it->name.GetString();
        TfToken textureNameToken(textureName, TfToken::Immortal);

        MSL_ShaderBinding const* const textureBinding = MSL_FindBinding(
            _bindingMap, textureNameToken, kMSL_BindingType_Texture, 0xFFFFFFFF,
            0);
        if(!textureBinding)
        {
            TF_FATAL_CODING_ERROR("Could not bind a texture to the shader?!");
        }

        HdBinding::Type type = textureBinding->_binding.GetType();
        HdStTextureResourceSharedPtr const & textureResource =
            it->handle->GetTextureResource();
        
        if (type == HdBinding::TEXTURE_UDIM_LAYOUT ||
            type == HdBinding::TEXTURE_PTEX_LAYOUT) {
            BindTexture(it->name, textureResource->GetLayoutTextureId());
        }
        else {
            BindTexture(it->name, textureResource->GetTexelsTextureId());
            BindSampler(it->name, textureResource->GetTexelsSamplerId());
        }
    }
}

void HdStGLSLProgramMSL::UnbindResources(HdStSurfaceShader* surfaceShader, HdSt_ResourceBinder const &binder) const
{
    // Nothing
}

void HdStGLSLProgramMSL::SetProgram(char const* const label) {
    
    MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();
    
    context->SetShadingPrograms(_vertexFunction,
                                _fragmentFunction,
                                (_buildTarget == kMSL_BuildTarget_MVA || _buildTarget == kMSL_BuildTarget_MVA_ComputeGS));
    
    if (_buildTarget == kMSL_BuildTarget_MVA_ComputeGS) {
         context->SetGSProgram(_computeGeometryFunction);
    }
    
    // Ignore a compute program being set as it will be provided directly to SetComputeEncoderState (may revisit later)
    if (_computeFunction) {
        return;
    }
    
    if (_currentlySet) {
        _currentlySet = true;
    }
    _currentlySet = true;
    
    //Create defaults for old-style uniforms
    struct _LoopParameters {
        TfToken uniformToken;
        MSL_ProgramStage stage;
    } static const loopParams[2] = {
        { TfToken("fsUniforms"), kMSL_ProgramStage_Fragment },
        { TfToken("vsUniforms"), kMSL_ProgramStage_Vertex }
    };
    
    for(UInt32 i = 0; i < (sizeof(loopParams) / sizeof(loopParams[0])); i++) {
        auto it_range = _bindingMap.equal_range(loopParams[i].uniformToken.Hash());
        for(auto it = it_range.first; it != it_range.second; ++it) {
            const MSL_ShaderBinding& binding = *(*it).second;
            if(binding._stage != loopParams[i].stage || binding._type != kMSL_BindingType_UniformBuffer)
                continue;

            context->SetOldStyleUniformBuffer(binding._index, loopParams[i].stage, binding._uniformBufferSize);
        }
    }
}

void HdStGLSLProgramMSL::UnsetProgram() {
    MtlfMetalContext::GetMetalContext()->ClearRenderEncoderState();

    if (!_currentlySet) {
        _currentlySet = false;
    }
    _currentlySet = false;
}


void HdStGLSLProgramMSL::DrawElementsInstancedBaseVertex(int primitiveMode,
                                                     int indexCount,
                                                     int indexType,
                                                     int firstIndex,
                                                     int instanceCount,
                                                     int baseVertex) const {
    MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();
    id<MTLBuffer> indexBuffer = context->GetIndexBuffer();
    const bool doMVAComputeGS = _buildTarget == kMSL_BuildTarget_MVA_ComputeGS;
    const bool doMVA = doMVAComputeGS || _buildTarget == kMSL_BuildTarget_MVA;
    
    MTLIndexType indexTypeMetal;
    int indexSize;
    switch(indexType) {
        default:
            TF_FATAL_CODING_ERROR("Not Implemented");
            return;
        case GL_UNSIGNED_SHORT:
            if(doMVA) {  //MTL_FIXME: We should probably find a way to support this at some point.
                TF_FATAL_CODING_ERROR("Not Implemented");
            }
            indexTypeMetal = MTLIndexTypeUInt16;
            indexSize = sizeof(uint16_t);
            break;
        case GL_UNSIGNED_INT:
            indexTypeMetal = MTLIndexTypeUInt32;
            indexSize = sizeof(uint32_t);
            break;
    }

    MTLPrimitiveType primType = GetMetalPrimType(primitiveMode);
    bool const drawingQuads = (primitiveMode == GL_LINES_ADJACENCY);
    bool const tempPointsWorkaround = context->IsTempPointWorkaroundActive();

    uint32_t numOutVertsPerInPrim(3), numOutPrimsPerInPrim(1);
    if (drawingQuads) {
        if (!doMVA) {
            indexCount = (indexCount * 6) / 4;
            firstIndex = (firstIndex * 6) / 4;
            if (!tempPointsWorkaround) {
                indexBuffer = context->GetQuadIndexBuffer(indexTypeMetal);
            }
        }
        else if(doMVAComputeGS) {
            numOutVertsPerInPrim = 6;
            numOutPrimsPerInPrim = 2;
        }
    }
    
    if (tempPointsWorkaround) {
        primType = MTLPrimitiveTypePoint;
        if (indexBuffer == nil) {
            firstIndex = 0;
            indexBuffer = context->GetPointIndexBuffer(indexTypeMetal, indexCount, drawingQuads);
        }
    }

    const uint32_t vertsPerPrimitive = (drawingQuads && doMVAComputeGS) ? 4 : 3;
    uint32_t numPrimitives = (indexCount / vertsPerPrimitive) * instanceCount;
    const uint32_t maxPrimitivesPerPart = doMVAComputeGS ? context->GetMaxComputeGSPartSize(numOutVertsPerInPrim, numOutPrimsPerInPrim, _gsVertOutStructSize, _gsPrimOutStructSize) : numPrimitives;

    int maxThreadsPerThreadgroup = 0;
    bool useDispatchThreads = true;//[context->device supportsFeatureSet:METAL_FEATURESET_FOR_DISPATCHTHREADS];
    if (doMVAComputeGS && !useDispatchThreads) {
        maxThreadsPerThreadgroup = METAL_GS_THREADGROUP_SIZE;
    }

    int const maxThreadsPerGroup = 32;
    
    if (maxThreadsPerThreadgroup > maxThreadsPerGroup) {
        maxThreadsPerThreadgroup = maxThreadsPerGroup;
    }

    id<MTLTexture> texture = context->GetRenderPassDescriptor().colorAttachments[0].texture;
    float renderTargetWidth = texture.width;
    float renderTargetHeight = texture.height;
    
    uint32_t partIndexOffset = 0;
    while(numPrimitives > 0) {
        uint32_t numPrimitivesInPart = MIN(numPrimitives, maxPrimitivesPerPart);

        if (doMVAComputeGS && !useDispatchThreads && (numPrimitivesInPart > maxThreadsPerThreadgroup)) {
            numPrimitivesInPart = numPrimitivesInPart / maxThreadsPerThreadgroup * maxThreadsPerThreadgroup;
        }
        
        uint32_t const numIndicesInPart = numPrimitivesInPart * vertsPerPrimitive;
        
        uint32_t const gsVertDataSize = numPrimitivesInPart * numOutVertsPerInPrim * _gsVertOutStructSize;
        uint32_t const gsPrimDataSize = numPrimitivesInPart * numOutPrimsPerInPrim * _gsPrimOutStructSize;
        id<MTLBuffer> gsDataBuffer = nil;
        uint32_t gsVertDataOffset(0), gsPrimDataOffset(0);
        if(doMVAComputeGS) {
            context->PrepareForComputeGSPart(gsVertDataSize, gsPrimDataSize, gsDataBuffer, gsVertDataOffset, gsPrimDataOffset);
        }
        
        id<MTLRenderCommandEncoder>  renderEncoder = context->GetRenderEncoder(METALWORKQUEUE_DEFAULT);

        const_cast<HdStGLSLProgramMSL*>(this)->BakeState();

        id<MTLComputeCommandEncoder> computeEncoder = doMVAComputeGS ? context->GetComputeEncoder(METALWORKQUEUE_GEOMETRY_SHADER) : nil;
        
        if(doMVA) {
            //Setup Draw Args on the render context
            struct { uint32_t _indexCount, _startIndex, _baseVertex, _instanceCount, _batchIndexOffset, _primitiveCount, _batchPrimitiveOffset; }
            drawArgs = { (uint32_t)indexCount,
                         (uint32_t)firstIndex,
                         (uint32_t)baseVertex,
                         (uint32_t)instanceCount,
                         partIndexOffset,
                         indexCount / vertsPerPrimitive,
                         partIndexOffset / vertsPerPrimitive,
            };
            [renderEncoder setVertexBytes:(const void*)&drawArgs
                                   length:sizeof(drawArgs)
                                  atIndex:_drawArgsSlot];
            
            if ((tempPointsWorkaround && _indicesSlot >= 0) ||
                _reapplyIndexBuffer) {
                [renderEncoder setVertexBuffer:indexBuffer offset:0 atIndex:_indicesSlot];
            }
    
            if(doMVAComputeGS) {
                //Setup Draw Args on the compute context
                [computeEncoder setBytes:(const void*)&drawArgs length:sizeof(drawArgs) atIndex:_drawArgsSlot];

                [computeEncoder setBuffer:gsDataBuffer offset:gsVertDataOffset atIndex:_gsVertOutBufferSlot];
                [computeEncoder setBuffer:gsDataBuffer offset:gsPrimDataOffset atIndex:_gsPrimOutBufferSlot];
                [renderEncoder setVertexBuffer:gsDataBuffer offset:gsVertDataOffset atIndex:_gsVertOutBufferSlot];
                [renderEncoder setVertexBuffer:gsDataBuffer offset:gsPrimDataOffset atIndex:_gsPrimOutBufferSlot];
                [renderEncoder setFragmentBuffer:gsDataBuffer offset:gsVertDataOffset atIndex:_gsVertOutBufferSlot];
                [renderEncoder setFragmentBuffer:gsDataBuffer offset:gsPrimDataOffset atIndex:_gsPrimOutBufferSlot];
                
                if (tempPointsWorkaround && _indicesSlot >= 0) {
                    [computeEncoder setBuffer:indexBuffer offset:0 atIndex:_indicesSlot];
                }
            }
        }
        
        //Setup Frag Extras on the render context
        struct { float _renderTargetWidth, _renderTargetHeight; }
        fragExtraArgs = { renderTargetWidth, renderTargetHeight };
        [renderEncoder setFragmentBytes:(const void*)&fragExtraArgs
                                 length:sizeof(fragExtraArgs)
                                atIndex:_fragExtrasSlot];
        
        if(doMVAComputeGS) {
            if (useDispatchThreads) {
                [computeEncoder dispatchThreads:MTLSizeMake(numPrimitivesInPart, 1, 1)
                          threadsPerThreadgroup:MTLSizeMake(MIN(numPrimitivesInPart, METAL_GS_THREADGROUP_SIZE), 1, 1)];
            }
            else {
                MTLSize threadgroupCount = MTLSizeMake(fmin(maxThreadsPerThreadgroup, numPrimitivesInPart), 1, 1);
                MTLSize threadsPerGrid   = MTLSizeMake(numPrimitivesInPart / threadgroupCount.width, 1, 1);

                [computeEncoder dispatchThreadgroups:threadsPerGrid threadsPerThreadgroup:threadgroupCount];
            }
            
            [renderEncoder drawPrimitives:primType
                              vertexStart:0
                              vertexCount:(numPrimitivesInPart * numOutVertsPerInPrim)];
        }
        else if(doMVA) {
            if (instanceCount == 1) {
                [renderEncoder drawPrimitives:primType
                                  vertexStart:0
                                  vertexCount:indexCount];
            }
            else {
                [renderEncoder drawPrimitives:primType
                                  vertexStart:0
                                  vertexCount:indexCount
                                instanceCount:instanceCount
                                 baseInstance:0];
            }
        }
        else {
            if (instanceCount == 1) {
                [renderEncoder drawIndexedPrimitives:primType
                                          indexCount:indexCount
                                           indexType:indexTypeMetal
                                         indexBuffer:indexBuffer
                                   indexBufferOffset:(firstIndex * indexSize)];
            }
            else {
                [renderEncoder drawIndexedPrimitives:primType
                                          indexCount:indexCount
                                           indexType:indexTypeMetal
                                         indexBuffer:indexBuffer
                                   indexBufferOffset:(firstIndex * indexSize)
                                       instanceCount:instanceCount
                                          baseVertex:baseVertex
                                        baseInstance:0];
            }
        }

        if (doMVAComputeGS) {
            context->ReleaseEncoder(false, METALWORKQUEUE_GEOMETRY_SHADER);
        }
        context->ReleaseEncoder(false, METALWORKQUEUE_DEFAULT);
        
        numPrimitives -= numPrimitivesInPart;
        partIndexOffset += numIndicesInPart;
    }
    
    context->IncNumberPrimsDrawn((indexCount / vertsPerPrimitive) * instanceCount, false);
}

void HdStGLSLProgramMSL::DrawArraysInstanced(int primitiveMode,
                                         int baseVertex,
                                         int vertexCount,
                                         int instanceCount) const {
    MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();
    if (primitiveMode == GL_POINTS) {
        vertexCount += 2;
    }
    context->SetIndexBuffer(context->GetTriListIndexBuffer(MTLIndexTypeUInt32, vertexCount / 3));

    _reapplyIndexBuffer = _indicesSlot >= 0;
    DrawElementsInstancedBaseVertex(primitiveMode, vertexCount, GL_UNSIGNED_INT, 0, instanceCount, baseVertex);
    _reapplyIndexBuffer = false;

    return;
}

void HdStGLSLProgramMSL::DrawArrays(int primitiveMode,
                                int baseVertex,
                                int vertexCount) const {
    
    MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();
    
    MTLPrimitiveType primType = GetMetalPrimType(primitiveMode);
    
    // Possibly move this outside this function as we shouldn't need to get a render encoder every draw call
    id <MTLRenderCommandEncoder> renderEncoder = context->GetRenderEncoder();
    
    const_cast<HdStGLSLProgramMSL*>(this)->BakeState();
    
    [renderEncoder drawPrimitives:primType vertexStart:baseVertex vertexCount:vertexCount];
    
    context->ReleaseEncoder(false);
    
    bool const drawingQuads = (primitiveMode == GL_LINES_ADJACENCY);
    uint32_t const vertsPerPrimitive = drawingQuads ? 4:3;

    context->IncNumberPrimsDrawn(vertexCount / vertsPerPrimitive, false);
}

void HdStGLSLProgramMSL::BakeState()
{
    MtlfMetalContext::GetMetalContext()->SetRenderEncoderState();
}

std::string HdStGLSLProgramMSL::GetComputeHeader() const
{
    return HdSt_CodeGenMSL::GetComputeHeader();
}

PXR_NAMESPACE_CLOSE_SCOPE

