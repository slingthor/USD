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
#include "pxr/pxr.h"

#include "pxr/imaging/glf/glew.h"

#include "pxr/usdImaging/usdImagingMetal/metal.h"
#include "pxr/usdImaging/usdImagingMetal/hdEngine.h"

#include "pxr/base/tf/getenv.h"
#include "pxr/base/tf/diagnostic.h"

#include "pxr/imaging/mtlf/mtlDevice.h"
#include "pxr/imaging/garch/textureRegistry.h"

PXR_NAMESPACE_OPEN_SCOPE


namespace {

static
bool
_IsEnabledHydra()
{
    // Make sure there is a Metal context when
    // trying to initialize Hydra
    MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();
    if (!context) {
        TF_CODING_ERROR("Metal context required. Crashing");
        return false;
    }
    if (!UsdImagingMetalHdEngine::IsDefaultPluginAvailable()) {
        return false;
    }

    return true;
}

}

/*static*/
bool
UsdImagingMetal::IsEnabledHydra()
{
    static bool isEnabledHydra = _IsEnabledHydra();
    return isEnabledHydra;
}

static
UsdImagingMetalEngine* _InitEngine(const SdfPath& rootPath,
                              const SdfPathVector& excludedPaths,
                              const SdfPathVector& invisedPaths,
                              const SdfPath& delegateID =
                                        SdfPath::AbsoluteRootPath())
{
    if (UsdImagingMetal::IsEnabledHydra()) {
        return new UsdImagingMetalHdEngine(rootPath, excludedPaths,
                                        invisedPaths, delegateID);
    } else {
        return nullptr;
    }
}

UsdImagingMetal::UsdImagingMetal()
{
    SdfPathVector excluded, invised;
    _engine.reset(_InitEngine(SdfPath::AbsoluteRootPath(), excluded, invised));
}

UsdImagingMetal::UsdImagingMetal(const SdfPath& rootPath,
                           const SdfPathVector& excludedPaths,
                           const SdfPathVector& invisedPaths,
                           const SdfPath& delegateID)
{
    _engine.reset(_InitEngine(rootPath, excludedPaths,
                              invisedPaths, delegateID));
}

UsdImagingMetal::~UsdImagingMetal()
{
    _engine->InvalidateBuffers();
}

void
UsdImagingMetal::InvalidateBuffers()
{
    _engine->InvalidateBuffers();
}

/* static */
bool
UsdImagingMetal::IsBatchingSupported()
{
    // Currently, batch drawing is supported only by the Hydra engine.
    return IsEnabledHydra();
}

/* static */
void
UsdImagingMetal::PrepareBatch(
    const UsdImagingMetalSharedPtrVector& renderers,
    const UsdPrimVector& rootPrims,
    const std::vector<UsdTimeCode>& times,
    RenderParams params)
{
    if (!IsBatchingSupported()) {
        return;
    }

    // Batching is only supported if the Hydra engine is enabled, and if
    // it is then all of the UsdImagingMetal instances we've been given
    // must use a UsdImagingMetalHdEngine engine. So we explicitly call the
    // the static method on that class.
    UsdImagingMetalHdEngineSharedPtrVector hdEngines;
    hdEngines.reserve(renderers.size());
    TF_FOR_ALL(it, renderers) {
        hdEngines.push_back(
            boost::dynamic_pointer_cast<UsdImagingMetalHdEngine>(
                (*it)->_engine));
    }

    UsdImagingMetalHdEngine::PrepareBatch(hdEngines, rootPrims, times, params);
}

/*virtual*/
void
UsdImagingMetal::PrepareBatch(const UsdPrim& root, RenderParams params)
{
    _engine->PrepareBatch(root, params);
}

/*virtual*/
void
UsdImagingMetal::RenderBatch(const SdfPathVector& paths, RenderParams params) {
    _engine->RenderBatch(paths, params);
}

/*virtual*/
void
UsdImagingMetal::Render(const UsdPrim& root, RenderParams params)
{
    _engine->Render(root, params);
}

/*virtual*/
void
UsdImagingMetal::SetSelectionColor(GfVec4f const& color)
{
    _engine->SetSelectionColor(color);
}

/*virtual*/
void 
UsdImagingMetal::SetCameraState(const GfMatrix4d& viewMatrix,
                            const GfMatrix4d& projectionMatrix,
                            const GfVec4d& viewport)
{
    _engine->SetCameraState(
        viewMatrix, projectionMatrix,
        viewport);
}

