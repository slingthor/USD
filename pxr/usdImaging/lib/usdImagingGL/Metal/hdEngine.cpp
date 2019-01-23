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

#include "pxr/usdImaging/usdImagingGL/Metal/hdEngine.h"
#include "pxr/usdImaging/usdImaging/tokens.h"

#include "pxr/imaging/hdSt/resourceRegistry.h"
#include "pxr/imaging/hdSt/Metal/resourceFactoryMetal.h"

#include "pxr/imaging/hd/debugCodes.h"
#include "pxr/imaging/hd/renderDelegate.h"
#include "pxr/imaging/hd/resourceRegistry.h"
#include "pxr/imaging/hd/tokens.h"
#include "pxr/imaging/hd/version.h"

#include "pxr/imaging/hdx/intersector.h"
#include "pxr/imaging/hdx/rendererPluginRegistry.h"
#include "pxr/imaging/hdx/tokens.h"

#include "pxr/base/gf/matrix4d.h"
#include "pxr/base/gf/range3d.h"
#include "pxr/base/gf/rotation.h"
#include "pxr/base/gf/vec3d.h"

#include "pxr/base/tf/getenv.h"
#include "pxr/base/tf/stringUtils.h"

#include "pxr/imaging/mtlf/mtlDevice.h"
#include "pxr/imaging/mtlf/diagnostic.h"
#include "pxr/imaging/mtlf/info.h"
#include "pxr/imaging/mtlf/utils.h"

#include "pxr/imaging/garch/simpleLightingContext.h"

#import <simd/simd.h>

typedef struct {
    vector_float2 position;
    vector_float2 uv;
} Vertex;

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PRIVATE_TOKENS(
    _tokens,
    (proxy)
    (render)
);

static std::string _MetalPluginDescriptor(id<MTLDevice> device)
{
    return std::string("Hydra Metal - ") + [[device name] UTF8String];
}

UsdImagingGLMetalHdEngine::UsdImagingGLMetalHdEngine(
        RenderOutput outputTarget,
        const SdfPath& rootPath,
        const SdfPathVector& excludedPrimPaths,
        const SdfPathVector& invisedPrimPaths,
        const SdfPath& delegateID)
    : UsdImagingGLEngine()
    , _engine(HdEngine::Metal)
    , _renderIndex(nullptr)
    , _selTracker(new HdxSelectionTracker)
    , _delegateID(delegateID)
    , _delegate(nullptr)
    , _rendererPlugin(nullptr)
    , _taskController(nullptr)
    , _selectionColor(1.0f, 1.0f, 0.0f, 1.0f)
    , _rootPath(rootPath)
    , _excludedPrimPaths(excludedPrimPaths)
    , _invisedPrimPaths(invisedPrimPaths)
    , _isPopulated(false)
    , _renderTags()
    , _renderOutput(outputTarget)
    , _mtlRenderPassDescriptorForInterop(nil)
    , _mtlRenderPassDescriptor(nil)
    , _sharedCaptureManager(nil)
    , _captureScope(nil)
    , _resourceFactory(nil)
{
    _resourceFactory = new HdStResourceFactoryMetal();

    GarchResourceFactory::GetInstance().SetResourceFactory(dynamic_cast<GarchResourceFactoryInterface*>(_resourceFactory));
    HdStResourceFactory::GetInstance().SetResourceFactory(_resourceFactory);

    // _renderIndex, _taskController, and _delegate are initialized
    // by the plugin system.
    if (!SetRendererPlugin(GetDefaultRendererPluginId())) {
        TF_CODING_ERROR("No renderer plugins found! Check before creation.");
    }

    MtlfRegisterDefaultDebugOutputMessageCallback();
    
    _mtlRenderPassDescriptorForNativeMetal = [[MTLRenderPassDescriptor alloc] init];

    _InitializeCapturing();
}

UsdImagingGLMetalHdEngine::~UsdImagingGLMetalHdEngine()
{
    _DeleteHydraResources();
    HdStResourceFactory::GetInstance().SetResourceFactory(NULL);
    GarchResourceFactory::GetInstance().SetResourceFactory(NULL);
    
    [_mtlRenderPassDescriptorForNativeMetal release];
    _mtlRenderPassDescriptorForNativeMetal = nil;
    
    delete _resourceFactory;
    _resourceFactory = nil;
}

HdRenderIndex *
UsdImagingGLMetalHdEngine::GetRenderIndex() const
{
    return _renderIndex;
}

void
UsdImagingGLMetalHdEngine::InvalidateBuffers()
{
    //_delegate->GetRenderIndex().GetChangeTracker().MarkPrimDirty(path, flag);
}

static int
_GetRefineLevel(float c)
{
    // TODO: Change complexity to refineLevel when we refactor UsdImaging.
    //
    // Convert complexity float to refine level int.
    int refineLevel = 0;

    // to avoid floating point inaccuracy (e.g. 1.3 > 1.3f)
    c = std::min(c + 0.01f, 2.0f);

    if (1.0f <= c && c < 1.1f) { 
        refineLevel = 0;
    } else if (1.1f <= c && c < 1.2f) { 
        refineLevel = 1;
    } else if (1.2f <= c && c < 1.3f) { 
        refineLevel = 2;
    } else if (1.3f <= c && c < 1.4f) { 
        refineLevel = 3;
    } else if (1.4f <= c && c < 1.5f) { 
        refineLevel = 4;
    } else if (1.5f <= c && c < 1.6f) { 
        refineLevel = 5;
    } else if (1.6f <= c && c < 1.7f) { 
        refineLevel = 6;
    } else if (1.7f <= c && c < 1.8f) { 
        refineLevel = 7;
    } else if (1.8f <= c && c <= 2.0f) { 
        refineLevel = 8;
    } else {
        TF_CODING_ERROR("Invalid complexity %f, expected range is [1.0,2.0]\n", 
                c);
    }
    return refineLevel;
}

