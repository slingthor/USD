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
#include "pxr/imaging/hgiInterop/hgiInterop.h"
#include "pxr/base/arch/defines.h"
#include "pxr/base/plug/plugin.h"
#include "pxr/base/plug/registry.h"

#include "pxr/imaging/hgiMetal/hgi.h"
#include "pxr/imaging/hgiMetal/texture.h"

#include "pxr/imaging/hgiInterop/metal.h"

PXR_NAMESPACE_OPEN_SCOPE

HgiInterop::HgiInterop()
: _flipImage(false)
{
}

HgiInterop::~HgiInterop()
{
}

void HgiInterop::SetFlipOnBlit(bool flipY)
{
    _flipImage = flipY;
}

void HgiInterop::TransferToApp(
    Hgi *hgi,
    HgiTextureHandle const &color,
    HgiTextureHandle const &depth)
{
    HgiMetal *hgiMetal = static_cast<HgiMetal*>(hgi);
    
    if (!hgiMetal->GetNeedsInterop()) {
        return;
    }

    HgiMetalTexture *metalColor = static_cast<HgiMetalTexture*>(color.Get());
    HgiMetalTexture *metalDepth = static_cast<HgiMetalTexture*>(depth.Get());
 
    if (!_metalToOpenGL) {
        _metalToOpenGL.reset(
            new HgiInteropMetal(hgiMetal->GetDevice()));
        _metalToOpenGL->AllocateAttachments(
            metalColor->GetDescriptor().dimensions[0],
            metalColor->GetDescriptor().dimensions[1]);
    }
    
    id<MTLTexture> colorTexture = nil;
    id<MTLTexture> depthTexture = nil;
    if (metalColor) {
        colorTexture = metalColor->GetTextureId();
    }
    if (metalDepth) {
        depthTexture = metalDepth->GetTextureId();
    }

    _metalToOpenGL->CopyToInterop(
        hgi,
        colorTexture,
        depthTexture,
        _flipImage);
}

PXR_NAMESPACE_CLOSE_SCOPE
