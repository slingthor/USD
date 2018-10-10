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

#include "pxr/imaging/garch/glslfx.h"

#include "pxr/imaging/hdSt/Metal/mslProgram.h"
#include "pxr/imaging/hdSt/package.h"
#include "pxr/imaging/hdSt/surfaceShader.h"

#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/resourceRegistry.h"
#include "pxr/imaging/hd/tokens.h"
#include "pxr/base/tf/diagnostic.h"
#include "pxr/base/tf/envSetting.h"
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
        case GL_PATCHES:
            primType = MTLPrimitiveTypePoint;
            TF_FATAL_CODING_ERROR("Not Implemented");
            break;
    }
    
    return primType;
}

const MSL_ShaderBinding& MSL_FindBinding(const MSL_ShaderBindingMap& bindings, const TfToken& name, bool& outFound, uint bindingTypeMask, uint programStageMask, uint skipCount, int level)
{
    TfToken nameToFind;
    if (level < 0) {
        nameToFind = name;
    }
    else {
        // follow nested instancing naming convention.
        std::stringstream n;
        n << name << "_" << level;
        nameToFind = TfToken(n.str());
    }
    auto it_range = bindings.equal_range(nameToFind.Hash());
    auto it = it_range.first;
    outFound = false;
    for(; it != it_range.second; ++it) {
        if( ((*it).second->_type & bindingTypeMask) == 0 ||
            ((*it).second->_stage & programStageMask) == 0 ||
            skipCount-- != 0)
            continue;
        outFound = true;
        break;
    }
    return *(*it).second;
}

HdStMSLProgram::HdStMSLProgram(TfToken const &role)
: HdStProgram(role)
, _role(role)
, _vertexFunction(nil), _fragmentFunction(nil), _computeFunction(nil), _computeGeometryFunction(nil)
, _vertexFunctionIdx(0), _fragmentFunctionIdx(0), _computeFunctionIdx(0), _computeGeometryFunctionIdx(0)
, _valid(false)
, _uniformBuffer(role)
, _buildTarget(kMSL_BuildTarget_Regular)
, _gsVertOutBufferSlot(-1), _gsPrimOutBufferSlot(-1), _gsVertOutStructSize(-1), _gsPrimOutStructSize(-1)
, _drawArgsSlot(-1), _indicesSlot(-1)
, _currentlySet(false)
{
}

HdStMSLProgram::~HdStMSLProgram()
{
    for(auto it = _bindingMap.begin(); it != _bindingMap.end(); ++it)
        delete (*it).second;
    _bindingMap.clear();

    id<MTLBuffer> uniformBuffer = _uniformBuffer.GetId();
    if (uniformBuffer) {
        [uniformBuffer release];
        uniformBuffer = nil;
        _uniformBuffer.SetAllocation(HdResourceGPUHandle(), 0);
    }
}

#if GENERATE_METAL_DEBUG_SOURCE_CODE

NSUInteger dumpedFileCount = 0;

const HdStProgram* previousProgram = 0;
NSUInteger totalPrograms = 0;

