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
#ifndef PXR_IMAGING_HD_ST_TEXTURE_RESOURCE_H
#define PXR_IMAGING_HD_ST_TEXTURE_RESOURCE_H

#include "pxr/pxr.h"
#include "pxr/imaging/hd/textureResource.h"
#include "pxr/imaging/hd/enums.h"
#include "pxr/imaging/hdSt/api.h"

#include "pxr/imaging/garch/gl.h"
#include "pxr/imaging/garch/texture.h"
#include "pxr/imaging/garch/textureHandle.h"

#include "pxr/base/tf/declarePtrs.h"
#include "pxr/base/tf/token.h"
#include "pxr/base/gf/vec4f.h"

#include <boost/noncopyable.hpp>

#include <memory>
#include <cstdint>

PXR_NAMESPACE_OPEN_SCOPE


using HdStTextureResourceSharedPtr = std::shared_ptr<class HdStTextureResource>;

/// HdStTextureResource is an interface to a GL-backed texture.
class HdStTextureResource : public HdTextureResource, boost::noncopyable {
public:
    HDST_API
    virtual ~HdStTextureResource();

    // Access to underlying GL storage.
    HDST_API virtual GarchTextureGPUHandle GetTexelsTextureId() = 0;
    HDST_API virtual GarchSamplerGPUHandle GetTexelsSamplerId() = 0;
    HDST_API virtual GarchTextureGPUHandle GetTexelsTextureHandle() = 0;
    HDST_API virtual GarchTextureGPUHandle GetLayoutTextureId() = 0;
    HDST_API virtual GarchTextureGPUHandle GetLayoutTextureHandle() = 0;
};

/// HdStSimpleTextureResource is a simple (non-drawtarget) texture.
class HdStSimpleTextureResource : public HdStTextureResource {
public:

    HDST_API
    virtual ~HdStSimpleTextureResource();
    
    virtual HdTextureType GetTextureType() const override;
    HDST_API virtual GarchTextureGPUHandle GetTexelsTextureId() override;
    HDST_API virtual GarchTextureGPUHandle GetLayoutTextureId() override;

    HDST_API virtual GarchSamplerGPUHandle GetTexelsSamplerId() override = 0;
    HDST_API virtual GarchTextureGPUHandle GetTexelsTextureHandle() override = 0;
    HDST_API virtual GarchTextureGPUHandle GetLayoutTextureHandle() override = 0;

    virtual size_t GetMemoryUsed() override = 0;

protected:
    GarchTextureHandleRefPtr _textureHandle;
    GarchTextureRefPtr _texture;
    GfVec4f _borderColor;
    float _maxAnisotropy;
    GarchSamplerGPUHandle _sampler;
    HdTextureType _textureType;
    size_t _memoryRequest;
    HdWrap _wrapS;
    HdWrap _wrapT;
    HdWrap _wrapR;
    HdMinFilter _minFilter;
    HdMagFilter _magFilter;

    
protected:
    HDST_API
    HdStSimpleTextureResource(GarchTextureHandleRefPtr const &textureHandle,
                              HdTextureType textureType,
                              HdWrap wrapS, HdWrap wrapT, HdWrap wrapR,
                              HdMinFilter minFilter, HdMagFilter magFilter,
                              size_t memoryRequest);
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif //PXR_IMAGING_HD_ST_TEXTURE_RESOURCE_H