bool 
UsdImagingGLMetalHdEngine::_CanPrepareBatch(const UsdPrim& root,
                                            const UsdImagingGLRenderParams& params)
{
    HD_TRACE_FUNCTION();

    if (!TF_VERIFY(root, "Attempting to draw an invalid/null prim\n")) 
        return false;

    if (!root.GetPath().HasPrefix(_rootPath)) {
        TF_CODING_ERROR("Attempting to draw path <%s>, but HdEngine is rooted"
                    "at <%s>\n",
                    root.GetPath().GetText(),
                    _rootPath.GetText());
        return false;
    }

    return true;
}

void
UsdImagingGLMetalHdEngine::_PreSetTime(const UsdPrim& root,
                                       const UsdImagingGLRenderParams& params)
{
    HD_TRACE_FUNCTION();

    // Set the fallback refine level, if this changes from the existing value,
    // all prim refine levels will be dirtied.
    int refineLevel = _GetRefineLevel(params.complexity);
    _delegate->SetRefineLevelFallback(refineLevel);
    
    // Apply any queued up scene edits.
    _delegate->ApplyPendingUpdates();
}

void
UsdImagingGLMetalHdEngine::_PostSetTime(const UsdPrim& root,
                                        const UsdImagingGLRenderParams& params)
{
    HD_TRACE_FUNCTION();
}

/*virtual*/
void
UsdImagingGLMetalHdEngine::PrepareBatch(const UsdPrim& root,
                                        const UsdImagingGLRenderParams& params)
{
    HD_TRACE_FUNCTION();
    
    if (_CanPrepareBatch(root, params)) {
        if (!_isPopulated) {
            _delegate->SetUsdDrawModesEnabled(params.enableUsdDrawModes);
            _delegate->Populate(root.GetStage()->GetPrimAtPath(_rootPath),
                                _excludedPrimPaths);
            _delegate->SetInvisedPrimPaths(_invisedPrimPaths);
            _isPopulated = true;
        }
        
        _PreSetTime(root, params);
        // SetTime will only react if time actually changes.
        _delegate->SetTime(params.frame);
        _PostSetTime(root, params);
    }
}

/* static */
bool
UsdImagingGLMetalHdEngine::_UpdateHydraCollection(HdRprimCollection *collection,
                          SdfPathVector const& roots,
                          UsdImagingGLRenderParams const& params,
                          TfTokenVector *renderTags)
{
    if (collection == nullptr) {
        TF_CODING_ERROR("Null passed to _UpdateHydraCollection");
        return false;
    }
    
    // choose repr
    HdReprSelector reprSelector = HdReprSelector(HdReprTokens->smoothHull);
    bool refined = params.complexity > 1.0;
    
    if (params.drawMode == UsdImagingGLDrawMode::DRAW_GEOM_FLAT ||
        params.drawMode == UsdImagingGLDrawMode::DRAW_SHADED_FLAT) {
        // Flat shading
        reprSelector = HdReprSelector(HdReprTokens->hull);
    } else if (
        params.drawMode == UsdImagingGLDrawMode::DRAW_WIREFRAME_ON_SURFACE) {
        // Wireframe on surface
        reprSelector = HdReprSelector(refined ?
            HdReprTokens->refinedWireOnSurf : HdReprTokens->wireOnSurf);
    } else if (params.drawMode == UsdImagingGLDrawMode::DRAW_WIREFRAME) {
        // Wireframe
        reprSelector = HdReprSelector(refined ?
            HdReprTokens->refinedWire : HdReprTokens->wire);
    } else {
        // Smooth shading
        reprSelector = HdReprSelector(refined ?
            HdReprTokens->refined : HdReprTokens->smoothHull);
    }
    
    // Calculate the rendertags needed based on the parameters passed by
    // the application
    renderTags->clear();
    renderTags->push_back(HdTokens->geometry);
    if (params.showGuides) {
        renderTags->push_back(HdxRenderTagsTokens->guide);
    }
    if (params.showProxy) {
        renderTags->push_back(_tokens->proxy);
    }
    if (params.showRender) {
        renderTags->push_back(_tokens->render);
    }
    
    // By default our main collection will be called geometry
    TfToken colName = HdTokens->geometry;
    
    // Check if the collection needs to be updated (so we can avoid the sort).
    SdfPathVector const& oldRoots = collection->GetRootPaths();
    
    // inexpensive comparison first
    bool match = collection->GetName() == colName &&
        oldRoots.size() == roots.size() &&
        collection->GetReprSelector() == reprSelector &&
        collection->GetRenderTags().size() == renderTags->size();
    
    // Only take the time to compare root paths if everything else matches.
    if (match) {
        // Note that oldRoots is guaranteed to be sorted.
        for(size_t i = 0; i < roots.size(); i++) {
            // Avoid binary search when both vectors are sorted.
            if (oldRoots[i] == roots[i])
                continue;
            // Binary search to find the current root.
            if (!std::binary_search(oldRoots.begin(), oldRoots.end(), roots[i]))
            {
                match = false;
                break;
            }
        }
        
        // Compare if rendertags match
        if (*renderTags != collection->GetRenderTags()) {
            match = false;
        }
        
        // if everything matches, do nothing.
        if (match) return false;
    }
    
    // Recreate the collection.
    *collection = HdRprimCollection(colName, reprSelector);
    collection->SetRootPaths(roots);
    collection->SetRenderTags(*renderTags);
    
    return true;
}

