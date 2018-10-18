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

#include "pxr/imaging/hdSt/textureResource.h"
#include "pxr/imaging/hdSt/GL/glConversions.h"
#include "pxr/imaging/hdSt/GL/textureResourceGL.h"
#if defined(ARCH_GFX_METAL)
#include "pxr/imaging/hdSt/Metal/textureResourceMetal.h"
#endif

#include "pxr/imaging/hd/engine.h"
#include "pxr/imaging/hd/perfLog.h"

PXR_NAMESPACE_OPEN_SCOPE

HdStSimpleTextureResource *HdStSimpleTextureResource::New(GarchTextureHandleRefPtr const &textureHandle, bool isPtex,
                                                          size_t memoryRequest)
{
    HdEngine::RenderAPI api = HdEngine::GetRenderAPI();
    switch(api)
    {
        case HdEngine::OpenGL:
            return new HdStSimpleTextureResourceGL(textureHandle, isPtex, memoryRequest);
#if defined(ARCH_GFX_METAL)
        case HdEngine::Metal:
            return new HdStSimpleTextureResourceMetal(textureHandle, isPtex, memoryRequest);
#endif
        default:
            TF_FATAL_CODING_ERROR("No HdStBufferResource for this API");
    }
    return NULL;
}

HdStSimpleTextureResource *HdStSimpleTextureResource::New(
    GarchTextureHandleRefPtr const &textureHandle, bool isPtex,
    HdWrap wrapS, HdWrap wrapT,
    HdMinFilter minFilter, HdMagFilter magFilter,
    size_t memoryRequest)
{
    HdEngine::RenderAPI api = HdEngine::GetRenderAPI();
    switch(api)
    {
        case HdEngine::OpenGL:
            return new HdStSimpleTextureResourceGL(textureHandle, isPtex, wrapS, wrapT, minFilter, magFilter, memoryRequest);
#if defined(ARCH_GFX_METAL)
        case HdEngine::Metal:
            return new HdStSimpleTextureResourceMetal(textureHandle, isPtex, wrapS, wrapT, minFilter, magFilter, memoryRequest);
#endif
        default:
            TF_FATAL_CODING_ERROR("No HdStBufferResource for this API");
    }
    return NULL;
}

HdStTextureResource::~HdStTextureResource()
{
    /*Nothing*/
}

HdStSimpleTextureResource::HdStSimpleTextureResource(
     GarchTextureHandleRefPtr const &textureHandle, bool isPtex,
     HdWrap wrapS, HdWrap wrapT,
     HdMinFilter minFilter, HdMagFilter magFilter,
     size_t memoryRequest)
: _textureHandle(textureHandle)
, _texture()
, _borderColor(0.0,0.0,0.0,0.0)
, _maxAnisotropy(16.0)
, _sampler()
, _isPtex(isPtex)
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
    /*Nothing*/
}

GarchTextureGPUHandle HdStSimpleTextureResource::GetTexelsTextureId()
{
    if (_isPtex) {
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
    
    return _texture->GetTextureName();
}

GarchTextureGPUHandle HdStSimpleTextureResource::GetLayoutTextureId()
{
#ifdef PXR_PTEX_SUPPORT_ENABLED
    GarchPtexTextureRefPtr ptexTexture =
        TfDynamic_cast<GarchPtexTextureRefPtr>(_texture);
    
    if (ptexTexture) {
        return ptexTexture->GetLayoutTextureName();
    }
    return GarchTextureGPUHandle();
#else
    TF_CODING_ERROR("Ptex support is disabled.  "
                    "This code path should be unreachable");
    return GarchTextureGPUHandle();
#endif
}

PXR_NAMESPACE_CLOSE_SCOPE

