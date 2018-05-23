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

HdStMSLProgram::HdStMSLProgram(TfToken const &role)
: HdStProgram(role)
, _role(role)
, _vertexFunction(nil)
, _fragmentFunction(nil)
, _computeFunction(nil)
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
    if (type == GL_VERTEX_SHADER) {
        shaderType = "Vertx Shader";
        entryPoint = @"vertexEntryPoint";
//    } else if (type == GL_TESS_CONTROL_SHADER) {
//        shaderType = "GL_TESS_CONTROL_SHADER";
//    } else if (type == GL_TESS_EVALUATION_SHADER) {
//        shaderType = "GL_TESS_EVALUATION_SHADER";
//    } else if (type == GL_GEOMETRY_SHADER) {
//        shaderType = "GL_GEOMETRY_SHADER";
    } else if (type == GL_FRAGMENT_SHADER) {
        shaderType = "Fragment Shader";
        entryPoint = @"fragmentEntryPoint";
    } else if (type == GL_COMPUTE_SHADER) {
        shaderType = "Compute Shader";
        entryPoint = @"computeEntryPoint";
    } else {
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
    
    id<MTLLibrary> library = [device newLibraryWithSource:[NSString stringWithUTF8String:shaderSource.c_str()]
                                                  options:options
                                                    error:&error];
    
    // Load the function into the library
    id <MTLFunction> function = [library newFunctionWithName:entryPoint];
    if (!function) {
        // XXX:validation
        TF_WARN("Failed to compile shader (%s): \n%s",
                shaderType, [[error localizedDescription] UTF8String]);
        
        return false;
    }
    
    if (type == GL_VERTEX_SHADER) {
        _vertexFunction = function;
    } else if (type == GL_FRAGMENT_SHADER) {
        _fragmentFunction = function;
    } else if (type == GL_COMPUTE_SHADER) {
        _computeFunction = function;
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

//    mtlfBindingMap->AssignUniformBindingsToProgram(nil);
}

void HdStMSLProgram::AssignSamplerUnits(GarchBindingMapRefPtr bindingMap) const
{
    MtlfBindingMapRefPtr mtlfBindingMap(TfDynamic_cast<MtlfBindingMapRefPtr>(bindingMap));
    
    mtlfBindingMap->AssignSamplerUnitsToProgram(nil);
}

void HdStMSLProgram::AddCustomBindings(GarchBindingMapRefPtr bindingMap) const
{
    MtlfBindingMapRefPtr mtlfBindingMap(TfDynamic_cast<MtlfBindingMapRefPtr>(bindingMap));
    
    mtlfBindingMap->AddCustomBindings(nil);
}

void HdStMSLProgram::BindResources(HdStSurfaceShader* surfaceShader, HdSt_ResourceBinder const &binder) const
{
    // XXX: there's an issue where other shaders try to use textures.
    TF_FOR_ALL(it, surfaceShader->GetTextureDescriptors()) {
        HdBinding binding = binder.GetBinding(it->name);
        // XXX: put this into resource binder.
        if (binding.GetType() == HdBinding::TEXTURE_2D ||
            binding.GetType() == HdBinding::TEXTURE_PTEX_TEXEL) {
            MtlfMetalContext::GetMetalContext()->SetTexture(binding.GetLocation(), it->handle);
            MtlfMetalContext::GetMetalContext()->SetSampler(binding.GetLocation(), it->sampler);
        } else if (binding.GetType() == HdBinding::TEXTURE_PTEX_LAYOUT) {
            TF_FATAL_CODING_ERROR("Not Implemented");
            // glActiveTexture(GL_TEXTURE0 + samplerUnit);
            // glBindTexture(GL_TEXTURE_BUFFER, it->handle);
            //glProgramUniform1i(_program, binding.GetLocation(), samplerUnit);
        }
    }
}

void HdStMSLProgram::UnbindResources(HdStSurfaceShader* surfaceShader, HdSt_ResourceBinder const &binder) const
{
    // Nothing
}

void HdStMSLProgram::SetProgram() const {
//    TF_FATAL_CODING_ERROR("Not Implemented");
}

void HdStMSLProgram::UnsetProgram() const {
//    TF_FATAL_CODING_ERROR("Not Implemented");
}


void HdStMSLProgram::DrawElementsInstancedBaseVertex(GLenum primitiveMode,
                                                      int indexCount,
                                                      GLint indexType,
                                                      GLint firstIndex,
                                                      GLint instanceCount,
                                                      GLint baseVertex) const {
    const_cast<HdStMSLProgram*>(this)->BakeState();
}

void HdStMSLProgram::DrawArraysInstanced(GLenum primitiveMode,
                                          GLint baseVertex,
                                          GLint vertexCount,
                                          GLint instanceCount) const {
    const_cast<HdStMSLProgram*>(this)->BakeState();
}

void HdStMSLProgram::BakeState()
{

}

PXR_NAMESPACE_CLOSE_SCOPE