/* static */
HdxRenderTaskParams
UsdImagingGLMetalHdEngine::_MakeHydraUsdImagingGLRenderParams(
                  const UsdImagingGLRenderParams& renderParams)
{
    static const HdCullStyle USD_2_HD_CULL_STYLE[] =
    {
        HdCullStyleDontCare,              // Cull No Opinion (unused)
        HdCullStyleNothing,               // CULL_STYLE_NOTHING,
        HdCullStyleBack,                  // CULL_STYLE_BACK,
        HdCullStyleFront,                 // CULL_STYLE_FRONT,
        HdCullStyleBackUnlessDoubleSided, // CULL_STYLE_BACK_UNLESS_DOUBLE_SIDED
    };
    static_assert(((sizeof(USD_2_HD_CULL_STYLE) /
                    sizeof(USD_2_HD_CULL_STYLE[0]))
                   == (size_t)UsdImagingGLCullStyle::CULL_STYLE_COUNT),"enum size mismatch");
    
    HdxRenderTaskParams params;
    
    params.overrideColor       = renderParams.overrideColor;
    params.wireframeColor      = renderParams.wireframeColor;
    
    if (renderParams.drawMode == UsdImagingGLDrawMode::DRAW_GEOM_ONLY ||
        renderParams.drawMode == UsdImagingGLDrawMode::DRAW_POINTS) {
        params.enableLighting = false;
    } else {
        params.enableLighting =  renderParams.enableLighting &&
                                !renderParams.enableIdRender;
    }
    
    params.enableIdRender      = renderParams.enableIdRender;
    params.depthBiasUseDefault = true;
    params.depthFunc           = HdCmpFuncLess;
    params.cullStyle           = USD_2_HD_CULL_STYLE[
        (size_t)renderParams.cullStyle];
    // 32.0 is the default tessLevel of HdRasterState. we can change if we like.
    params.tessLevel           = 32.0;
    
    const float tinyThreshold = 0.9f;
    params.drawingRange = GfVec2f(tinyThreshold, -1.0f);
    
    // Decrease the alpha threshold if we are using sample alpha to
    // coverage.
    if (renderParams.alphaThreshold < 0.0) {
        params.alphaThreshold =
            renderParams.enableSampleAlphaToCoverage ? 0.1f : 0.5f;
    } else {
        params.alphaThreshold =
            renderParams.alphaThreshold;
    }
    
    params.enableSceneMaterials = renderParams.enableSceneMaterials;
    
    // Leave default values for:
    // - params.geomStyle
    // - params.complexity
    // - params.hullVisibility
    // - params.surfaceVisibility
    
    // We don't provide the following because task controller ignores them:
    // - params.camera
    // - params.viewport
    
    return params;
}

/*virtual*/
void
UsdImagingGLMetalHdEngine::RenderBatch(const SdfPathVector& paths,
                                       const UsdImagingGLRenderParams& params)
{
    _taskController->SetCameraClipPlanes(params.clipPlanes);
    _UpdateHydraCollection(&_renderCollection, paths, params, &_renderTags);
    _taskController->SetCollection(_renderCollection);
    
    HdxRenderTaskParams hdParams = _MakeHydraUsdImagingGLRenderParams(params);
    _taskController->SetRenderParams(hdParams);
    _taskController->SetEnableSelection(params.highlight);
    
    Render(params);
}

/*virtual*/
void
UsdImagingGLMetalHdEngine::Render(const UsdPrim& root,
                                const UsdImagingGLRenderParams& params)
{
    PrepareBatch(root, params);
    
    SdfPath rootPath = _delegate->GetPathForIndex(root.GetPath());
    SdfPathVector roots(1, rootPath);
    
    _taskController->SetCameraClipPlanes(params.clipPlanes);
    _UpdateHydraCollection(&_renderCollection, roots, params, &_renderTags);
    _taskController->SetCollection(_renderCollection);
    
    HdxRenderTaskParams hdParams = _MakeHydraUsdImagingGLRenderParams(params);
    _taskController->SetRenderParams(hdParams);
    _taskController->SetEnableSelection(params.highlight);
    
    Render(params);
}

