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

HdStSimpleTextureResourceMetal::HdStSimpleTextureResourceMetal(
    GarchTextureHandleRefPtr const &textureHandle, bool isPtex, size_t memoryRequest):
        HdStSimpleTextureResourceMetal(textureHandle, isPtex,
            /*wrapS*/ HdWrapUseMetadata, /*wrapT*/ HdWrapUseMetadata,
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
            , _texture(nil)
            , _borderColor(0.0,0.0,0.0,0.0)
            , _maxAnisotropy(16.0)
            , _sampler(nil)
            , _isPtex(isPtex)
            , _memoryRequest(memoryRequest)
            , _wrapS(wrapS)
            , _wrapT(wrapT)
            , _minFilter(minFilter)
            , _magFilter(magFilter)
{
    // In cases of upstream errors, texture handle can be null.
    if (_textureHandle) {
        _texture = _textureHandle->GetTexture();
        
        
        // Unconditionally add the memory request, before the early function
        // exit so that the destructor doesn't need to figure out if the request
        // was added or not.
        _textureHandle->AddMemoryRequest(_memoryRequest);
    }
}

HdStSimpleTextureResourceMetal::~HdStSimpleTextureResourceMetal()
{
    if (_textureHandle) {
        _textureHandle->DeleteMemoryRequest(_memoryRequest);
    }

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
        MtlfPtexTextureRefPtr ptexTexture =
            TfDynamic_cast<MtlfPtexTextureRefPtr>(_texture);
        
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

GarchSamplerGPUHandle HdStSimpleTextureResourceMetal::GetTexelsSamplerId()
{
    if (!TF_VERIFY(!_isPtex)) {
        return GarchSamplerGPUHandle();
    }

    if (_sampler == nil) {
        // If the HdSimpleTextureResource defines a wrap mode it will
        // use it, otherwise it gives an opportunity to the texture to define
        // its own wrap mode. The fallback value is always HdWrapRepeat
        MTLSamplerAddressMode fwrapS = HdStMetalConversions::GetWrap(_wrapS);
        MTLSamplerAddressMode fwrapT = HdStMetalConversions::GetWrap(_wrapT);
        MTLSamplerMinMagFilter fminFilter = HdStMetalConversions::GetMinFilter(_minFilter);
        MTLSamplerMinMagFilter fmagFilter = HdStMetalConversions::GetMagFilter(_magFilter);
        MTLSamplerMipFilter fmipFilter = HdStMetalConversions::GetMipFilter(_minFilter);
        
        if (_texture) {
            VtDictionary txInfo = _texture->GetTextureInfo(true);
            
            if (_wrapS == HdWrapUseMetadata &&
                VtDictionaryIsHolding<GLuint>(txInfo, "wrapModeS")) {
                fwrapS = HdStMetalConversions::ConvertGLWrap(VtDictionaryGet<GLuint>(txInfo, "wrapModeS"));
            }
            
            if (_wrapT == HdWrapUseMetadata &&
                VtDictionaryIsHolding<GLuint>(txInfo, "wrapModeT")) {
                fwrapT = HdStMetalConversions::ConvertGLWrap(VtDictionaryGet<GLuint>(txInfo, "wrapModeT"));
            }
            
            if (!_texture->IsMinFilterSupported(fminFilter)) {
                fminFilter = MTLSamplerMinMagFilterNearest;
            }
            if (!_texture->IsMagFilterSupported(fmagFilter)) {
                fmagFilter = MTLSamplerMinMagFilterNearest;
            }
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
    return _sampler;
}

GarchTextureGPUHandle HdStSimpleTextureResourceMetal::GetTexelsTextureHandle()
{ 
    GarchTextureGPUHandle textureId = GetTexelsTextureId();

    if (_isPtex) {
        return textureId;
    } 

    return textureId;
}

GarchTextureGPUHandle HdStSimpleTextureResourceMetal::GetLayoutTextureId()
{
#ifdef PXR_PTEX_SUPPORT_ENABLED
    MtlfPtexTextureRefPtr ptexTexture =
    TfDynamic_cast<MtlfPtexTextureRefPtr>(_texture);
    
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

