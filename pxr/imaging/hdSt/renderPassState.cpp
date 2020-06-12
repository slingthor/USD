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
#include "pxr/imaging/glf/diagnostic.h"

#include "pxr/imaging/garch/contextCaps.h"
#include "pxr/imaging/garch/resourceFactory.h"

#include "pxr/imaging/hdSt/drawItem.h"
#include "pxr/imaging/hdSt/fallbackLightingShader.h"
#include "pxr/imaging/hdSt/glConversions.h"
#include "pxr/imaging/hdSt/renderBuffer.h"
#include "pxr/imaging/hdSt/renderPassShader.h"
#include "pxr/imaging/hdSt/renderPassState.h"
#include "pxr/imaging/hdSt/resourceFactory.h"
#include "pxr/imaging/hdSt/resourceRegistry.h"
#include "pxr/imaging/hdSt/shaderCode.h"

#include "pxr/imaging/hd/aov.h"
#include "pxr/imaging/hd/bufferArrayRange.h"
#include "pxr/imaging/hd/changeTracker.h"
#include "pxr/imaging/hd/engine.h"
#include "pxr/imaging/hd/tokens.h"
#include "pxr/imaging/hd/vtBufferSource.h"

#include "pxr/imaging/hgi/graphicsCmdsDesc.h"

#include "pxr/base/gf/frustum.h"
#include "pxr/base/tf/staticTokens.h"
#include "pxr/base/tf/stringUtils.h"
#include "pxr/base/vt/array.h"

#include <boost/functional/hash.hpp>

PXR_NAMESPACE_OPEN_SCOPE


TF_DEFINE_PRIVATE_TOKENS(
    _tokens,
    (renderPassState)
);

HdStRenderPassState::HdStRenderPassState()
    : HdStRenderPassState(std::make_shared<HdStRenderPassShader>())
{
}

HdStRenderPassState::HdStRenderPassState(
    HdStRenderPassShaderSharedPtr const &renderPassShader)
    : HdRenderPassState()
    , _renderPassShader(renderPassShader)
    , _fallbackLightingShader(std::make_shared<HdSt_FallbackLightingShader>())
    , _clipPlanesBufferSize(0)
    , _alphaThresholdCurrent(0)
    , _hasCustomGraphicsCmdsDesc(false)
{
    _lightingShader = _fallbackLightingShader;
}

HdStRenderPassState::~HdStRenderPassState() = default;

bool
HdStRenderPassState::_UseAlphaMask() const
{
    return (_alphaThreshold > 0.0f);
}

