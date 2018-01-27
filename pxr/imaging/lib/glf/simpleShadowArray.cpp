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
/// \file simpleShadowArray.cpp

#include "pxr/imaging/glf/glew.h"

#include "pxr/imaging/glf/simpleShadowArray.h"
#include "pxr/imaging/glf/diagnostic.h"
#include "pxr/imaging/glf/glContext.h"

#include "pxr/base/gf/vec2i.h"
#include "pxr/base/gf/vec4d.h"

PXR_NAMESPACE_OPEN_SCOPE


GlfSimpleShadowArray::GlfSimpleShadowArray(GfVec2i const & size,
                                           size_t numLayers) :
    GarchSimpleShadowArray(size, numLayers),
    _unbindRestoreDrawFramebuffer(0),
    _unbindRestoreReadFramebuffer(0),
    _unbindRestoreViewport{0,0,0,0}
{
}

GlfSimpleShadowArray::~GlfSimpleShadowArray()
{
    _FreeTextureArray();
}

void
GlfSimpleShadowArray::SetSize(GfVec2i const & size)
{
    if (_size != size) {
        _FreeTextureArray();
    }
    GarchSimpleShadowArray::SetSize(size);
}

void
GlfSimpleShadowArray::SetNumLayers(size_t numLayers)
{
    if (_numLayers != numLayers) {
        _FreeTextureArray();
    }
    GarchSimpleShadowArray::SetNumLayers(numLayers);
}

void
GlfSimpleShadowArray::BeginCapture(size_t index, bool clear)
{
    _BindFramebuffer(index);

    if (clear) {
        glClear(GL_DEPTH_BUFFER_BIT);
    }

    // save the current viewport
    glGetIntegerv(GL_VIEWPORT, _unbindRestoreViewport);

    glViewport(0, 0, GetSize()[0], GetSize()[1]);

    // depth 1.0 means infinity (no occluders).
    // This value is also used as a border color
    glDepthRange(0, 0.99999);
    glEnable(GL_DEPTH_CLAMP);

    GLF_POST_PENDING_GL_ERRORS();
}

void
GlfSimpleShadowArray::EndCapture(size_t)
{
    // reset to GL default, except viewport
    glDepthRange(0, 1.0);
    glDisable(GL_DEPTH_CLAMP);

    _UnbindFramebuffer();

    // restore viewport
    glViewport(_unbindRestoreViewport[0],
               _unbindRestoreViewport[1],
               _unbindRestoreViewport[2],
               _unbindRestoreViewport[3]);

    GLF_POST_PENDING_GL_ERRORS();
}

void
GlfSimpleShadowArray::_AllocTextureArray()
{
    GLuint shadowDepthSampler;
    GLuint shadowCompareSampler;
    GLuint framebuffer;
    GLuint texture;

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texture);

    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT32F,
                 _size[0], _size[1], _numLayers, 0, 
                 GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

    GLfloat border[] = {1, 1, 1, 1};
    glGenSamplers(1, &shadowDepthSampler);
    glSamplerParameteri(shadowDepthSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glSamplerParameteri(shadowDepthSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glSamplerParameteri(shadowDepthSampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glSamplerParameteri(shadowDepthSampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glSamplerParameterfv(shadowDepthSampler, GL_TEXTURE_BORDER_COLOR, border);

    glGenSamplers(1, &shadowCompareSampler);
    glSamplerParameteri(shadowCompareSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glSamplerParameteri(shadowCompareSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glSamplerParameteri(shadowCompareSampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glSamplerParameteri(shadowCompareSampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glSamplerParameterfv(shadowCompareSampler, GL_TEXTURE_BORDER_COLOR, border);
    glSamplerParameteri(shadowCompareSampler, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE );
    glSamplerParameteri(shadowCompareSampler, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL );

    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    glFramebufferTextureLayer(GL_FRAMEBUFFER,
                              GL_DEPTH_ATTACHMENT, texture, 0, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    _shadowDepthSampler = (GarchSamplerGPUHandle)(uint64_t)shadowDepthSampler;
    _shadowCompareSampler = (GarchSamplerGPUHandle)(uint64_t)shadowCompareSampler;
    _framebuffer = (GarchSamplerGPUHandle)(uint64_t)framebuffer;
    _texture = (GarchSamplerGPUHandle)(uint64_t)texture;
}

void
GlfSimpleShadowArray::_FreeTextureArray()
{
    GlfSharedGLContextScopeHolder sharedContextScopeHolder;
    
    if (_texture) {
        GLuint texture = (GLuint)(uint64_t)_texture;
        glDeleteTextures(1, &texture);
        _texture = 0;
    }
    if (_framebuffer) {
        GLuint framebuffer = (GLuint)(uint64_t)_framebuffer;
        glDeleteFramebuffers(1, &framebuffer);
        _framebuffer = 0;
    }
    if (_shadowDepthSampler) {
        GLuint shadowDepthSampler = (GLuint)(uint64_t)_shadowDepthSampler;
        glDeleteSamplers(1, &shadowDepthSampler);
        _shadowDepthSampler = 0;
    }
    if (_shadowCompareSampler) {
        GLuint shadowCompareSampler = (GLuint)(uint64_t)_shadowCompareSampler;
        glDeleteSamplers(1, &shadowCompareSampler);
        _shadowCompareSampler = 0;
    }
}

void
GlfSimpleShadowArray::_BindFramebuffer(size_t index)
{
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,
                  (GLint*)&_unbindRestoreDrawFramebuffer);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,
                  (GLint*)&_unbindRestoreReadFramebuffer);

    if (!_framebuffer || !_texture) {
        _AllocTextureArray();
    }

    GLuint framebuffer = (GLuint)(uint64_t)_framebuffer;
    GLuint texture = (GLuint)(uint64_t)_texture;
    
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTextureLayer(GL_FRAMEBUFFER,
                              GL_DEPTH_ATTACHMENT, texture, 0, index);
}

void
GlfSimpleShadowArray::_UnbindFramebuffer()
{
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _unbindRestoreDrawFramebuffer);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, _unbindRestoreReadFramebuffer);
}

PXR_NAMESPACE_CLOSE_SCOPE

