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
#include "pxr/imaging/glf/glew.h"
#include "pxr/imaging/glf/contextCaps.h"
#include "pxr/imaging/glf/debugCodes.h"

#include "pxr/imaging/garch/gl.h"

#include "pxr/base/tf/diagnostic.h"
#include "pxr/base/tf/envSetting.h"

#include <iostream>
#include <mutex>

PXR_NAMESPACE_OPEN_SCOPE


TF_DEFINE_ENV_SETTING(GLF_ENABLE_SHADER_STORAGE_BUFFER, true,
                      "Use GL shader storage buffer (OpenGL 4.3)");
TF_DEFINE_ENV_SETTING(GLF_ENABLE_BINDLESS_BUFFER, false,
                      "Use GL bindless buffer extention");
TF_DEFINE_ENV_SETTING(GLF_ENABLE_BINDLESS_TEXTURE, false,
                      "Use GL bindless texture extention");
TF_DEFINE_ENV_SETTING(GLF_ENABLE_MULTI_DRAW_INDIRECT, true,
                      "Use GL multi draw indirect extention");
TF_DEFINE_ENV_SETTING(GLF_ENABLE_DIRECT_STATE_ACCESS, true,
                      "Use GL direct state access extention");
TF_DEFINE_ENV_SETTING(GLF_ENABLE_COPY_BUFFER, true,
                      "Use GL copy buffer data");
TF_DEFINE_ENV_SETTING(GLF_ENABLE_SHADER_DRAW_PARAMETERS, true,
                      "Use GL shader draw params if available (OpenGL 4.5+)");


TF_DEFINE_ENV_SETTING(GLF_GLSL_VERSION, 0,
                      "GLSL version");


// Set defaults based on GL spec minimums
static const int _DefaultMaxArrayTextureLayers        = 256;
static const int _DefaultMaxUniformBlockSize          = 16*1024;
static const int _DefaultMaxShaderStorageBlockSize    = 16*1024*1024;
static const int _DefaultMaxTextureBufferSize         = 64*1024;
static const int _DefaultGLSLVersion                  = 400;

// Initialize members to ensure a sane starting state.
GlfContextCaps::GlfContextCaps()
{
    _LoadCaps();
}

void GlfContextCaps::InitInstance()
{
    // MTLTODO: Figure this out
}

int
GlfContextCaps::GetAPIVersion()
{
    const char *glVersionStr = (const char*)glGetString(GL_VERSION);
    
    // GL hasn't been initialized yet.
    if (glVersionStr == NULL) {
        return 0;
    }
    
    const char *dot = strchr(glVersionStr, '.');
    if (TF_VERIFY((dot && dot != glVersionStr),
                  "Can't parse GL_VERSION %s", glVersionStr)) {
        // GL_VERSION = "4.5.0 <vendor> <version>"
        //              "4.1 <vendor-os-ver> <version>"
        //              "4.1 <vendor-os-ver>"
        int major = std::max(0, std::min(9, *(dot-1) - '0'));
        int minor = std::max(0, std::min(9, *(dot+1) - '0'));
        return major * 100 + minor * 10;
    }
    return 0;
}

