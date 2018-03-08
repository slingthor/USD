//
// Copyright 2017 Pixar
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

#include "pxr/imaging/hdSt/GL/drawTargetTextureResourceGL.h"
#include "pxr/imaging/hdSt/GL/glConversions.h"

PXR_NAMESPACE_OPEN_SCOPE



HdSt_DrawTargetTextureResourceGL::HdSt_DrawTargetTextureResourceGL()
 : HdSt_DrawTargetTextureResource()
{
    // GL initialization guard for headless unit testing
    GLuint s;
    if (glGenSamplers) {
        glGenSamplers(1, &s);
        _sampler = (GarchSamplerGPUHandle)(uint64_t)s;
    }
}

HdSt_DrawTargetTextureResourceGL::~HdSt_DrawTargetTextureResourceGL()
{
    // GL initialization guard for headless unit test
    if (glDeleteSamplers) {
        GLuint s = (uint32_t)(uint64_t)_sampler;
        glDeleteSamplers(1, &s);
    }
}

void
HdSt_DrawTargetTextureResourceGL::SetSampler(HdWrap wrapS,
                                             HdWrap wrapT,
                                             HdMinFilter minFilter,
                                             HdMagFilter magFilter)
{
    static const float borderColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    // Convert params to Gl
    GLenum glWrapS = HdStGLConversions::GetWrap(wrapS);
    GLenum glWrapT = HdStGLConversions::GetWrap(wrapT);
    GLenum glMinFilter = HdStGLConversions::GetMinFilter(minFilter);
    GLenum glMagFilter = HdStGLConversions::GetMagFilter(magFilter);

    GLuint s = (uint32_t)(uint64_t)_sampler;
    glSamplerParameteri(s, GL_TEXTURE_WRAP_S, glWrapS);
    glSamplerParameteri(s, GL_TEXTURE_WRAP_T, glWrapT);
    glSamplerParameteri(s, GL_TEXTURE_MIN_FILTER, glMinFilter);
    glSamplerParameteri(s, GL_TEXTURE_MAG_FILTER, glMagFilter);
    glSamplerParameterf(s, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.0);
    glSamplerParameterfv(s, GL_TEXTURE_BORDER_COLOR, borderColor);
}

GarchTextureGPUHandle
HdSt_DrawTargetTextureResourceGL::GetTexelsTextureHandle()
{
    GLuint textureId = (GLuint)(uint64_t)GetTexelsTextureId();

    if (textureId == 0) {
        return 0;
    }

    if (!TF_VERIFY(glGetTextureHandleARB) ||
        !TF_VERIFY(glGetTextureSamplerHandleARB)) {
        return 0;
    }

    GLuint s = (GLuint)(uint64_t)GetTexelsSamplerId();

    return (GarchTextureGPUHandle)(uint64_t)glGetTextureSamplerHandleARB(textureId, s);
}

PXR_NAMESPACE_CLOSE_SCOPE

