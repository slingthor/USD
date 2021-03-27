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

#include "pxr/imaging/mtlf/mtlDevice.h"

#include "pxr/imaging/hdSt/drawItem.h"
#include "pxr/imaging/hdSt/fallbackLightingShader.h"
#include "pxr/imaging/hdSt/renderPassShader.h"
#include "pxr/imaging/hdSt/resourceRegistry.h"
#include "pxr/imaging/hdSt/shaderCode.h"

#include "pxr/imaging/hdSt/Metal/renderPassStateMetal.h"
#include "pxr/imaging/hdSt/Metal/metalConversions.h"

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

HdStRenderPassStateMetal::HdStRenderPassStateMetal()
    : HdStRenderPassState()
{
    /*NOTHING*/
}

HdStRenderPassStateMetal::HdStRenderPassStateMetal(
    HdStRenderPassShaderSharedPtr const &renderPassShader)
    : HdStRenderPassState(renderPassShader)
{
    /*NOTHING*/
}

HdStRenderPassStateMetal::~HdStRenderPassStateMetal()
{
    /*NOTHING*/
}

// Note: The geometric shader may override the state set below if necessary,
// including disabling h/w culling altogether.
// Disabling h/w culling is required to handle instancing wherein
// instanceScale/instanceTransform can flip the xform handedness.
namespace {

void
_SetCullState(MtlfMetalContextSharedPtr context, HdCullStyle cullstyle)
{
    switch (cullstyle) {
        case HdCullStyleFront:
        case HdCullStyleFrontUnlessDoubleSided:
            context->SetCullMode(MTLCullModeFront);
            break;
        case HdCullStyleBack:
        case HdCullStyleBackUnlessDoubleSided:
            context->SetCullMode(MTLCullModeBack);
            break;
        case HdCullStyleNothing:
        case HdCullStyleDontCare:
        default:
            // disable culling
            context->SetCullMode(MTLCullModeNone);
            break;
    }
}

void
_SetColorMask(MtlfMetalContextSharedPtr context,
              int drawBufferIndex,
              HdRenderPassState::ColorMask const& mask)
{
    MTLColorWriteMask colorWriteMask;
    switch (mask)
    {
        case HdStRenderPassState::ColorMaskNone:
            colorWriteMask = MTLColorWriteMaskNone;
            break;
        case HdStRenderPassState::ColorMaskRGB:
            colorWriteMask = MTLColorWriteMaskRed | MTLColorWriteMaskGreen
                | MTLColorWriteMaskBlue;
            break;
        default:
            colorWriteMask = MTLColorWriteMaskAll;
            break;
    }

    if (drawBufferIndex == -1) {
        context->SetColorWriteMask(colorWriteMask);
    } else {
        context->SetColorWriteMask(drawBufferIndex, colorWriteMask);
    }
}

}

void
HdStRenderPassStateMetal::Bind()
{
    HdStRenderPassState::Bind();

    // XXX: viewport should be set.
    // glViewport((GLint)_viewport[0], (GLint)_viewport[1],
    //            (GLsizei)_viewport[2], (GLsizei)_viewport[3]);

    // when adding another GL state change here, please document
    // which states to be altered at the comment in the header file
    
    MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();

    _SetCullState(context, _cullStyle);
    
    // Blending
    if (_blendEnabled) {
        context->SetAlphaBlendingEnable(true);
        context->SetBlendOps(HdStMetalConversions::GetGlBlendOp(_blendColorOp),
                             HdStMetalConversions::GetGlBlendOp(_blendAlphaOp));
        context->SetBlendFactors(
            HdStMetalConversions::GetGlBlendFactor(_blendColorSrcFactor),
            HdStMetalConversions::GetGlBlendFactor(_blendColorDstFactor),
            HdStMetalConversions::GetGlBlendFactor(_blendAlphaSrcFactor),
            HdStMetalConversions::GetGlBlendFactor(_blendAlphaDstFactor));
        context->SetBlendColor(_blendConstantColor);
    } else {
        context->SetAlphaBlendingEnable(false);
    }
    
    if (_alphaToCoverageEnabled) {
        context->SetAlphaCoverageEnable(true, true);
    } else {
        context->SetAlphaCoverageEnable(false, false);
    }
    
    context->SetDepthComparisonFunction(HdStMetalConversions::GetGlDepthFunc(_depthFunc));
    context->SetDepthWriteEnable(_depthMaskEnabled);
    if (_colorMaskUseDefault) {
        // Enable color writes for all components for all attachments.
        _SetColorMask(context, -1, ColorMaskRGBA);
    } else {
        if (_colorMasks.size() == 1) {
            // Use the same color mask for all attachments.
            _SetColorMask(context, -1, _colorMasks[0]);
        } else {
            for (size_t i = 0; i < _colorMasks.size(); i++) {
                _SetColorMask(context, i, _colorMasks[i]);
            }
        }
    }
/*
    // Apply polygon offset to whole pass.
    if (!GetDepthBiasUseDefault()) {
        if (GetDepthBiasEnabled()) {
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(_depthBiasSlopeFactor, _depthBiasConstantFactor);
        } else {
            glDisable(GL_POLYGON_OFFSET_FILL);
        }
    }

    if (GetEnableDepthTest()) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(HdStGLConversions::GetGlDepthFunc(_depthFunc));
        glDepthMask(GetEnableDepthMask()); // depth writes are enabled only
                                           // when the test is enabled.
    } else {
        glDisable(GL_DEPTH_TEST);
    }

    // Stencil
    if (GetStencilEnabled()) {
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
    } else {
        glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    }
    glEnable(GL_PROGRAM_POINT_SIZE);

    if (!_colorMaskUseDefault) {
        switch(_colorMask) {
            case HdStRenderPassState::ColorMaskNone:
                glColorMask(false, false, false, false);
                break;
            case HdStRenderPassState::ColorMaskRGB:
                glColorMask(true, true, true, false);
                break;
            case HdStRenderPassState::ColorMaskRGBA:
                glColorMask(true, true, true, true);
                break;
        }
    }
    */
}

void
HdStRenderPassStateMetal::Unbind()
{
    HdStRenderPassState::Unbind();
    
    MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();
    
    context->SetAlphaCoverageEnable(false, false);

    context->SetAlphaBlendingEnable(false);
    context->SetBlendOps(MTLBlendOperationAdd, MTLBlendOperationAdd);
    context->SetBlendFactors(MTLBlendFactorOne, MTLBlendFactorZero,
                             MTLBlendFactorOne, MTLBlendFactorZero);

    context->SetDepthWriteEnable(true);
    context->SetDepthComparisonFunction(MTLCompareFunctionLess);
    context->SetColorWriteMask(MTLColorWriteMaskAll);
/*
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    glDisable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glPolygonOffset(0, 0);
    glLineWidth(1.0f);

    glDisable(GL_BLEND);
    glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
    glBlendFuncSeparate(GL_ONE, GL_ZERO, GL_ONE, GL_ZERO);
    glBlendColor(0.0f, 0.0f, 0.0f, 0.0f);
 
    glColorMask(true, true, true, true);
 */
}

PXR_NAMESPACE_CLOSE_SCOPE
