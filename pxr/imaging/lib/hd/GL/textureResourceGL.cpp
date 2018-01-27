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

#include "pxr/imaging/hd/GL/textureResourceGL.h"

#include "pxr/imaging/hd/conversions.h"
#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/renderContextCaps.h"
#include "pxr/imaging/glf/baseTexture.h"
#include "pxr/imaging/glf/ptexTexture.h"

PXR_NAMESPACE_OPEN_SCOPE

HdSimpleTextureResourceGL::HdSimpleTextureResourceGL(
    GarchTextureHandleRefPtr const &textureHandle, bool isPtex):
        HdSimpleTextureResourceGL(textureHandle, isPtex,
            /*wrapS*/ HdWrapUseMetaDict, /*wrapT*/ HdWrapUseMetaDict, 
            /*minFilter*/ HdMinFilterNearestMipmapLinear, 
            /*magFilter*/ HdMagFilterLinear)
{
}

HdSimpleTextureResourceGL::HdSimpleTextureResourceGL(
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
    if (!glGenSamplers) { // GL initialization guard for headless unit test
        return;
    }

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
        GLuint handle = (GLuint)(uint64_t)GetTexelsTextureHandle();
        if (handle) {
            if (!glIsTextureHandleResidentNV(handle)) {
                glMakeTextureHandleResidentNV(handle);
            }
        }

        if (_isPtex) {
            handle = (GLuint)(uint64_t)GetLayoutTextureHandle();
            if (handle) {
                if (!glIsTextureHandleResidentNV(handle)) {
                    glMakeTextureHandleResidentNV(handle);
                }
            }
        }
    }
}

HdSimpleTextureResourceGL::~HdSimpleTextureResourceGL()
{ 
    if (!_isPtex) {
        if (!glDeleteSamplers) { // GL initialization guard for headless unit test
            return;
        }
        glDeleteSamplers(1, &_sampler);
    }
}

bool HdSimpleTextureResourceGL::IsPtex() const
{ 
    return _isPtex; 
}

GarchTextureGPUHandle HdSimpleTextureResourceGL::GetTexelsTextureId()
{
    if (_isPtex) {
#ifdef PXR_PTEX_SUPPORT_ENABLED
        return TfDynamic_cast<GlfPtexTextureRefPtr>(_texture)->GetTexelsTextureName();
#else
        TF_CODING_ERROR("Ptex support is disabled.  "
            "This code path should be unreachable");
        return 0;
#endif
    }
    
    return _texture->GetTextureName();
}

GarchSamplerGPUHandle HdSimpleTextureResourceGL::GetTexelsSamplerId()
{
    return (GarchSamplerGPUHandle)(uint64_t)_sampler;
}

GarchTextureGPUHandle HdSimpleTextureResourceGL::GetTexelsTextureHandle()
{ 
    GLuint textureId = (GLuint)(uint64_t)GetTexelsTextureId();
    GLuint samplerId = (GLuint)(uint64_t)GetTexelsSamplerId();

    if (!TF_VERIFY(glGetTextureHandleARB) ||
        !TF_VERIFY(glGetTextureSamplerHandleARB)) {
        return 0;
    }

    if (_isPtex) {
        return textureId ? (GarchTextureGPUHandle)(uint64_t)glGetTextureHandleARB(textureId) : 0;
    } 

    return textureId ? (GarchTextureGPUHandle)(uint64_t)glGetTextureSamplerHandleARB(textureId, samplerId) : 0;
}

GarchTextureGPUHandle HdSimpleTextureResourceGL::GetLayoutTextureId()
{
#ifdef PXR_PTEX_SUPPORT_ENABLED
    TF_CODING_ERROR("Not Implemented"); // Make this graphics api abstract
    return TfDynamic_cast<GarchTextureGPUHandle>(_texture)->GetLayoutTextureName();
#else
    TF_CODING_ERROR("Ptex support is disabled.  "
        "This code path should be unreachable");
    return 0;
#endif
}

GarchTextureGPUHandle HdSimpleTextureResourceGL::GetLayoutTextureHandle()
{
    if (!TF_VERIFY(_isPtex)) {
        return 0;
    }
    
    if (!TF_VERIFY(glGetTextureHandleARB)) {
        return 0;
    }

    GarchTextureGPUHandle textureId = GetLayoutTextureId();

    return textureId ? (GarchTextureGPUHandle)(uint64_t)glGetTextureHandleARB((GLuint)(uint64_t)textureId) : 0;
}

size_t HdSimpleTextureResourceGL::GetMemoryUsed()
{
    return _texture->GetMemoryUsed();
}

PXR_NAMESPACE_CLOSE_SCOPE

