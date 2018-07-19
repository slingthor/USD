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

#include "pxr/imaging/hdSt/GL/textureResourceGL.h"
#include "pxr/imaging/hdSt/GL/glConversions.h"

#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/glf/baseTexture.h"
#include "pxr/imaging/glf/ptexTexture.h"

#include "pxr/imaging/garch/contextCaps.h"
#include "pxr/imaging/garch/resourceFactory.h"

PXR_NAMESPACE_OPEN_SCOPE

HdStSimpleTextureResourceGL::HdStSimpleTextureResourceGL(
    GarchTextureHandleRefPtr const &textureHandle, bool isPtex, size_t memoryRequest):
        HdStSimpleTextureResourceGL(textureHandle, isPtex,
            /*wrapS*/ HdWrapUseMetaDict, /*wrapT*/ HdWrapUseMetaDict, 
            /*minFilter*/ HdMinFilterNearestMipmapLinear, 
            /*magFilter*/ HdMagFilterLinear, memoryRequest)
{
}

HdStSimpleTextureResourceGL::HdStSimpleTextureResourceGL(
    GarchTextureHandleRefPtr const &textureHandle, bool isPtex,
    HdWrap wrapS, HdWrap wrapT,
    HdMinFilter minFilter, HdMagFilter magFilter,
    size_t memoryRequest)
        : _textureHandle(textureHandle)
        , _texture()
        , _borderColor(0.0,0.0,0.0,0.0)
        , _maxAnisotropy(16.0)
        , _sampler(0)
        , _isPtex(isPtex)
        , _memoryRequest(memoryRequest)
{
    // In cases of upstream errors, texture handle can be null.
    if (_textureHandle) {
        _texture = _textureHandle->GetTexture();
        
        
        // Unconditionally add the memory request, before the early function
        // exit so that the destructor doesn't need to figure out if the request
        // was added or not.
        _textureHandle->AddMemoryRequest(_memoryRequest);
    }

    if (!glGenSamplers) { // GL initialization guard for headless unit test
        return;
    }

    // When we are not using Ptex we will use samplers,
    // that includes both, bindless textures and no-bindless textures
    if (!_isPtex) {
        // If the HdSimpleTextureResource defines a wrap mode it will 
        // use it, otherwise it gives an opportunity to the texture to define
        // its own wrap mode. The fallback value is always HdWrapRepeat
        GLenum fwrapS = HdStGLConversions::GetWrap(wrapS);
        GLenum fwrapT = HdStGLConversions::GetWrap(wrapT);
        GLenum fminFilter = HdStGLConversions::GetMinFilter(minFilter);
        GLenum fmagFilter = HdStGLConversions::GetMagFilter(magFilter);
        
        if (_texture) {
            VtDictionary txInfo = _texture->GetTextureInfo();

            if (wrapS == HdWrapUseMetaDict &&
                VtDictionaryIsHolding<GLuint>(txInfo, "wrapModeS")) {
                fwrapS = VtDictionaryGet<GLuint>(txInfo, "wrapModeS");
            }

            if (wrapT == HdWrapUseMetaDict &&
                VtDictionaryIsHolding<GLuint>(txInfo, "wrapModeT")) {
                fwrapT = VtDictionaryGet<GLuint>(txInfo, "wrapModeT");
            }

            if (!_texture->IsMinFilterSupported(fminFilter)) {
                fminFilter = GL_NEAREST;
            }
            if (!_texture->IsMagFilterSupported(fmagFilter)) {
                fmagFilter = GL_NEAREST;
            }
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
        GarchResourceFactory::GetInstance()->GetContextCaps().bindlessTextureEnabled;
    if (bindlessTexture) {
        GLuint handle = GetTexelsTextureHandle();
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
}

HdStSimpleTextureResourceGL::~HdStSimpleTextureResourceGL()
{
    if (_textureHandle) {
        _textureHandle->DeleteMemoryRequest(_memoryRequest);
    }

    if (!_isPtex) {
        if (!glDeleteSamplers) { // GL initialization guard for headless unit test
            return;
        }
        glDeleteSamplers(1, &_sampler);
    }
}

bool HdStSimpleTextureResourceGL::IsPtex() const
{ 
    return _isPtex; 
}

GarchTextureGPUHandle HdStSimpleTextureResourceGL::GetTexelsTextureId()
{
    if (_isPtex) {
#ifdef PXR_PTEX_SUPPORT_ENABLED
        GlfPtexTextureRefPtr ptexTexture =
        TfDynamic_cast<GlfPtexTextureRefPtr>(_texture);
        
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

GarchSamplerGPUHandle HdStSimpleTextureResourceGL::GetTexelsSamplerId()
{
    return _sampler;
}

GarchTextureGPUHandle HdStSimpleTextureResourceGL::GetTexelsTextureHandle()
{ 
    GLuint textureId = GetTexelsTextureId();
    GLuint samplerId = GetTexelsSamplerId();

    if (!TF_VERIFY(glGetTextureHandleARB) ||
        !TF_VERIFY(glGetTextureSamplerHandleARB)) {
        return GarchTextureGPUHandle();
    }

    if (_isPtex) {
        return GarchTextureGPUHandle(textureId ? glGetTextureHandleARB(textureId) : 0);
    } 

    return GarchTextureGPUHandle(textureId ? glGetTextureSamplerHandleARB(textureId, samplerId) : 0);
}

GarchTextureGPUHandle HdStSimpleTextureResourceGL::GetLayoutTextureId()
{
#ifdef PXR_PTEX_SUPPORT_ENABLED
    TF_FATAL_CODING_ERROR("Not Implemented"); // Make this graphics api abstract
    GlfPtexTextureRefPtr ptexTexture =
    TfDynamic_cast<GlfPtexTextureRefPtr>(_texture);
    
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

GarchTextureGPUHandle HdStSimpleTextureResourceGL::GetLayoutTextureHandle()
{
    if (!TF_VERIFY(_isPtex)) {
        return GarchTextureGPUHandle();
    }
    
    if (!TF_VERIFY(glGetTextureHandleARB)) {
        return GarchTextureGPUHandle();
    }

    GLuint textureId = GetLayoutTextureId();

    return GarchTextureGPUHandle(textureId ? glGetTextureHandleARB(textureId) : 0);
}

size_t HdStSimpleTextureResourceGL::GetMemoryUsed()
{
    return _texture->GetMemoryUsed();
}

PXR_NAMESPACE_CLOSE_SCOPE