void
GlfContextCaps::_LoadCaps()
{
    // Reset Values to reasonable defaults based of OpenGL minimums.
    // So that if we early out, systems can still depend on the
    // caps values being valid.
    //
    // _LoadCaps can also be called multiple times, so not want
    // to mix and match values in the event of an early out.
    apiVersion                   = 0;
    coreProfile                  = false;
    maxArrayTextureLayers        = _DefaultMaxArrayTextureLayers;
    maxUniformBlockSize          = _DefaultMaxUniformBlockSize;
    maxShaderStorageBlockSize    = _DefaultMaxShaderStorageBlockSize;
    maxTextureBufferSize         = _DefaultMaxTextureBufferSize;
    uniformBufferOffsetAlignment = 0;
    arrayTexturesEnabled         = false;
    shaderStorageBufferEnabled   = false;
    bufferStorageEnabled         = false;
    directStateAccessEnabled     = false;
    multiDrawIndirectEnabled     = false;
    bindlessTextureEnabled       = false;
    bindlessBufferEnabled        = false;
    glslVersion                  = _DefaultGLSLVersion;
    explicitUniformLocation      = false;
    shadingLanguage420pack       = false;
    shaderDrawParametersEnabled  = false;
    maxArrayTextureLayers        = 256;          // GL spec minimum
    maxUniformBlockSize          = 16*1024;      // GL spec minimum
    maxShaderStorageBlockSize    = 16*1024*1024; // GL spec minimum
    maxTextureBufferSize         = 64*1024;      // GL spec minimum
    uniformBufferOffsetAlignment = 0;
    flipTexturesOnLoad           = true;
    useCppShaderPadding          = false;
    hasSubDataCopy               = glBufferSubData != NULL;

    const char *glVersionStr = (const char*)glGetString(GL_VERSION);

    // GL hasn't been initialized yet.
    if (glVersionStr == NULL) return;

    apiVersion = GetAPIVersion();

    const char *dot = strchr(glVersionStr, '.');

    if (apiVersion >= 200) {
        const char *glslVersionStr =
            (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
        dot = strchr(glslVersionStr, '.');
        if (TF_VERIFY((dot && dot != glslVersionStr),
                      "Can't parse GL_SHADING_LANGUAGE_VERSION %s",
                      glslVersionStr)) {
            // GL_SHADING_LANGUAGE_VERSION = "4.10"
            //                               "4.50 <vendor>"
            int major = std::max(0, std::min(9, *(dot-1) - '0'));
            int minor = std::max(0, std::min(9, *(dot+1) - '0'));
            glslVersion = major * 100 + minor * 10;
        }
    } else {
        glslVersion = 0;
    }

    if (apiVersion >= 300) {
        glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxArrayTextureLayers);
        arrayTexturesEnabled = true;
    }

    // initialize by Core versions
    if (apiVersion >= 310) {
        glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE,
                      &maxUniformBlockSize);
        glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE,
                      &maxTextureBufferSize);
        glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT,
                      &uniformBufferOffsetAlignment);
    }
    if (apiVersion >= 320) {
        GLint profileMask = 0;
        glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profileMask);
        coreProfile = (profileMask & GL_CONTEXT_CORE_PROFILE_BIT);
    }
    if (apiVersion >= 400) {
        // Older versions of GL maybe support R16F and D32F, but for now we set
        // the minimum GL at 4.
        floatingPointBuffersEnabled = true;
    }
    if (apiVersion >= 420) {
        shadingLanguage420pack = true;
    }
    if (apiVersion >= 430) {
        shaderStorageBufferEnabled = true;
        explicitUniformLocation = true;
        glGetIntegerv(GL_MAX_SHADER_STORAGE_BLOCK_SIZE,
                      &maxShaderStorageBlockSize);
    }
    if (apiVersion >= 440) {
        bufferStorageEnabled = true;
    }
    if (apiVersion >= 450) {
        multiDrawIndirectEnabled = true;
        directStateAccessEnabled = true;
    }
    if (apiVersion >= 460) {
        shaderDrawParametersEnabled = true;
    }

    // initialize by individual extension.
    if (GLEW_ARB_bindless_texture && glMakeTextureHandleResidentNV) {
        bindlessTextureEnabled = true;
    }
    if (GLEW_NV_shader_buffer_load && glMakeNamedBufferResidentNV) {
        bindlessBufferEnabled = true;
    }
    if (GLEW_ARB_explicit_uniform_location) {
        explicitUniformLocation = true;
    }
    if (GLEW_ARB_shading_language_420pack) {
        shadingLanguage420pack = true;
    }
    if (GLEW_ARB_multi_draw_indirect) {
        multiDrawIndirectEnabled = true;
    }
#if defined(GLEW_VERSION_4_5)  // glew 1.11 or newer (usd requirement is 1.10)
    if (GLEW_ARB_direct_state_access) {
        directStateAccessEnabled = true;
    }
    if (GLEW_ARB_shader_draw_parameters) {
        shaderDrawParametersEnabled = true;
    }