void DumpMetalSource(const HdStProgram* program, NSString *metalSrc, NSString *fileSuffix, NSString *compilerMessages)
{
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
#else
#define DumpMetalSource(a, b, c, d)
#endif

bool
HdStMSLProgram::CompileShader(GLenum type,
                              std::string const &shaderSource)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    // early out for empty source.
    // this may not be an error, since glslfx gives empty string
    // for undefined shader stages (i.e. null geometry shader)
    if (shaderSource.empty()) return false;
    
    const char *shaderType = NULL;
    switch (type) {
        case GL_TESS_CONTROL_SHADER:
        case GL_TESS_EVALUATION_SHADER:
            //TF_CODING_ERROR("Unsupported shader type on Metal %d\n", type);
            NSLog(@"Unsupported shader type on Metal %d\n", type); //MTL_FIXME - remove the above error so it doesn't propogate all the way back but really we should never see these types of shaders
            DumpMetalSource(this, [NSString stringWithUTF8String:shaderSource.c_str()], @"InvalidType", nil); //MTL_FIXME
            return true;
        default:
            break;
    }

    // create a shader, compile it
    NSError *error = NULL;
    id<MTLDevice> device = MtlfMetalContext::GetMetalContext()->device;
    
    bool success = true;
    NSString *entryPoint = nil;
    switch (type) {
    case GL_VERTEX_SHADER: shaderType = "VS"; entryPoint = @"vertexEntryPoint"; break;
    case GL_FRAGMENT_SHADER: shaderType = "FS"; entryPoint = @"fragmentEntryPoint"; break;
    case GL_GEOMETRY_SHADER: shaderType = "Compute_GS"; entryPoint = @"computeEntryPoint"; break;
    case GL_COMPUTE_SHADER: shaderType = "CS"; entryPoint = @"computeEntryPoint"; break;
    default: TF_FATAL_CODING_ERROR("Not allowed!");
    }
    
    if (TfDebug::IsEnabled(HD_DUMP_SHADER_SOURCE)) {
        std::cout   << "--------- " << shaderType << " ----------\n"
                    << shaderSource
                    << "---------------------------\n"
                    << std::flush;
    }
    
    MTLCompileOptions *options = [[MTLCompileOptions alloc] init];
    options.fastMathEnabled = YES;
    options.languageVersion = MTLLanguageVersion2_0;
    options.preprocessorMacros = @{
        @"HD_MTL_VERTEXSHADER":(type==GL_VERTEX_SHADER)?@1:@0,
        @"HD_MTL_COMPUTESHADER":(type==GL_GEOMETRY_SHADER || type==GL_COMPUTE_SHADER)?@1:@0,
        @"HD_MTL_FRAGMENTSHADER":(type==GL_FRAGMENT_SHADER)?@1:@0,
    };

    id<MTLLibrary> library = [device newLibraryWithSource:@(shaderSource.c_str())
                                                  options:options
                                                    error:&error];
    
    [options release];
    options = nil;

    // Load the function into the library
    id <MTLFunction> function = [library newFunctionWithName:entryPoint];
    std::string filePostFix = shaderType;
    if (!function) {
        // XXX:validation
        TF_WARN("Failed to compile shader (%s): \n%s",
                shaderType, [[error localizedDescription] UTF8String]);
        filePostFix += "_Fail";
        success = false;
    }
    
    //MTL_FIXME: Remove this debug line once done.
    DumpMetalSource(this, [NSString stringWithUTF8String:shaderSource.c_str()], [NSString stringWithUTF8String:filePostFix.c_str()], error != nil ? [error localizedDescription] : nil);
    
    if (type == GL_VERTEX_SHADER) {
        _vertexFunction = function;
        _vertexFunctionIdx = dumpedFileCount;
    } else if (type == GL_FRAGMENT_SHADER) {
        _fragmentFunction = function;
        _fragmentFunctionIdx = dumpedFileCount;
    } else if (type == GL_COMPUTE_SHADER) {
        _computeFunction = function;
        _computeFunctionIdx = dumpedFileCount;
    } else if (type == GL_GEOMETRY_SHADER) {
        _computeGeometryFunction = function;
        _computeGeometryFunctionIdx = dumpedFileCount;
    }

    return success;
}

bool
HdStMSLProgram::Link()
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
        
    
    id<MTLDevice> device = MtlfMetalContext::GetMetalContext()->device;

    // update the program resource allocation
    _valid = true;
    
    // create an uniform buffer
    id<MTLBuffer> uniformBuffer = _uniformBuffer.GetId();
    if (uniformBuffer == 0) {
        int const defaultLength = 1024;
        uniformBuffer = [device newBufferWithLength:defaultLength options:MTLResourceStorageModeManaged];
        _uniformBuffer.SetAllocation(uniformBuffer, defaultLength);
    }
    
    for(auto it = _bindingMap.begin(); it != _bindingMap.end(); ++it) {
        const MSL_ShaderBinding& binding = *(*it).second;
        if(binding._stage == kMSL_ProgramStage_Vertex || binding._stage == kMSL_ProgramStage_Compute) {
            if(binding._type == kMSL_BindingType_DrawArgs) _drawArgsSlot = binding._index;
            if(binding._type == kMSL_BindingType_IndexBuffer) _indicesSlot = binding._index;
            if(binding._type == kMSL_BindingType_GSVertOutput) _gsVertOutBufferSlot = binding._index;
            if(binding._type == kMSL_BindingType_GSPrimOutput) _gsPrimOutBufferSlot = binding._index;
        }
    }

    return true;
}