bool
UsdImagingGLMetalHdEngine::TestIntersection(
    const GfMatrix4d &viewMatrix,
    const GfMatrix4d &inProjectionMatrix,
    const GfMatrix4d &worldToLocalSpace,
    const UsdPrim& root, 
    const UsdImagingGLRenderParams& params,
    GfVec3d *outHitPoint,
    SdfPath *outHitPrimPath,
    SdfPath *outHitInstancerPath,
    int *outHitInstanceIndex,
    int *outHitElementIndex)
{
    GfMatrix4d projectionMatrix;
    static GfMatrix4d zTransform;
    
    // Transform from [-1, 1] to [0, 1] clip space
    static bool _zTransformSet = false;
    if (!_zTransformSet) {
        _zTransformSet = true;
        zTransform.SetIdentity();
        zTransform.SetScale(GfVec3d(1.0, 1.0, 0.5));
        zTransform.SetTranslateOnly(GfVec3d(0.0, 0.0, 0.5));
    }
    
    projectionMatrix = inProjectionMatrix * zTransform;

    SdfPath rootPath = _delegate->GetPathForIndex(root.GetPath());
    SdfPathVector roots(1, rootPath);
    _UpdateHydraCollection(&_intersectCollection, roots, params, &_renderTags);

    HdxIntersector::HitVector allHits;
    HdxIntersector::Params qparams;
    qparams.viewMatrix = worldToLocalSpace * viewMatrix;
    qparams.projectionMatrix = projectionMatrix;
    qparams.alphaThreshold = params.alphaThreshold;
    qparams.renderTags = _renderTags;
    qparams.cullStyle = HdCullStyleNothing;
    qparams.enableSceneMaterials = params.enableSceneMaterials;

    MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();
    
    //MTLCaptureManager *sharedCaptureManager = [MTLCaptureManager sharedCaptureManager];
    //[sharedCaptureManager startCaptureWithScope:_captureScope];
    [_captureScope beginScope];

    bool success = _taskController->TestIntersection(
         &_engine,
         _intersectCollection,
         qparams,
         HdxIntersectionModeTokens->nearest,
         &allHits);

    [_captureScope endScope];

    if (!success) {
        return false;
    }

    // Since we are in nearest-hit mode, and TestIntersection
    // returned true, we know allHits has a single point in it.
    TF_VERIFY(allHits.size() == 1);

    HdxIntersector::Hit &hit = allHits[0];

    if (outHitPoint) {
        *outHitPoint = GfVec3d(hit.worldSpaceHitPoint[0],
                               hit.worldSpaceHitPoint[1],
                               hit.worldSpaceHitPoint[2]);
    }
    if (outHitPrimPath) {
        *outHitPrimPath = hit.objectId;
    }
    if (outHitInstancerPath) {
        *outHitInstancerPath = hit.instancerId;
    }
    if (outHitInstanceIndex) {
        *outHitInstanceIndex = hit.instanceIndex;
    }
    if (outHitElementIndex) {
        *outHitElementIndex = hit.elementIndex;
    }

    return true;
}

bool
UsdImagingGLMetalHdEngine::TestIntersectionBatch(
    const GfMatrix4d &viewMatrix,
    const GfMatrix4d &inProjectionMatrix,
    const GfMatrix4d &worldToLocalSpace,
    const SdfPathVector& paths, 
    const UsdImagingGLRenderParams& params,
    unsigned int pickResolution,
    PathTranslatorCallback pathTranslator,
    HitBatch *outHit)
{
    GfMatrix4d projectionMatrix;
    static GfMatrix4d zTransform;
    
    // Transform from [-1, 1] to [0, 1] clip space
    static bool _zTransformSet = false;
    if (!_zTransformSet) {
        _zTransformSet = true;
        zTransform.SetIdentity();
        zTransform.SetScale(GfVec3d(1.0, 1.0, 0.5));
        zTransform.SetTranslateOnly(GfVec3d(0.0, 0.0, 0.5));
    }
    
    projectionMatrix = inProjectionMatrix * zTransform;

    _UpdateHydraCollection(&_intersectCollection, paths, params, &_renderTags);

    static const HdCullStyle USD_2_HD_CULL_STYLE[] =
    {
        HdCullStyleDontCare,              // No opinion, unused
        HdCullStyleNothing,               // CULL_STYLE_NOTHING,
        HdCullStyleBack,                  // CULL_STYLE_BACK,
        HdCullStyleFront,                 // CULL_STYLE_FRONT,
        HdCullStyleBackUnlessDoubleSided, // CULL_STYLE_BACK_UNLESS_DOUBLE_SIDED
    };
    static_assert(((sizeof(USD_2_HD_CULL_STYLE) / 
                    sizeof(USD_2_HD_CULL_STYLE[0])) 
                == (size_t)UsdImagingGLCullStyle::CULL_STYLE_COUNT),"enum size mismatch");

    HdxIntersector::HitVector allHits;
    HdxIntersector::Params qparams;
    qparams.viewMatrix = worldToLocalSpace * viewMatrix;
    qparams.projectionMatrix = projectionMatrix;
    qparams.alphaThreshold = params.alphaThreshold;
    qparams.cullStyle = USD_2_HD_CULL_STYLE[
        (size_t)params.cullStyle];
    qparams.renderTags = _renderTags;
    qparams.enableSceneMaterials = params.enableSceneMaterials;

    MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();
//    MTLCaptureManager *sharedCaptureManager = [MTLCaptureManager sharedCaptureManager];
//    [sharedCaptureManager startCaptureWithScope:_captureScope];
    [_captureScope beginScope];

    _taskController->SetPickResolution(pickResolution);
    bool success = _taskController->TestIntersection(
         &_engine,
         _intersectCollection,
         qparams,
         HdxIntersectionModeTokens->unique,
         &allHits);

    [_captureScope endScope];

    if (!success) {
        return false;
    }

    if (!outHit) {
        return true;
    }

    for (const HdxIntersector::Hit& hit : allHits) {
        const SdfPath primPath = hit.objectId;
        const SdfPath instancerPath = hit.instancerId;
        const int instanceIndex = hit.instanceIndex;

        HitInfo& info = (*outHit)[pathTranslator(primPath, instancerPath,
            instanceIndex)];
        info.worldSpaceHitPoint = GfVec3d(hit.worldSpaceHitPoint[0],
                                          hit.worldSpaceHitPoint[1],
                                          hit.worldSpaceHitPoint[2]);
        info.hitInstanceIndex = instanceIndex;
    }

    return true;
}

class _DebugGroupTaskWrapper : public HdTask {
    const HdTaskSharedPtr _task;
    public:
    _DebugGroupTaskWrapper(const HdTaskSharedPtr task)
    : _task(task)
    {
    }
    
    void
    _Execute(HdTaskContext* ctx) override
    {
        _task->Execute(ctx);
    }
    
    void
    _Sync(HdTaskContext* ctx) override
    {
        _task->Sync(ctx);
    }
};

