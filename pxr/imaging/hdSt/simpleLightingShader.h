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
#ifndef PXR_IMAGING_HDST_SIMPLE_LIGHTING_SHADER_H
#define PXR_IMAGING_HDST_SIMPLE_LIGHTING_SHADER_H

#include "pxr/pxr.h"
#include "pxr/imaging/hdSt/api.h"
#include "pxr/imaging/hdSt/lightingShader.h"

#include "pxr/imaging/hd/resource.h"
#include "pxr/imaging/hd/texture.h"
#include "pxr/imaging/hd/version.h"

#include "pxr/imaging/hio/glslfx.h"


#include "pxr/imaging/garch/bindingMap.h"
#include "pxr/imaging/garch/simpleLightingContext.h"

#include "pxr/base/gf/matrix4d.h"

#include "pxr/base/tf/declarePtrs.h"
#include "pxr/base/tf/token.h"

#include <memory>
#include <string>

PXR_NAMESPACE_OPEN_SCOPE

class HdSceneDelegate;
using HdStSimpleLightingShaderSharedPtr =
    std::shared_ptr<class HdStSimpleLightingShader>;

/// \class HdStSimpleLightingShader
///
/// A shader that supports simple lighting functionality.
///
class HdStSimpleLightingShader : public HdStLightingShader 
{
public:
    HDST_API
    HdStSimpleLightingShader();
    HDST_API
    ~HdStSimpleLightingShader() override;

    /// HdShader overrides
    HDST_API
    ID ComputeHash() const override;
    HDST_API
    std::string GetSource(TfToken const &shaderStageKey) const override;
    HDST_API
    void BindResources(HdStProgram const &program,
                       HdSt_ResourceBinder const &binder,
                       HdRenderPassState const &state) override;
    HDST_API
    void UnbindResources(HdStProgram const &program,
                         HdSt_ResourceBinder const &binder,
                         HdRenderPassState const &state) override;
    HDST_API
    void AddBindings(HdBindingRequestVector *customBindings) override;

    /// Adds computations to create the dome light textures that
    /// are pre-calculated from the environment map texture.
    HDST_API
    void AddResourcesFromTextures(ResourceContext &ctx) const override;

    /// HdStShaderCode overrides
    HDST_API
    HdSt_MaterialParamVector const& GetParams() const override;

    /// HdStLightingShader overrides
    HDST_API
    void SetCamera(
        GfMatrix4d const &worldToViewMatrix,
        GfMatrix4d const &projectionMatrix) override;
    HDST_API
    void SetLightingStateFromOpenGL();
    HDST_API
    void SetLightingState(GarchSimpleLightingContextPtr const &lightingContext);

    GarchSimpleLightingContextRefPtr GetLightingContext() {
        return _lightingContext;
    };

    /// Allocates texture handles (texture loading happens later during commit)
    /// needed for lights.
    ///
    /// Call after lighting context has been set or updated in Sync-phase.
    ///
    HDST_API
    void AllocateTextureHandles(HdSceneDelegate *delegate);

    /// Get GL texture name for a dome light texture of given time.
    HDST_API
    GarchTextureGPUHandle GetGLTextureName(const TfToken &token) const;

    /// Set GL texture name for a dome light texture of given time.
    HDST_API
    void SetGLTextureName(const TfToken &token, const GarchTextureGPUHandle glName);

    /// Get GL sampler name for a dome light texture of given time.
    HDST_API
    GarchSamplerGPUHandle GetGLSamplerName(const TfToken &token) const;

    /// Set GL sampler name for a dome light texture of given time.
    HDST_API
    void SetGLSamplerName(const TfToken &token, const GarchSamplerGPUHandle glName);

private:
    // Create samplers for dome light textures if not previously created.
    void _CreateSamplersIfNecessary();

private:
    GarchSimpleLightingContextRefPtr _lightingContext;
    GarchBindingMapRefPtr _bindingMap;
    bool _useLighting;
    std::unique_ptr<HioGlslfx> _glslfx;

    // The environment map used as source for the dome light textures.
    //
    // Handle is allocated in AllocateTextureHandles. Actual loading
    // happens during commit.
    HdStTextureHandleSharedPtr _domeLightTextureHandle;

    // The pre-calculated dome light textures.
    // Created by HdSt_DomelightComputationGPU.
    GarchTextureGPUHandle _domeLightIrradianceGLName;
    GarchTextureGPUHandle _domeLightPrefilterGLName;
    GarchTextureGPUHandle _domeLightBrdfGLName;

    GarchSamplerGPUHandle _domeLightIrradianceGLSampler;
    GarchSamplerGPUHandle _domeLightPrefilterGLSampler;
    GarchSamplerGPUHandle _domeLightBrdfGLSampler;

    HdSt_MaterialParamVector _lightTextureParams;
    bool _isOpenGL;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_IMAGING_HDST_SIMPLE_LIGHTING_SHADER_H