void
HdStRenderPassState::Prepare(
    HdResourceRegistrySharedPtr const &resourceRegistry)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();
    GLF_GROUP_FUNCTION();

    HdRenderPassState::Prepare(resourceRegistry);

    HdStResourceRegistrySharedPtr const& hdStResourceRegistry =
        std::static_pointer_cast<HdStResourceRegistry>(resourceRegistry);

    VtVec4fArray clipPlanes;
    TF_FOR_ALL(it, GetClipPlanes()) {
        clipPlanes.push_back(GfVec4f(*it));
    }
    
    size_t maxClipPlanes = GarchResourceFactory::GetInstance()->GetContextCaps().maxClipPlanes;
    if (clipPlanes.size() >= maxClipPlanes) {
        clipPlanes.resize(maxClipPlanes);
    }

    // allocate bar if not exists
    if (!_renderPassStateBar || 
        (_clipPlanesBufferSize != clipPlanes.size()) ||
        _alphaThresholdCurrent != _alphaThreshold) {
        HdBufferSpecVector bufferSpecs;

        // note: InterleavedMemoryManager computes the offsets in the packed
        // struct of following entries, which CodeGen generates the struct
        // definition into GLSL source in accordance with.
        HdType matType = HdVtBufferSource::GetDefaultMatrixType();

        bufferSpecs.emplace_back(
            HdShaderTokens->worldToViewMatrix,
            HdTupleType{matType, 1});
        bufferSpecs.emplace_back(
            HdShaderTokens->worldToViewInverseMatrix,
            HdTupleType{matType, 1});
        bufferSpecs.emplace_back(
            HdShaderTokens->projectionMatrix,
            HdTupleType{matType, 1});
        bufferSpecs.emplace_back(
            HdShaderTokens->overrideColor,
            HdTupleType{HdTypeFloatVec4, 1});
        bufferSpecs.emplace_back(
            HdShaderTokens->wireframeColor,
            HdTupleType{HdTypeFloatVec4, 1});
        bufferSpecs.emplace_back(
            HdShaderTokens->maskColor,
            HdTupleType{HdTypeFloatVec4, 1});
        bufferSpecs.emplace_back(
            HdShaderTokens->indicatorColor,
            HdTupleType{HdTypeFloatVec4, 1});
        bufferSpecs.emplace_back(
            HdShaderTokens->pointColor,
            HdTupleType{HdTypeFloatVec4, 1});
        bufferSpecs.emplace_back(
            HdShaderTokens->pointSize,
            HdTupleType{HdTypeFloat, 1});
        bufferSpecs.emplace_back(
            HdShaderTokens->pointSelectedSize,
            HdTupleType{HdTypeFloat, 1});
        bufferSpecs.emplace_back(
            HdShaderTokens->lightingBlendAmount,
            HdTupleType{HdTypeFloat, 1});

        if (_UseAlphaMask()) {
            bufferSpecs.emplace_back(
                HdShaderTokens->alphaThreshold,
                HdTupleType{HdTypeFloat, 1});
        }
        _alphaThresholdCurrent = _alphaThreshold;

        bufferSpecs.emplace_back(
            HdShaderTokens->tessLevel,
            HdTupleType{HdTypeFloat, 1});
        bufferSpecs.emplace_back(
            HdShaderTokens->viewport,
            HdTupleType{HdTypeFloatVec4, 1});

        if (clipPlanes.size() > 0) {
            bufferSpecs.emplace_back(
                HdShaderTokens->clipPlanes,
                HdTupleType{HdTypeFloatVec4, clipPlanes.size()});
        }
        _clipPlanesBufferSize = clipPlanes.size();

        // allocate interleaved buffer
        _renderPassStateBar = 
            hdStResourceRegistry->AllocateUniformBufferArrayRange(
                HdTokens->drawingShader, bufferSpecs, HdBufferArrayUsageHint());

        HdBufferArrayRangeSharedPtr _renderPassStateBar_ =
            std::static_pointer_cast<HdBufferArrayRange> (_renderPassStateBar);

        // add buffer binding request
        _renderPassShader->AddBufferBinding(
            HdBindingRequest(HdBinding::UBO, _tokens->renderPassState,
                             _renderPassStateBar_, /*interleaved=*/true));
    }

    // Lighting hack supports different blending amounts, but we are currently
    // only using the feature to turn lighting on and off.
    float lightingBlendAmount = (_lightingEnabled ? 1.0f : 0.0f);

    GfMatrix4d const& worldToViewMatrix = GetWorldToViewMatrix();
    GfMatrix4d projMatrix = GetProjectionMatrix();

    HdBufferSourceSharedPtrVector sources = {
        std::make_shared<HdVtBufferSource>(
            HdShaderTokens->worldToViewMatrix,
            worldToViewMatrix),
        std::make_shared<HdVtBufferSource>(
            HdShaderTokens->worldToViewInverseMatrix,
            worldToViewMatrix.GetInverse()),
        std::make_shared<HdVtBufferSource>(
            HdShaderTokens->projectionMatrix,
            projMatrix),
        // Override color alpha component is used as the amount to blend in the
        // override color over the top of the regular fragment color.
        std::make_shared<HdVtBufferSource>(
            HdShaderTokens->overrideColor,
            VtValue(_overrideColor)),
        std::make_shared<HdVtBufferSource>(
            HdShaderTokens->wireframeColor,
            VtValue(_wireframeColor)),
        std::make_shared<HdVtBufferSource>(
            HdShaderTokens->maskColor,
            VtValue(_maskColor)),
        std::make_shared<HdVtBufferSource>(
            HdShaderTokens->indicatorColor,
            VtValue(_indicatorColor)),
        std::make_shared<HdVtBufferSource>(
            HdShaderTokens->pointColor,
            VtValue(_pointColor)),
        std::make_shared<HdVtBufferSource>(
            HdShaderTokens->pointSize,
            VtValue(_pointSize)),
        std::make_shared<HdVtBufferSource>(
            HdShaderTokens->pointSelectedSize,
            VtValue(_pointSelectedSize)),
        std::make_shared<HdVtBufferSource>(
            HdShaderTokens->lightingBlendAmount,
            VtValue(lightingBlendAmount))
    };

    if (_UseAlphaMask()) {
        sources.push_back(
            std::make_shared<HdVtBufferSource>(
                HdShaderTokens->alphaThreshold,
                VtValue(_alphaThreshold)));
    }

    sources.push_back(
        std::make_shared<HdVtBufferSource>(
            HdShaderTokens->tessLevel,
            VtValue(_tessLevel)));
    sources.push_back(
        std::make_shared<HdVtBufferSource>(
            HdShaderTokens->viewport,
            VtValue(_viewport)));

    if (clipPlanes.size() > 0) {
        sources.push_back(
            std::make_shared<HdVtBufferSource>(
                HdShaderTokens->clipPlanes,
                VtValue(clipPlanes),
                clipPlanes.size()));
    }

    hdStResourceRegistry->AddSources(_renderPassStateBar, std::move(sources));

    // notify view-transform to the lighting shader to update its uniform block
    _lightingShader->SetCamera(worldToViewMatrix, projMatrix);

    // Update cull style on renderpass shader
    // XXX: Ideanlly cullstyle should stay in renderPassState.
    // However, geometric shader also sets cullstyle during batch
    // execution.
    _renderPassShader->SetCullStyle(_cullStyle);
}

