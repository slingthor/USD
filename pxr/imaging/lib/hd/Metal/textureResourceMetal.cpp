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

#include "pxr/imaging/hd/Metal/textureResourceMetal.h"

#include "pxr/imaging/hd/conversions.h"
#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/renderContextCaps.h"

#include "pxr/imaging/mtlf/ptexTexture.h"

#include "pxr/imaging/garch/texture.h"

PXR_NAMESPACE_OPEN_SCOPE


TF_DEFINE_PRIVATE_TOKENS(
    _tokens,

    ((fallbackPtexPath, "PtExNoNsEnSe"))
    ((fallbackUVPath, "UvNoNsEnSe"))
);

HdSimpleTextureResourceMetal::HdSimpleTextureResourceMetal(
    GarchTextureHandleRefPtr const &textureHandle, bool isPtex):
        HdSimpleTextureResourceMetal(textureHandle, isPtex,
            /*wrapS*/ HdWrapUseMetaDict, /*wrapT*/ HdWrapUseMetaDict, 
            /*minFilter*/ HdMinFilterNearestMipmapLinear, 
            /*magFilter*/ HdMagFilterLinear)
{
}

HdSimpleTextureResourceMetal::HdSimpleTextureResourceMetal(
    GarchTextureHandleRefPtr const &textureHandle, bool isPtex,
        HdWrap wrapS, HdWrap wrapT,
        HdMinFilter minFilter, HdMagFilter magFilter)
            : _textureHandle(textureHandle)
            , _texture(textureHandle->GetTexture())
            , _borderColor(0.0,0.0,0.0,0.0)
            , _maxAnisotropy(16.0)
            , _sampler(0)
            , _isPtex(isPtex)
{
    TF_CODING_ERROR("Not Implemented");
    /*
    // When we are not using Ptex we will use samplers,
    // that includes both, bindless textures and no-bindless textures
    if (!_isPtex) {
        // If the HdSimpleTextureResource defines a wrap mode it will 
        // use it, otherwise it gives an opportunity to the texture to define
        // its own wrap mode. The fallback value is always HdWrapRepeat
        GLenum fwrapS = HdConversions::GetWrap(wrapS);
        GLenum fwrapT = HdConversions::GetWrap(wrapT);
        VtDictionary txInfo = _texture->GetTextureInfo();

        if (wrapS == HdWrapUseMetaDict && 
            VtDictionaryIsHolding<GLuint>(txInfo, "wrapModeS")) {
            fwrapS = VtDictionaryGet<GLuint>(txInfo, "wrapModeS");
        }

        if (wrapT == HdWrapUseMetaDict && 
            VtDictionaryIsHolding<GLuint>(txInfo, "wrapModeT")) {
            fwrapT = VtDictionaryGet<GLuint>(txInfo, "wrapModeT");
        }

        GLenum fminFilter = HdConversions::GetMinFilter(minFilter);
        GLenum fmagFilter = HdConversions::GetMagFilter(magFilter);
        if (!_texture->IsMinFilterSupported(fminFilter)) {
            fminFilter = GL_NEAREST;
        }
        if (!_texture->IsMagFilterSupported(fmagFilter)) {
            fmagFilter = GL_NEAREST;
        }

        glGenSamplers(1, &_sampler);
        glSamplerParameteri(_sampler, GL_TEXTURE_WRAP_S, fwrapS);
        glSamplerParameteri(_sampler, GL_TEXTURE_WRAP_T, fwrapT);
        glSamplerParameteri(_sampler, GL_TEXTURE_MIN_FILTER, fminFilter);
        glSamplerParameteri(_sampler, GL_TEXTURE_MAG_FILTER, fmagFilter);
        glSamplerParameterf(_sampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, 
            _maxAnisotropy);
        glSamplerParameterfv(_sampler, GL_TEXTURE_BORDER_COLOR, 
            _borderColor.GetArray());
    }

    bool bindlessTexture = 
        HdRenderContextCaps::GetInstance().bindlessTextureEnabled;
    if (bindlessTexture) {
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
    }
     */
}

HdSimpleTextureResourceMetal::~HdSimpleTextureResourceMetal()
{
    TF_CODING_ERROR("Not Implemented");
    /*
    if (!_isPtex) {
        glDeleteSamplers(1, &_sampler);
    }
     */
}

bool HdSimpleTextureResourceMetal::IsPtex() const
{ 
    return _isPtex; 
}

GarchTextureGPUHandle HdSimpleTextureResourceMetal::GetTexelsTextureId()
{
    if (_isPtex) {
#ifdef PXR_PTEX_SUPPORT_ENABLED
        TF_CODING_ERROR("Not Implemented"); // Make this graphics api abstract
        return TfDynamic_cast<MtlfPtexTextureRefPtr>(_texture)->GetTexelsTextureName();
#else
        TF_CODING_ERROR("Ptex support is disabled.  "
            "This code path should be unreachable");
        return 0;
#endif
    }
    
    return _texture->GetTextureName();
}

GarchSamplerGPUHandle HdSimpleTextureResourceMetal::GetTexelsSamplerId()
{
    return _sampler;
}

GarchTextureGPUHandle HdSimpleTextureResourceMetal::GetTexelsTextureHandle()
{ 
    GarchTextureGPUHandle textureId = GetTexelsTextureId();
    GarchSamplerGPUHandle samplerId = GetTexelsSamplerId();

    TF_CODING_ERROR("Not Implemented"); // Make this graphics api abstract
    return 0;
    /*
    if (!TF_VERIFY(glGetTextureHandleARB) ||
        !TF_VERIFY(glGetTextureSamplerHandleARB)) {
        return 0;
    }

    if (_isPtex) {
        return textureId ? glGetTextureHandleARB(textureId) : 0;
    } 

    return textureId ? glGetTextureSamplerHandleARB(textureId, samplerId) : 0;
     */
}

GarchTextureGPUHandle HdSimpleTextureResourceMetal::GetLayoutTextureId()
{
#ifdef PXR_PTEX_SUPPORT_ENABLED
    TF_CODING_ERROR("Not Implemented"); // Make this graphics api abstract
    return TfDynamic_cast<MtlfPtexTextureRefPtr>(_texture)->GetLayoutTextureName();
#else
    TF_CODING_ERROR("Ptex support is disabled.  "
        "This code path should be unreachable");
    return 0;
#endif
}

GarchTextureGPUHandle HdSimpleTextureResourceMetal::GetLayoutTextureHandle()
{
    if (!TF_VERIFY(_isPtex)) {
        return 0;
    }

    GarchTextureGPUHandle textureId = GetLayoutTextureId();

    TF_CODING_ERROR("Not Implemented");
    return textureId;
}

size_t HdSimpleTextureResourceMetal::GetMemoryUsed()
{
    return _texture->GetMemoryUsed();
}

PXR_NAMESPACE_CLOSE_SCOPE