/*virtual*/
SdfPath
UsdImagingMetal::GetRprimPathFromPrimId(int primId) const
{
    return _engine->GetRprimPathFromPrimId(primId);
}

/* virtual */
SdfPath 
UsdImagingMetal::GetPrimPathFromInstanceIndex(
    const SdfPath& protoPrimPath,
    int instanceIndex,
    int *absoluteInstanceIndex,
    SdfPath * rprimPath,
    SdfPathVector *instanceContext)
{
    return _engine->GetPrimPathFromInstanceIndex(protoPrimPath, instanceIndex,
                                                 absoluteInstanceIndex, 
                                                 rprimPath,
                                                 instanceContext);
}

/* virtual */
void
UsdImagingMetal::SetLightingStateFromOpenGL()
{
    _engine->SetLightingStateFromOpenGL();
}

/* virtual */
void
UsdImagingMetal::SetLightingState(GarchSimpleLightVector const &lights,
                                  GarchSimpleMaterial const &material,
                                  GfVec4f const &sceneAmbient)
{
    _engine->SetLightingState(lights, material, sceneAmbient);
}


/* virtual */
void
UsdImagingMetal::SetLightingState(GarchSimpleLightingContextPtr const &src)
{
    _engine->SetLightingState(src);
}

/* virtual */
void
UsdImagingMetal::SetRootTransform(GfMatrix4d const& xf)
{
    _engine->SetRootTransform(xf);
}

/* virtual */
void
UsdImagingMetal::SetRootVisibility(bool isVisible)
{
    _engine->SetRootVisibility(isVisible);
}

/* virtual */
void
UsdImagingMetal::SetSelected(SdfPathVector const& paths)
{
    _engine->SetSelected(paths);
}

/* virtual */
void
UsdImagingMetal::ClearSelected()
{
    _engine->ClearSelected();
}

/* virtual */
void
UsdImagingMetal::AddSelected(SdfPath const &path, int instanceIndex)
{
    _engine->AddSelected(path, instanceIndex);
}

/* virtual */
bool
UsdImagingMetal::IsConverged() const
{
    return _engine->IsConverged();
}

/* virtual */
TfTokenVector
UsdImagingMetal::GetRendererPlugins() const
{
    return _engine->GetRendererPlugins();
}

/* virtual */
std::string
UsdImagingMetal::GetRendererPluginDesc(TfToken const &id) const
{
    return _engine->GetRendererPluginDesc(id);
}

/* virtual */
bool
UsdImagingMetal::SetRendererPlugin(TfToken const &id)
{
    return _engine->SetRendererPlugin(id);
}

bool
UsdImagingMetal::TestIntersection(
    const GfMatrix4d &viewMatrix,
    const GfMatrix4d &projectionMatrix,
    const GfMatrix4d &worldToLocalSpace,
    const UsdPrim& root, 
    RenderParams params,
    GfVec3d *outHitPoint,
    SdfPath *outHitPrimPath,
    SdfPath *outHitInstancerPath,
    int *outHitInstanceIndex,
    int *outHitElementIndex)
{
    return _engine->TestIntersection(viewMatrix, projectionMatrix,
                worldToLocalSpace, root, params, outHitPoint,
                outHitPrimPath, outHitInstancerPath, outHitInstanceIndex,
                outHitElementIndex);
}

bool
UsdImagingMetal::TestIntersectionBatch(
    const GfMatrix4d &viewMatrix,
    const GfMatrix4d &projectionMatrix,
    const GfMatrix4d &worldToLocalSpace,
    const SdfPathVector& paths, 
    RenderParams params,
    unsigned int pickResolution,
    PathTranslatorCallback pathTranslator,
    HitBatch *outHit)
{
    return _engine->TestIntersectionBatch(viewMatrix, projectionMatrix,
                worldToLocalSpace, paths, params, pickResolution,
                pathTranslator, outHit);
}

/* virtual */
VtDictionary
UsdImagingMetal::GetResourceAllocation() const
{
    VtDictionary dict;
    dict = _engine->GetResourceAllocation();

    // append texture usage
    size_t texMem = 0;
    for (auto const &texInfo :
             GarchTextureRegistry::GetInstance().GetTextureInfos()) {
        VtDictionary::const_iterator it = texInfo.find("memoryUsed");
        if (it != texInfo.end()) {
            VtValue mem = it->second;
            if (mem.IsHolding<size_t>()) {
                texMem += mem.Get<size_t>();
            }
        }
    }
    dict["textureMemoryUsed"] = texMem;
    return dict;
}

PXR_NAMESPACE_CLOSE_SCOPE

