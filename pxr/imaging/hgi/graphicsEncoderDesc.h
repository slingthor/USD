//
// Copyright 2019 Pixar
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
#ifndef PXR_IMAGING_HGI_GRAPHICS_ENCODER_DESC_H
#define PXR_IMAGING_HGI_GRAPHICS_ENCODER_DESC_H

#include "pxr/pxr.h"
#include "pxr/imaging/hgi/api.h"
#include "pxr/imaging/hgi/enums.h"
#include "pxr/imaging/hgi/texture.h"
#include "pxr/base/gf/vec4f.h"
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE


/// \struct HgiAttachmentDesc
///
/// Describes the properties of a framebuffer attachment.
///
/// <ul>
/// <li>texture:
///   The texture used as render target attachment.</li>
/// <li>loadOp:
///   The operation to perform on the attachment pixel data prior to rendering.</li>
/// <li>storeOp:
///   The operation to perform on the attachment pixel data after rendering.</li>
/// <li>clearValue:
///   The value to clear the attachment with (r,g,b,a) or (depth,stencil,x,x)</li>
/// <li>blendEnabled:
///   Determines if a blend operation should be applied to the attachment.</li>
/// <li> ***BlendFactor:
///   The blend factors for source and destination.</li>
/// <li> ***BlendOp: 
///   The blending operation.</li>
///
struct HgiAttachmentDesc
{
    HgiAttachmentDesc() 
    : loadOp(HgiAttachmentLoadOpLoad)
    , storeOp(HgiAttachmentStoreOpStore)
    , clearValue(0)
    , blendEnabled(false)
    , srcColorBlendFactor(HgiBlendFactorZero)
    , dstColorBlendFactor(HgiBlendFactorZero)
    , colorBlendOp(HgiBlendOpAdd)
    , srcAlphaBlendFactor(HgiBlendFactorZero)
    , dstAlphaBlendFactor(HgiBlendFactorZero)
    , alphaBlendOp(HgiBlendOpAdd)
    {}

    HgiAttachmentLoadOp loadOp;
    HgiAttachmentStoreOp storeOp;
    GfVec4f clearValue;
    bool blendEnabled;
    HgiBlendFactor srcColorBlendFactor;
    HgiBlendFactor dstColorBlendFactor;
    HgiBlendOp colorBlendOp;
    HgiBlendFactor srcAlphaBlendFactor;
    HgiBlendFactor dstAlphaBlendFactor;
    HgiBlendOp alphaBlendOp;
};

typedef std::vector<HgiAttachmentDesc> HgiAttachmentDescVector;

HGI_API
bool operator==(
    const HgiAttachmentDesc& lhs,
    const HgiAttachmentDesc& rhs);

HGI_API
bool operator!=(
    const HgiAttachmentDesc& lhs,
    const HgiAttachmentDesc& rhs);

HGI_API
std::ostream& operator<<(
    std::ostream& out,
    const HgiAttachmentDesc& attachment);


/// \struct HgiGraphicsEncoderDesc
///
/// Describes the properties to begin a HgiGraphicsEncoder.
///
/// <ul>
/// <li>colorAttachmentDescs:
///   Describes each of the color attachments.</li>
/// <li>depthAttachmentDesc:
///   Describes the depth attachment (optional)</li>
/// <li>colorTextures:
///   The color attachment render targets.</li>
/// <li>depthAttachment:
///   The depth attachment render target (optional)</li>
/// <li>width:
///   Render target width (in pixels)</li>
/// <li>height:
///   Render target height (in pixels)</li>
/// </ul>
///
struct HgiGraphicsEncoderDesc
{
    HgiGraphicsEncoderDesc()
    : colorAttachmentDescs()
    , depthAttachmentDesc()
    , colorTextures()
    , depthTexture()
    , width(0)
    , height(0)
    {}

    inline bool HasAttachments() const {
        return !colorAttachmentDescs.empty() || depthTexture;
    }

    HgiAttachmentDescVector colorAttachmentDescs;
    HgiAttachmentDesc depthAttachmentDesc;

    HgiTextureHandleVector colorTextures;
    HgiTextureHandle depthTexture;

    uint32_t width;
    uint32_t height;
};

HGI_API
bool operator==(
    const HgiGraphicsEncoderDesc& lhs,
    const HgiGraphicsEncoderDesc& rhs);

HGI_API
bool operator!=(
    const HgiGraphicsEncoderDesc& lhs,
    const HgiGraphicsEncoderDesc& rhs);

HGI_API
std::ostream& operator<<(
    std::ostream& out,
    const HgiGraphicsEncoderDesc& encoder);


PXR_NAMESPACE_CLOSE_SCOPE

#endif
