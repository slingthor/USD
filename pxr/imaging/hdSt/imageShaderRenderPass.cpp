//
// Copyright 2019 Pixar
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

#include "pxr/imaging/mtlf/mtlDevice.h"

#include "pxr/imaging/hdSt/imageShaderRenderPass.h"
#include "pxr/imaging/hdSt/imageShaderShaderKey.h"
#include "pxr/imaging/hdSt/resourceRegistry.h"
#include "pxr/imaging/hdSt/renderDelegate.h"
#include "pxr/imaging/hdSt/renderPassState.h"
#include "pxr/imaging/hdSt/renderPassShader.h"
#include "pxr/imaging/hdSt/geometricShader.h"
#include "pxr/imaging/hdSt/resourceFactory.h"
#include "pxr/imaging/hdSt/surfaceShader.h"
#include "pxr/imaging/hdSt/glslfxShader.h"
#include "pxr/imaging/hdSt/immediateDrawBatch.h"
#include "pxr/imaging/hdSt/package.h"
#include "pxr/imaging/hd/drawingCoord.h"
#include "pxr/imaging/hd/driver.h"
#include "pxr/imaging/hd/vtBufferSource.h"
#include "pxr/imaging/hgi/graphicsEncoder.h"
#include "pxr/imaging/hgi/graphicsEncoderDesc.h"
#include "pxr/imaging/hgi/hgi.h"
#include "pxr/imaging/hgi/immediateCommandBuffer.h"
#include "pxr/imaging/hgi/tokens.h"
#include "pxr/imaging/glf/diagnostic.h"

PXR_NAMESPACE_OPEN_SCOPE

HdSt_ImageShaderRenderPass::HdSt_ImageShaderRenderPass(
    HdRenderIndex *index,
    HdRprimCollection const &collection)
    : HdRenderPass(index, collection)
    , _sharedData(1)
    , _drawItem(&_sharedData)
    , _drawItemInstance(&_drawItem)
    , _hgi(nullptr)
{
    _sharedData.instancerLevels = 0;
    _sharedData.rprimID = SdfPath("/imageShaderRenderPass");
    _immediateBatch = HdSt_DrawBatchSharedPtr(
        new HdSt_ImmediateDrawBatch(&_drawItemInstance));

    HdDriverVector const& drivers = index->GetDrivers();
    for (HdDriver* hdDriver : drivers) {
        if (hdDriver->name == HgiTokens->renderDriver &&
            hdDriver->driver.IsHolding<Hgi*>()) {
            _hgi = hdDriver->driver.UncheckedGet<Hgi*>();
            break;
        }
    }
}

HdSt_ImageShaderRenderPass::~HdSt_ImageShaderRenderPass()
{
}

void
HdSt_ImageShaderRenderPass::_SetupVertexPrimvarBAR(
    HdStResourceRegistrySharedPtr const& registry)
{
    // The current logic in HdSt_ImmediateDrawBatch::ExecuteDraw will use
    // glDrawArraysInstanced if it finds a VertexPrimvar buffer but no
    // index buffer, We setup the BAR to meet this requirement to draw our
    // full-screen triangle for post-process shaders.

    HdBufferSourceVector sources;
    HdBufferSpecVector bufferSpecs;

    HdBufferSourceSharedPtr pointsSource(
        new HdVtBufferSource(HdTokens->points, VtValue(VtVec3fArray(3))));

    sources.push_back(pointsSource);
    pointsSource->GetBufferSpecs(&bufferSpecs);

    HdBufferArrayRangeSharedPtr vertexPrimvarRange =
        registry->AllocateNonUniformBufferArrayRange(
            HdTokens->primvar, bufferSpecs, HdBufferArrayUsageHint());

    registry->AddSources(vertexPrimvarRange, sources);

    HdDrawingCoord* drawingCoord = _drawItem.GetDrawingCoord();
    _sharedData.barContainer.Set(
        drawingCoord->GetVertexPrimvarIndex(), 
        vertexPrimvarRange);
}