void
HdStRenderPassState::SetLightingShader(HdStLightingShaderSharedPtr const &lightingShader)
{
    if (lightingShader) {
        _lightingShader = lightingShader;
    } else {
        _lightingShader = _fallbackLightingShader;
    }
}

void 
HdStRenderPassState::SetRenderPassShader(HdStRenderPassShaderSharedPtr const &renderPassShader)
{
    if (_renderPassShader == renderPassShader) return;

    _renderPassShader = renderPassShader;
    if (_renderPassStateBar) {

        HdBufferArrayRangeSharedPtr _renderPassStateBar_ =
            std::static_pointer_cast<HdBufferArrayRange> (_renderPassStateBar);

        _renderPassShader->AddBufferBinding(
            HdBindingRequest(HdBinding::UBO, _tokens->renderPassState,
                             _renderPassStateBar_, /*interleaved=*/true));
    }
}

void 
HdStRenderPassState::SetOverrideShader(HdStShaderCodeSharedPtr const &overrideShader)
{
    _overrideShader = overrideShader;
}

HdStShaderCodeSharedPtrVector
HdStRenderPassState::GetShaders() const
{
    HdStShaderCodeSharedPtrVector shaders;
    shaders.reserve(2);
    shaders.push_back(_lightingShader);
    shaders.push_back(_renderPassShader);
    return shaders;
}

void
HdStRenderPassState::Bind()
{
    GLF_GROUP_FUNCTION();
    
    // notify view-transform to the lighting shader to update its uniform block
    // this needs to be done in execute as a multi camera setup may have been 
    // synced with a different view matrix baked in for shadows.
    // SetCamera will no-op if the transforms are the same as before.
    
    // METALTODO: THIS IS A TEMP FIX - INVESTIGAGE WITH LINUX
//    _lightingShader->SetCamera(_worldToViewMatrix, _projectionMatrix);
}

void
HdStRenderPassState::Unbind()
{
    /*NOTHING*/
}

size_t
HdStRenderPassState::GetShaderHash() const
{
    size_t hash = 0;
    if (_lightingShader) {
        boost::hash_combine(hash, _lightingShader->ComputeHash());
    }
    if (_renderPassShader) {
        boost::hash_combine(hash, _renderPassShader->ComputeHash());
    }
    boost::hash_combine(hash, GetClipPlanes().size());
    boost::hash_combine(hash, _UseAlphaMask());
    return hash;
}

GfVec2f
HdStRenderPassState::GetAovDimensions() const
{
    const HdRenderPassAovBindingVector& aovBindings = GetAovBindings();
    if (aovBindings.empty()) {
        return GfVec2f(0, 0);
    }

    // Assume AOVs have the same dimensions so pick size of any.
    auto aov = aovBindings[0];

    return GfVec2f(aov.renderBuffer->GetWidth(), aov.renderBuffer->GetHeight());
}

