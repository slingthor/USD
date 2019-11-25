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

//#include "pxr/imaging/glf/glew.h"

#include "pxr/imaging/garch/contextCaps.h"
#include "pxr/imaging/garch/resourceFactory.h"

#include "pxr/imaging/hdSt/fieldResource.h"
#include "pxr/imaging/hdSt/resourceFactory.h"

PXR_NAMESPACE_OPEN_SCOPE

static
GfBBox3d
_GetBoundingBox(const GarchTextureHandleRefPtr &textureHandle)
{
    // Eventually, we want to use an abstract class like GlfFieldTexture
    // to abstract away the fact that this is baked by an OpenVDB file.
    using FieldTextureRefPtr = GarchVdbTextureRefPtr;

    if (!textureHandle) {
        return GfBBox3d();
    }
    FieldTextureRefPtr const vdbTexture = TfDynamic_cast<FieldTextureRefPtr>(
        textureHandle->GetTexture());
    if (!vdbTexture) {
        return GfBBox3d();
    }
    return vdbTexture->GetBoundingBox();
}

HdStFieldResource::HdStFieldResource(const GarchTextureHandleRefPtr &textureHandle)
    : _simpleTextureResource(
         HdStResourceFactory::GetInstance()->NewSimpleTextureResource(
             textureHandle,
             HdTextureType::Uvw,
             HdWrapBlack,
             HdWrapBlack,
             HdWrapBlack,
             HdMinFilterLinear,
             HdMagFilterLinear)),
      _textureId(_simpleTextureResource->GetTexelsTextureId()),
      _samplerId(_simpleTextureResource->GetTexelsSamplerId()),
      // Note that we use the grid transform from the grid in the openvdb file
      // but ignore the transform on the field prim itself (e.g., authored
      // on OpenVDBAsset in USD).
      _boundingBox(_GetBoundingBox(textureHandle))
{
    GarchContextCaps const &caps = GarchResourceFactory::GetInstance()->GetContextCaps();

    if (caps.bindlessTextureEnabled) {
        _glTextureHandle = _simpleTextureResource->GetTexelsTextureHandle();
    }
}

HdStFieldResource::~HdStFieldResource() = default;

HdTextureType 
HdStFieldResource::GetTextureType() const
{
    return HdTextureType::Uvw;
}

size_t
HdStFieldResource::GetMemoryUsed()
{
    return _simpleTextureResource->GetMemoryUsed();
}

GarchTextureGPUHandle
HdStFieldResource::GetTexelsTextureId()
{
    return _textureId;
}

GarchSamplerGPUHandle
HdStFieldResource::GetTexelsSamplerId()
{
    return _samplerId;
}

GarchTextureGPUHandle
HdStFieldResource::GetTexelsTextureHandle()
{
    return _glTextureHandle;
}

GarchTextureGPUHandle
HdStFieldResource::GetLayoutTextureId()
{
    return GarchTextureGPUHandle();
}

GarchTextureGPUHandle
HdStFieldResource::GetLayoutTextureHandle()
{
    return GarchTextureGPUHandle();
}

PXR_NAMESPACE_CLOSE_SCOPE
