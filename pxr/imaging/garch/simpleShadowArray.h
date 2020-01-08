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
#ifndef GARCH_SIMPLE_SHADOW_ARRAY_H
#define GARCH_SIMPLE_SHADOW_ARRAY_H

/// \file garch/simpleShadowArray.h

#include "pxr/pxr.h"
#include "pxr/imaging/garch/api.h"
#include "pxr/imaging/garch/texture.h"
#include "pxr/base/tf/declarePtrs.h"
#include "pxr/base/tf/refPtr.h"
#include "pxr/base/tf/weakPtr.h"
#include "pxr/base/gf/matrix4d.h"
#include "pxr/base/gf/vec2i.h"
#include "pxr/base/gf/vec4d.h"

#include <boost/noncopyable.hpp>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE


class GarchSimpleShadowArray : public TfRefBase, public TfWeakBase, boost::noncopyable {
public:
    static GARCH_API
    GarchSimpleShadowArray* New();

    GARCH_API
    virtual ~GarchSimpleShadowArray();

    // Driven by the env var GARCH_ENABLE_BINDLESS_SHADOW_TEXTURE, this returns
    // whether bindless shadow maps are enabled, which in turn dictates the API
    // to use. See below.
    GARCH_API static
    bool GetBindlessShadowMapsEnabled();

    ///  Bindful API:

    // Set the 2D size of the shadow map texture array.
    GARCH_API
    virtual void SetSize(GfVec2i const & size);

    // Set the depth of the shadow map texture array, which corresponds to the
    // number of shadow maps necessary. Each shadow casting light uses one
    // shadow map.
    GARCH_API
    virtual void SetNumLayers(size_t numLayers);
    
    // Returns the GL texture id of the texture array.
    GARCH_API
    virtual GarchTextureGPUHandle GetShadowMapTexture() const;

    // Returns the GL sampler id of the sampler object used to read the raw
    // depth values.
    GARCH_API
    virtual GarchSamplerGPUHandle GetShadowMapDepthSampler() const;

    // Returns the GL sampler id of the sampler object used for depth comparison
    GARCH_API
    virtual GarchSamplerGPUHandle GetShadowMapCompareSampler() const;

    /// Bindless API:

    // Set the resolutions of all the shadow maps necessary. The number of
    // resolutions corresponds to the number of shadow map textures necessary,
    // which is currently one per shadow casting light.
    GARCH_API
    virtual void SetShadowMapResolutions(std::vector<GfVec2i> const& resolutions);

    // Returns a vector of the 64bit bindless handles corresponding to the
    // bindless shadow map textures.
    GARCH_API
    virtual std::vector<uint64_t> const& GetBindlessShadowMapHandles() const;

    /// Common API (for shadow map generation)
    
    // Returns the number of shadow map generation passes required, which is
    // currently one per shadow map (corresponding to a shadow casting light).
    GARCH_API
    virtual size_t GetNumShadowMapPasses() const;
    
    // Returns the shadow map resolution for a given pass. For bindful shadows,
    // this returns a single size for all passes, while for bindless, it returns
    // the resolution of the corresponding shadow map,
    GARCH_API
    virtual GfVec2i GetShadowMapSize(size_t pass) const;

    // Get/Set the view (world to shadow camera) transform to use for a given
    // shadow map generation pass.
    GARCH_API
    virtual GfMatrix4d GetViewMatrix(size_t index) const;
    GARCH_API
    virtual void SetViewMatrix(size_t index, GfMatrix4d const & matrix);

    // Get/Set the projection transform to use for a given shadow map generation
    // pass.
    GARCH_API
    virtual GfMatrix4d GetProjectionMatrix(size_t index) const;
    GARCH_API
    virtual void SetProjectionMatrix(size_t index, GfMatrix4d const & matrix);

    GARCH_API
    virtual GfMatrix4d GetWorldToShadowMatrix(size_t index) const;

    GARCH_API
    virtual void InitCaptureEnvironment(bool   depthBiasEnable,
                                        float  depthBiasConstantFactor,
                                        float  depthBiasSlopeFactor,
                                        GLenum depthFunc) = 0;

    // Bind necessary resources for a given shadow map generation pass.
    GARCH_API
    virtual void BeginCapture(size_t index, bool clear) = 0;
    
    // Unbind necssary resources after a shadow map gneration pass.
    GARCH_API
    virtual void EndCapture(size_t index) = 0;

protected:
    virtual void _AllocResources() = 0;
    virtual void _AllocBindfulTextures() = 0;
    virtual void _AllocBindlessTextures() = 0;
    virtual void _FreeResources() = 0;
    virtual void _FreeBindfulTextures() = 0;
    virtual void _FreeBindlessTextures() = 0;
    
    bool _ShadowMapExists() const;

protected:
    GARCH_API
    GarchSimpleShadowArray();

    // bindful state
    GfVec2i _size;
    size_t _numLayers;
    GarchTextureGPUHandle _bindfulTexture;
    GarchSamplerGPUHandle _shadowDepthSampler;

    // bindless state
    std::vector<GfVec2i> _resolutions;
    std::vector<GarchTextureGPUHandle> _bindlessTextures;
    std::vector<uint64_t> _bindlessTextureHandles;

    // common state
    bool _usingBindlessShadowMaps;

    std::vector<GfMatrix4d> _viewMatrix;
    std::vector<GfMatrix4d> _projectionMatrix;

    GarchTextureGPUHandle _framebuffer;

    GarchSamplerGPUHandle _shadowCompareSampler;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif
