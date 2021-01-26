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
#include "pxr/imaging/garch/glApi.h"
#include "pxr/imaging/glf/diagnostic.h"

#include "pxr/imaging/hdSt/drawItem.h"
#include "pxr/imaging/hdSt/fallbackLightingShader.h"
#include "pxr/imaging/hdSt/renderPassShader.h"
#include "pxr/imaging/hdSt/resourceRegistry.h"
#include "pxr/imaging/hdSt/shaderCode.h"
#include "pxr/imaging/hdSt/GL/renderPassStateGL.h"
#include "pxr/imaging/hdSt/glConversions.h"

#include "pxr/imaging/hd/bufferArrayRange.h"
#include "pxr/imaging/hd/changeTracker.h"
#include "pxr/imaging/hd/tokens.h"
#include "pxr/imaging/hd/vtBufferSource.h"

#include "pxr/base/gf/frustum.h"
#include "pxr/base/tf/staticTokens.h"
#include "pxr/base/tf/stringUtils.h"

#include <boost/functional/hash.hpp>

PXR_NAMESPACE_OPEN_SCOPE


TF_DEFINE_PRIVATE_TOKENS(
    _tokens,
    (renderPassState)
);

HdStRenderPassStateGL::HdStRenderPassStateGL()
    : HdStRenderPassState()
{
    /*NOTHING*/
}

HdStRenderPassStateGL::HdStRenderPassStateGL(
    HdStRenderPassShaderSharedPtr const &renderPassShader)
    : HdStRenderPassState(renderPassShader)
{
    /*NOTHING*/
}

HdStRenderPassStateGL::~HdStRenderPassStateGL()
{
    /*NOTHING*/
}

// Note: The geometric shader may override the state set below if necessary,
// including disabling h/w culling altogether.
// Disabling h/w culling is required to handle instancing wherein
// instanceScale/instanceTransform can flip the xform handedness.
namespace {

void
_SetGLCullState(HdCullStyle cullstyle)
{
    switch (cullstyle) {
        case HdCullStyleFront:
        case HdCullStyleFrontUnlessDoubleSided:
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT);
            break;
        case HdCullStyleBack:
        case HdCullStyleBackUnlessDoubleSided:
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            break;
        case HdCullStyleNothing:
        case HdCullStyleDontCare:
        default:
            // disable culling
            glDisable(GL_CULL_FACE);
            break;
    }
}

void
_SetColorMask(int drawBufferIndex, HdRenderPassState::ColorMask const& mask)
{
    bool colorMask[4] = {true, true, true, true};
    switch (mask)
    {
        case HdStRenderPassState::ColorMaskNone:
            colorMask[0] = colorMask[1] = colorMask[2] = colorMask[3] = false;
            break;
        case HdStRenderPassState::ColorMaskRGB:
            colorMask[3] = false;
            break;
        default:
            ; // no-op
    }

    if (drawBufferIndex == -1) {
        glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
    } else {
        glColorMaski((uint32_t) drawBufferIndex,
                     colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
    }
}

} // anonymous namespace

void
HdStRenderPassStateGL::Bind()
{
    HdStRenderPassState::Bind();

    if (!glBlendColor) {
        return;
    }

    // XXX: viewport should be set.
    // glViewport((GLint)_viewport[0], (GLint)_viewport[1],
    //            (GLsizei)_viewport[2], (GLsizei)_viewport[3]);

    // when adding another GL state change here, please document
    // which states to be altered at the comment in the header file

    // Apply polygon offset to whole pass.
    if (!_depthBiasUseDefault) {
        if (_depthBiasEnabled) {
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(_depthBiasSlopeFactor, _depthBiasConstantFactor);
        } else {
            glDisable(GL_POLYGON_OFFSET_FILL);
        }
    }

    glDepthFunc(HdStGLConversions::GetGlDepthFunc(_depthFunc));
    glDepthMask(_depthMaskEnabled);
    
    // Stencil
    if (_stencilEnabled) {
        glEnable(GL_STENCIL_TEST);
        glStencilFunc(HdStGLConversions::GetGlStencilFunc(_stencilFunc),
                _stencilRef, _stencilMask);
        glStencilOp(HdStGLConversions::GetGlStencilOp(_stencilFailOp),
                HdStGLConversions::GetGlStencilOp(_stencilZFailOp),
                HdStGLConversions::GetGlStencilOp(_stencilZPassOp));
    } else {
        glDisable(GL_STENCIL_TEST);
    }
    
    // Line width
    if (_lineWidth > 0) {
        glLineWidth(_lineWidth);
    }
    
    // Blending
    if (_blendEnabled) {
        glEnable(GL_BLEND);
        glBlendEquationSeparate(
                                HdStGLConversions::GetGlBlendOp(_blendColorOp),
                                HdStGLConversions::GetGlBlendOp(_blendAlphaOp));
        glBlendFuncSeparate(
                            HdStGLConversions::GetGlBlendFactor(_blendColorSrcFactor),
                            HdStGLConversions::GetGlBlendFactor(_blendColorDstFactor),
                            HdStGLConversions::GetGlBlendFactor(_blendAlphaSrcFactor),
                            HdStGLConversions::GetGlBlendFactor(_blendAlphaDstFactor));
        glBlendColor(_blendConstantColor[0],
                     _blendConstantColor[1],
                     _blendConstantColor[2],
                     _blendConstantColor[3]);
    } else {
        glDisable(GL_BLEND);
    }

    if (_alphaToCoverageEnabled) {
        glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
        glEnable(GL_SAMPLE_ALPHA_TO_ONE);
    } else {
        glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    }
    glEnable(GL_PROGRAM_POINT_SIZE);
    GLint glMaxClipPlanes;
    glGetIntegerv(GL_MAX_CLIP_PLANES, &glMaxClipPlanes);
    for (size_t i = 0; i < GetClipPlanes().size(); ++i) {
        if (i >= (size_t)glMaxClipPlanes) {
            break;
        }
        glEnable(GL_CLIP_DISTANCE0 + i);
    }

    if (_colorMaskUseDefault) {
        // Enable color writes for all components for all attachments.
        _SetColorMask(-1, ColorMaskRGBA);
    } else {
        if (_colorMasks.size() == 1) {
            // Use the same color mask for all attachments.
            _SetColorMask(-1, _colorMasks[0]);
        } else {
            for (size_t i = 0; i < _colorMasks.size(); i++) {
                _SetColorMask(i, _colorMasks[i]);
            }
        }
    }
}

void
HdStRenderPassStateGL::Unbind()
{
    HdStRenderPassState::Unbind();

    GLF_GROUP_FUNCTION();
    // restore back to the GL defaults
    
    if (!glBlendColor) {
        return;
    }
    
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    glDisable(GL_SAMPLE_ALPHA_TO_ONE);
    glDisable(GL_PROGRAM_POINT_SIZE);
    glDisable(GL_STENCIL_TEST);
    glDepthFunc(GL_LESS);
    glPolygonOffset(0, 0);
    glLineWidth(1.0f);
    
    glDisable(GL_BLEND);
    glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
    glBlendFuncSeparate(GL_ONE, GL_ZERO, GL_ONE, GL_ZERO);
    glBlendColor(0.0f, 0.0f, 0.0f, 0.0f);

    for (size_t i = 0; i < GetClipPlanes().size(); ++i) {
        glDisable(GL_CLIP_DISTANCE0 + i);
    }
    
    glColorMask(true, true, true, true);
    glDepthMask(true);
}

PXR_NAMESPACE_CLOSE_SCOPE
