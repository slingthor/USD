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
    GarchSimpleShadowArray* New(GfVec2i const & size, size_t numLayers);

    GARCH_API
    virtual ~GarchSimpleShadowArray();

    GARCH_API
    virtual GfVec2i GetSize() const;
    GARCH_API
    virtual void SetSize(GfVec2i const & size);

    GARCH_API
    virtual size_t GetNumLayers() const;
    GARCH_API
    virtual void SetNumLayers(size_t numLayers);

    GARCH_API
    virtual GfMatrix4d GetViewMatrix(size_t index) const;
    GARCH_API
    virtual void SetViewMatrix(size_t index, GfMatrix4d const & matrix);

    GARCH_API
    virtual GfMatrix4d GetProjectionMatrix(size_t index) const;
    GARCH_API
    virtual void SetProjectionMatrix(size_t index, GfMatrix4d const & matrix);

    GARCH_API
    virtual GfMatrix4d GetWorldToShadowMatrix(size_t index) const;

    GARCH_API
    virtual GarchTextureGPUHandle GetShadowMapTexture() const;
    GARCH_API
    virtual GarchSamplerGPUHandle GetShadowMapDepthSampler() const;
    GARCH_API
    virtual GarchSamplerGPUHandle GetShadowMapCompareSampler() const;

    GARCH_API
    virtual void InitCaptureEnvironment(bool   depthBiasEnable,
                                        float  depthBiasConstantFactor,
                                        float  depthBiasSlopeFactor,
                                        GLenum depthFunc) = 0;
    GARCH_API
    virtual void DisableCaptureEnvironment() = 0;

    GARCH_API
    virtual void BeginCapture(size_t index, bool clear) = 0;
    GARCH_API
    virtual void EndCapture(size_t index) = 0;

protected:
    GARCH_API
    GarchSimpleShadowArray(GfVec2i const & size, size_t numLayers);

    GfVec2i _size;
    size_t _numLayers;

    std::vector<GfMatrix4d> _viewMatrix;
    std::vector<GfMatrix4d> _projectionMatrix;

    GarchTextureGPUHandle _texture;
    GarchTextureGPUHandle _framebuffer;

    GarchSamplerGPUHandle _shadowDepthSampler;
    GarchSamplerGPUHandle _shadowCompareSampler;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif
