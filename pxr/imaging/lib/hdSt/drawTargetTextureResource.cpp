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
#include "pxr/imaging/glf/glew.h"

#include "pxr/imaging/hdSt/drawTargetTextureResource.h"
#include "pxr/imaging/hdSt/glConversions.h"

#include "pxr/imaging/hd/engine.h"

PXR_NAMESPACE_OPEN_SCOPE

HdSt_DrawTargetTextureResource::HdSt_DrawTargetTextureResource()
 : HdStTextureResource()
 , _attachment()
 , _sampler()
 , _borderColor(0.0,0.0,0.0,0.0)
 , _maxAnisotropy(16.0)
{
}

HdSt_DrawTargetTextureResource::~HdSt_DrawTargetTextureResource()
{
}

void
HdSt_DrawTargetTextureResource::SetAttachment(
                              const GarchDrawTarget::AttachmentRefPtr &attachment)
{
    _attachment = attachment;
}

HdTextureType
HdSt_DrawTargetTextureResource::GetTextureType() const
{
    return HdTextureType::Uv;
}

GarchTextureGPUHandle
HdSt_DrawTargetTextureResource::GetTexelsTextureId()
{
    return _attachment->GetTextureName();
}

GarchSamplerGPUHandle
HdSt_DrawTargetTextureResource::GetTexelsSamplerId()
{
    return _sampler;
}

GarchTextureGPUHandle
HdSt_DrawTargetTextureResource::GetLayoutTextureId()
{
    TF_CODING_ERROR("Draw targets are not ptex");
    return GarchTextureGPUHandle();
}

GarchTextureGPUHandle
HdSt_DrawTargetTextureResource::GetLayoutTextureHandle()
{
    TF_CODING_ERROR("Draw targets are not ptex");
    return GarchTextureGPUHandle();
}

size_t
HdSt_DrawTargetTextureResource::GetMemoryUsed()
{
    return _attachment->GetMemoryUsed();
}

PXR_NAMESPACE_CLOSE_SCOPE

