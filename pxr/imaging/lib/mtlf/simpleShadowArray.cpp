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


MtlfSimpleShadowArray::MtlfSimpleShadowArray(GfVec2i const & size,
                                           size_t numLayers) :
    GarchSimpleShadowArray(size, numLayers)
{
    _AllocSamplers();
}

MtlfSimpleShadowArray::~MtlfSimpleShadowArray()
{
    _FreeSamplers();
    _FreeTextureArray();
}

void
MtlfSimpleShadowArray::SetSize(GfVec2i const & size)
{
    if (_size != size) {
        _FreeTextureArray();
    }
    GarchSimpleShadowArray::SetSize(size);
}

void
MtlfSimpleShadowArray::SetNumLayers(size_t numLayers)
{
    if (_numLayers != numLayers) {
        _FreeTextureArray();
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
MtlfSimpleShadowArray::DisableCaptureEnvironment()
{
    // restore GL states to default
    /*glDisable(GL_PROGRAM_POINT_SIZE);
    glDisable(GL_POLYGON_OFFSET_FILL);*/
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
MtlfSimpleShadowArray::_AllocSamplers()
{
    MtlfMetalContextSharedPtr mtlContext = MtlfMetalContext::GetMetalContext()->GetMetalContext();
    MTLSamplerDescriptor* samplerDescriptor = [MTLSamplerDescriptor new];
    samplerDescriptor.tAddressMode = MTLSamplerAddressModeClampToBorderColor;
    samplerDescriptor.sAddressMode = MTLSamplerAddressModeClampToBorderColor;
    samplerDescriptor.minFilter = MTLSamplerMinMagFilterLinear;
    samplerDescriptor.magFilter = MTLSamplerMinMagFilterLinear;
    samplerDescriptor.borderColor = MTLSamplerBorderColorOpaqueWhite;
    _shadowDepthSampler = [mtlContext->device newSamplerStateWithDescriptor:samplerDescriptor];
    
    //METAL TODO: Check whether the sampler below is really going to provide the same functionality as the GL sample in the comments.
    samplerDescriptor.tAddressMode = MTLSamplerAddressModeClampToBorderColor;
    samplerDescriptor.sAddressMode = MTLSamplerAddressModeClampToBorderColor;
    samplerDescriptor.minFilter = MTLSamplerMinMagFilterLinear;
    samplerDescriptor.magFilter = MTLSamplerMinMagFilterLinear;
    samplerDescriptor.borderColor = MTLSamplerBorderColorOpaqueWhite;
    samplerDescriptor.compareFunction = MTLCompareFunctionLessEqual;
    _shadowCompareSampler = [mtlContext->device newSamplerStateWithDescriptor:samplerDescriptor];
}

void
MtlfSimpleShadowArray::_FreeSamplers()
{
    if (_shadowDepthSampler.IsSet()) {
        [_shadowDepthSampler release];
        _shadowDepthSampler = nil;
    }
    if (_shadowCompareSampler.IsSet()) {
        [_shadowCompareSampler release];
        _shadowCompareSampler = nil;
    }
}

void
MtlfSimpleShadowArray::_AllocTextureArray()
{
    TF_FATAL_CODING_ERROR("Not Implemented");
    
    /*
    glGenTextures(1, &_texture);
    glBindTexture(GL_TEXTURE_2D_ARRAY, _texture);

    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT32F,
                 _size[0], _size[1], _numLayers, 0, 
                 GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

    GLfloat border[] = {1, 1, 1, 1};

    glGenSamplers(1, &_shadowDepthSampler);
    glSamplerParameteri(_shadowDepthSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glSamplerParameteri(_shadowDepthSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glSamplerParameteri(_shadowDepthSampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glSamplerParameteri(_shadowDepthSampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glSamplerParameterfv(_shadowDepthSampler, GL_TEXTURE_BORDER_COLOR, border);

    glGenSamplers(1, &_shadowCompareSampler);
    glSamplerParameteri(_shadowCompareSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glSamplerParameteri(_shadowCompareSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glSamplerParameteri(_shadowCompareSampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glSamplerParameteri(_shadowCompareSampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glSamplerParameterfv(_shadowCompareSampler, GL_TEXTURE_BORDER_COLOR, border);
    glSamplerParameteri(_shadowCompareSampler, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE );
    glSamplerParameteri(_shadowCompareSampler, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL );

    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    glGenFramebuffers(1, &_framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, _framebuffer);

    glFramebufferTextureLayer(GL_FRAMEBUFFER,
                              GL_DEPTH_ATTACHMENT, _texture, 0, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
*/
}

void
MtlfSimpleShadowArray::_FreeTextureArray()
{
    if (_texture.IsSet()) {
        [_texture release];
        _texture = nil;
    }
    if (_framebuffer.IsSet()) {
        [_framebuffer release];
        _framebuffer = nil;
    }
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

