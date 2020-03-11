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

#include "pxr/imaging/mtlf/mtlDevice.h"

#include "pxr/imaging/mtlf/simpleShadowArray.h"
#include "pxr/imaging/mtlf/diagnostic.h"

#include "pxr/base/gf/vec2i.h"
#include "pxr/base/gf/vec4d.h"

PXR_NAMESPACE_OPEN_SCOPE


MtlfSimpleShadowArray::MtlfSimpleShadowArray() :
    GarchSimpleShadowArray()
{
}

MtlfSimpleShadowArray::~MtlfSimpleShadowArray()
{
    _FreeResources();
}

void
MtlfSimpleShadowArray::SetSize(GfVec2i const & size)
{
    if (_size != size) {
        _FreeBindfulTextures();
    }
    GarchSimpleShadowArray::SetSize(size);
}

void
MtlfSimpleShadowArray::SetNumLayers(size_t numLayers)
{
    if (_numLayers != numLayers) {
        _FreeBindfulTextures();
    }
    GarchSimpleShadowArray::SetNumLayers(numLayers);
}

void
MtlfSimpleShadowArray::InitCaptureEnvironment(bool   depthBiasEnable,
                                              float  depthBiasConstantFactor,
                                              float  depthBiasSlopeFactor,
                                              GLenum depthFunc)
{
    /*if (depthBiasEnable) {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(depthBiasSlopeFactor, depthBiasConstantFactor);
    } else {
        glDisable(GL_POLYGON_OFFSET_FILL);
    }
    
    // XXX: Move conversion to sync time once Task header becomes private.
    glDepthFunc(depthFunc);
    glEnable(GL_PROGRAM_POINT_SIZE);*/
}


void
MtlfSimpleShadowArray::BeginCapture(size_t index, bool clear)
{
    _BindFramebuffer(index);
    
    TF_FATAL_CODING_ERROR("Not Implemented");
    /*
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
     */
}

void
MtlfSimpleShadowArray::EndCapture(size_t)
{
    TF_FATAL_CODING_ERROR("Not Implemented");
    /*
    // reset to GL default, except viewport
    glDepthRange(0, 1.0);
    glDisable(GL_DEPTH_CLAMP);

    _UnbindFramebuffer();

    // restore viewport
    glViewport(_unbindRestoreViewport[0],
               _unbindRestoreViewport[1],
               _unbindRestoreViewport[2],
               _unbindRestoreViewport[3]);
     */
}

void
MtlfSimpleShadowArray::_AllocResources()
{
    MtlfMetalContextSharedPtr mtlContext = MtlfMetalContext::GetMetalContext();
    
    if (!_shadowDepthSampler.IsSet()) {
        MTLSamplerDescriptor* samplerDescriptor = [[MTLSamplerDescriptor alloc] init];
#if defined(ARCH_OS_IOS)
        samplerDescriptor.tAddressMode = MTLSamplerAddressModeClampToZero;
        samplerDescriptor.sAddressMode = MTLSamplerAddressModeClampToZero;
#else
        samplerDescriptor.tAddressMode = MTLSamplerAddressModeClampToBorderColor;
        samplerDescriptor.sAddressMode = MTLSamplerAddressModeClampToBorderColor;
        samplerDescriptor.borderColor = MTLSamplerBorderColorOpaqueWhite;
#endif
        samplerDescriptor.minFilter = MTLSamplerMinMagFilterLinear;
        samplerDescriptor.magFilter = MTLSamplerMinMagFilterLinear;
        _shadowDepthSampler = [mtlContext->currentDevice newSamplerStateWithDescriptor:samplerDescriptor];
        [samplerDescriptor release];
    }

    if (!_shadowCompareSampler.IsSet()) {
        MTLSamplerDescriptor* samplerDescriptor = [[MTLSamplerDescriptor alloc] init];
        //METAL TODO: Check whether the sampler below is really going to provide the same functionality as the GL sample in the comments.
#if defined(ARCH_OS_IOS)
        samplerDescriptor.tAddressMode = MTLSamplerAddressModeClampToZero;
        samplerDescriptor.sAddressMode = MTLSamplerAddressModeClampToZero;
        if ([mtlContext->currentDevice supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily3_v1]) {
            samplerDescriptor.compareFunction = MTLCompareFunctionLessEqual;
        }
#else
        samplerDescriptor.tAddressMode = MTLSamplerAddressModeClampToBorderColor;
        samplerDescriptor.sAddressMode = MTLSamplerAddressModeClampToBorderColor;
        samplerDescriptor.borderColor = MTLSamplerBorderColorOpaqueWhite;
        samplerDescriptor.compareFunction = MTLCompareFunctionLessEqual;
#endif
        samplerDescriptor.minFilter = MTLSamplerMinMagFilterLinear;
        samplerDescriptor.magFilter = MTLSamplerMinMagFilterLinear;
        _shadowCompareSampler = [mtlContext->currentDevice newSamplerStateWithDescriptor:samplerDescriptor];
        [samplerDescriptor release];
    }
    
    // Shadow maps
    if (GetBindlessShadowMapsEnabled()) {
        _AllocBindlessTextures();
    } else {
       _AllocBindfulTextures();
    }
}