void
UsdImagingGLMetalHdEngine::Render(const UsdImagingGLRenderParams& params)
{
    // Forward scene materials enable option to delegate
    _delegate->SetSceneMaterialsEnabled(params.enableSceneMaterials);

    MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();

    //MTLCaptureManager *sharedCaptureManager = [MTLCaptureManager sharedCaptureManager];
    //[sharedCaptureManager startCaptureWithScope:_captureScope];
    [_captureScope beginScope];

#if defined(ARCH_GFX_OPENGL)
    // Make sure the Metal render targets, and GL interop textures match the GL viewport size
    GLint viewport[4];
    glGetIntegerv( GL_VIEWPORT, viewport );

    if (context->mtlColorTexture.width != viewport[2] ||
        context->mtlColorTexture.height != viewport[3]) {
        context->AllocateAttachments(viewport[2], viewport[3]);
    }

    if (_renderOutput == RenderOutput::OpenGL) {
        if (_mtlRenderPassDescriptorForInterop == nil)
            _mtlRenderPassDescriptorForInterop = [[MTLRenderPassDescriptor alloc] init];
        
        //Set this state every frame because it may have changed during rendering.
        
        // create a color attachment every frame since we have to recreate the texture every frame
        MTLRenderPassColorAttachmentDescriptor *colorAttachment = _mtlRenderPassDescriptorForInterop.colorAttachments[0];

        // make sure to clear every frame for best performance
        colorAttachment.loadAction = MTLLoadActionClear;
    
        // store only attachments that will be presented to the screen, as in this case
        colorAttachment.storeAction = MTLStoreActionStore;

        MTLRenderPassDepthAttachmentDescriptor *depthAttachment = _mtlRenderPassDescriptorForInterop.depthAttachment;
        depthAttachment.loadAction = MTLLoadActionClear;
        depthAttachment.storeAction = MTLStoreActionStore;
        depthAttachment.clearDepth = 1.0f;
        
        colorAttachment.texture = context->mtlColorTexture;

        GLfloat clearColor[4];
        glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor);
        clearColor[3] = 1.0f;
    
        colorAttachment.clearColor = MTLClearColorMake(clearColor[0],
                                                       clearColor[1],
                                                       clearColor[2],
                                                       clearColor[3]);
        depthAttachment.texture = context->mtlDepthTexture;
        
        _mtlRenderPassDescriptor = _mtlRenderPassDescriptorForInterop;
    }
    else
#else
    if (false) {}
    else
#endif
    {
        if (_mtlRenderPassDescriptor == nil) {
            TF_FATAL_CODING_ERROR("SetMetalRenderPassDescriptor must be called prior "
                                  "to rendering when render output is set to Metal");
        }
    }

    context->StartFrame();
    
    // Create a new command buffer for each render pass to the current drawable
    context->CreateCommandBuffer(METALWORKQUEUE_DEFAULT);
    context->LabelCommandBuffer(@"HdEngine::Render", METALWORKQUEUE_DEFAULT);
    
    // Set the render pass descriptor to use for the render encoders
    context->SetRenderPassDescriptor(_mtlRenderPassDescriptor);
    if (_renderOutput == RenderOutput::Metal) {
        _mtlRenderPassDescriptor = nil;
    }
    // hydra orients all geometry during topological processing so that
    // front faces have ccw winding. We disable culling because culling
    // is handled by fragment shader discard.
    if (params.flipFrontFacing) {
        context->SetFrontFaceWinding(MTLWindingClockwise);
    } else {
        context->SetFrontFaceWinding(MTLWindingCounterClockwise);
    }
    context->SetCullMode(MTLCullModeNone);

    if (params.applyRenderState) {
        // drawmode.
        // XXX: Temporary solution until shader-based styling implemented.
        switch (params.drawMode) {
            case UsdImagingGLDrawMode::DRAW_POINTS:
                context->SetTempPointWorkaround(true);
                break;
            default:
                context->SetPolygonFillMode(MTLTriangleFillModeFill);
                context->SetTempPointWorkaround(false);
                break;
        }
    }

    VtValue selectionValue(_selTracker);
    _engine.SetTaskContextData(HdxTokens->selectionState, selectionValue);
    VtValue renderTags(_renderTags);
    _engine.SetTaskContextData(HdxTokens->renderTags, renderTags);
    
    HdTaskSharedPtrVector tasks;
    
    if (false) {
        tasks = _taskController->GetTasks();
    } else {
        TF_FOR_ALL(it, _taskController->GetTasks()) {
            tasks.push_back(boost::make_shared<_DebugGroupTaskWrapper>(*it));
        }
    }
    _engine.Execute(*_renderIndex, tasks);
   
    if (_renderOutput == RenderOutput::OpenGL) {
        // Depth texture copy
        context->CopyDepthTextureToOpenGL();
    }

    if (context->GeometryShadersActive()) {
        // Complete the GS command buffer if we have one
        context->CommitCommandBuffer(true, false, METALWORKQUEUE_GEOMETRY_SHADER);
    }

    // Commit the render buffer (will wait for GS to complete if present)
    // We wait until scheduled, because we're about to consume the Metal
    // generated textures in an OpenGL blit
    context->CommitCommandBuffer(_renderOutput == RenderOutput::OpenGL, false);
    
    context->EndFrame();
    
    // Finalize rendering here & push the command buffer to the GPU
    [_captureScope endScope];

    if (_renderOutput == RenderOutput::OpenGL) {
        context->BlitColorTargetToOpenGL();
        GLF_POST_PENDING_GL_ERRORS();
    }

    return;
}

