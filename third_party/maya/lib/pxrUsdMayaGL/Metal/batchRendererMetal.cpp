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

// glew needs to be included before any other OpenGL headers.
#include "pxr/imaging/glf/glew.h"

#include "pxr/imaging/mtlf/mtlDevice.h"

#include "pxr/pxr.h"
#include "pxrUsdMayaGL/Metal/batchRendererMetal.h"
#include "pxrUsdMayaGL/debugCodes.h"
#include "pxrUsdMayaGL/renderParams.h"
#include "pxrUsdMayaGL/sceneDelegate.h"
#include "pxrUsdMayaGL/shapeAdapter.h"
#include "pxrUsdMayaGL/softSelectHelper.h"
#include "pxrUsdMayaGL/userData.h"

#include "px_vp20/utils.h"
#include "px_vp20/utils_legacy.h"

#include "pxr/base/gf/matrix4d.h"
#include "pxr/base/gf/vec2i.h"
#include "pxr/base/gf/vec3f.h"
#include "pxr/base/gf/vec4d.h"
#include "pxr/base/gf/vec4f.h"
#include "pxr/base/tf/debug.h"
#include "pxr/base/tf/diagnostic.h"
#include "pxr/base/tf/envSetting.h"
#include "pxr/base/tf/getenv.h"
#include "pxr/base/tf/instantiateSingleton.h"
#include "pxr/base/tf/singleton.h"
#include "pxr/base/tf/staticTokens.h"
#include "pxr/base/tf/stringUtils.h"
#include "pxr/base/tf/stl.h"
#include "pxr/base/tf/token.h"
#include "pxr/base/vt/types.h"
#include "pxr/base/vt/value.h"
#include "pxr/imaging/hd/renderIndex.h"
#include "pxr/imaging/hd/rprimCollection.h"
#include "pxr/imaging/hd/task.h"
#include "pxr/imaging/hd/tokens.h"
#include "pxr/imaging/hdx/intersector.h"
#include "pxr/imaging/hdx/selectionTracker.h"
#include "pxr/imaging/hdx/tokens.h"
#include "pxr/usd/sdf/path.h"

#include <maya/M3dView.h>
#include <maya/MDagPath.h>
#include <maya/MDrawContext.h>
#include <maya/MDrawData.h>
#include <maya/MDrawRequest.h>
#include <maya/MEventMessage.h>
#include <maya/MFileIO.h>
#include <maya/MFrameContext.h>
#include <maya/MGlobal.h>
#include <maya/MMatrix.h>
#include <maya/MObject.h>
#include <maya/MObjectHandle.h>
#include <maya/MSceneMessage.h>
#include <maya/MSelectInfo.h>
#include <maya/MSelectionContext.h>
#include <maya/MStatus.h>
#include <maya/MString.h>
#include <maya/MTypes.h>
#include <maya/MUserData.h>
#include <maya/MViewport2Renderer.h>

#include <utility>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

UsdMayaGLBatchRendererMetal::UsdMayaGLBatchRendererMetal() :
    _hdEngine(HdEngine::Metal)
{
    GarchResourceFactory::GetInstance().SetResourceFactory(&_resourceFactory);
}

/* virtual */
UsdMayaGLBatchRendererMetal::~UsdMayaGLBatchRendererMetal()
{
}

