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
#include "pxr/imaging/glf/debugCodes.h"
#include "pxr/imaging/glf/diagnostic.h"
#include "pxr/imaging/glf/glContext.h"

#include "pxr/imaging/garch/image.h"

#include "pxr/base/arch/fileSystem.h"
#include "pxr/base/gf/vec2i.h"
#include "pxr/base/gf/vec4d.h"
#include "pxr/base/tf/debug.h"
#include "pxr/base/tf/stringUtils.h"

#include <string>
#include <vector>


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

void GlfSimpleShadowArray::InitCaptureEnvironment(bool   depthBiasEnable,
                                                  float  depthBiasConstantFactor,
                                                  float  depthBiasSlopeFactor,
                                                  GLenum depthFunc)
{
    if (depthBiasEnable) {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(depthBiasSlopeFactor, depthBiasConstantFactor);
    } else {
        glDisable(GL_POLYGON_OFFSET_FILL);
    }

    // XXX: Move conversion to sync time once Task header becomes private.
    glDepthFunc(depthFunc);
    glEnable(GL_PROGRAM_POINT_SIZE);
}

void GlfSimpleShadowArray::DisableCaptureEnvironment()
{
    // restore GL states to default
    glDisable(GL_PROGRAM_POINT_SIZE);
    glDisable(GL_POLYGON_OFFSET_FILL);
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
GlfSimpleShadowArray::EndCapture(size_t index)
{
    // reset to GL default, except viewport
    glDepthRange(0, 1.0);
    glDisable(GL_DEPTH_CLAMP);

    if (TfDebug::IsEnabled(GLF_DEBUG_SHADOW_TEXTURES)) {
        GarchImage::StorageSpec storage;
        storage.width = GetSize()[0];
        storage.height = GetSize()[1];
        storage.format = GL_DEPTH_COMPONENT;
        storage.type = GL_FLOAT;

        // In OpenGL, (0, 0) is the lower left corner.
        storage.flipped = true;

        const int numPixels = storage.width * storage.height;
        std::vector<GLfloat> pixelData(static_cast<size_t>(numPixels));
        storage.data = static_cast<void*>(pixelData.data());

        glReadPixels(0,
                     0,
                     storage.width,
                     storage.height,
                     storage.format,
                     storage.type,
                     storage.data);

        GLfloat minValue = std::numeric_limits<float>::max();
        GLfloat maxValue = -std::numeric_limits<float>::max();
        for (int i = 0; i < numPixels; ++i) {
            const GLfloat pixelValue = pixelData[i];
            if (pixelValue < minValue) {
                minValue = pixelValue;
            }
            if (pixelValue > maxValue) {
                maxValue = pixelValue;
            }
        }

        // Remap the pixel data so that the furthest depth sample is white and
        // the nearest depth sample is black.
        for (int i = 0; i < numPixels; ++i) {
            pixelData[i] = (pixelData[i] - minValue) / (maxValue - minValue);
        }

        const std::string outputImageFile = ArchNormPath(
            TfStringPrintf("%s/GlfSimpleShadowArray.index_%zu.tif",
                           ArchGetTmpDir(),
                           index));
        GarchImageSharedPtr image = GarchImage::OpenForWriting(outputImageFile);
        if (image->Write(storage)) {
            TF_DEBUG(GLF_DEBUG_SHADOW_TEXTURES).Msg(
                "Wrote shadow texture: %s\n", outputImageFile.c_str());
        } else {
            TF_DEBUG(GLF_DEBUG_SHADOW_TEXTURES).Msg(
                "Failed to write shadow texture: %s\n", outputImageFile.c_str());
        }
    }

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
    _framebuffer = framebuffer;
    _texture = texture;
}

void
GlfSimpleShadowArray::_FreeTextureArray()
{
    GlfSharedGLContextScopeHolder sharedContextScopeHolder;
    
    if (_texture.IsSet()) {
        GLuint texture = _texture;
        glDeleteTextures(1, &texture);
        _texture.Clear();
    }
    if (_framebuffer.IsSet()) {
        GLuint framebuffer = _framebuffer;
        glDeleteFramebuffers(1, &framebuffer);
        _framebuffer.Clear();
    }
    if (_shadowDepthSampler.IsSet()) {
        GLuint shadowDepthSampler = (GLuint)(uint64_t)_shadowDepthSampler;
        glDeleteSamplers(1, &shadowDepthSampler);
        _shadowDepthSampler.Clear();
    }
    if (_shadowCompareSampler.IsSet()) {
        GLuint shadowCompareSampler = (GLuint)(uint64_t)_shadowCompareSampler;
        glDeleteSamplers(1, &shadowCompareSampler);
        _shadowCompareSampler.Clear();
    }
}

void
GlfSimpleShadowArray::_BindFramebuffer(size_t index)
{
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,
                  (GLint*)&_unbindRestoreDrawFramebuffer);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,
                  (GLint*)&_unbindRestoreReadFramebuffer);

    if (!_framebuffer.IsSet() || !_texture.IsSet()) {
        _AllocTextureArray();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, _framebuffer);
    glFramebufferTextureLayer(GL_FRAMEBUFFER,
                              GL_DEPTH_ATTACHMENT, _texture, 0, index);
}

void
GlfSimpleShadowArray::_UnbindFramebuffer()
{
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _unbindRestoreDrawFramebuffer);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, _unbindRestoreReadFramebuffer);
}

PXR_NAMESPACE_CLOSE_SCOPE

