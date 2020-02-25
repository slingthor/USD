//
// Copyright 2019 Pixar
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
#include <mutex>

#include "pxr/imaging/hgiGL/hgi.h"
#include "pxr/imaging/hgiGL/buffer.h"
#include "pxr/imaging/hgiGL/conversions.h"
#include "pxr/imaging/hgiGL/diagnostic.h"
#include "pxr/imaging/hgiGL/shaderFunction.h"
#include "pxr/imaging/hgiGL/shaderProgram.h"
#include "pxr/imaging/hgiGL/texture.h"

#include "pxr/base/tf/envSetting.h"
#include "pxr/base/tf/registryManager.h"
#include "pxr/base/tf/type.h"


PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_ENV_SETTING(HGIGL_ENABLE_GL_VERSION_VALIDATION, true,
    "Enables validation OpenGL version.");

TF_REGISTRY_FUNCTION(TfType)
{
    TfType t = TfType::Define<HgiGL, TfType::Bases<Hgi> >();
    t.SetFactory<HgiFactory<HgiGL>>();
}


HgiGL::HgiGL()
{
    static std::once_flag versionOnce;
    std::call_once(versionOnce, [](){
        const bool validate=TfGetEnvSetting(HGIGL_ENABLE_GL_VERSION_VALIDATION);
        if (validate && !HgiGLMeetsMinimumRequirements()) {
            TF_WARN(
                "HgiGL minimum OpenGL requirements not met. Please ensure "
                "that OpenGL is initialized and supports version 4.5."
            );
        }
    });

    HgiGLSetupGL4Debug();
}

HgiGL::~HgiGL()
{
}

HgiImmediateCommandBuffer&
HgiGL::GetImmediateCommandBuffer()
{
    return _immediateCommandBuffer;
}

HgiTextureHandle
HgiGL::CreateTexture(HgiTextureDesc const & desc)
{
    return new HgiGLTexture(desc);
}

void
HgiGL::DestroyTexture(HgiTextureHandle* texHandle)
{
    if (TF_VERIFY(texHandle, "Invalid texture")) {
        delete *texHandle;
        *texHandle = nullptr;
    }
}

HgiBufferHandle
HgiGL::CreateBuffer(HgiBufferDesc const & desc)
{
    return new HgiGLBuffer(desc);
}

void
HgiGL::DestroyBuffer(HgiBufferHandle* bufHandle)
{
    if (TF_VERIFY(bufHandle, "Invalid buffer")) {
        delete *bufHandle;
        *bufHandle = nullptr;
    }
}

HgiShaderFunctionHandle
HgiGL::CreateShaderFunction(HgiShaderFunctionDesc const& desc)
{
    return new HgiGLShaderFunction(desc);
}

void
HgiGL::DestroyShaderFunction(HgiShaderFunctionHandle* shaderFunctionHandle)
{
    if (TF_VERIFY(shaderFunctionHandle, "Invalid function handle")) {
        delete *shaderFunctionHandle;
        *shaderFunctionHandle = nullptr;
    }
}

HgiShaderProgramHandle
HgiGL::CreateShaderProgram(HgiShaderProgramDesc const& desc)
{
    return new HgiGLShaderProgram(desc);
}

void
HgiGL::DestroyShaderProgram(HgiShaderProgramHandle* shaderProgramHandle)
{
    if (TF_VERIFY(shaderProgramHandle, "Invalid program handle")) {
        delete *shaderProgramHandle;
        *shaderProgramHandle = nullptr;
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