/*virtual*/
void 
UsdImagingGLMetalHdEngine::SetCameraState(const GfMatrix4d& viewMatrix,
                            const GfMatrix4d& projectionMatrix,
                            const GfVec4d& viewport)
{
    GfMatrix4d modifiedProjMatrix;
    static GfMatrix4d zTransform;
    
    // Transform from [-1, 1] to [0, 1] clip space
    static bool _zTransformSet = false;
    if (!_zTransformSet) {
        _zTransformSet = true;
        zTransform.SetIdentity();
        zTransform.SetScale(GfVec3d(1.0, 1.0, 0.5));
        zTransform.SetTranslateOnly(GfVec3d(0.0, 0.0, 0.5));
    }
    
    modifiedProjMatrix = projectionMatrix * zTransform;

    // usdview passes these matrices from OpenGL state.
    // update the camera in the task controller accordingly.
    _taskController->SetCameraMatrices(viewMatrix, modifiedProjMatrix);
    _taskController->SetCameraViewport(viewport);
}

/*virtual*/
SdfPath
UsdImagingGLMetalHdEngine::GetRprimPathFromPrimId(int primId) const
{
    return _delegate->GetRenderIndex().GetRprimPathFromPrimId(primId);
}

/* virtual */
SdfPath
UsdImagingGLMetalHdEngine::GetPrimPathFromInstanceIndex(
    SdfPath const& protoPrimPath,
    int instanceIndex,
    int *absoluteInstanceIndex,
    SdfPath * rprimPath,
    SdfPathVector *instanceContext)
{
    return _delegate->GetPathForInstanceIndex(protoPrimPath, instanceIndex,
                                             absoluteInstanceIndex, rprimPath,
                                             instanceContext);
}

/* virtual */
void
UsdImagingGLMetalHdEngine::SetLightingStateFromOpenGL()
{
    if (!_lightingContextForOpenGLState) {
        _lightingContextForOpenGLState = GarchSimpleLightingContext::New();
    }
    _lightingContextForOpenGLState->SetStateFromOpenGL();
    
    _taskController->SetLightingState(_lightingContextForOpenGLState);
}

/* virtual */
void
UsdImagingGLMetalHdEngine::SetLightingState(GarchSimpleLightVector const &lights,
                                          GarchSimpleMaterial const &material,
                                          GfVec4f const &sceneAmbient)
{
    // we still use _lightingContextForOpenGLState for convenience, but
    // set the values directly.
    if (!_lightingContextForOpenGLState) {
        _lightingContextForOpenGLState = GarchSimpleLightingContext::New();
    }
    _lightingContextForOpenGLState->SetLights(lights);
    _lightingContextForOpenGLState->SetMaterial(material);
    _lightingContextForOpenGLState->SetSceneAmbient(sceneAmbient);
    _lightingContextForOpenGLState->SetUseLighting(lights.size() > 0);
    
    _taskController->SetLightingState(_lightingContextForOpenGLState);
}

/* virtual */
void
UsdImagingGLMetalHdEngine::SetLightingState(GarchSimpleLightingContextPtr const &src)
{
    _taskController->SetLightingState(src);
}

/* virtual */
void
UsdImagingGLMetalHdEngine::SetRootTransform(GfMatrix4d const& xf)
{
    _delegate->SetRootTransform(xf);
}

/* virtual */
void
UsdImagingGLMetalHdEngine::SetRootVisibility(bool isVisible)
{
    _delegate->SetRootVisibility(isVisible);
}


/*virtual*/
void
UsdImagingGLMetalHdEngine::SetSelected(SdfPathVector const& paths)
{
    // populate new selection
    HdSelectionSharedPtr selection(new HdSelection);
    // XXX: Usdview currently supports selection on click. If we extend to
    // rollover (locate) selection, we need to pass that mode here.
    HdSelection::HighlightMode mode = HdSelection::HighlightModeSelect;
    for (SdfPath const& path : paths) {
        _delegate->PopulateSelection(mode,
                                     path,
                                     UsdImagingDelegate::ALL_INSTANCES,
                                     selection);
    }

    // set the result back to selection tracker
    _selTracker->SetSelection(selection);
}

/*virtual*/
void
UsdImagingGLMetalHdEngine::ClearSelected()
{
    HdSelectionSharedPtr selection(new HdSelection);
    _selTracker->SetSelection(selection);
}

/* virtual */
void
UsdImagingGLMetalHdEngine::AddSelected(SdfPath const &path, int instanceIndex)
{
    HdSelectionSharedPtr selection = _selTracker->GetSelectionMap();
    if (!selection) {
        selection.reset(new HdSelection);
    }
    // XXX: Usdview currently supports selection on click. If we extend to
    // rollover (locate) selection, we need to pass that mode here.
    HdSelection::HighlightMode mode = HdSelection::HighlightModeSelect;
    _delegate->PopulateSelection(mode, path, instanceIndex, selection);

    // set the result back to selection tracker
    _selTracker->SetSelection(selection);
}

/*virtual*/
void
UsdImagingGLMetalHdEngine::SetSelectionColor(GfVec4f const& color)
{
    _selectionColor = color;
    _taskController->SetSelectionColor(_selectionColor);
}

/* virtual */
bool
UsdImagingGLMetalHdEngine::IsConverged() const
{
    return _taskController->IsConverged();
}

/* virtual */
TfTokenVector
UsdImagingGLMetalHdEngine::GetRendererPlugins() const
{
    HfPluginDescVector pluginDescriptors;
    HdxRendererPluginRegistry::GetInstance().GetPluginDescs(&pluginDescriptors);

#if defined(ARCH_OS_OSX)
    NSArray<id<MTLDevice>> *_deviceList = MTLCopyAllDevices();
#else
    NSMutableArray<id<MTLDevice>> *_deviceList = [[NSMutableArray alloc] init];
    [_deviceList addObject:MTLCreateSystemDefaultDevice()];
#endif

    TfTokenVector plugins;
    
    if (pluginDescriptors.size() != 1) {
        TF_FATAL_CODING_ERROR("There should only be one plugin!");
    }

    for (id<MTLDevice> dev in _deviceList) {
        plugins.push_back(TfToken(_MetalPluginDescriptor(dev)));
    }

    return plugins;
}