void
HdSt_ImageShaderRenderPass::_Prepare(TfTokenVector const &renderTags)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();
    GLF_GROUP_FUNCTION();

    HdStResourceRegistrySharedPtr const& resourceRegistry = 
        boost::dynamic_pointer_cast<HdStResourceRegistry>(
        GetRenderIndex()->GetResourceRegistry());
    TF_VERIFY(resourceRegistry);

    // First time we must create a VertexPrimvar BAR for the triangle and setup
    // the geometric shader that provides the vertex and fragment shaders.
    if (!_sharedData.barContainer.Get(
            _drawItem.GetDrawingCoord()->GetVertexPrimvarIndex())) 
    {
        _SetupVertexPrimvarBAR(resourceRegistry);

        HdSt_ImageShaderShaderKey shaderKey;
        HdSt_GeometricShaderSharedPtr geometricShader =
            HdSt_GeometricShader::Create(shaderKey, resourceRegistry);

        _drawItem.SetGeometricShader(geometricShader);
    }
}

void
HdSt_ImageShaderRenderPass::_Execute(
    HdRenderPassStateSharedPtr const &renderPassState,
    TfTokenVector const& renderTags)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    // Downcast render pass state
    HdStRenderPassStateSharedPtr stRenderPassState =
        boost::dynamic_pointer_cast<HdStRenderPassState>(
        renderPassState);
    if (!TF_VERIFY(stRenderPassState)) return;

    HdStResourceRegistrySharedPtr const& resourceRegistry = 
        boost::dynamic_pointer_cast<HdStResourceRegistry>(
        GetRenderIndex()->GetResourceRegistry());
    TF_VERIFY(resourceRegistry);

	MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();
    context->StartFrameForThread();
	
#if defined(ARCH_GFX_OPENGL)
    // XXX Non-Hgi tasks expect default FB. Remove once all tasks use Hgi.
    bool isOpenGL = HdStResourceFactory::GetInstance()->IsOpenGL();
    GLint fb;
    if (isOpenGL) {
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fb);
    }
#endif
    // Create graphics encoder to render into Aovs.
    HgiGraphicsEncoderDesc desc = stRenderPassState->MakeGraphicsEncoderDesc();
    HgiImmediateCommandBuffer& icb = _hgi->GetImmediateCommandBuffer();
    HgiGraphicsEncoderUniquePtr gfxEncoder = icb.CreateGraphicsEncoder(desc);

    GfVec4i vp;

    // XXX Some tasks do not yet use Aov, so gfx encoder might be null
    if (gfxEncoder) {
        gfxEncoder->PushDebugGroup(__ARCH_PRETTY_FUNCTION__);
#if defined(ARCH_GFX_OPENGL)
        // XXX The application may have directly called into glViewport.
        // We need to remove the offset to avoid double offset when we composite
        // the Aov back into the client framebuffer.
        // E.g. UsdView CameraMask.
        if (isOpenGL) {
            glGetIntegerv(GL_VIEWPORT, vp.data());
            GfVec4i aovViewport(0, 0, vp[2]+vp[0], vp[3]+vp[1]);
            gfxEncoder->SetViewport(aovViewport);
        }
#endif
    }

    // Draw
    _immediateBatch->PrepareDraw(stRenderPassState, resourceRegistry);
    _immediateBatch->ExecuteDraw(stRenderPassState, resourceRegistry);

    if (gfxEncoder) {
        gfxEncoder->SetViewport(vp);
        gfxEncoder->PopDebugGroup();
        gfxEncoder->EndEncoding();

#if defined(ARCH_GFX_OPENGL)
        if (isOpenGL) {
            // XXX Non-Hgi tasks expect default FB. Remove once all tasks use Hgi.
            glBindFramebuffer(GL_FRAMEBUFFER, fb);
        }
#endif
    }
    
    // Flush commands for execution
    icb.FlushEncoders();

	if (context->GeometryShadersActive()) {
        // Complete the GS command buffer if we have one
        context->CommitCommandBufferForThread(false, false, METALWORKQUEUE_GEOMETRY_SHADER);
    }
    
    if (context->GetWorkQueue(METALWORKQUEUE_DEFAULT).commandBuffer != nil) {
        context->CommitCommandBufferForThread(false, false);
        
        context->EndFrameForThread();
    }
}

void
HdSt_ImageShaderRenderPass::_MarkCollectionDirty()
{
}


PXR_NAMESPACE_CLOSE_SCOPE
