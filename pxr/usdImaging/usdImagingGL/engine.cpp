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
#include "pxr/usdImaging/usdImagingGL/engine.h"

#if defined(ARCH_GFX_OPENGL)
#include "pxr/imaging/hdSt/GL/resourceFactoryGL.h"
#include "pxr/usdImaging/usdImagingGL/legacyEngine.h"
#endif

#if defined(ARCH_GFX_METAL)
#include "pxr/imaging/mtlf/mtlDevice.h"
#include "pxr/imaging/hdSt/Metal/resourceFactoryMetal.h"
#endif

#include "pxr/usdImaging/usdImaging/delegate.h"

#include "pxr/usd/usdGeom/tokens.h"
#include "pxr/usd/usdGeom/camera.h"

#include "pxr/imaging/garch/contextCaps.h"
#include "pxr/imaging/garch/resourceFactory.h"

#include "pxr/imaging/glf/diagnostic.h"
#include "pxr/imaging/glf/glContext.h"
#include "pxr/imaging/glf/info.h"

#include "pxr/imaging/hd/rendererPlugin.h"
#include "pxr/imaging/hd/rendererPluginRegistry.h"
#include "pxr/imaging/hdx/taskController.h"
#include "pxr/imaging/hdx/tokens.h"

#include "pxr/imaging/hdSt/tokens.h"

#include "pxr/base/tf/getenv.h"
#include "pxr/base/tf/stl.h"

#include "pxr/base/gf/matrix4d.h"
#include "pxr/base/gf/vec3d.h"

PXR_NAMESPACE_OPEN_SCOPE

namespace {

#if defined(ARCH_GFX_METAL)
std::mutex engineCountMutex;
int engineCount = 0;
#endif

static
bool
_GetHydraEnabledEnvVar()
{
    // XXX: Note that we don't cache the result here.  This is primarily because
    // of the way usdview currently interacts with this setting.  This should
    // be cleaned up, and the new class hierarchy around UsdImagingGLEngine
    // makes it much easier to do so.
    return TfGetenv("HD_ENABLED", "1") == "1";
}

static
void _InitGL()
{
#if defined(ARCH_GFX_OPENGL)
    static std::once_flag initFlag;

    std::call_once(initFlag, []{

        // Initialize Glew library for GL Extensions if needed
        GlfGlewInit();

        // Initialize if needed and switch to shared GL context.
        GlfSharedGLContextScopeHolder sharedContext;

        // Initialize GL context caps based on shared context
        GlfContextCaps::InitInstance();

    });
#endif
}

static
bool
_IsHydraEnabled(const UsdImagingGLEngine::RenderAPI api)
{
#if defined(ARCH_GFX_OPENGL)
    if (api == UsdImagingGLEngine::OpenGL) {
        // Make sure there is an OpenGL context when
        // trying to initialize Hydra/Reference
        GlfGLContextSharedPtr context = GlfGLContext::GetCurrentGLContext();
        if (!context || !context->IsValid()) {
            TF_CODING_ERROR("OpenGL context required, "
                "using reference renderer");
            return false;
        }
    }
#endif
    if (!_GetHydraEnabledEnvVar()) {
        return false;
    }

    // Check to see if we have a default plugin for the renderer
    TfToken defaultPlugin = 
        HdRendererPluginRegistry::GetInstance().GetDefaultPluginId();

    return !defaultPlugin.IsEmpty();
}

} // anonymous namespace

std::recursive_mutex UsdImagingGLEngine::ResourceFactoryGuard::contextLock;

UsdImagingGLEngine::ResourceFactoryGuard::ResourceFactoryGuard(HdStResourceFactoryInterface *resourceFactory) {
    contextLock.lock();
    GarchResourceFactory::GetInstance().SetResourceFactory(
        dynamic_cast<GarchResourceFactoryInterface*>(resourceFactory));
    HdStResourceFactory::GetInstance().SetResourceFactory(resourceFactory);
}

UsdImagingGLEngine::ResourceFactoryGuard::~ResourceFactoryGuard() {
    GarchResourceFactory::GetInstance().SetResourceFactory(NULL);
    HdStResourceFactory::GetInstance().SetResourceFactory(NULL);
    contextLock.unlock();
}

//----------------------------------------------------------------------------
// Global State
//----------------------------------------------------------------------------

/*static*/
bool
UsdImagingGLEngine::IsHydraEnabled()
{
    static bool isHydraEnabled = _IsHydraEnabled(Unset);
    return isHydraEnabled;
}

//----------------------------------------------------------------------------
// Construction
//----------------------------------------------------------------------------

UsdImagingGLEngine::UsdImagingGLEngine(const RenderAPI api)
    : _engine(NULL)
    , _renderIndex(nullptr)
    , _selTracker(new HdxSelectionTracker)
    , _delegateID(SdfPath::AbsoluteRootPath())
    , _delegate(nullptr)
    , _rendererPlugin(nullptr)
    , _taskController(nullptr)
    , _selectionColor(1.0f, 1.0f, 0.0f, 1.0f)
    , _rootPath(SdfPath::AbsoluteRootPath())
    , _excludedPrimPaths()
    , _invisedPrimPaths()
    , _isPopulated(false)
    , _renderAPI(api)
