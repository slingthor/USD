//
// Copyright 2020 Pixar
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

#include "pxr/imaging/hdSt/samplerObject.h"

#include "pxr/imaging/hdSt/textureObject.h"
#include "pxr/imaging/hdSt/glConversions.h"

#include "pxr/imaging/glf/diagnostic.h"
#if defined(PXR_OPENGL_SUPPORT_ENABLED)
#include "pxr/imaging/hgiGL/texture.h"
#endif

PXR_NAMESPACE_OPEN_SCOPE

///////////////////////////////////////////////////////////////////////////////
// HdStTextureObject

HdStSamplerObject::~HdStSamplerObject() = default;

///////////////////////////////////////////////////////////////////////////////
// Helpers

// Generate GL sampler
static
GLuint
_GenGLSampler(HdSamplerParameters const &samplerParameters,
              const bool createSampler)
{
    if (!createSampler) {
        return 0;
    }

    GLuint result = 0;
#if defined(___PXR_OPENGL_SUPPORT_ENABLED)
    glGenSamplers(1, &result);

    glSamplerParameteri(
        result,
        GL_TEXTURE_WRAP_S,
        HdStGLConversions::GetWrap(samplerParameters.wrapS));

    glSamplerParameteri(
        result,
        GL_TEXTURE_WRAP_T,
        HdStGLConversions::GetWrap(samplerParameters.wrapT));

    glSamplerParameteri(
        result,
        GL_TEXTURE_WRAP_R,
        HdStGLConversions::GetWrap(samplerParameters.wrapR));

    glSamplerParameteri(
        result,
        GL_TEXTURE_MIN_FILTER,
        HdStGLConversions::GetMinFilter(samplerParameters.minFilter));

    glSamplerParameteri(
        result,
        GL_TEXTURE_MAG_FILTER,
        HdStGLConversions::GetMagFilter(samplerParameters.magFilter));

    static const GfVec4f borderColor(0.0);
    glSamplerParameterfv(
        result,
        GL_TEXTURE_BORDER_COLOR,
        borderColor.GetArray());

    static const float _maxAnisotropy = 16.0;

    glSamplerParameterf(
        result,
        GL_TEXTURE_MAX_ANISOTROPY_EXT,
        _maxAnisotropy);

    GLF_POST_PENDING_GL_ERRORS();
#endif
    return result;
}

// Get texture sampler handle for bindless textures.
static
GLuint64EXT
_GenGLTextureSamplerHandle(const GLuint textureName,
                           const GLuint samplerName,
                           const bool createBindlessHandle)
{
    if (!createBindlessHandle) {
        return 0;
    }

    if (textureName == 0) {
        return 0;
    }

    if (samplerName == 0) {
        return 0;
    }
#if defined(___PXR_OPENGL_SUPPORT_ENABLED)
    const GLuint64EXT result =
        glGetTextureSamplerHandleARB(textureName, samplerName);

    glMakeTextureHandleResidentARB(result);

    GLF_POST_PENDING_GL_ERRORS();

    return result;
#else
    TF_CODING_ERROR("OpenGL not enabled");
    return 0;
#endif
}

// Get texture sampler handle for bindless textures.
static
GLuint64EXT 
_GenGLTextureSamplerHandle(HgiTextureHandle const &textureHandle,
                           const GLuint samplerName,
                           const bool createBindlessHandle)
{
    if (!createBindlessHandle) {
        return 0;
    }

    HgiTexture * const texture = textureHandle.Get();
    if (texture == nullptr) {
        return 0;
    }
#if defined(PXR_OPENGL_SUPPORT_ENABLED)
    HgiGLTexture * const glTexture = dynamic_cast<HgiGLTexture*>(texture);
    if (glTexture == nullptr) {
        TF_CODING_ERROR("Only OpenGL textures supported");
        return 0;
    }

    return _GenGLTextureSamplerHandle(
        glTexture->GetTextureId(), samplerName, createBindlessHandle);
#else
    TF_CODING_ERROR("OpenGL not enabled");
    return 0;
#endif
}

// Get texture handle for bindless textures.
static
GLuint64EXT
_GenGlTextureHandle(const GLuint textureName,
                    const bool createGLTextureHandle)
{
    if (!createGLTextureHandle) {
        return 0;
    }

    if (textureName == 0) {
        return 0;
    }
#if defined(___PXR_OPENGL_SUPPORT_ENABLED)
    const GLuint64EXT result = glGetTextureHandleARB(textureName);
    glMakeTextureHandleResidentARB(result);

    GLF_POST_PENDING_GL_ERRORS();

    return result;
#else
    TF_CODING_ERROR("OpenGL not enabled");
    return 0;
#endif
}

///////////////////////////////////////////////////////////////////////////////
// Uv sampler

// Resolve a wrap parameter using the opinion authored in the metadata of a
// texture file.
static
void
_ResolveSamplerParameter(
    const HdWrap textureOpinion,
    HdWrap * const parameter)
{
    if (*parameter == HdWrapNoOpinion) {
        *parameter = textureOpinion;
    }

    // Legacy behavior for HwUvTexture_1
    if (*parameter == HdWrapLegacyNoOpinionFallbackRepeat) {
        if (textureOpinion == HdWrapNoOpinion) {
            // Use repeat if there is no opinion on either the
            // texture node or in the texture file.
            *parameter = HdWrapRepeat;
        } else {
            *parameter = textureOpinion;
        }
    }
}