/* virtual */
std::string
UsdImagingGLMetalHdEngine::GetRendererDisplayName(TfToken const &pluginId) const
{
    return pluginId;
}

/* virtual */
TfToken
UsdImagingGLMetalHdEngine::GetCurrentRendererId() const
{
    return _rendererId;
}

TfToken
UsdImagingGLMetalHdEngine::GetDefaultRendererPluginId()
{
    std::string defaultRendererDisplayName =
        TfGetenv("HD_DEFAULT_RENDERER", "");
    
    if (defaultRendererDisplayName.empty()) {
        return TfToken();
    }
    
    HfPluginDescVector pluginDescs;
    HdxRendererPluginRegistry::GetInstance().GetPluginDescs(&pluginDescs);
    
    // Look for the one with the matching display name
    for (size_t i = 0; i < pluginDescs.size(); ++i) {
        if (pluginDescs[i].displayName == defaultRendererDisplayName) {
            return pluginDescs[i].id;
        }
    }
    
    TF_WARN("Failed to find default renderer with display name '%s'.",
            defaultRendererDisplayName.c_str());
    
    return TfToken();
}

/* static */
bool
UsdImagingGLMetalHdEngine::IsDefaultRendererPluginAvailable()
{
    HfPluginDescVector descs;
    HdxRendererPluginRegistry::GetInstance().GetPluginDescs(&descs);
    return !descs.empty();
}

/* virtual */
bool
UsdImagingGLMetalHdEngine::SetRendererPlugin(TfToken const &pluginId)
{
    HdxRendererPlugin *plugin = nullptr;
    TfToken actualId = pluginId;
    bool forceReload = false;
    
    // Special case: TfToken() selects the first plugin in the list.
    if (actualId.IsEmpty()) {
        actualId = HdxRendererPluginRegistry::GetInstance().
            GetDefaultPluginId();
    }
    else {
#if defined(ARCH_OS_OSX)
        NSArray<id<MTLDevice>> *_deviceList = MTLCopyAllDevices();
#else
        NSMutableArray<id<MTLDevice>> *_deviceList = [[NSMutableArray alloc] init];
        [_deviceList addObject:MTLCreateSystemDefaultDevice()];
#endif

        for (id<MTLDevice> dev in _deviceList) {
            if (pluginId == _MetalPluginDescriptor(dev))
            {
                actualId = HdxRendererPluginRegistry::GetInstance().
                    GetDefaultPluginId();

                if (dev != MtlfMetalContext::GetMetalContext()->device) {
                    // Tear it down and bring it back up with the new Metal device
                    forceReload = true;
                    
                    // Recreate the underlying Metal context
                    MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();
                    MtlfMetalContext::RecreateInstance(dev, context->mtlColorTexture.width, context->mtlColorTexture.height);
                    
                    //Also recreate a capture scope with the new device
                    _InitializeCapturing();
                }
                break;
            }
        }
    }
    plugin = HdxRendererPluginRegistry::GetInstance().
        GetRendererPlugin(actualId);
    
    if (plugin == nullptr) {
        TF_CODING_ERROR("Couldn't find plugin for id %s", actualId.GetText());
        return false;
    } else if (plugin == _rendererPlugin) {
        if (!forceReload) {
            // It's a no-op to load the same plugin twice.
            HdxRendererPluginRegistry::GetInstance().ReleasePlugin(plugin);
            return true;
        }
    } else if (!plugin->IsSupported()) {
        // Don't do anything if the plugin isn't supported on the running
        // system, just return that we're not able to set it.
        HdxRendererPluginRegistry::GetInstance().ReleasePlugin(plugin);
        return false;
    }
    
    // Pull old delegate/task controller state.
    GfMatrix4d rootTransform = GfMatrix4d(1.0);
    bool isVisible = true;
    if (_delegate != nullptr) {
        rootTransform = _delegate->GetRootTransform();
        isVisible = _delegate->GetRootVisibility();
    }
    HdSelectionSharedPtr selection = _selTracker->GetSelectionMap();
    if (!selection) {
        selection.reset(new HdSelection);
    }
    
    // Delete hydra state.
    _DeleteHydraResources();
    
    // Recreate the render index.
    _rendererPlugin = plugin;
    _rendererId = TfToken(_MetalPluginDescriptor(MtlfMetalContext::GetMetalContext()->device));
    
    HdRenderDelegate *renderDelegate = _rendererPlugin->CreateRenderDelegate();
    _renderIndex = HdRenderIndex::New(renderDelegate);
    
    // Create the new delegate & task controller.
    _delegate = new UsdImagingDelegate(_renderIndex, _delegateID);
    _isPopulated = false;
    
    _taskController = new HdxTaskController(_renderIndex,
        _delegateID.AppendChild(TfToken(TfStringPrintf(
           "_UsdImaging_%s_%p",
           TfMakeValidIdentifier(actualId.GetText()).c_str(),
           this))));
    
    // Rebuild state in the new delegate/task controller.
    _delegate->SetRootVisibility(isVisible);
    _delegate->SetRootTransform(rootTransform);
    _selTracker->SetSelection(selection);
    _taskController->SetSelectionColor(_selectionColor);
    
    return true;
}

