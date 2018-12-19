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
#include "pxr/imaging/glf/glew.h"

#include "pxr/imaging/garch/baseTexture.h"
#include "pxr/imaging/garch/contextCaps.h"
#include "pxr/imaging/garch/ptexTexture.h"
#include "pxr/imaging/garch/resourceFactory.h"
#include "pxr/imaging/garch/udimTexture.h"

#include "pxr/imaging/hdSt/glConversions.h"
#include "pxr/imaging/hdSt/textureResource.h"

#include "pxr/imaging/hd/engine.h"
#include "pxr/imaging/hd/perfLog.h"

PXR_NAMESPACE_OPEN_SCOPE

HdStTextureResource::~HdStTextureResource()
{
    /*Nothing*/
}

HdStSimpleTextureResource::HdStSimpleTextureResource(
     								GarchTextureHandleRefPtr const &textureHandle,
                                    HdTextureType textureType,
     								HdWrap wrapS, HdWrap wrapT,
     								HdMinFilter minFilter,
									HdMagFilter magFilter,
     								size_t memoryRequest)
: _textureHandle(textureHandle)
, _texture()
, _borderColor(0.0,0.0,0.0,0.0)
, _maxAnisotropy(16.0)
, _sampler()
, _textureType(textureType)
, _memoryRequest(memoryRequest)
, _wrapS(wrapS)
, _wrapT(wrapT)
, _minFilter(minFilter)
, _magFilter(magFilter)
{
    /*Nothing*/
}

HdStSimpleTextureResource::~HdStSimpleTextureResource()
{
}

HdTextureType HdStSimpleTextureResource::GetTextureType() const
{
    return _textureType;
}

GarchTextureGPUHandle HdStSimpleTextureResource::GetTexelsTextureId()
{
    if (_textureType == HdTextureType::Ptex) {
#ifdef PXR_PTEX_SUPPORT_ENABLED
        GarchPtexTextureRefPtr ptexTexture =
            TfDynamic_cast<GarchPtexTextureRefPtr>(_texture);
        
        if (ptexTexture) {
            return ptexTexture->GetTexelsTextureName();
        }
        return GarchTextureGPUHandle();
#else
        TF_CODING_ERROR("Ptex support is disabled.  "
                        "This code path should be unreachable");
        return GarchTextureGPUHandle();
#endif
    }
    
    if (_textureType == HdTextureType::Udim) {
        GarchUdimTextureRefPtr udimTexture =
            TfDynamic_cast<GarchUdimTextureRefPtr>(_texture);
        if (udimTexture) {
            return udimTexture->GetTextureName();
        }

        return GarchTextureGPUHandle();
    }

    if (_texture) {
        return _texture->GetTextureName();
    }
    return GarchTextureGPUHandle();
}

GarchTextureGPUHandle HdStSimpleTextureResource::GetLayoutTextureId()
{
    if (_textureType == HdTextureType::Udim) {
        GarchUdimTextureRefPtr udimTexture =
            TfDynamic_cast<GarchUdimTextureRefPtr>(_texture);
        if (udimTexture) {
            return udimTexture->GetLayoutName();
        }
    } else if (_textureType == HdTextureType::Ptex) {
#ifdef PXR_PTEX_SUPPORT_ENABLED
        GarchPtexTextureRefPtr ptexTexture =
            TfDynamic_cast<GarchPtexTextureRefPtr>(_texture);

        if (ptexTexture) {
            return ptexTexture->GetLayoutTextureName();
        }
#else
        TF_CODING_ERROR("Ptex support is disabled.  "
                        "This code path should be unreachable");
#endif
    } else {
        TF_CODING_ERROR(
            "Using GetLayoutTextureId in a Uv texture is incorrect");
    }
    return GarchTextureGPUHandle();
}

PXR_NAMESPACE_CLOSE_SCOPE

