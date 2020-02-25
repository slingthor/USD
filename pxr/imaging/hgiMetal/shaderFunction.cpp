//
// Copyright 2020 Pixar
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
#include "pxr/base/tf/diagnostic.h"

#include "pxr/imaging/hgiMetal/conversions.h"
#include "pxr/imaging/hgiMetal/shaderFunction.h"

PXR_NAMESPACE_OPEN_SCOPE


HgiMetalShaderFunction::HgiMetalShaderFunction(
    HgiShaderFunctionDesc const& desc)
    : HgiShaderFunction(desc)
    , _descriptor(desc)
    , _shaderId(nil)
{/*
    std::vector<GLenum> stages = 
        HgiMetalConversions::GetShaderStages(desc.shaderStage);

    if (!TF_VERIFY(stages.size()==1)) return;

    _shaderId = glCreateShader(stages[0]);
    glObjectLabel(GL_SHADER, _shaderId, -1, _descriptor.debugName.c_str());

    const char* src = desc.shaderCode.c_str();
    glShaderSource(_shaderId, 1, &src, nullptr);
    glCompileShader(_shaderId);

    // Grab compile errors
    GLint status;
    glGetShaderiv(_shaderId, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        int logSize = 0;
        glGetShaderiv(_shaderId, GL_INFO_LOG_LENGTH, &logSize);
        _errors.resize(logSize+1);
        glGetShaderInfoLog(_shaderId, logSize, NULL, &_errors[0]);
        glDeleteShader(_shaderId);
        _shaderId = nil;
    }
*/
}

HgiMetalShaderFunction::~HgiMetalShaderFunction()
{
    [_shaderId release];
    _shaderId = nil;
}

bool
HgiMetalShaderFunction::IsValid() const
{
    return _errors.empty();
}

std::string const&
HgiMetalShaderFunction::GetCompileErrors()
{
    return _errors;
}

id<MTLFunction>
HgiMetalShaderFunction::GetShaderId() const
{
    return _shaderId;
}

PXR_NAMESPACE_CLOSE_SCOPE