void
UsdImagingGLMetalHdEngine::_InitializeCapturing()
{
    if(_sharedCaptureManager == nil)
        _sharedCaptureManager = [MTLCaptureManager sharedCaptureManager];
    else
        [_sharedCaptureManager.defaultCaptureScope release];
    
    if (_captureScope) {
        [_captureScope release];
    }
    _captureScope = [_sharedCaptureManager newCaptureScopeWithDevice:MtlfMetalContext::GetMetalContext()->device];
    _captureScope.label = @"Hydra Capture Scope";
    if (_renderOutput == RenderOutput::OpenGL) {
        _sharedCaptureManager.defaultCaptureScope = _captureScope;
    }
}

void
UsdImagingGLMetalHdEngine::_DeleteHydraResources()
{
    // Unwinding order: remove data sources first (task controller, scene
    // delegate); then render index; then render delegate; finally the
    // renderer plugin used to manage the render delegate.
    
    if (_taskController != nullptr) {
        delete _taskController;
        _taskController = nullptr;
    }
    if (_delegate != nullptr) {
        delete _delegate;
        _delegate = nullptr;
    }
    HdRenderDelegate *renderDelegate = nullptr;
    if (_renderIndex != nullptr) {
        renderDelegate = _renderIndex->GetRenderDelegate();
        delete _renderIndex;
        _renderIndex = nullptr;
    }
    if (_rendererPlugin != nullptr) {
        if (renderDelegate != nullptr) {
            _rendererPlugin->DeleteRenderDelegate(renderDelegate);
        }
        HdxRendererPluginRegistry::GetInstance().ReleasePlugin(_rendererPlugin);
        _rendererPlugin = nullptr;
        _rendererId = TfToken();
    }
    
    if (_mtlRenderPassDescriptorForInterop != nil) {
        [_mtlRenderPassDescriptorForInterop release];
        _mtlRenderPassDescriptorForInterop = NULL;
    }
}

/* virtual */
TfTokenVector
UsdImagingGLMetalHdEngine::GetRendererAovs() const
{
    if (_renderIndex->IsBprimTypeSupported(HdPrimTypeTokens->renderBuffer)) {
        return TfTokenVector(
             { HdAovTokens->color,
                 HdAovTokens->primId,
                 HdAovTokens->depth,
                 HdAovTokens->normal,
                 HdAovTokensMakePrimvar(TfToken("st")) }
             );
    }
    return TfTokenVector();
}

/* virtual */
bool
UsdImagingGLMetalHdEngine::SetRendererAov(TfToken const& id)
{
    if (_renderIndex->IsBprimTypeSupported(HdPrimTypeTokens->renderBuffer)) {
        // For color, render straight to the viewport instead of rendering
        // to an AOV and colorizing (which is the same, but more work).
        if (id == HdAovTokens->color) {
            _taskController->SetRenderOutputs(TfTokenVector());
        } else {
            _taskController->SetRenderOutputs({id});
        }
        return true;
    }
    return false;
}

/* virtual */
VtDictionary
UsdImagingGLMetalHdEngine::GetResourceAllocation() const
{
    return _renderIndex->GetResourceRegistry()->GetResourceAllocation();
}

/* virtual */
UsdImagingGLRendererSettingsList
UsdImagingGLMetalHdEngine::GetRendererSettingsList() const
{
    HdRenderSettingDescriptorList descriptors =
    _renderIndex->GetRenderDelegate()->GetRenderSettingDescriptors();
    UsdImagingGLRendererSettingsList ret;
    
    for (auto const& desc : descriptors) {
        UsdImagingGLRendererSetting r;
        r.key = desc.key;
        r.name = desc.name;
        r.defValue = desc.defaultValue;
        
        // Use the type of the default value to tell us what kind of
        // widget to create...
        if (r.defValue.IsHolding<bool>()) {
            r.type = UsdImagingGLRendererSetting::TYPE_FLAG;
        } else if (r.defValue.IsHolding<int>() ||
                   r.defValue.IsHolding<unsigned int>()) {
            r.type = UsdImagingGLRendererSetting::TYPE_INT;
        } else if (r.defValue.IsHolding<float>()) {
            r.type = UsdImagingGLRendererSetting::TYPE_FLOAT;
        } else if (r.defValue.IsHolding<std::string>()) {
            r.type = UsdImagingGLRendererSetting::TYPE_STRING;
        } else {
            TF_WARN("Setting '%s' with type '%s' doesn't have a UI"
                    " implementation...",
                    r.name.c_str(),
                    r.defValue.GetTypeName().c_str());
            continue;
        }
        ret.push_back(r);
    }
    
    return ret;
}

/* virtual */
VtValue
UsdImagingGLMetalHdEngine::GetRendererSetting(TfToken const& id) const
{
    return _renderIndex->GetRenderDelegate()->GetRenderSetting(id);
}

/* virtual */
void
UsdImagingGLMetalHdEngine::SetRendererSetting(TfToken const& id,
                                            VtValue const& value)
{
    _renderIndex->GetRenderDelegate()->SetRenderSetting(id, value);
}

void
UsdImagingGLMetalHdEngine::SetMetalRenderPassDescriptor(
    MTLRenderPassDescriptor *renderPassDescriptor)
{
    if (_renderOutput == RenderOutput::OpenGL) {
        TF_CODING_ERROR("SetMetalRenderPassDescriptor isn't valid to call "
                        "when using OpenGL as the output target");
        return;
    }
    _mtlRenderPassDescriptorForNativeMetal = [renderPassDescriptor copy];
    _mtlRenderPassDescriptor = _mtlRenderPassDescriptorForNativeMetal;
}

PXR_NAMESPACE_CLOSE_SCOPE

