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
#ifndef HDST_TEXTURE_RESOURCE_METAL_H
#define HDST_TEXTURE_RESOURCE_METAL_H

#include "pxr/pxr.h"
#include "pxr/imaging/hd/api.h"
#include "pxr/imaging/hd/enums.h"
#include "pxr/imaging/hd/texture.h"
#include "pxr/imaging/hdSt/textureResource.h"

#include "pxr/imaging/garch/textureHandle.h"
#include "pxr/imaging/garch/gl.h"

#include "pxr/base/tf/declarePtrs.h"
#include "pxr/base/tf/token.h"
#include "pxr/base/gf/vec4f.h"

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>

#include <cstdint>

PXR_NAMESPACE_OPEN_SCOPE


class HdStSimpleTextureResourceMetal : public HdStSimpleTextureResource {
public:
    HDST_API
    HdStSimpleTextureResourceMetal(GarchTextureHandleRefPtr const &textureHandle, bool isPtex, size_t memoryRequest);
    HDST_API
    HdStSimpleTextureResourceMetal(GarchTextureHandleRefPtr const &textureHandle, bool isPtex,
        HdWrap wrapS, HdWrap wrapT, HdMinFilter minFilter, HdMagFilter magFilter, size_t memoryRequest);
    HDST_API
    virtual ~HdStSimpleTextureResourceMetal();

    HDST_API
    virtual bool IsPtex() const override;

    HDST_API
    virtual GarchSamplerGPUHandle GetTexelsSamplerId() override;
    HDST_API
    virtual GarchTextureGPUHandle GetTexelsTextureHandle() override;

    HDST_API
    virtual GarchTextureGPUHandle GetLayoutTextureHandle() override;

    HDST_API
    virtual size_t GetMemoryUsed();
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif //HDST_TEXTURE_RESOURCE_METAL_H
