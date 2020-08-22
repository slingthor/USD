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
#include "pxr/base/tf/envSetting.h"
#include "pxr/base/tf/stringUtils.h"

#include <string>
#include <vector>


PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_ENV_SETTING(GLF_ENABLE_BINDLESS_SHADOW_TEXTURES, true,
                      "Enable use of bindless shadow maps");

GlfSimpleShadowArray::GlfSimpleShadowArray() :
    GarchSimpleShadowArray(),
    _unbindRestoreDrawFramebuffer(0),
    _unbindRestoreReadFramebuffer(0),
    _unbindRestoreViewport{0,0,0,0}
{
}

GlfSimpleShadowArray::~GlfSimpleShadowArray()
{
    _FreeResources();
}

void
GlfSimpleShadowArray::SetSize(GfVec2i const & size)
{
    if (_size != size) {
        _FreeBindfulTextures();
    }
    GarchSimpleShadowArray::SetSize(size);
}

void
GlfSimpleShadowArray::SetNumLayers(size_t numLayers)
{
    if (_numLayers != numLayers) {
        _FreeBindfulTextures();
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

void
GlfSimpleShadowArray::BeginCapture(size_t index, bool clear)
{
    _BindFramebuffer(index);

    if (clear) {
        glClear(GL_DEPTH_BUFFER_BIT);
    }

    // save the current viewport
    glGetIntegerv(GL_VIEWPORT, _unbindRestoreViewport);

    GfVec2i resolution = GetShadowMapSize(index);
    glViewport(0, 0, resolution[0], resolution[1]);

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

    if (TfDebug::IsEnabled(GLF_DEBUG_DUMP_SHADOW_TEXTURES)) {
        GarchImage::StorageSpec storage;
        GfVec2i resolution = GetShadowMapSize(index);
        storage.width = resolution[0];
        storage.height = resolution[1];
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
            TfDebug::Helper().Msg(
                "Wrote shadow texture: %s\n", outputImageFile.c_str());
        } else {
            TfDebug::Helper().Msg(
                "Failed to write shadow texture: %s\n", outputImageFile.c_str()
            );
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
GlfSimpleShadowArray::_AllocResources()
{
    GLfloat border[] = {1, 1, 1, 1};

	if (!_shadowDepthSampler.IsSet()) {
        GLuint shadowDepthSampler;
		glGenSamplers(1, &shadowDepthSampler);
    	glSamplerParameteri(shadowDepthSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    	glSamplerParameteri(shadowDepthSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    	glSamplerParameteri(shadowDepthSampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    	glSamplerParameteri(shadowDepthSampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    	glSamplerParameterfv(shadowDepthSampler, GL_TEXTURE_BORDER_COLOR, border);
        _shadowDepthSampler = GarchSamplerGPUHandle(shadowDepthSampler);

	}

	if (!_shadowCompareSampler.IsSet()) {
        GLuint shadowCompareSampler;
    	glGenSamplers(1, &shadowCompareSampler);
    	glSamplerParameteri(shadowCompareSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    	glSamplerParameteri(shadowCompareSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    	glSamplerParameteri(shadowCompareSampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    	glSamplerParameteri(shadowCompareSampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    	glSamplerParameterfv(shadowCompareSampler, GL_TEXTURE_BORDER_COLOR, border);
    	glSamplerParameteri(shadowCompareSampler, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE );
    	glSamplerParameteri(shadowCompareSampler, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL );
        _shadowCompareSampler = GarchSamplerGPUHandle(shadowCompareSampler);
	}
    
    // Shadow maps
    if (GetBindlessShadowMapsEnabled()) {
        _AllocBindlessTextures();
    } else {
       _AllocBindfulTextures();
    }

    // Framebuffer
    if (!_framebuffer.IsSet()) {
        GLuint framebuffer;
        glGenFramebuffers(1, &framebuffer);
        _framebuffer = GarchTextureGPUHandle(framebuffer);
    }
}

void
GlfSimpleShadowArray::_AllocBindfulTextures()
{
    GLuint bindfulTexture;
    glGenTextures(1, &bindfulTexture);
    glBindTexture(GL_TEXTURE_2D_ARRAY, _bindfulTexture);

    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT32F,
                _size[0], _size[1], _numLayers, 0,
                GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    
    _bindfulTexture = GarchTextureGPUHandle(bindfulTexture);

    TF_DEBUG(GLF_DEBUG_SHADOW_TEXTURES).Msg(
        "Created bindful shadow map texture array with %lu %dx%d textures\n"
        , _numLayers, _size[0], _size[1]);
}

void
GlfSimpleShadowArray::_AllocBindlessTextures()
{
    if (!TF_VERIFY(_shadowCompareSampler.IsSet()) ||
        !TF_VERIFY(_bindlessTextures.empty()) ||
        !TF_VERIFY(_bindlessTextureHandles.empty())) {
        TF_CODING_ERROR("Unexpected entry state in %s\n",
                        TF_FUNC_NAME().c_str());
        return;
    }

    // Commenting out the line below results in the residency check in
    // _FreeBindlessTextures failing.
    GlfSharedGLContextScopeHolder sharedContextScopeHolder;

    // XXX: Currently, we allocate/reallocate ALL shadow maps each time.
    for (GfVec2i const& size : _resolutions) {
        GLuint id;
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F,
            size[0], size[1], 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        _bindlessTextures.push_back(id);

        GLuint64 gpuHandle =
            glGetTextureSamplerHandleARB(id, _shadowCompareSampler);
        
        _bindlessTextureHandles.push_back(gpuHandle);

        if (TF_VERIFY(!glIsTextureHandleResidentARB(gpuHandle))) {
            glMakeTextureHandleResidentARB(gpuHandle);
        } else {
            GLF_POST_PENDING_GL_ERRORS();
        }

        TF_DEBUG(GLF_DEBUG_SHADOW_TEXTURES).Msg(
            "Created bindless shadow map texture of size %dx%d "
            "(id %#x, handle %#llx)\n" , size[0], size[1], id, gpuHandle);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}

void
GlfSimpleShadowArray::_FreeResources()
{
    GlfSharedGLContextScopeHolder sharedContextScopeHolder;

    if (GetBindlessShadowMapsEnabled()) {
        _FreeBindlessTextures();
    } else {
        _FreeBindfulTextures();
    }

    if (_framebuffer.IsSet()) {
        GLuint h = _framebuffer;
        glDeleteFramebuffers(1, &h);
        _framebuffer.Clear();
    }
    if (_shadowDepthSampler.IsSet()) {
        GLuint h = _shadowDepthSampler;
        glDeleteSamplers(1, &h);
        _shadowDepthSampler.Clear();
    }
    if (_shadowCompareSampler.IsSet()) {
        GLuint h = _shadowCompareSampler;
        glDeleteSamplers(1, &h);
        _shadowCompareSampler.Clear();
    }
}

void
GlfSimpleShadowArray::_FreeBindfulTextures()
{
    GlfSharedGLContextScopeHolder sharedContextScopeHolder;

    if (_bindfulTexture.IsSet()) {
        GLuint h = _bindfulTexture;
        glDeleteTextures(1, &h);
        _bindfulTexture.Clear();
    }

    // GLF_POST_PENDING_GL_ERRORS();
}

void
GlfSimpleShadowArray::_FreeBindlessTextures()
{
    GlfSharedGLContextScopeHolder sharedContextScopeHolder;
    // XXX: Ideally, we don't deallocate all textures, and only those that have
    // resolution modified.

    if (!_bindlessTextureHandles.empty()) {
        for (uint64_t handle : _bindlessTextureHandles) {
            // Handles are made resident on creation.
            if (TF_VERIFY(glIsTextureHandleResidentARB(handle))) {
                glMakeTextureHandleNonResidentARB(handle);
            }
        }
        _bindlessTextureHandles.clear();
    }

    for (GLuint const& id : _bindlessTextures) {
        if (id) {
            glDeleteTextures(1, &id);
        }
    }
    _bindlessTextures.clear();
    
    // GLF_POST_PENDING_GL_ERRORS();
}

void
GlfSimpleShadowArray::_BindFramebuffer(size_t index)
{
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,
                  (GLint*)&_unbindRestoreDrawFramebuffer);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,
                  (GLint*)&_unbindRestoreReadFramebuffer);

    if (!_framebuffer.IsSet() || !_ShadowMapExists()) {
        _AllocResources();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, _framebuffer);
    if (GetBindlessShadowMapsEnabled()) {
        glFramebufferTexture(GL_FRAMEBUFFER,
            GL_DEPTH_ATTACHMENT, _bindlessTextures[index], 0);
    } else {
        glFramebufferTextureLayer(GL_FRAMEBUFFER,
            GL_DEPTH_ATTACHMENT, _bindfulTexture, 0, index);
    }

    GLF_POST_PENDING_GL_ERRORS();
}

void
GlfSimpleShadowArray::_UnbindFramebuffer()
{
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _unbindRestoreDrawFramebuffer);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, _unbindRestoreReadFramebuffer);

    GLF_POST_PENDING_GL_ERRORS();
}


PXR_NAMESPACE_CLOSE_SCOPE

