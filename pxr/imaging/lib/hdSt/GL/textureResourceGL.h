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
#ifndef HDST_TEXTURE_RESOURCE_GL_H
#define HDST_TEXTURE_RESOURCE_GL_H

#include "pxr/pxr.h"
#include "pxr/imaging/hdSt/api.h"
#include "pxr/imaging/hdSt/textureResource.h"

PXR_NAMESPACE_OPEN_SCOPE


class HdStSimpleTextureResourceGL : public HdStSimpleTextureResource {
public:
    HDST_API
    HdStSimpleTextureResourceGL(GarchTextureHandleRefPtr const &textureHandle,
                                HdTextureType textureType,
                                size_t memoryRequest);
    HDST_API
    HdStSimpleTextureResourceGL(GarchTextureHandleRefPtr const &textureHandle,
                                HdTextureType textureType,
                                HdWrap wrapS, HdWrap wrapT,
                                HdMinFilter minFilter, HdMagFilter magFilter,
                                size_t memoryRequest);
    HDST_API
    virtual ~HdStSimpleTextureResourceGL();

    HDST_API
    virtual GarchSamplerGPUHandle GetTexelsSamplerId() override;
    HDST_API
    virtual GarchTextureGPUHandle GetTexelsTextureHandle() override;

    HDST_API
    virtual GarchTextureGPUHandle GetLayoutTextureHandle() override;

    HDST_API
    virtual size_t GetMemoryUsed() override;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif //HD_TEXTURE_RESOURCE_GL_H
