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


const MSL_ShaderBinding& MSL_FindBinding(const MSL_ShaderBindings& bindings, const std::string& name, bool& outFound, uint bindingTypeMask, uint programStageMask, uint skipCount)
{
    outFound = false;
    auto it = bindings.begin();
    for(; it != bindings.end(); ++it)
    {
        if( (it->_type & bindingTypeMask) == 0 ||
            (it->_stage & programStageMask) == 0 ||
            it->_name != name ||
            skipCount-- != 0)
            continue;
        outFound = true;
        break;
    }
    return (*it);
}

HdStMSLProgram::HdStMSLProgram(TfToken const &role)
: HdStProgram(role)
, _role(role)
, _vertexFunction(nil)
, _fragmentFunction(nil)
, _computeFunction(nil)
, _vertexFunctionIdx(0), _fragmentFunctionIdx(0), _computeFunctionIdx(0)
, _valid(false)
, _uniformBuffer(role)
{
}

HdStMSLProgram::~HdStMSLProgram()
{
    id<MTLBuffer> uniformBuffer = _uniformBuffer.GetId();
    if (uniformBuffer) {
        [uniformBuffer release];
        uniformBuffer = nil;
        _uniformBuffer.SetAllocation(HdResourceGPUHandle(), 0);
    }
}

#if GENERATE_METAL_DEBUG_SOURCE_CODE

NSUInteger dumpedFileCount = 0;

void DumpMetalSource(NSString *metalSrc, NSString *fileSuffix, NSString *compilerMessages)
{
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
    
    NSString *fileName = [NSString stringWithFormat:@"HydraMetalSource_%lu_%@.metal", dumpedFileCount++, fileSuffix];
    NSString *srcDumpFilePath = [srcDumpLocation stringByAppendingPathComponent:fileName];
    [fileContents writeToFile:srcDumpFilePath atomically:YES encoding:NSUTF8StringEncoding error:nil];
    NSLog(@"Dumping Metal Source to %@", srcDumpFilePath);
}
#else
#define DumpMetalSource(a, b)
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
    NSString *entryPoint = nil;
    switch (type) {
        case GL_VERTEX_SHADER:
            shaderType = "Vertex Shader";
            entryPoint = @"vertexEntryPoint";
            break;
        case GL_FRAGMENT_SHADER:
            shaderType = "Fragment Shader";
            entryPoint = @"fragmentEntryPoint";
            break;
        case GL_COMPUTE_SHADER:
            shaderType = "Compute Shader";
            entryPoint = @"computeEntryPoint";
            break;
        case GL_TESS_CONTROL_SHADER:
        case GL_TESS_EVALUATION_SHADER:
        case GL_GEOMETRY_SHADER:
            //TF_CODING_ERROR("Unsupported shader type on Metal %d\n", type);
            NSLog(@"Unsupported shader type on Metal %d\n", type); //MTL_FIXME - remove the above error so it doesn't propogate all the way back but really we should never see these types of shaders
            DumpMetalSource([NSString stringWithUTF8String:shaderSource.c_str()], @"InvalidType", nil); //MTL_FIXME
            return true;
        default:
            TF_CODING_ERROR("Invalid shader type %d\n", type);
            return false;
    }

    if (TfDebug::IsEnabled(HD_DUMP_SHADER_SOURCE)) {
        std::cout << "--------- " << shaderType << " ----------\n";
        std::cout << shaderSource;
        std::cout << "---------------------------\n";
        std::cout << std::flush;
    }

    // create a shader, compile it
    NSError *error = NULL;
    id<MTLDevice> device = MtlfMetalContext::GetMetalContext()->device;
    
    MTLCompileOptions *options = [[MTLCompileOptions alloc] init];
    options.fastMathEnabled = YES;
    options.languageVersion = MTLLanguageVersion2_0;
    
    id<MTLLibrary> library = [device newLibraryWithSource:@(shaderSource.c_str())
                                                  options:options
                                                    error:&error];
    
    // Load the function into the library
     id <MTLFunction> function = [library newFunctionWithName:entryPoint];
    if (!function) {
        // XXX:validation
        TF_WARN("Failed to compile shader (%s): \n%s",
                shaderType, [[error localizedDescription] UTF8String]);
        DumpMetalSource([NSString stringWithUTF8String:shaderSource.c_str()], @"Fail", error != nil ? [error localizedDescription] : nil); //MTL_FIXME
        
        return false;
    }
    
    DumpMetalSource([NSString stringWithUTF8String:shaderSource.c_str()], [NSString stringWithUTF8String:shaderType], error != nil ? [error localizedDescription] : nil); //MTL_FIXME
    
    if (type == GL_VERTEX_SHADER) {
        _vertexFunction = function;
        _vertexFunctionIdx = dumpedFileCount;
    } else if (type == GL_FRAGMENT_SHADER) {
        _fragmentFunction = function;
        _fragmentFunctionIdx = dumpedFileCount;
    } else if (type == GL_COMPUTE_SHADER) {
        _computeFunction = function;
        _computeFunctionIdx = dumpedFileCount;
    }
    else {
        TF_FATAL_CODING_ERROR("Not Implemented");
        return false;
    }

    return true;
}