HgiGraphicsCmdsDesc
HdStRenderPassState::MakeGraphicsCmdsDesc() const
{
    const HdRenderPassAovBindingVector& aovBindings = GetAovBindings();

    if (_hasCustomGraphicsCmdsDesc) {
        if (!aovBindings.empty()) {
            TF_CODING_ERROR(
                "Cannot specify a graphics cmds desc and aov bindings "
                "at the same time.");
        }

        return _customGraphicsCmdsDesc;
    }

    static const size_t maxColorTex = 8;
    const bool useMultiSample = GetUseAovMultiSample();

    HgiGraphicsCmdsDesc desc;

    // If the AOV bindings have not changed that does NOT mean the
    // graphicsCmdsDescriptor will not change. The HdRenderBuffer may be
    // resized at any time, which will destroy and recreate the HgiTextureHandle
    // that backs the render buffer and was attached for graphics encoding.

    for (const HdRenderPassAovBinding& aov : aovBindings) {
        if (!TF_VERIFY(aov.renderBuffer, "Invalid render buffer")) {
            continue;
        }

        bool multiSampled= useMultiSample && aov.renderBuffer->IsMultiSampled();
        VtValue rv = aov.renderBuffer->GetResource(multiSampled);

        if (!TF_VERIFY(rv.IsHolding<HgiTextureHandle>(), 
            "Invalid render buffer texture")) {
            continue;
        }

        // Get render target texture
        HgiTextureHandle hgiTexHandle = rv.UncheckedGet<HgiTextureHandle>();

        // Get resolve texture target.
        HgiTextureHandle hgiResolveHandle;
        if (multiSampled) {
            VtValue resolveRes = aov.renderBuffer->GetResource(/*ms*/false);
            if (!TF_VERIFY(resolveRes.IsHolding<HgiTextureHandle>())) {
                continue;
            }
            hgiResolveHandle = resolveRes.UncheckedGet<HgiTextureHandle>();
        }

        // Assume AOVs have the same dimensions so pick size of any.
        desc.width = aov.renderBuffer->GetWidth();
        desc.height = aov.renderBuffer->GetHeight();

        HgiAttachmentDesc attachmentDesc;

        attachmentDesc.format = hgiTexHandle.Get()->GetDescriptor().format;

        // We need to use LoadOpLoad instead of DontCare because we can have
        // multiple render passes that use the same attachments.
        // For example, translucent renders after opaque so we must load the
        // opaque results before rendering translucent objects.
        HgiAttachmentLoadOp loadOp = aov.clearValue.IsEmpty() ?
            HgiAttachmentLoadOpLoad :
            HgiAttachmentLoadOpClear;

        attachmentDesc.loadOp = loadOp;

        // Don't store multisample images. Only store the resolved versions.
        // This saves a bunch of bandwith (especially on tiled gpu's).
        attachmentDesc.storeOp = multiSampled ?
            HgiAttachmentStoreOpDontCare :
            HgiAttachmentStoreOpStore;

        if (aov.clearValue.IsHolding<float>()) {
            float depth = aov.clearValue.UncheckedGet<float>();
            attachmentDesc.clearValue = GfVec4f(depth,0,0,0);
        } else if (aov.clearValue.IsHolding<GfVec4f>()) {
            const GfVec4f& col = aov.clearValue.UncheckedGet<GfVec4f>();
            attachmentDesc.clearValue = col;
        }

        // HdSt expresses blending per RenderPassState, where Hgi expresses
        // blending per-attachment. Transfer pass blend state to attachments.
        attachmentDesc.blendEnabled = _blendEnabled;
        attachmentDesc.srcColorBlendFactor=HgiBlendFactor(_blendColorSrcFactor);
        attachmentDesc.dstColorBlendFactor=HgiBlendFactor(_blendColorDstFactor);
        attachmentDesc.colorBlendOp = HgiBlendOp(_blendColorOp);
        attachmentDesc.srcAlphaBlendFactor=HgiBlendFactor(_blendAlphaSrcFactor);
        attachmentDesc.dstAlphaBlendFactor=HgiBlendFactor(_blendAlphaDstFactor);
        attachmentDesc.alphaBlendOp = HgiBlendOp(_blendAlphaOp);

        if (HdAovHasDepthSemantic(aov.aovName)) {
            desc.depthAttachmentDesc = std::move(attachmentDesc);
            desc.depthTexture = hgiTexHandle;
            if (hgiResolveHandle) {
                desc.depthResolveTexture = hgiResolveHandle;
            }
        } else if (TF_VERIFY(desc.colorAttachmentDescs.size() < maxColorTex,
                   "Too many aov bindings for color attachments"))
        {
            desc.colorAttachmentDescs.push_back(std::move(attachmentDesc));
            desc.colorTextures.push_back(hgiTexHandle);
            if (hgiResolveHandle) {
                desc.colorResolveTextures.push_back(hgiResolveHandle);
            }
        }
    }

    return desc;
}

void
HdStRenderPassState::SetCustomGraphicsCmdsDesc(
    const HgiGraphicsCmdsDesc &graphicsCmdDesc)
{
    _customGraphicsCmdsDesc = graphicsCmdDesc;
    _hasCustomGraphicsCmdsDesc = true;
}

void
HdStRenderPassState::ClearCustomGraphicsCmdsDesc()
{
    _customGraphicsCmdsDesc = HgiGraphicsCmdsDesc();
    _hasCustomGraphicsCmdsDesc = false;
}


PXR_NAMESPACE_CLOSE_SCOPE