// Resolve wrapS or wrapT of the samplerParameters using metadata
// from the texture file.
static
HdSamplerParameters
_ResolveUvSamplerParameters(
    HdStUvTextureObject const &texture,
    HdSamplerParameters const &samplerParameters)
{
    HdSamplerParameters result = samplerParameters;
    _ResolveSamplerParameter(
        texture.GetWrapParameters().first,
        &result.wrapS);

    _ResolveSamplerParameter(
        texture.GetWrapParameters().second,
        &result.wrapT);

    return result;
}

HdStUvSamplerObject::HdStUvSamplerObject(
    HdStUvTextureObject const &texture,
    HdSamplerParameters const &samplerParameters,
    const bool createBindlessHandle)
  : _glSamplerName(
      _GenGLSampler(
          _ResolveUvSamplerParameters(
              texture, samplerParameters),
          texture.IsValid()))
  , _glTextureSamplerHandle(
      _GenGLTextureSamplerHandle(
          texture.GetTexture(),
          _glSamplerName,
          createBindlessHandle && texture.IsValid()))
{
}

HdStUvSamplerObject::~HdStUvSamplerObject()
{
    // Deleting the GL sampler automatically deletes the
    // texture sampler handle.
    // In fact, even destroying the underlying texture (which
    // is out of our control here), deletes the texture sampler
    // handle and the same texture sampler handle might be re-used
    // by the driver, so it is unsafe to call
    // glMakeTextureHandleNonResidentARB(_glTextureSamplerHandle);
    // here: HdStTextureObject might destroy a GPU texture either
    // because it itself was destroyed or because the file was
    // reloaded or target memory was changed.

    if (_glSamplerName) {
#if defined(PXR_OPENGL_SUPPORT_ENABLED)
        glDeleteSamplers(1, &_glSamplerName);
#endif
    }
}

///////////////////////////////////////////////////////////////////////////////
// Field sampler

HdStFieldSamplerObject::HdStFieldSamplerObject(
    HdStFieldTextureObject const &texture,
    HdSamplerParameters const &samplerParameters,
    const bool createBindlessHandle)
  : _glSamplerName(
      _GenGLSampler(
          samplerParameters,
          texture.IsValid()))
  , _glTextureSamplerHandle(
      _GenGLTextureSamplerHandle(
          texture.GetTexture(),
          _glSamplerName,
          createBindlessHandle && texture.IsValid()))
{
}

HdStFieldSamplerObject::~HdStFieldSamplerObject()
{
    // See above comment about destroying _glTextureSamplerHandle
    if (_glSamplerName) {
#if defined(PXR_OPENGL_SUPPORT_ENABLED)
        glDeleteSamplers(1, &_glSamplerName);
#endif
    }
}

///////////////////////////////////////////////////////////////////////////////
// Ptex sampler

HdStPtexSamplerObject::HdStPtexSamplerObject(
    HdStPtexTextureObject const &ptexTexture,
    // samplerParameters are ignored are ptex
    HdSamplerParameters const &samplerParameters,
    const bool createBindlessHandle)
  : _texelsGLTextureHandle(
      _GenGlTextureHandle(
#if defined(PXR_OPENGL_SUPPORT_ENABLED)
          ptexTexture.GetTexelGLTextureName(),
#else
          0,
#endif
          createBindlessHandle && ptexTexture.IsValid()))
  , _layoutGLTextureHandle(
      _GenGlTextureHandle(
#if defined(PXR_OPENGL_SUPPORT_ENABLED)
          ptexTexture.GetLayoutGLTextureName(),
#else
          0,
#endif
          createBindlessHandle && ptexTexture.IsValid()))
{
}

// See above comment about destroying bindless texture handles
HdStPtexSamplerObject::~HdStPtexSamplerObject() = default;

///////////////////////////////////////////////////////////////////////////////
// Udim sampler

// Wrap modes such as repeat or mirror do not make sense for udim, so set them
// to clamp.
//
// Mipmaps would make sense for udim up to a certain level, but
// GlfUdimTexture produces broken mipmaps, so forcing HdMinFilterLinear.
// The old texture system apparently never exercised the case of using
// mipmaps for a udim.
static
HdSamplerParameters UDIM_SAMPLER_PARAMETERS{
    HdWrapClamp,
    HdWrapClamp,
    HdWrapClamp,
    HdMinFilterLinear,
    HdMagFilterLinear};

HdStUdimSamplerObject::HdStUdimSamplerObject(
    HdStUdimTextureObject const &udimTexture,
    HdSamplerParameters const &samplerParameters,
    const bool createBindlessHandle)
  : _glTexelsSamplerName(
      _GenGLSampler(
          UDIM_SAMPLER_PARAMETERS,
          udimTexture.IsValid()))
  , _texelsGLTextureHandle(
      _GenGLTextureSamplerHandle(
#if defined(PXR_OPENGL_SUPPORT_ENABLED)
          udimTexture.GetTexelGLTextureName(),
#else
          0,
#endif
          _glTexelsSamplerName,
          createBindlessHandle && udimTexture.IsValid()))
  , _layoutGLTextureHandle(
      _GenGlTextureHandle(
#if defined(PXR_OPENGL_SUPPORT_ENABLED)
          udimTexture.GetLayoutGLTextureName(),
#else
          0,
#endif
          createBindlessHandle && udimTexture.IsValid()))
{
}

HdStUdimSamplerObject::~HdStUdimSamplerObject()
{
    // See above comment about destroying bindless texture handles
#if defined(PXR_OPENGL_SUPPORT_ENABLED)
    if (_glTexelsSamplerName) {
        glDeleteSamplers(1, &_glTexelsSamplerName);
    }
#endif
}
   
PXR_NAMESPACE_CLOSE_SCOPE
