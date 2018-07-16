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

#include "pxr/imaging/hdSt/Metal/textureResourceMetal.h"
#include "pxr/imaging/hdSt/Metal/metalConversions.h"

#include "pxr/imaging/hd/perfLog.h"

#include "pxr/imaging/mtlf/mtlDevice.h"
#include "pxr/imaging/mtlf/ptexTexture.h"

#include "pxr/imaging/garch/texture.h"

PXR_NAMESPACE_OPEN_SCOPE


TF_DEFINE_PRIVATE_TOKENS(
    _tokens,

    ((fallbackPtexPath, "PtExNoNsEnSe"))
    ((fallbackUVPath, "UvNoNsEnSe"))
);

static MTLSamplerAddressMode
ConvertWrap(GLuint wrap)
{
    switch (wrap) {
        case GL_CLAMP_TO_EDGE:
            return MTLSamplerAddressModeClampToEdge;
        case GL_REPEAT:
            return MTLSamplerAddressModeRepeat;
        case GL_CLAMP_TO_BORDER:
            return MTLSamplerAddressModeClampToBorderColor;
        case GL_MIRRORED_REPEAT:
            return MTLSamplerAddressModeMirrorRepeat;
    }
    
    TF_CODING_ERROR("Unexpected GL wrap type %d", wrap);
    return MTLSamplerAddressModeRepeat;
}

HdStSimpleTextureResourceMetal::HdStSimpleTextureResourceMetal(
    GarchTextureHandleRefPtr const &textureHandle, bool isPtex, size_t memoryRequest):
        HdStSimpleTextureResourceMetal(textureHandle, isPtex,
            /*wrapS*/ HdWrapUseMetaDict, /*wrapT*/ HdWrapUseMetaDict, 
            /*minFilter*/ HdMinFilterNearestMipmapLinear, 
            /*magFilter*/ HdMagFilterLinear, memoryRequest)
{
}

HdStSimpleTextureResourceMetal::HdStSimpleTextureResourceMetal(
    GarchTextureHandleRefPtr const &textureHandle, bool isPtex,
        HdWrap wrapS, HdWrap wrapT,
        HdMinFilter minFilter, HdMagFilter magFilter,
        size_t memoryRequest)
            : _textureHandle(textureHandle)
            , _texture(textureHandle->GetTexture())
            , _borderColor(0.0,0.0,0.0,0.0)
            , _maxAnisotropy(16.0)
            , _sampler()
            , _isPtex(isPtex)
            , _memoryRequest(memoryRequest)
{
    // When we are not using Ptex we will use samplers,
    // that includes both, bindless textures and no-bindless textures
    if (!_isPtex) {
        // If the HdSimpleTextureResource defines a wrap mode it will 
        // use it, otherwise it gives an opportunity to the texture to define
        // its own wrap mode. The fallback value is always HdWrapRepeat
        MTLSamplerAddressMode fwrapS = HdStMetalConversions::GetWrap(wrapS);
        MTLSamplerAddressMode fwrapT = HdStMetalConversions::GetWrap(wrapT);
        VtDictionary txInfo = _texture->GetTextureInfo();

        if (wrapS == HdWrapUseMetaDict && 
            VtDictionaryIsHolding<GLuint>(txInfo, "wrapModeS")) {
            fwrapS = ConvertWrap(VtDictionaryGet<GLuint>(txInfo, "wrapModeS"));
        }

        if (wrapT == HdWrapUseMetaDict && 
            VtDictionaryIsHolding<GLuint>(txInfo, "wrapModeT")) {
            fwrapT = ConvertWrap(VtDictionaryGet<GLuint>(txInfo, "wrapModeT"));
        }

        MTLSamplerMinMagFilter fminFilter = HdStMetalConversions::GetMinFilter(minFilter);
        MTLSamplerMinMagFilter fmagFilter = HdStMetalConversions::GetMagFilter(magFilter);
        MTLSamplerMipFilter fmipFilter = HdStMetalConversions::GetMipFilter(minFilter);
        if (!_texture->IsMinFilterSupported(fminFilter)) {
            fminFilter = MTLSamplerMinMagFilterNearest;
        }
        if (!_texture->IsMagFilterSupported(fmagFilter)) {
            fmagFilter = MTLSamplerMinMagFilterNearest;
        }

        MTLSamplerDescriptor* samplerDesc = [[MTLSamplerDescriptor alloc] init];

        samplerDesc.sAddressMode = fwrapS;
        samplerDesc.tAddressMode = fwrapT;
        samplerDesc.minFilter = fminFilter;
        samplerDesc.magFilter = fmagFilter;
        samplerDesc.mipFilter = fmipFilter;
        samplerDesc.maxAnisotropy = _maxAnisotropy;
        samplerDesc.borderColor = MTLSamplerBorderColorOpaqueBlack;
        
        id<MTLDevice> device = MtlfMetalContext::GetMetalContext()->device;
        _sampler = [device newSamplerStateWithDescriptor:samplerDesc];
    }
/*
    GarchTextureGPUHandle handle = GetTexelsTextureHandle();
    if (handle) {
        if (!glIsTextureHandleResidentNV(handle)) {
            glMakeTextureHandleResidentNV(handle);
        }
    }

    if (_isPtex) {
        handle = GetLayoutTextureHandle();
        if (handle) {
            if (!glIsTextureHandleResidentNV(handle)) {
                glMakeTextureHandleResidentNV(handle);
            }
        }
    }

     MTLTextureDescriptor* texDesc =
     [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format
        width:numPixels
        height:1
        mipmapped:NO];
     _texId = [_id newTextureWithDescriptor:texDesc offset:0 bytesPerRow:pixelSize * numPixels];
     */
}

