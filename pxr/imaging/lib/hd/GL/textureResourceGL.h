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
#ifndef HD_TEXTURE_RESOURCE_GL_H
#define HD_TEXTURE_RESOURCE_GL_H

#include "pxr/pxr.h"
#include "pxr/imaging/hd/api.h"
#include "pxr/imaging/hd/textureResource.h"

PXR_NAMESPACE_OPEN_SCOPE


typedef boost::shared_ptr<class HdTextureResource> HdTextureResourceSharedPtr;

class HdSimpleTextureResourceGL : public HdTextureResource
                                , boost::noncopyable {
public:
    HD_API
    HdSimpleTextureResourceGL(GarchTextureHandleRefPtr const &textureHandle, bool isPtex);
    HD_API
    HdSimpleTextureResourceGL(GarchTextureHandleRefPtr const &textureHandle, bool isPtex,
        HdWrap wrapS, HdWrap wrapT, HdMinFilter minFilter, HdMagFilter magFilter);
    HD_API
    virtual ~HdSimpleTextureResourceGL();

    HD_API
    virtual bool IsPtex() const override;

    HD_API
    virtual GarchTextureGPUHandle GetTexelsTextureId() override;
    HD_API
    virtual GarchSamplerGPUHandle GetTexelsSamplerId() override;
    HD_API
    virtual GarchTextureGPUHandle GetTexelsTextureHandle() override;

    HD_API
    virtual GarchTextureGPUHandle GetLayoutTextureId() override;
    HD_API
    virtual GarchTextureGPUHandle GetLayoutTextureHandle() override;

    HD_API
    virtual size_t GetMemoryUsed() override;

private:
    GarchTextureHandleRefPtr _textureHandle;
    GarchTextureRefPtr _texture;
    GfVec4f _borderColor;
    float _maxAnisotropy;
    GLuint _sampler;
    bool _isPtex;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif //HD_TEXTURE_RESOURCE_GL_H