bool
HdStMSLProgram::GetProgramLinkStatus(std::string * reason) const
{
    return _valid;
}

bool
HdStMSLProgram::Validate() const
{
    return _valid;
}

void HdStMSLProgram::AssignUniformBindings(GarchBindingMapRefPtr bindingMap) const
{
    MtlfBindingMapRefPtr mtlfBindingMap(TfDynamic_cast<MtlfBindingMapRefPtr>(bindingMap));
    
    for (GarchBindingMap::UniformBindingMap::value_type& p : mtlfBindingMap->_uniformBindings) {
        auto it_range = _bindingMap.equal_range(p.first.Hash());
        for(auto it = it_range.first; it != it_range.second; ++it) {
            const MSL_ShaderBinding& binding = *(*it).second;
            if(binding._type != kMSL_BindingType_UniformBuffer)
                continue;
            MtlfBindingMap::MTLFBindingIndex mtlfIndex(binding._index, (uint32)binding._type, (uint32)binding._stage, true);
            p.second = mtlfIndex.asInt;
        }
    }
}

void HdStMSLProgram::AssignSamplerUnits(GarchBindingMapRefPtr bindingMap) const
{
    //Samplers really means OpenGL style samplers (ancient style) where a sampler is both a texture and an actual sampler.
    //For us this means a texture always needs to have an accompanying sampler that is bound to the same slot index.
    //This way when an index is returned it can be used for both.
    
    MtlfBindingMapRefPtr mtlfBindingMap(TfDynamic_cast<MtlfBindingMapRefPtr>(bindingMap));
    
    for (GarchBindingMap::SamplerBindingMap::value_type& p : mtlfBindingMap->_samplerBindings) {
        auto it_range = _bindingMap.equal_range(p.first.Hash());
        for(auto it = it_range.first; it != it_range.second; ++it) {
            const MSL_ShaderBinding& binding = *(*it).second;
            if(binding._type != kMSL_BindingType_Texture)
                continue;
            MtlfBindingMap::MTLFBindingIndex mtlfIndex(binding._index, (uint32)binding._type, (uint32)binding._stage, true);
            p.second = mtlfIndex.asInt;
        }
    }
}

void HdStMSLProgram::AddBinding(std::string const &name, int index, MSL_BindingType bindingType, MSL_ProgramStage programStage, int offsetWithinResource, int uniformBufferSize) {
    _locationMap.insert(make_pair(name, index));
    MSL_ShaderBinding* newBinding = new MSL_ShaderBinding(bindingType, programStage, index, name, offsetWithinResource, uniformBufferSize);
    _bindingMap.insert(std::make_pair(newBinding->_nameToken.Hash(), newBinding));
}

