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
#ifndef GARCH_CONTEXT_CAPS_H
#define GARCH_CONTEXT_CAPS_H

#include "pxr/pxr.h"
#include "pxr/imaging/garch/api.h"

#include <boost/noncopyable.hpp>

PXR_NAMESPACE_OPEN_SCOPE


/// \class GarchContextCaps
///
/// This class is intended to be a cache of the capabilites
/// (resource limits and features) of the underlying
/// GL context.
///
/// It serves two purposes.  Firstly to reduce driver
/// transition overhead of querying these values.
/// Secondly to provide access to these values from other
/// threads that don't have the context bound.
///
/// TO DO (bug #124971):
///   - LoadCaps() should be called whenever the context
///     changes.
///   - Provide a mechanism where other Hd systems can
///     subscribe to when the caps changes, so they can
///     update and invalidate.
///
class GarchContextCaps : boost::noncopyable {
public:

    // GL version
    int apiVersion;                    // 400 (4.0), 410 (4.1), ...

    // Whether or not we are running with core profile
    bool coreProfile;

    // Max constants
    int maxUniformBlockSize;
    int maxShaderStorageBlockSize;
    int maxTextureBufferSize;
    int uniformBufferOffsetAlignment;

    // GL extensions (ordered by version)
    bool shaderStorageBufferEnabled;  // ARB_shader_storage_buffer_object (4.3)
    bool bufferStorageEnabled;        // ARB_buffer_storage               (4.4)
    bool directStateAccessEnabled;    // ARB_direct_state_access          (4.5)
    bool multiDrawIndirectEnabled;    // ARB_multi_draw_indirect          (4.5)

    bool bindlessTextureEnabled;      // ARB_bindless_texture
    bool bindlessBufferEnabled;       // NV_shader_buffer_load

    // GLSL version and extensions 
    int glslVersion;                  // 400, 410, ...
    bool explicitUniformLocation;     // ARB_explicit_uniform_location    (4.3)
    bool shadingLanguage420pack;      // ARB_shading_language_420pack     (4.2)
    bool shaderDrawParametersEnabled; // ARB_shader_draw_parameters       (4.5)

    // workarounds for driver issues
    bool copyBufferEnabled;

    // GPU compute
    bool gpuComputeEnabled;           // GPU subdivision                  (4.3)
	bool gpuComputeNormalsEnabled;    // GPU smooth normals               (4.3)
    
    // Texture vertical flip
    bool flipTexturesOnLoad;
    
    // Shader language padding for structures
    bool useCppShaderPadding;

    /// Returns whether to do frustum culling on the GPU
    virtual bool IsEnabledGPUFrustumCulling() const;
    
    /// Returns whether to read back the count of visible items from the GPU
    /// Disabled by default, since there is some performance penalty.
    virtual bool IsEnabledGPUCountVisibleInstances() const;
    
    /// Returns whether to cull tiny prims (in screen space) during GPU culling
    /// Enabled by default.
    virtual bool IsEnabledGPUTinyPrimCulling() const;
    
    /// Returns whether to do per-instance culling on the GPU
    virtual bool IsEnabledGPUInstanceFrustumCulling() const;
    
    /// Returns a string identifying the renderer, that matches the renderer specific
    /// part of the adapter names
    virtual char const* const GetRendererName() const = 0;
    
protected:
    GarchContextCaps();
    virtual ~GarchContextCaps() = default;
    
    bool IsGPUComputeEnabled();
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // GARCH_CONTEXT_CAPS_H