HdStSimpleTextureResourceMetal::~HdStSimpleTextureResourceMetal()
{
    _textureHandle->DeleteMemoryRequest(_memoryRequest);

    if (!_isPtex) {
        [_sampler release];
    }
}

bool HdStSimpleTextureResourceMetal::IsPtex() const
{ 
    return _isPtex; 
}

GarchTextureGPUHandle HdStSimpleTextureResourceMetal::GetTexelsTextureId()
{
    if (_isPtex) {
#ifdef PXR_PTEX_SUPPORT_ENABLED
        TF_FATAL_CODING_ERROR("Not Implemented"); // Make this graphics api abstract
        return TfDynamic_cast<MtlfPtexTextureRefPtr>(_texture)->GetTexelsTextureName();
#else
        TF_CODING_ERROR("Ptex support is disabled.  "
            "This code path should be unreachable");
        return GarchTextureGPUHandle();
#endif
    }
    
    return _texture->GetTextureName();
}

GarchSamplerGPUHandle HdStSimpleTextureResourceMetal::GetTexelsSamplerId()
{
    return _sampler;
}

GarchTextureGPUHandle HdStSimpleTextureResourceMetal::GetTexelsTextureHandle()
{ 
    GarchTextureGPUHandle textureId = GetTexelsTextureId();
    GarchSamplerGPUHandle samplerId = GetTexelsSamplerId();

    if (_isPtex) {
        return textureId;
    } 

    return textureId;
}

GarchTextureGPUHandle HdStSimpleTextureResourceMetal::GetLayoutTextureId()
{
#ifdef PXR_PTEX_SUPPORT_ENABLED
    TF_FATAL_CODING_ERROR("Not Implemented"); // Make this graphics api abstract
    return TfDynamic_cast<MtlfPtexTextureRefPtr>(_texture)->GetLayoutTextureName();
#else
    TF_CODING_ERROR("Ptex support is disabled.  "
        "This code path should be unreachable");
    return GarchTextureGPUHandle();
#endif
}

GarchTextureGPUHandle HdStSimpleTextureResourceMetal::GetLayoutTextureHandle()
{
    if (!TF_VERIFY(_isPtex)) {
        return GarchTextureGPUHandle();
    }

    GarchTextureGPUHandle textureId = GetLayoutTextureId();

    TF_FATAL_CODING_ERROR("Not Implemented");
    return textureId;
}

size_t HdStSimpleTextureResourceMetal::GetMemoryUsed()
{
    return _texture->GetMemoryUsed();
}

PXR_NAMESPACE_CLOSE_SCOPE

