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

#include "pxr/imaging/hd/conversions.h"
#include "pxr/imaging/hd/engine.h"

#include "pxr/imaging/hdSt/GL/drawTargetTextureResourceGL.h"
#if defined(ARCH_GFX_METAL)
#include "pxr/imaging/hdSt/Metal/drawTargetTextureResourceMetal.h"
#endif

PXR_NAMESPACE_OPEN_SCOPE

HdTextureResourceSharedPtr HdSt_DrawTargetTextureResource::New()
{
    switch(HdEngine::GetRenderAPI()) {
        case HdEngine::OpenGL:
            return HdTextureResourceSharedPtr(new HdSt_DrawTargetTextureResourceGL());
#if defined(ARCH_GFX_METAL)
        case HdEngine::Metal:
            return HdTextureResourceSharedPtr(new HdSt_DrawTargetTextureResourceMetal());
#endif
        default:
            TF_FATAL_CODING_ERROR("No program for this API");
    }
    return HdTextureResourceSharedPtr();
}

HdSt_DrawTargetTextureResource::HdSt_DrawTargetTextureResource()
 : HdTextureResource()
 , _attachment()
 , _sampler(0)
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

bool
HdSt_DrawTargetTextureResource::IsPtex() const
{
    return false;
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
    return 0;
}

GarchTextureGPUHandle
HdSt_DrawTargetTextureResource::GetLayoutTextureHandle()
{
    TF_CODING_ERROR("Draw targets are not ptex");
    return 0;
}

size_t
HdSt_DrawTargetTextureResource::GetMemoryUsed()
{
    return _attachment->GetMemoryUsed();
}

PXR_NAMESPACE_CLOSE_SCOPE