void
UsdMayaGLBatchRendererMetal::_Render(
        const GfMatrix4d& worldToViewMatrix,
        const GfMatrix4d& projectionMatrix,
        const GfVec4d& viewport,
        const std::vector<UsdMayaGLBatchRenderer::_RenderItem>& items)
{
    _taskDelegate->SetCameraState(worldToViewMatrix,
                                  projectionMatrix,
                                  viewport);

    // save the current GL states which hydra may reset to default
    glPushAttrib(GL_LIGHTING_BIT |
                 GL_ENABLE_BIT |
                 GL_POLYGON_BIT |
                 GL_DEPTH_BUFFER_BIT |
                 GL_VIEWPORT_BIT);

    // XXX: When Maya is using OpenGL Core Profile as the rendering engine (in
    // either compatibility or strict mode), batch renders like those done in
    // the "Render View" window or through the ogsRender command do not
    // properly track uniform buffer binding state. This was causing issues
    // where the first batch render performed would look correct, but then all
    // subsequent renders done in that Maya session would be completely black
    // (no alpha), even if the frame contained only Maya-native geometry or if
    // a new scene was created/opened.
    //
    // To avoid this problem, we need to save and restore Maya's bindings
    // across Hydra calls. We try not to bog down performance by saving and
    // restoring *all* GL_MAX_UNIFORM_BUFFER_BINDINGS possible bindings, so
    // instead we only do just enough to avoid issues. Empirically, the
    // problematic binding has been the material binding at index 4.
    static constexpr size_t _UNIFORM_BINDINGS_TO_SAVE = 5u;
    std::vector<GLint> uniformBufferBindings(_UNIFORM_BINDINGS_TO_SAVE, 0);
    for (size_t i = 0u; i < uniformBufferBindings.size(); ++i) {
        glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING,
                        (GLuint)i,
                        &uniformBufferBindings[i]);
    }

    // hydra orients all geometry during topological processing so that
    // front faces have ccw winding. We disable culling because culling
    // is handled by fragment shader discard.
    glFrontFace(GL_CCW); // < State is pushed via GL_POLYGON_BIT
    glDisable(GL_CULL_FACE);

    // note: to get benefit of alpha-to-coverage, the target framebuffer
    // has to be a MSAA buffer.
    glDisable(GL_BLEND);
    glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);

    // In all cases, we can should enable gamma correction:
    // - in viewport 1.0, we're expected to do it
    // - in viewport 2.0 without color correction, we're expected to do it
    // - in viewport 2.0 with color correction, the render target ignores this
    //   bit meaning we properly are blending linear colors in the render
    //   target.  The color management pipeline is responsible for the final
    //   correction.
    glEnable(GL_FRAMEBUFFER_SRGB_EXT);

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    // render task setup
    HdTaskSharedPtrVector tasks = _taskDelegate->GetSetupTasks(); // lighting etc

    for (const auto& iter : items) {
        const PxrMayaHdRenderParams& params = iter.first;
        const size_t paramsHash = params.Hash();

        const HdRprimCollectionVector& rprimCollections = iter.second;

        TF_DEBUG(PXRUSDMAYAGL_BATCHED_DRAWING).Msg(
            "    *** renderBucket, parameters hash: %zu, bucket size %zu\n",
            paramsHash,
            rprimCollections.size());

        HdTaskSharedPtrVector renderTasks =
            _taskDelegate->GetRenderTasks(paramsHash, params, rprimCollections);
        tasks.insert(tasks.end(), renderTasks.begin(), renderTasks.end());
    }

    MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();
    
    // Make sure the Metal render targets, and GL interop textures match the GL viewport size
    if (context->mtlColorTexture.width != viewport[2] ||
        context->mtlColorTexture.height != viewport[3]) {
        context->AllocateAttachments(viewport[2], viewport[3]);
    }
    
    MTLCaptureManager *sharedCaptureManager = [MTLCaptureManager sharedCaptureManager];
    //[sharedCaptureManager startCaptureWithScope:sharedCaptureManager.defaultCaptureScope];
    [sharedCaptureManager.defaultCaptureScope beginScope];
    
    static MTLRenderPassDescriptor *_mtlRenderPassDescriptor = nil;
    if (_mtlRenderPassDescriptor == nil)
        _mtlRenderPassDescriptor = [[MTLRenderPassDescriptor alloc] init];
    
    //Set this state every frame because it may have changed during rendering.
    {
        // create a color attachment every frame since we have to recreate the texture every frame
        MTLRenderPassColorAttachmentDescriptor *colorAttachment = _mtlRenderPassDescriptor.colorAttachments[0];
        
        // make sure to clear every frame for best performance
        colorAttachment.loadAction = MTLLoadActionClear;
        
        // store only attachments that will be presented to the screen, as in this case
        colorAttachment.storeAction = MTLStoreActionStore;
        
        MTLRenderPassDepthAttachmentDescriptor *depthAttachment = _mtlRenderPassDescriptor.depthAttachment;
        depthAttachment.loadAction = MTLLoadActionClear;
        depthAttachment.storeAction = MTLStoreActionStore;
        depthAttachment.clearDepth = 1.0f;
        
        GLfloat clearColor[4];
        glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor);
        clearColor[3] = 1.0f;
        
        colorAttachment.texture = context->mtlColorTexture;
        colorAttachment.clearColor = MTLClearColorMake(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
        
        depthAttachment.texture = context->mtlDepthTexture;
    }
    
    context->StartFrame();
    
    // Create a new command buffer for each render pass to the current drawable
    context->CreateCommandBuffer(METALWORKQUEUE_DEFAULT);
    context->LabelCommandBuffer(@"HdEngine::Render", METALWORKQUEUE_DEFAULT);
    
    // Set the render pass descriptor to use for the render encoders
    context->SetRenderPassDescriptor(_mtlRenderPassDescriptor);

    context->SetFrontFaceWinding(MTLWindingCounterClockwise);
    context->SetCullMode(MTLCullModeNone);

    VtValue selectionTrackerValue(_selectionTracker);
    _hdEngine.SetTaskContextData(HdxTokens->selectionState,
                                 selectionTrackerValue);
    
    _hdEngine.Execute(*_renderIndex, tasks);

    // Depth texture copy
    context->CopyDepthTextureToOpenGL();
    
    if (context->GeometryShadersActive()) {
        // Complete the GS command buffer if we have one
        context->CommitCommandBuffer(true, false, METALWORKQUEUE_GEOMETRY_SHADER);
    }
    
    // Commit the render buffer (will wait for GS to complete if present)
    // We wait until scheduled, because we're about to consume the Metal
    // generated textures in an OpenGL blit
    context->CommitCommandBuffer(true, false);
    
    context->EndFrame();
    
    // Finalize rendering here & push the command buffer to the GPU
    [sharedCaptureManager.defaultCaptureScope endScope];
    
    context->BlitColorTargetToOpenGL();

    glDisable(GL_FRAMEBUFFER_SRGB_EXT);

    // XXX: Restore Maya's uniform buffer binding state. See above for details.
    for (size_t i = 0u; i < uniformBufferBindings.size(); ++i) {
        glBindBufferBase(GL_UNIFORM_BUFFER,
                         (GLuint)i,
                         uniformBufferBindings[i]);
    }

    glPopAttrib(); // GL_LIGHTING_BIT | GL_ENABLE_BIT | GL_POLYGON_BIT |
                   // GL_DEPTH_BUFFER_BIT | GL_VIEWPORT_BIT
}

PXR_NAMESPACE_CLOSE_SCOPE