#if defined(ARCH_GFX_METAL)
    , _legacyImpl(nullptr)
#endif
{

#if defined(ARCH_GFX_METAL)
    engineCountMutex.lock();
    engineCount++;
#endif

    
    _engine = new HdEngine();
#if defined(ARCH_GFX_METAL)
    if (_renderAPI == Metal) {
        _resourceFactory = new HdStResourceFactoryMetal();
    }
    else
#endif
#if defined(ARCH_GFX_OPENGL)
    if (_renderAPI == OpenGL) {
    	_InitGL();

        _resourceFactory = new HdStResourceFactoryGL();
    }
    else
#endif
    {
        TF_FATAL_CODING_ERROR("No valid rendering API specified: %d", _renderAPI);
    }
    
    if (IsHydraEnabled()) {

        // _renderIndex, _taskController, and _delegate are initialized
        // by the plugin system.
        if (!SetRendererPlugin(GetDefaultRendererPluginId())) {
            TF_CODING_ERROR("No renderer plugins found! "
                            "Check before creation.");
        }

    } else {

        SdfPathVector excluded;
#if defined(ARCH_GFX_OPENGL)
        _legacyImpl.reset(new UsdImagingGLLegacyEngine(excluded));
#endif
    }

#if defined(ARCH_GFX_METAL)
    engineCountMutex.unlock();
#endif
}

UsdImagingGLEngine::UsdImagingGLEngine(
    const RenderAPI api,
    const SdfPath& rootPath,
    const SdfPathVector& excludedPaths,
    const SdfPathVector& invisedPaths,
    const SdfPath& delegateID)
    : _engine(NULL)
    , _renderIndex(nullptr)
    , _selTracker(new HdxSelectionTracker)
    , _delegateID(delegateID)
    , _delegate(nullptr)
    , _rendererPlugin(nullptr)
    , _taskController(nullptr)
    , _selectionColor(1.0f, 1.0f, 0.0f, 1.0f)
    , _rootPath(rootPath)
    , _excludedPrimPaths(excludedPaths)
    , _invisedPrimPaths(invisedPaths)
    , _isPopulated(false)
    , _renderAPI(api)
#if defined(ARCH_GFX_METAL)
    , _legacyImpl(nullptr)
#endif
{

#if defined(ARCH_GFX_METAL)
    engineCountMutex.lock();
    engineCount++;
#endif

    _engine = new HdEngine();
#if defined(ARCH_GFX_METAL)
    if (_renderAPI == Metal) {
        _resourceFactory = new HdStResourceFactoryMetal();
    }
    else
#endif
#if defined(ARCH_GFX_OPENGL)
    if (_renderAPI == OpenGL) {
		_InitGL();
        _resourceFactory = new HdStResourceFactoryGL();
    }
    else
#endif
    {
        TF_FATAL_CODING_ERROR("No valid rendering API specified: %d", _renderAPI);
    }

    if (IsHydraEnabled()) {

        // _renderIndex, _taskController, and _delegate are initialized
        // by the plugin system.
        if (!SetRendererPlugin(GetDefaultRendererPluginId())) {
            TF_CODING_ERROR("No renderer plugins found! "
                            "Check before creation.");
        }

    } else {

        // In the legacy implementation, both excluded paths and invised paths 
        // are treated the same way.
        SdfPathVector pathsToExclude = excludedPaths;
        pathsToExclude.insert(pathsToExclude.end(), 
            invisedPaths.begin(), invisedPaths.end());
#if defined(ARCH_GFX_OPENGL)
        _legacyImpl.reset(new UsdImagingGLLegacyEngine(pathsToExclude));
#endif
    }

#if defined(ARCH_GFX_METAL)
    engineCountMutex.unlock();
#endif
}

UsdImagingGLEngine::~UsdImagingGLEngine()
{
    {
        ResourceFactoryGuard guard(_resourceFactory);
        _DeleteHydraResources();
    }

    delete _engine;
    _engine = NULL;
    
    delete _resourceFactory;
    _resourceFactory = NULL;

#if defined(ARCH_GFX_METAL)
    engineCountMutex.lock();
    engineCount--;
    if (MtlfMetalContext::context && engineCount == 0)  {
        MtlfMetalContext::context = NULL;
    }
    engineCountMutex.unlock();
#endif
}

//----------------------------------------------------------------------------
// Rendering
//----------------------------------------------------------------------------