void HdStMSLProgram::UpdateUniformBinding(std::string const &name, int index) {
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

void HdStMSLProgram::AddCustomBindings(GarchBindingMapRefPtr bindingMap) const
{
    MtlfBindingMapRefPtr mtlfBindingMap(TfDynamic_cast<MtlfBindingMapRefPtr>(bindingMap));
    
    TF_FATAL_CODING_ERROR("Not Implemented");
}

void HdStMSLProgram::BindResources(HdStSurfaceShader* surfaceShader, HdSt_ResourceBinder const &binder) const
{
    // XXX: there's an issue where other shaders try to use textures.
    TF_FOR_ALL(it, surfaceShader->GetTextureDescriptors()) {
        uint i = 0;
        while(1)
        {
            //When more types are added to the switch below, don't forget to update the mask too.
            bool found;
            std::string textureName = "texture2d_" + it->name.GetString();
            std::string samplerName = "sampler2d_" + it->name.GetString();
            TfToken textureNameToken(textureName);
            TfToken samplerNameToken(samplerName);

            MSL_ShaderBinding const& textureBinding = MSL_FindBinding(_bindingMap, textureNameToken, found, kMSL_BindingType_Texture, 0xFFFFFFFF, i);
            if(!found)
                break;

            MSL_ShaderBinding const& samplerBinding = MSL_FindBinding(_bindingMap, samplerNameToken, found, kMSL_BindingType_Sampler, 0xFFFFFFFF, i);
            if(!found)
                break;
            
            MtlfMetalContext::GetMetalContext()->SetTexture(textureBinding._index, it->handle, textureNameToken, textureBinding._stage);
            MtlfMetalContext::GetMetalContext()->SetSampler(samplerBinding._index, it->sampler, samplerNameToken, samplerBinding._stage);

            i++;
            break;
        }
        
        if(i == 0)
            TF_FATAL_CODING_ERROR("Could not bind a texture to the shader?!");
    
//        HdBinding binding = binder.GetBinding(it->name);
//        // XXX: put this into resource binder.
//        if (binding.GetType() == HdBinding::TEXTURE_2D ||
//            binding.GetType() == HdBinding::TEXTURE_PTEX_TEXEL) {
//            MtlfMetalContext::GetMetalContext()->SetTexture(binding.GetLocation(), it->handle);
//            MtlfMetalContext::GetMetalContext()->SetSampler(binding.GetLocation(), it->sampler);
//        } else if (binding.GetType() == HdBinding::TEXTURE_PTEX_LAYOUT) {
//            TF_FATAL_CODING_ERROR("Not Implemented");
//            // glActiveTexture(GL_TEXTURE0 + samplerUnit);
//            // glBindTexture(GL_TEXTURE_BUFFER, it->handle);
//            //glProgramUniform1i(_program, binding.GetLocation(), samplerUnit);
//        }
    }
}

void HdStMSLProgram::UnbindResources(HdStSurfaceShader* surfaceShader, HdSt_ResourceBinder const &binder) const
{
    // Nothing
}

void HdStMSLProgram::SetProgram(char const* const label) {
    
    MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();
    
    context->SetShadingPrograms(_vertexFunction,
                                _fragmentFunction,
                                (_buildTarget == kMSL_BuildTarget_MVA || _buildTarget == kMSL_BuildTarget_MVA_ComputeGS));
    
    if (_buildTarget == kMSL_BuildTarget_MVA_ComputeGS) {
         context->SetGSProgram(_computeGeometryFunction);
    }
    
    if (_currentlySet) {
        TF_FATAL_CODING_ERROR("HdStProgram is already set");
    }
    _currentlySet = true;
    
    // Ignore a compute program being set as it will be provided directly to SetComputeEncoderState (may revisit later)
    if (_computeFunction) {
        return;
    }
    
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

            id<MTLBuffer> mtlBuffer = [context->device newBufferWithLength:(binding._uniformBufferSize * METAL_OLD_STYLE_UNIFORM_BUFFER_SIZE) options:MTLResourceStorageModeManaged];
            context->SetUniformBuffer(binding._index, mtlBuffer, binding._nameToken, loopParams[i].stage, 0 /*offset*/, binding._uniformBufferSize);
            _buffers.push_back(mtlBuffer);
        }
    }
}

void HdStMSLProgram::UnsetProgram() {
    MtlfMetalContext::GetMetalContext()->ClearRenderEncoderState();
    
    if (!_currentlySet) {
        TF_FATAL_CODING_ERROR("HdStProgram wasn't previously set, or has already been unset");
    }
    _currentlySet = false;

    for(auto buffer : _buffers) {
        [buffer release];
    }
    _buffers.clear();
}