bool
HdStMSLProgram::Link()
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    bool vertexFuncPresent = _vertexFunction != nil;
    bool fragmentFuncPresent = _fragmentFunction != nil;
    bool computeFuncPresent = _computeFunction != nil;
    
    if (computeFuncPresent && (vertexFuncPresent ^ fragmentFuncPresent)) {
        TF_CODING_ERROR("A compute shader can't be set with a vertex shader or fragment shader also set.");
        return false;
    }
    
    if (vertexFuncPresent ^ fragmentFuncPresent) {
        TF_CODING_ERROR("Both a vertex shader and a fragment shader must be compiled before linking.");
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
        for(auto it = _bindings.begin(); it != _bindings.end(); ++it) {
            if((*it)._type != kMSL_BindingType_UniformBuffer || (*it)._name != p.first.GetText())
                continue;
            MtlfBindingMap::MTLFBindingIndex mtlfIndex(it->_index, (uint32)it->_type, (uint32)it->_stage, true);
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
        for(auto it = _bindings.begin(); it != _bindings.end(); ++it) {
            if(it->_type != kMSL_BindingType_Texture || (*it)._name != p.first.GetText())
                continue;
            MtlfBindingMap::MTLFBindingIndex mtlfIndex(it->_index, (uint32)it->_type, (uint32)it->_stage, true);
            p.second = mtlfIndex.asInt;
        }
    }
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
            const MSL_ShaderBinding& binding = MSL_FindBinding(_bindings, it->name, found, kMSL_BindingType_Sampler | kMSL_BindingType_Texture, 0xFFFFFFFF, i);
            
            if(!found)
                break;
            
            switch(binding._type)
            {
                case kMSL_BindingType_Texture:
                    MtlfMetalContext::GetMetalContext()->SetTexture(binding._index, it->handle, it->name, binding._stage);
                case kMSL_BindingType_Sampler:
                    MtlfMetalContext::GetMetalContext()->SetSampler(binding._index, it->sampler, it->name, binding._stage);
                default:
                    TF_FATAL_CODING_ERROR("Not implemented!");
            }
            
            i++;
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

void HdStMSLProgram::SetProgram() const {
    MtlfMetalContext::GetMetalContext()->SetShadingPrograms(_vertexFunction, _fragmentFunction);
    
    //Create defaults for old-style uniforms
    for(auto it = _bindings.begin(); it != _bindings.end(); ++it) {
        if(it->_name == "fragUniforms" && it->_stage == kMSL_ProgramStage_Fragment && it->_type == kMSL_BindingType_UniformBuffer)
        {
            //Add new default buffer for the default inputs
            MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();
            // Because we want to be able to store multiple copies of the uniforms within the buffer we need to multiply the size
            id<MTLBuffer> mtlBuffer = [context->device newBufferWithLength:(it->_uniformBufferSize * METAL_OLD_STYLE_UNIFORM_BUFFER_SIZE) options:MTLResourceStorageModeManaged];
            
            MtlfMetalContext::GetMetalContext()->SetUniformBuffer(it->_index, mtlBuffer, TfToken(it->_name), kMSL_ProgramStage_Fragment, 0 /*offset*/, it->_uniformBufferSize);
        }
        if(it->_name == "vtxUniforms" && it->_stage == kMSL_ProgramStage_Vertex && it->_type == kMSL_BindingType_UniformBuffer)
        {
            MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();
            // Because we want to be able to store multiple copies of the uniforms within the buffer we need to multiply the size
            id<MTLBuffer> mtlBuffer = [context->device newBufferWithLength:(it->_uniformBufferSize * METAL_OLD_STYLE_UNIFORM_BUFFER_SIZE) options:MTLResourceStorageModeManaged];
            
            MtlfMetalContext::GetMetalContext()->SetUniformBuffer(it->_index, mtlBuffer, TfToken(it->_name), kMSL_ProgramStage_Vertex, 0 /*offset*/, it->_uniformBufferSize);
        }
    }
}

void HdStMSLProgram::UnsetProgram() const {
    MtlfMetalContext::GetMetalContext()->ClearState();
}


void HdStMSLProgram::DrawElementsInstancedBaseVertex(GLenum primitiveMode,
                                                      int indexCount,
                                                      GLint indexType,
                                                      GLint firstIndex,
                                                      GLint instanceCount,
                                                      GLint baseVertex) const {
    const_cast<HdStMSLProgram*>(this)->BakeState();
    
    MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();
    
    MTLIndexType indexTypeMetal;
    int indexSize;
    switch(indexType) {
        default:
            TF_FATAL_CODING_ERROR("Not Implemented");
            return;
        case GL_UNSIGNED_SHORT:
            indexTypeMetal = MTLIndexTypeUInt16;
            indexSize = sizeof(uint16_t);
            break;
        case GL_UNSIGNED_INT:
            indexTypeMetal = MTLIndexTypeUInt32;
            indexSize = sizeof(uint32_t);
            break;
    }

    MTLPrimitiveType primType = GetMetalPrimType(primitiveMode);
    
    /*
     Currently quads get mapped to GL_LINES_ADJACENCY - presumably this is a bit of a hack as GL_QUADS aren't supported by OpenGL > 3.x and it's been
     done because they share the same number of vertices per prim (4). This doesn't appear to be a problem for OpenGL as the OpenGL subdiv doesn't genereate
     PRIM_MESH_COARSE/REFINED_QUADS (at least for the content we've currently seen) but Metal *does* so we need a way of drawing quads. We currently do this by
     remapping the index buffer and mapping GL_LINES_ADJACENCY to MTLPrimitiveTypeTriangle.
     */
    bool bDrawingQuads = (primitiveMode == GL_LINES_ADJACENCY);
    
    if (bDrawingQuads) {
        [context->renderEncoder drawIndexedPrimitives:primType indexCount:((indexCount/4)*6) indexType:indexTypeMetal indexBuffer:context->GetQuadIndexBuffer(indexTypeMetal) indexBufferOffset:(((firstIndex/4)*6) * indexSize) instanceCount:instanceCount baseVertex:baseVertex baseInstance:0];
    }
    else  {
        [context->renderEncoder drawIndexedPrimitives:primType indexCount:indexCount indexType:indexTypeMetal indexBuffer:context->GetIndexBuffer() indexBufferOffset:(firstIndex * indexSize) instanceCount:instanceCount baseVertex:baseVertex baseInstance:0];
    }
}

void HdStMSLProgram::DrawArraysInstanced(GLenum primitiveMode,
                                          GLint baseVertex,
                                          GLint vertexCount,
                                          GLint instanceCount) const {
    const_cast<HdStMSLProgram*>(this)->BakeState();
    
    MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();
    
    MTLPrimitiveType primType = GetMetalPrimType(primitiveMode);
    [context->renderEncoder drawPrimitives:primType vertexStart:baseVertex vertexCount:vertexCount instanceCount:instanceCount];
}

void HdStMSLProgram::BakeState()
{
    id<MTLDevice> device = MtlfMetalContext::GetMetalContext()->device;

    MtlfMetalContext::GetMetalContext()->BakeState();
}

PXR_NAMESPACE_CLOSE_SCOPE