void
UsdImagingGLEngine::PrepareBatch(
    const UsdPrim& root, 
    const UsdImagingGLRenderParams& params)
{
    if (ARCH_UNLIKELY(_legacyImpl)) {
        return;
    }

    HD_TRACE_FUNCTION();

    TF_VERIFY(_delegate);

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

void
UsdImagingGLEngine::RenderBatch(
    const SdfPathVector& paths, 
    const UsdImagingGLRenderParams& params)
{
    if (ARCH_UNLIKELY(_legacyImpl)) {
        return;
    }

    ResourceFactoryGuard guard(_resourceFactory);

    TF_VERIFY(_taskController);

    _taskController->SetFreeCameraClipPlanes(params.clipPlanes);
    _UpdateHydraCollection(&_renderCollection, paths, params);
    _taskController->SetCollection(_renderCollection);

    TfTokenVector renderTags;
    _ComputeRenderTags(params, &renderTags);
    _taskController->SetRenderTags(renderTags);

    HdxRenderTaskParams hdParams = _MakeHydraUsdImagingGLRenderParams(params);

    _taskController->SetRenderParams(hdParams);
    _taskController->SetEnableSelection(params.highlight);

    SetColorCorrectionSettings(params.colorCorrectionMode, 
                               params.renderResolution);

    // XXX App sets the clear color via 'params' instead of setting up Aovs 
    // that has clearColor in their descriptor. So for now we must pass this
    // clear color to the color AOV.
    HdAovDescriptor colorAovDesc = 
        _taskController->GetRenderOutputSettings(HdAovTokens->color);
    if (colorAovDesc.format != HdFormatInvalid) {
        colorAovDesc.clearValue = VtValue(params.clearColor);
        _taskController->SetRenderOutputSettings(
            HdAovTokens->color, colorAovDesc);
    }

    // Forward scene materials enable option to delegate
    _delegate->SetSceneMaterialsEnabled(params.enableSceneMaterials);

    VtValue selectionValue(_selTracker);
    _engine->SetTaskContextData(HdxTokens->selectionState, selectionValue);
    _Execute(params, _taskController->GetRenderingTasks());
}

void 
UsdImagingGLEngine::Render(
    const UsdPrim& root, 
    const UsdImagingGLRenderParams &params)
{
#if defined(ARCH_GFX_METAL)
@autoreleasepool{
#endif

    if (ARCH_UNLIKELY(_legacyImpl)) {
#if defined(ARCH_GFX_OPENGL)
        return _legacyImpl->Render(root, params);
#endif
    }

    TF_VERIFY(_taskController);

    PrepareBatch(root, params);

    // XXX(UsdImagingPaths): Is it correct to map USD root path directly
    // to the cachePath here?
    SdfPath cachePath = root.GetPath();
    SdfPathVector paths(1, _delegate->ConvertCachePathToIndexPath(cachePath));

    RenderBatch(paths, params);
#if defined(ARCH_GFX_METAL)
}
#endif

}

void
UsdImagingGLEngine::InvalidateBuffers()
{
    if (ARCH_UNLIKELY(_legacyImpl)) {
#if defined(ARCH_GFX_OPENGL)
        return _legacyImpl->InvalidateBuffers();
#endif
    }
}

bool
UsdImagingGLEngine::IsConverged() const
{
    if (ARCH_UNLIKELY(_legacyImpl)) {
        return true;
    }

    TF_VERIFY(_taskController);
    return _taskController->IsConverged();
}

//----------------------------------------------------------------------------
// Root and Transform Visibility
//----------------------------------------------------------------------------

void
UsdImagingGLEngine::SetRootTransform(GfMatrix4d const& xf)
{
    if (ARCH_UNLIKELY(_legacyImpl)) {
        return;
    }

    TF_VERIFY(_delegate);
    _delegate->SetRootTransform(xf);
}

void
UsdImagingGLEngine::SetRootVisibility(bool isVisible)
{
    if (ARCH_UNLIKELY(_legacyImpl)) {
        return;
    }

    TF_VERIFY(_delegate);
    _delegate->SetRootVisibility(isVisible);
}

//----------------------------------------------------------------------------
// Camera and Light State
//----------------------------------------------------------------------------

void
UsdImagingGLEngine::SetRenderViewport(GfVec4d const& viewport)
{
    if (ARCH_UNLIKELY(_legacyImpl)) {
#if defined(ARCH_GFX_OPENGL)
        _legacyImpl->SetRenderViewport(viewport);
#endif
        return;
    }

    TF_VERIFY(_taskController);
    _taskController->SetRenderViewport(viewport);
}

void
UsdImagingGLEngine::SetWindowPolicy(CameraUtilConformWindowPolicy policy)
{
    if (ARCH_UNLIKELY(_legacyImpl)) {
#if defined(ARCH_GFX_OPENGL)
        _legacyImpl->SetWindowPolicy(policy);
#endif
        return;
    }

    TF_VERIFY(_taskController);
    // Note: Free cam uses SetCameraState, which expects the frustum to be
    // pre-adjusted for the viewport size.
    
    // The usdImagingDelegate manages the window policy for scene cameras.
    _delegate->SetWindowPolicy(policy);
}

void
UsdImagingGLEngine::SetCameraPath(SdfPath const& id)
{
    if (ARCH_UNLIKELY(_legacyImpl)) {
#if defined(ARCH_GFX_OPENGL)
        _legacyImpl->SetCameraPath(id);
#endif
        return;
    }

    TF_VERIFY(_taskController);
    _taskController->SetCameraPath(id);

    // The camera that is set for viewing will also be used for
    // time sampling.
    _delegate->SetCameraForSampling(id);
}

void 
UsdImagingGLEngine::SetCameraState(const GfMatrix4d& viewMatrix,
                                   const GfMatrix4d& projectionMatrix)
{
    if (ARCH_UNLIKELY(_legacyImpl)) {
#if defined(ARCH_GFX_OPENGL)
        _legacyImpl->SetFreeCameraMatrices(viewMatrix, projectionMatrix);
#endif
        return;
    }

#if defined(ARCH_GFX_METAL)
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
#else
    GfMatrix4d const &modifiedProjMatrix = projectionMatrix;
#endif

    TF_VERIFY(_taskController);
    _taskController->SetFreeCameraMatrices(viewMatrix, modifiedProjMatrix);
}

void
UsdImagingGLEngine::SetCameraStateFromOpenGL()
{
#if defined(ARCH_GFX_OPENGL)
    if (_renderAPI == OpenGL) {
        GfMatrix4d viewMatrix, projectionMatrix;
        GfVec4d viewport;
        glGetDoublev(GL_MODELVIEW_MATRIX, viewMatrix.GetArray());
        glGetDoublev(GL_PROJECTION_MATRIX, projectionMatrix.GetArray());
        glGetDoublev(GL_VIEWPORT, &viewport[0]);

        SetCameraState(viewMatrix, projectionMatrix);
		SetRenderViewport(viewport);
    }
#else
    TF_FATAL_CODING_ERROR("No OpenGL support available");
#endif
}

void
UsdImagingGLEngine::SetLightingStateFromOpenGL()
{
    if (ARCH_UNLIKELY(_legacyImpl)) {
        return;
    }

    TF_VERIFY(_taskController);

    if (!_lightingContextForOpenGLState) {
        _lightingContextForOpenGLState = GarchSimpleLightingContext::New();
    }
    _lightingContextForOpenGLState->SetStateFromOpenGL();

    _taskController->SetLightingState(_lightingContextForOpenGLState);
}

void
UsdImagingGLEngine::SetLightingState(GarchSimpleLightingContextPtr const &src)
{
    if (ARCH_UNLIKELY(_legacyImpl)) {
        return;
    }

    TF_VERIFY(_taskController);
    _taskController->SetLightingState(src);
}

void
UsdImagingGLEngine::SetLightingState(
    GarchSimpleLightVector const &lights,
    GarchSimpleMaterial const &material,
    GfVec4f const &sceneAmbient)
{
    if (ARCH_UNLIKELY(_legacyImpl)) {
#if defined(ARCH_GFX_OPENGL)
        _legacyImpl->SetLightingState(lights, material, sceneAmbient);
#endif
        return;
    }

    TF_VERIFY(_taskController);
    
    ResourceFactoryGuard guard(_resourceFactory);

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

//----------------------------------------------------------------------------
// Selection Highlighting
//----------------------------------------------------------------------------

void
UsdImagingGLEngine::SetSelected(SdfPathVector const& paths)
{
    if (ARCH_UNLIKELY(_legacyImpl)) {
        return;
    }

    TF_VERIFY(_delegate);

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

void
UsdImagingGLEngine::ClearSelected()
{
    if (ARCH_UNLIKELY(_legacyImpl)) {
        return;
    }

    TF_VERIFY(_selTracker);

    HdSelectionSharedPtr selection(new HdSelection);
    _selTracker->SetSelection(selection);
}

void
UsdImagingGLEngine::AddSelected(SdfPath const &path, int instanceIndex)
{
    if (ARCH_UNLIKELY(_legacyImpl)) {
        return;
    }

    TF_VERIFY(_delegate);

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

void
UsdImagingGLEngine::SetSelectionColor(GfVec4f const& color)
{
    if (ARCH_UNLIKELY(_legacyImpl)) {
        return;
    }

    TF_VERIFY(_taskController);

    _selectionColor = color;
    _taskController->SetSelectionColor(_selectionColor);
}

//----------------------------------------------------------------------------
// Picking
//----------------------------------------------------------------------------

bool 
UsdImagingGLEngine::TestIntersection(
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
    if (ARCH_UNLIKELY(_legacyImpl)) {
#if defined(ARCH_GFX_OPENGL)
        return _legacyImpl->TestIntersection(
            viewMatrix,
            inProjectionMatrix,
            worldToLocalSpace,
            root,
            params,
            outHitPoint,
            outHitPrimPath,
            outHitInstancerPath,
            outHitInstanceIndex,
            outHitElementIndex);
#endif
    }

    ResourceFactoryGuard guard(_resourceFactory);

    TF_VERIFY(_delegate);
#if defined(ARCH_GFX_METAL)
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
#else
    GfMatrix4d const &projectionMatrix = inProjectionMatrix;
#endif

    TF_VERIFY(_taskController);

    // XXX(UsdImagingPaths): Is it correct to map USD root path directly
    // to the cachePath here?
    SdfPath cachePath = root.GetPath();
    SdfPathVector roots(1, _delegate->ConvertCachePathToIndexPath(cachePath));
    _UpdateHydraCollection(&_intersectCollection, roots, params);

    TfTokenVector renderTags;
    _ComputeRenderTags(params, &renderTags);
    _taskController->SetRenderTags(renderTags);

    HdxRenderTaskParams hdParams = _MakeHydraUsdImagingGLRenderParams(params);
    _taskController->SetRenderParams(hdParams);

    // Forward scene materials enable option to delegate
    _delegate->SetSceneMaterialsEnabled(params.enableSceneMaterials);

    HdxPickHitVector allHits;
    HdxPickTaskContextParams pickParams;
    pickParams.resolveMode = HdxPickTokens->resolveNearestToCenter;
    pickParams.viewMatrix = worldToLocalSpace * viewMatrix;
    pickParams.projectionMatrix = projectionMatrix;
    pickParams.clipPlanes = params.clipPlanes;
    pickParams.collection = _intersectCollection;
    pickParams.outHits = &allHits;
    VtValue vtPickParams(pickParams);

    _engine->SetTaskContextData(HdxPickTokens->pickParams, vtPickParams);
    _Execute(params, _taskController->GetPickingTasks());

    // Since we are in nearest-hit mode, we expect allHits to have
    // a single point in it.
    if (allHits.size() != 1) {
        return false;
    }

    HdxPickHit &hit = allHits[0];

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

SdfPath
UsdImagingGLEngine::GetRprimPathFromPrimId(int primId) const
{
    if (ARCH_UNLIKELY(_legacyImpl)) {
#if defined(ARCH_GFX_OPENGL)
        return _legacyImpl->GetRprimPathFromPrimId(primId);
#endif
    }

    TF_VERIFY(_delegate);
    return _delegate->GetRenderIndex().GetRprimPathFromPrimId(primId);
}

SdfPath
UsdImagingGLEngine::GetPrimPathFromPrimIdColor(
    GfVec4i const &primIdColor,
    GfVec4i const &instanceIdColor,
    int * instanceIndexOut)
{
    unsigned char primIdColorBytes[] =  {
        uint8_t(primIdColor[0]),
        uint8_t(primIdColor[1]),
        uint8_t(primIdColor[2]),
        uint8_t(primIdColor[3])
    };

    int primId = DecodeIDRenderColor(primIdColorBytes);
    SdfPath result = GetRprimPathFromPrimId(primId);
    if (!result.IsEmpty()) {
        if (instanceIndexOut) {
            unsigned char instanceIdColorBytes[] =  {
                uint8_t(instanceIdColor[0]),
                uint8_t(instanceIdColor[1]),
                uint8_t(instanceIdColor[2]),
                uint8_t(instanceIdColor[3])
            };
            *instanceIndexOut = DecodeIDRenderColor(instanceIdColorBytes);
        }
    }
    return result;
}

SdfPath 
UsdImagingGLEngine::GetPrimPathFromInstanceIndex(
        const SdfPath &protoRprimId,
        int protoIndex,
        int *instancerIndex,
        SdfPath *masterCachePath,
        SdfPathVector *instanceContext)
{
    if (ARCH_UNLIKELY(_legacyImpl)) {
        return SdfPath();
    }

    TF_VERIFY(_delegate);

    return _delegate->GetPathForInstanceIndex(protoRprimId, protoIndex,
                                              instancerIndex, masterCachePath,
                                              instanceContext);
}

//----------------------------------------------------------------------------
// Renderer Plugin Management
//----------------------------------------------------------------------------
/* static */
TfTokenVector
UsdImagingGLEngine::GetRendererPlugins()
{
    if (ARCH_UNLIKELY(!_GetHydraEnabledEnvVar())) {
        // No plugins if the legacy implementation is active.
        return std::vector<TfToken>();
    }

    HfPluginDescVector pluginDescriptors;
    HdRendererPluginRegistry::GetInstance().GetPluginDescs(&pluginDescriptors);

    TfTokenVector plugins;

    for(size_t i = 0; i < pluginDescriptors.size(); ++i) {
        plugins.push_back(pluginDescriptors[i].id);
    }

    return plugins;
}

/* static */
std::string
UsdImagingGLEngine::GetRendererDisplayName(TfToken const &id)
{
    if (ARCH_UNLIKELY(!_GetHydraEnabledEnvVar() || id.IsEmpty())) {
        // No renderer name is returned if the user requested to disable Hydra, 
        // or if the machine does not support any of the available renderers 
        // and it automatically switches to our legacy engine.
        return std::string();
    }

    HfPluginDesc pluginDescriptor;
    if (!TF_VERIFY(HdRendererPluginRegistry::GetInstance().
                   GetPluginDesc(id, &pluginDescriptor))) {
        return std::string();
    }

    return pluginDescriptor.displayName;
}

TfToken
UsdImagingGLEngine::GetCurrentRendererId() const
{
    if (ARCH_UNLIKELY(_legacyImpl)) {
        // No renderer support if the legacy implementation is active.
        return TfToken();
    }

    return _rendererId;
}

bool
UsdImagingGLEngine::SetRendererPlugin(TfToken const &pluginId, bool forceReload)
{
    if (ARCH_UNLIKELY(_legacyImpl)) {
        return false;
    }
    
    ResourceFactoryGuard guard(_resourceFactory);

    HdRendererPlugin *plugin = nullptr;
    TfToken actualId = pluginId;

    // Special case: TfToken() selects the first plugin in the list.
    if (actualId.IsEmpty()) {
        actualId = HdRendererPluginRegistry::GetInstance().
            GetDefaultPluginId();
    }
    plugin = HdRendererPluginRegistry::GetInstance().
        GetRendererPlugin(actualId);

    if (plugin == nullptr) {
        TF_CODING_ERROR("Couldn't find plugin for id %s", actualId.GetText());
        return false;
    } else if (plugin == _rendererPlugin) {
        if (!forceReload) {
            // It's a no-op to load the same plugin twice.
            HdRendererPluginRegistry::GetInstance().ReleasePlugin(plugin);
            return true;
        }
    } else if (!plugin->IsSupported()) {
        // Don't do anything if the plugin isn't supported on the running
        // system, just return that we're not able to set it.
        HdRendererPluginRegistry::GetInstance().ReleasePlugin(plugin);
        return false;
    }

    HdRenderDelegate *renderDelegate = plugin->CreateRenderDelegate();
    if(!renderDelegate) {
        HdRendererPluginRegistry::GetInstance().ReleasePlugin(plugin);
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
    _rendererId = actualId;

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

//----------------------------------------------------------------------------
// AOVs and Renderer Settings
//----------------------------------------------------------------------------

TfTokenVector
UsdImagingGLEngine::GetRendererAovs() const
{
    if (ARCH_UNLIKELY(_legacyImpl)) {
        return std::vector<TfToken>();
    }

    TF_VERIFY(_renderIndex);

    if (_renderIndex->IsBprimTypeSupported(HdPrimTypeTokens->renderBuffer)) {
        TfTokenVector aovs;
        aovs.push_back(HdAovTokens->color);

        TfToken candidates[] =
            { HdAovTokens->primId,
              HdAovTokens->depth,
              HdAovTokens->normal,
              HdAovTokensMakePrimvar(TfToken("st")) };

        HdRenderDelegate *renderDelegate = _renderIndex->GetRenderDelegate();
        for (auto const& aov : candidates) {
            if (renderDelegate->GetDefaultAovDescriptor(aov).format 
                    != HdFormatInvalid) {
                aovs.push_back(aov);
            }
        }
        return aovs;
    }
    return TfTokenVector();
}

bool
UsdImagingGLEngine::SetRendererAov(TfToken const &id)
{
    if (ARCH_UNLIKELY(_legacyImpl)) {
        return false;
    }
    
    ResourceFactoryGuard guard(_resourceFactory);

    TF_VERIFY(_renderIndex);
    if (_renderIndex->IsBprimTypeSupported(HdPrimTypeTokens->renderBuffer)) {
        _taskController->SetRenderOutputs({id});
        return true;
    }
    return false;
}

UsdImagingGLRendererSettingsList
UsdImagingGLEngine::GetRendererSettingsList() const
{
    if (ARCH_UNLIKELY(_legacyImpl)) {
        return UsdImagingGLRendererSettingsList();
    }

    TF_VERIFY(_renderIndex);

    HdRenderSettingDescriptorList descriptors =
        _renderIndex->GetRenderDelegate()->GetRenderSettingDescriptors();
    UsdImagingGLRendererSettingsList ret;
    
    // Delegate options
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
        } else if (r.defValue.IsHolding<std::vector<std::string>>()) {
            r.type = UsdImagingGLRendererSetting::TYPE_OPTION;
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

VtValue
UsdImagingGLEngine::GetRendererSetting(TfToken const& id) const
{
    if (ARCH_UNLIKELY(_legacyImpl)) {
        return VtValue();
    }

    TF_VERIFY(_renderIndex);
    return _renderIndex->GetRenderDelegate()->GetRenderSetting(id);
}

void
UsdImagingGLEngine::SetRendererSetting(
    TfToken const& settingId, VtValue const& value)
{
    if (ARCH_UNLIKELY(_legacyImpl)) {
        return;
    }
    
    ResourceFactoryGuard guard(_resourceFactory);

    TF_VERIFY(_renderIndex);
    _renderIndex->GetRenderDelegate()->SetRenderSetting(settingId, value);

    if (settingId == HdStRenderSettingsTokens->graphicsAPI) {
        SetRendererPlugin(_rendererId, true);
    }
}

// ---------------------------------------------------------------------
// Control of background rendering threads.
// ---------------------------------------------------------------------
bool
UsdImagingGLEngine::IsPauseRendererSupported() const
{
    if (ARCH_UNLIKELY(_legacyImpl)) {
        return false;
    }

    TF_VERIFY(_renderIndex);
    return _renderIndex->GetRenderDelegate()->IsPauseSupported();
}

bool
UsdImagingGLEngine::PauseRenderer()
{
    if (ARCH_UNLIKELY(_legacyImpl)) {
        return false;
    }

    TF_VERIFY(_renderIndex);
    return _renderIndex->GetRenderDelegate()->Pause();
}

bool
UsdImagingGLEngine::ResumeRenderer()
{
    if (ARCH_UNLIKELY(_legacyImpl)) {
        return false;
    }

    TF_VERIFY(_renderIndex);
    return _renderIndex->GetRenderDelegate()->Resume();
}

//----------------------------------------------------------------------------
// Color Correction
//----------------------------------------------------------------------------
void 
UsdImagingGLEngine::SetColorCorrectionSettings(
    TfToken const& id,
    GfVec2i const& framebufferResolution)
{
    if (ARCH_UNLIKELY(_legacyImpl)) {
        return;
    }

    if (!IsColorCorrectionCapable()) {
        return;
    }

    TF_VERIFY(_taskController);

    HdxColorCorrectionTaskParams hdParams;
    hdParams.framebufferSize = framebufferResolution;
    hdParams.colorCorrectionMode = id;
    _taskController->SetColorCorrectionParams(hdParams);
}

bool 
UsdImagingGLEngine::IsColorCorrectionCapable()
{
    static bool first = false;
    static bool ColorCorrectionCapable = true;

    if (first) {
        first = false;

        GarchContextCaps const &caps =
            GarchResourceFactory::GetInstance()->GetContextCaps();
        ColorCorrectionCapable = caps.floatingPointBuffersEnabled &&
            IsHydraEnabled();
    }

    return ColorCorrectionCapable;
}

//----------------------------------------------------------------------------
// Resource Information
//----------------------------------------------------------------------------

VtDictionary
UsdImagingGLEngine::GetRenderStats() const
{
    if (ARCH_UNLIKELY(_legacyImpl)) {
        return VtDictionary();
    }

    TF_VERIFY(_renderIndex);
    return _renderIndex->GetRenderDelegate()->GetRenderStats();
}

//----------------------------------------------------------------------------
// Private/Protected
//----------------------------------------------------------------------------

HdRenderIndex *
UsdImagingGLEngine::_GetRenderIndex() const
{
    if (ARCH_UNLIKELY(_legacyImpl)) {
        return nullptr;
    }

    return _renderIndex;
}

void 
UsdImagingGLEngine::_Execute(const UsdImagingGLRenderParams &params,
                             HdTaskSharedPtrVector tasks)
{
    if (ARCH_UNLIKELY(_legacyImpl)) {
        return;
    }

    TF_VERIFY(_delegate);

    SetColorCorrectionSettings(params.colorCorrectionMode, 
                               params.renderResolution);

    // Forward scene materials enable option to delegate
    _delegate->SetSceneMaterialsEnabled(params.enableSceneMaterials);
    
    GarchContextCaps const &caps =
        GarchResourceFactory::GetInstance()->GetContextCaps();

    HdStRenderDelegate* hdStRenderDelegate =
        dynamic_cast<HdStRenderDelegate*>(_renderIndex->GetRenderDelegate());
    if (hdStRenderDelegate) {
        HdStRenderDelegate::DelegateParams delegateParams(
            params.flipFrontFacing,
            params.applyRenderState,
            params.enableIdRender,
            params.enableSampleAlphaToCoverage,
            params.sampleCount,
            params.drawMode,
#if defined(ARCH_GFX_METAL)
            ((_renderAPI == Metal &&
              params.mtlRenderPassDescriptorForNativeMetal) ||
             MtlfMetalContext::GetMetalContext()->GetDrawTarget()) ?
            HdStRenderDelegate::DelegateParams::RenderOutput::Metal :
#endif
            HdStRenderDelegate::DelegateParams::RenderOutput::OpenGL
              );
#if defined(ARCH_GFX_METAL)
            delegateParams.mtlRenderPassDescriptorForNativeMetal =
                params.mtlRenderPassDescriptorForNativeMetal;
#endif
        hdStRenderDelegate->PrepareRender(delegateParams);
    }

    _engine->Execute(_renderIndex, &tasks);

    if (hdStRenderDelegate) {
        hdStRenderDelegate->FinalizeRender();
    }
}

bool 
UsdImagingGLEngine::_CanPrepareBatch(
    const UsdPrim& root, 
    const UsdImagingGLRenderParams& params)
{
    HD_TRACE_FUNCTION();

    if (!TF_VERIFY(root, "Attempting to draw an invalid/null prim\n")) 
        return false;

    if (!root.GetPath().HasPrefix(_rootPath)) {
        TF_CODING_ERROR("Attempting to draw path <%s>, but engine is rooted"
                    "at <%s>\n",
                    root.GetPath().GetText(),
                    _rootPath.GetText());
        return false;
    }

    return true;
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

void
UsdImagingGLEngine::_PreSetTime(const UsdPrim& root, 
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
UsdImagingGLEngine::_PostSetTime(
    const UsdPrim& root, 
    const UsdImagingGLRenderParams& params)
{
    HD_TRACE_FUNCTION();
}


/* static */
bool
UsdImagingGLEngine::_UpdateHydraCollection(
    HdRprimCollection *collection,
    SdfPathVector const& roots,
    UsdImagingGLRenderParams const& params)
{
    if (collection == nullptr) {
        TF_CODING_ERROR("Null passed to _UpdateHydraCollection");
        return false;
    }

    // choose repr
    HdReprSelector reprSelector = HdReprSelector(HdReprTokens->smoothHull);
    bool refined = params.complexity > 1.0;
    
    if (params.drawMode == HdStDrawMode::DRAW_POINTS) {
        reprSelector = HdReprSelector(HdReprTokens->points);
    } else if (params.drawMode == HdStDrawMode::DRAW_GEOM_FLAT ||
        params.drawMode == HdStDrawMode::DRAW_SHADED_FLAT) {
        // Flat shading
        reprSelector = HdReprSelector(HdReprTokens->hull);
    } else if (
        params.drawMode == HdStDrawMode::DRAW_WIREFRAME_ON_SURFACE) {
        // Wireframe on surface
        reprSelector = HdReprSelector(refined ?
            HdReprTokens->refinedWireOnSurf : HdReprTokens->wireOnSurf);
    } else if (params.drawMode == HdStDrawMode::DRAW_WIREFRAME) {
        // Wireframe
        reprSelector = HdReprSelector(refined ?
            HdReprTokens->refinedWire : HdReprTokens->wire);
    } else {
        // Smooth shading
        reprSelector = HdReprSelector(refined ?
            HdReprTokens->refined : HdReprTokens->smoothHull);
    }

    // By default our main collection will be called geometry
    TfToken colName = HdTokens->geometry;

    // Check if the collection needs to be updated (so we can avoid the sort).
    SdfPathVector const& oldRoots = collection->GetRootPaths();

    // inexpensive comparison first
    bool match = collection->GetName() == colName &&
                 oldRoots.size() == roots.size() &&
                 collection->GetReprSelector() == reprSelector;

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

        // if everything matches, do nothing.
        if (match) return false;
    }

    // Recreate the collection.
    *collection = HdRprimCollection(colName, reprSelector);
    collection->SetRootPaths(roots);

    return true;
}

/* static */
HdxRenderTaskParams
UsdImagingGLEngine::_MakeHydraUsdImagingGLRenderParams(
    UsdImagingGLRenderParams const& renderParams)
{
    // Note this table is dangerous and making changes to the order of the 
    // enums in UsdImagingGLCullStyle, will affect this with no compiler help.
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
              == (size_t)UsdImagingGLCullStyle::CULL_STYLE_COUNT),
        "enum size mismatch");

    HdxRenderTaskParams params;

    params.overrideColor       = renderParams.overrideColor;
    params.wireframeColor      = renderParams.wireframeColor;

    if (renderParams.drawMode == HdStDrawMode::DRAW_GEOM_ONLY ||
        renderParams.drawMode == HdStDrawMode::DRAW_POINTS) {
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

    // We don't provide the following because task controller ignores them:
    // - params.camera
    // - params.viewport

    return params;
}

//static
void
UsdImagingGLEngine::_ComputeRenderTags(UsdImagingGLRenderParams const& params,
                                       TfTokenVector *renderTags)
{
    // Calculate the rendertags needed based on the parameters passed by
    // the application
    renderTags->clear();
    renderTags->reserve(4);
    renderTags->push_back(HdRenderTagTokens->geometry);
    if (params.showGuides) {
        renderTags->push_back(HdRenderTagTokens->guide);
    }
    if (params.showProxy) {
        renderTags->push_back(HdRenderTagTokens->proxy);
    }
    if (params.showRender) {
        renderTags->push_back(HdRenderTagTokens->render);
    }
}

void
UsdImagingGLEngine::_DeleteHydraResources()
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
        HdRendererPluginRegistry::GetInstance().ReleasePlugin(_rendererPlugin);
        _rendererPlugin = nullptr;
        _rendererId = TfToken();
    }
}

/* static */
TfToken
UsdImagingGLEngine::GetDefaultRendererPluginId()
{
    std::string defaultRendererDisplayName = 
        TfGetenv("HD_DEFAULT_RENDERER", "");

    if (defaultRendererDisplayName.empty()) {
        return TfToken();
    }

    HfPluginDescVector pluginDescs;
    HdRendererPluginRegistry::GetInstance().GetPluginDescs(&pluginDescs);

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

PXR_NAMESPACE_CLOSE_SCOPE