void
MtlfSimpleShadowArray::_AllocBindfulTextures()
{
    TF_FATAL_CODING_ERROR("Not Implemented");

    /*
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
     */
}

void
MtlfSimpleShadowArray::_AllocBindlessTextures()
{
    if (!TF_VERIFY(_shadowCompareSampler.IsSet()) ||
        !TF_VERIFY(_bindlessTextures.empty()) ||
        !TF_VERIFY(_bindlessTextureHandles.empty())) {
        TF_CODING_ERROR("Unexpected entry state in %s\n",
                        TF_FUNC_NAME().c_str());
        return;
    }

    TF_FATAL_CODING_ERROR("Not Implemented");
/*
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
 */
}

void
MtlfSimpleShadowArray::_FreeResources()
{
    if (GetBindlessShadowMapsEnabled()) {
        _FreeBindlessTextures();
    } else {
        _FreeBindfulTextures();
    }

    if (_shadowDepthSampler.IsSet()) {
        [_shadowDepthSampler release];
        _shadowDepthSampler.Clear();
    }
    if (_shadowCompareSampler.IsSet()) {
        [_shadowCompareSampler release];
        _shadowCompareSampler.Clear();
    }
}

void
MtlfSimpleShadowArray::_FreeBindfulTextures()
{
    if (_bindfulTexture.IsSet()) {
        [_bindfulTexture release];
        _bindfulTexture.Clear();
    }
}

void
MtlfSimpleShadowArray::_FreeBindlessTextures()
{
    // XXX: Ideally, we don't deallocate all textures, and only those that have
    // resolution modified.

    if (!_bindlessTextureHandles.empty()) {
        _bindlessTextureHandles.clear();
    }

    for (GarchTextureGPUHandle& id : _bindlessTextures) {
        if (id.IsSet()) {
            [id release];
            id.Clear();
        }
    }
    _bindlessTextures.clear();
}

void
MtlfSimpleShadowArray::_BindFramebuffer(size_t index)
{
    TF_FATAL_CODING_ERROR("Not Implemented");
    /*
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,
                  (GLint*)&_unbindRestoreDrawFramebuffer);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,
                  (GLint*)&_unbindRestoreReadFramebuffer);

    if (!_framebuffer || !_texture) {
        _AllocTextureArray();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, _framebuffer);
    glFramebufferTextureLayer(GL_FRAMEBUFFER,
                              GL_DEPTH_ATTACHMENT, _texture, 0, index);
     */
}

void
MtlfSimpleShadowArray::_UnbindFramebuffer()
{
    TF_FATAL_CODING_ERROR("Not Implemented");
    /*
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _unbindRestoreDrawFramebuffer);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, _unbindRestoreReadFramebuffer);
     */
}

PXR_NAMESPACE_CLOSE_SCOPE

