//
// Copyright 2017 Pixar
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
#ifndef HDST_DRAW_TARGET_TEXTURE_RESOURCE_H
#define HDST_DRAW_TARGET_TEXTURE_RESOURCE_H

#include "pxr/pxr.h"
#include "pxr/imaging/hd/textureResource.h"
#include "pxr/imaging/garch/drawTarget.h"

PXR_NAMESPACE_OPEN_SCOPE


class HdSt_DrawTargetTextureResource : public HdTextureResource {
public:
    virtual ~HdSt_DrawTargetTextureResource();

    static HdTextureResourceSharedPtr New();
    
    virtual void SetAttachment(const GarchDrawTarget::AttachmentRefPtr &attachment);
    virtual void SetSampler(HdWrap wrapS, HdWrap wrapT,
                            HdMinFilter minFilter, HdMagFilter magFilter) = 0;

    //
    // HdTextureResource API
    //
    virtual bool IsPtex() const override;

    virtual GarchTextureGPUHandle GetTexelsTextureId() override;
    virtual GarchSamplerGPUHandle GetTexelsSamplerId() override;
    virtual GarchTextureGPUHandle GetTexelsTextureHandle() = 0;

    virtual GarchTextureGPUHandle GetLayoutTextureId() override;
    virtual GarchTextureGPUHandle GetLayoutTextureHandle() override;

    virtual size_t GetMemoryUsed() override;

protected:
    GarchDrawTarget::AttachmentRefPtr  _attachment;
    GarchSamplerGPUHandle              _sampler;
    
    HdSt_DrawTargetTextureResource();

private:
    // No copying
    HdSt_DrawTargetTextureResource(const HdSt_DrawTargetTextureResource &)             = delete;
    HdSt_DrawTargetTextureResource &operator =(const HdSt_DrawTargetTextureResource &) = delete;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDST_DRAW_TARGET_TEXTURE_RESOURCE_H