#endif
    if (GLEW_EXT_direct_state_access) {
        directStateAccessEnabled = true;
    }

    // Environment variable overrides (only downgrading is possible)
    if (!TfGetEnvSetting(GLF_ENABLE_SHADER_STORAGE_BUFFER)) {
        shaderStorageBufferEnabled = false;
    }
    if (!TfGetEnvSetting(GLF_ENABLE_BINDLESS_TEXTURE)) {
        bindlessTextureEnabled = false;
    }
    if (!TfGetEnvSetting(GLF_ENABLE_BINDLESS_BUFFER)) {
        bindlessBufferEnabled = false;
    }
    if (!TfGetEnvSetting(GLF_ENABLE_MULTI_DRAW_INDIRECT)) {
        multiDrawIndirectEnabled = false;
    }
    if (!TfGetEnvSetting(GLF_ENABLE_DIRECT_STATE_ACCESS)) {
        directStateAccessEnabled = false;
    }
    if (!TfGetEnvSetting(GLF_ENABLE_SHADER_DRAW_PARAMETERS)) {
        shaderDrawParametersEnabled = false;
    }

    // For debugging and unit testing
    if (TfGetEnvSetting(GLF_GLSL_VERSION) > 0) {
        // GLSL version override
        glslVersion = std::min(glslVersion, TfGetEnvSetting(GLF_GLSL_VERSION));

        // downgrade to the overridden GLSL version
        shadingLanguage420pack      &= (glslVersion >= 420);
        explicitUniformLocation     &= (glslVersion >= 430);
        bindlessTextureEnabled      &= (glslVersion >= 430);
        bindlessBufferEnabled       &= (glslVersion >= 430);
        shaderStorageBufferEnabled  &= (glslVersion >= 430);
        shaderDrawParametersEnabled &= (glslVersion >= 450);
    }

    // For driver issues workaround
    if (!TfGetEnvSetting(GLF_ENABLE_COPY_BUFFER)) {
        copyBufferEnabled = false;
    }

    if (TfDebug::IsEnabled(GLF_DEBUG_CONTEXT_CAPS)) {
        std::cout
            << "GlfContextCaps: \n"
            << "  GL version                         = "
            <<    apiVersion << "\n"
            << "  GLSL version                       = "
            <<    glslVersion << "\n"

            << "  GL_MAX_UNIFORM_BLOCK_SIZE          = "
            <<    maxUniformBlockSize << "\n"
            << "  GL_MAX_SHADER_STORAGE_BLOCK_SIZE   = "
            <<    maxShaderStorageBlockSize << "\n"
            << "  GL_MAX_TEXTURE_BUFFER_SIZE         = "
            <<    maxTextureBufferSize << "\n"
            << "  GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT = "
            <<    uniformBufferOffsetAlignment << "\n"
            // order alphabetically
            << "  ARB_bindless_texture               = "
            <<    bindlessTextureEnabled << "\n"
            << "  ARB_direct_state_access            = "
            <<    directStateAccessEnabled << "\n"
            << "  ARB_explicit_uniform_location      = "
            <<    explicitUniformLocation << "\n"
            << "  ARB_multi_draw_indirect            = "
            <<    multiDrawIndirectEnabled << "\n"
            << "  ARB_shader_draw_parameters   = "
            <<    shaderDrawParametersEnabled << "\n"
            << "  ARB_shader_storage_buffer_object   = "
            <<    shaderStorageBufferEnabled << "\n"
            << "  ARB_shading_language_420pack       = "
            <<    shadingLanguage420pack << "\n"
            << "  NV_shader_buffer_load              = "
            <<    bindlessBufferEnabled << "\n"

            ;

        if (!copyBufferEnabled) {
            std::cout << "  CopyBuffer : disabled\n";
        }
    }
    
    // GPU Compute
    if (IsGPUComputeEnabled()) {
        gpuComputeEnabled = true;
        gpuComputeNormalsEnabled = true;

#if OPENSUBDIV_HAS_GLSL_COMPUTE
        if (glslVersion < 430) {
            TF_WARN("HD_ENABLE_GPU_COMPUTE can't be enabled "
                    "(OpenGL 4.3 required).\n");
        }
#else
        TF_WARN("HD_ENABLE_GPU_COMPUTE can't be enabled "
                "(OpenSubdiv hasn't been configured with GLSL compute).\n");
#endif
    }
}

PXR_NAMESPACE_CLOSE_SCOPE