void HdStMSLProgram::DrawElementsInstancedBaseVertex(GLenum primitiveMode,
                                                      int indexCount,
                                                      GLint indexType,
                                                      GLint firstIndex,
                                                      GLint instanceCount,
                                                      GLint baseVertex) const {
    
    /*
     Currently quads get mapped to GL_LINES_ADJACENCY - presumably this is a bit of a hack as GL_QUADS aren't supported by OpenGL > 3.x and it's been
     done because they share the same number of vertices per prim (4). This doesn't appear to be a problem for OpenGL as the OpenGL subdiv doesn't genereate
     PRIM_MESH_COARSE/REFINED_QUADS (at least for the content we've currently seen) but Metal *does* so we need a way of drawing quads. We currently do this by
     remapping the index buffer and mapping GL_LINES_ADJACENCY to MTLPrimitiveTypeTriangle.
    */
    MTLPrimitiveType primType = GetMetalPrimType(primitiveMode);
    const bool bDrawingQuads = (primitiveMode == GL_LINES_ADJACENCY);
    const bool doMVAComputeGS = _buildTarget == kMSL_BuildTarget_MVA_ComputeGS;
    const bool doMVA = doMVAComputeGS || _buildTarget == kMSL_BuildTarget_MVA;
    
    MTLIndexType indexTypeMetal;
    int indexSize;
    switch(indexType) {
        default:
            TF_FATAL_CODING_ERROR("Not Implemented");
            return;
        case GL_UNSIGNED_SHORT:
            if(doMVA)   //MTL_FIXME: We should probably find a way to support this at some point.
                TF_FATAL_CODING_ERROR("Not Implemented");
            indexTypeMetal = MTLIndexTypeUInt16;
            indexSize = sizeof(uint16_t);
            break;
        case GL_UNSIGNED_INT:
            indexTypeMetal = MTLIndexTypeUInt32;
            indexSize = sizeof(uint32_t);
            break;
    }
  
    MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();

    id<MTLBuffer> indexBuffer = context->GetIndexBuffer();
    if(bDrawingQuads) {
        indexCount = (indexCount / 4) * 6;
        firstIndex = (firstIndex / 4) * 6;
        indexBuffer = context->GetQuadIndexBuffer(indexTypeMetal);
    }
    // Possibly move this outside this function as we shouldn't need to get a render encoder every draw call
    id <MTLRenderCommandEncoder>    renderEncoder = context->GetRenderEncoder();
    id <MTLComputeCommandEncoder>   computeEncoder;
    
    //Encode a dependency on the Geometry Shader queue to ensure the GS data is there.
    if(doMVAComputeGS) {
        renderEncoder = nil;
        context->ReleaseEncoder(true);
        context->EncodeWaitForQueue(METALWORKQUEUE_DEFAULT, METALWORKQUEUE_GEOMETRY_SHADER);
        
        //Patch the drescriptor to prevent clearing attachments we just rendered to.
        MTLRenderPassDescriptor* rpd = context->GetRenderPassDescriptor();
        if(rpd.depthAttachment != nil)
            rpd.depthAttachment.loadAction = MTLLoadActionLoad;
        if(rpd.stencilAttachment != nil)
            rpd.stencilAttachment.loadAction = MTLLoadActionLoad;
        for(int i = 0; i < 8; i++) {
            if(rpd.colorAttachments[i] != nil)
                rpd.colorAttachments[i].loadAction = MTLLoadActionLoad;
        }
        context->SetRenderPassDescriptor(rpd);
        
        renderEncoder = context->GetRenderEncoder();
    }
    
    const_cast<HdStMSLProgram*>(this)->BakeState();
    
    if(doMVA) {
        //Setup Draw Args on the render context
        struct { int _indexCount, _startIndex, _baseVertex, _instanceCount; } drawArgs = { indexCount, firstIndex, baseVertex, instanceCount };
        [renderEncoder setVertexBytes:(const void*)&drawArgs length:sizeof(drawArgs) atIndex:_drawArgsSlot];
        //context->SetDrawArgsBuffer(indexCount, firstIndex, baseVertex, _drawArgsSlot, doMVAComputeGS);
        
        if(doMVAComputeGS) {
            // Get a compute encoder on the Geometry Shader work queue
            computeEncoder = context->GetComputeEncoder(METALWORKQUEUE_GEOMETRY_SHADER);
            
            //Setup Draw Args on the compute context
            [computeEncoder setBytes:(const void*)&drawArgs length:sizeof(drawArgs) atIndex:_drawArgsSlot];
            
            //MTL_FIXME: Would like to prevent re-creating the buffers each draw-call. Would be better to add a cache of old buffers.
            //           Would be even better if alternate compute/render is implemented with cut up draws to keep GS output in L2. Re-
            //           -use would then be possible with a single constant size buffer (sized to whatever is favorable for L2).
            id<MTLDevice> device = context->device;
            const int vertDataSize(_gsVertOutStructSize * indexCount),
                      primDataSize(_gsPrimOutStructSize * (indexCount / 3));
            id<MTLBuffer> vertBuffer = [device newBufferWithLength:vertDataSize options:MTLResourceStorageModePrivate|MTLResourceOptionCPUCacheModeDefault];
            id<MTLBuffer> primBuffer = [device newBufferWithLength:primDataSize options:MTLResourceStorageModePrivate|MTLResourceOptionCPUCacheModeDefault];
            
            [computeEncoder setBuffer:vertBuffer offset:0 atIndex:_gsVertOutBufferSlot];
            [computeEncoder setBuffer:primBuffer offset:0 atIndex:_gsPrimOutBufferSlot];
            
            [renderEncoder setVertexBuffer:vertBuffer offset:0 atIndex:_gsVertOutBufferSlot];
            [renderEncoder setVertexBuffer:primBuffer offset:0 atIndex:_gsPrimOutBufferSlot];
            [renderEncoder setFragmentBuffer:vertBuffer offset:0 atIndex:_gsVertOutBufferSlot];
            [renderEncoder setFragmentBuffer:primBuffer offset:0 atIndex:_gsPrimOutBufferSlot];
            
            [vertBuffer release];
            [primBuffer release];
        }
    }
    
    if(doMVAComputeGS)
    {
        [computeEncoder dispatchThreads:MTLSizeMake((indexCount / 3) * instanceCount, 1, 1) threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];

        [renderEncoder drawPrimitives:primType vertexStart:0 vertexCount:indexCount instanceCount:instanceCount baseInstance:0];
    }
    else if(doMVA)
        [renderEncoder drawPrimitives:primType vertexStart:0 vertexCount:indexCount instanceCount:instanceCount baseInstance:0];
    else
        [renderEncoder drawIndexedPrimitives:primType indexCount:indexCount indexType:indexTypeMetal indexBuffer:indexBuffer indexBufferOffset:(firstIndex * indexSize) instanceCount:instanceCount baseVertex:baseVertex baseInstance:0];

    context->ReleaseEncoder(false);
    
    if(doMVAComputeGS)
    {
        // Release the geometry shader encoder and encode the event
        computeEncoder = nil;
        context->ReleaseEncoder(false, METALWORKQUEUE_GEOMETRY_SHADER);
    }
}

