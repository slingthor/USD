//
// Copyright 2018 Pixar
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
#include "pxr/imaging/garch/contextCaps.h"

#include "pxr/base/tf/diagnostic.h"
#include "pxr/base/tf/envSetting.h"

#include <iostream>
#include <mutex>

PXR_NAMESPACE_OPEN_SCOPE


TF_DEFINE_ENV_SETTING(GARCH_ENABLE_SHADER_STORAGE_BUFFER, true,
                      "Use GL shader storage buffer (OpenGL 4.3)");
TF_DEFINE_ENV_SETTING(GARCH_ENABLE_BINDLESS_BUFFER, false,
                      "Use GL bindless buffer extention");
TF_DEFINE_ENV_SETTING(GARCH_ENABLE_BINDLESS_TEXTURE, false,
                      "Use GL bindless texture extention");
TF_DEFINE_ENV_SETTING(GARCH_ENABLE_MULTI_DRAW_INDIRECT, true,
                      "Use GL mult draw indirect extention");
TF_DEFINE_ENV_SETTING(GARCH_ENABLE_DIRECT_STATE_ACCESS, true,
                      "Use GL direct state access extention");
TF_DEFINE_ENV_SETTING(GARCH_ENABLE_COPY_BUFFER, true,
                      "Use GL copy buffer data");
TF_DEFINE_ENV_SETTING(HD_ENABLE_GPU_COUNT_VISIBLE_INSTANCES, false,
                      "Enable GPU frustum culling visible count query");
TF_DEFINE_ENV_SETTING(HD_ENABLE_GPU_FRUSTUM_CULLING, true,
                      "Enable GPU frustum culling");
TF_DEFINE_ENV_SETTING(HD_ENABLE_GPU_TINY_PRIM_CULLING, true,
                      "Enable tiny prim culling");
TF_DEFINE_ENV_SETTING(HD_ENABLE_GPU_INSTANCE_FRUSTUM_CULLING, true,
                      "Enable GPU per-instance frustum culling");

// To enable GPU compute features, OpenSubdiv must be configured to support
// GLSL compute kernel.
//
#if OPENSUBDIV_HAS_GLSL_COMPUTE || OPENSUBDIV_HAS_METAL_COMPUTE
// default to GPU
TF_DEFINE_ENV_SETTING(HD_ENABLE_GPU_COMPUTE, true,
                      "Enable GPU smooth, quadrangulation and refinement");
#else
// default to CPU
TF_DEFINE_ENV_SETTING(HD_ENABLE_GPU_COMPUTE, false,
                      "Enable GPU smooth, quadrangulation and refinement");
#endif



TF_DEFINE_ENV_SETTING(GARCH_GLSL_VERSION, 0,
                      "GLSL version");

bool GarchContextCaps::IsGPUComputeEnabled()
{
    return TfGetEnvSetting(HD_ENABLE_GPU_COMPUTE);
}

// Initialize members to ensure a sane starting state.
GarchContextCaps::GarchContextCaps()
    : apiVersion(0)
    , coreProfile(false)

    , maxArrayTextureLayers(0)
    , maxUniformBlockSize(0)
    , maxShaderStorageBlockSize(0)
    , maxTextureBufferSize(0)
    , uniformBufferOffsetAlignment(0)

    , arrayTexturesEnabled(false)
    , shaderStorageBufferEnabled(false)
    , bufferStorageEnabled(false)
    , directStateAccessEnabled(false)
    , multiDrawIndirectEnabled(false)
    , bindlessTextureEnabled(false)
    , bindlessBufferEnabled(false)

    , glslVersion(400)
    , explicitUniformLocation(false)
    , shadingLanguage420pack(false)
    , shaderDrawParametersEnabled(false)

    , copyBufferEnabled(true)
    , gpuComputeEnabled(false)
    , gpuComputeNormalsEnabled(false)

    , flipTexturesOnLoad(true)
    , hasSubDataCopy(false)
    , useCppShaderPadding(false)
    , alwaysNeedsBinding(false)
{
    // Empty
}

bool
GarchContextCaps::IsEnabledGPUFrustumCulling() const
{
    static bool isEnabledGPUFrustumCulling =
        TfGetEnvSetting(HD_ENABLE_GPU_FRUSTUM_CULLING) &&
        explicitUniformLocation;
    return isEnabledGPUFrustumCulling;
}

bool
GarchContextCaps::IsEnabledGPUCountVisibleInstances() const
{
    static bool isEnabledGPUCountVisibleInstances =
        TfGetEnvSetting(HD_ENABLE_GPU_COUNT_VISIBLE_INSTANCES);
    return isEnabledGPUCountVisibleInstances;
}

bool
GarchContextCaps::IsEnabledGPUTinyPrimCulling() const
{
    static bool isEnabledGPUTinyPrimCulling =
        TfGetEnvSetting(HD_ENABLE_GPU_TINY_PRIM_CULLING);
    return isEnabledGPUTinyPrimCulling;
}

bool
GarchContextCaps::IsEnabledGPUInstanceFrustumCulling() const
{
    static bool isEnabledGPUInstanceFrustumCulling =
        TfGetEnvSetting(HD_ENABLE_GPU_INSTANCE_FRUSTUM_CULLING) &&
        (shaderStorageBufferEnabled || bindlessBufferEnabled);
    return isEnabledGPUInstanceFrustumCulling;
}

PXR_NAMESPACE_CLOSE_SCOPE