void HdStMSLProgram::DrawArraysInstanced(GLenum primitiveMode,
                                          GLint baseVertex,
                                          GLint vertexCount,
                                          GLint instanceCount) const {
    
    MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();
    
    MTLPrimitiveType primType = GetMetalPrimType(primitiveMode);
    
    // Possibly move this outside this function as we shouldn't need to get a render encoder every draw call
    id <MTLRenderCommandEncoder> renderEncoder = context->GetRenderEncoder();
    
    const_cast<HdStMSLProgram*>(this)->BakeState();
    
    
    [renderEncoder drawPrimitives:primType vertexStart:baseVertex vertexCount:vertexCount instanceCount:instanceCount];
    
    context->ReleaseEncoder(false);
}

void HdStMSLProgram::DrawArrays(GLenum primitiveMode,
                                GLint baseVertex,
                                GLint vertexCount) const {
    
    MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();
    
    MTLPrimitiveType primType = GetMetalPrimType(primitiveMode);
    
    // Possibly move this outside this function as we shouldn't need to get a render encoder every draw call
    id <MTLRenderCommandEncoder> renderEncoder = context->GetRenderEncoder();
    
    const_cast<HdStMSLProgram*>(this)->BakeState();
    
    [renderEncoder drawPrimitives:primType vertexStart:baseVertex vertexCount:vertexCount];
    
    context->ReleaseEncoder(false);
}

void HdStMSLProgram::BakeState()
{
    MtlfMetalContext::GetMetalContext()->SetRenderEncoderState();
}

std::string HdStMSLProgram::GetComputeHeader() const
{
    return "#include <metal_stdlib>\nusing namespace metal;\n";
}

PXR_NAMESPACE_CLOSE_SCOPE

