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

#include "pxr/imaging/hdx/rendererPluginRegistry.h"

#include "pxr/base/tf/getenv.h"
#include "pxr/base/tf/diagnostic.h"

#include "pxr/imaging/mtlf/mtlDevice.h"
#include "pxr/imaging/garch/textureRegistry.h"

PXR_NAMESPACE_OPEN_SCOPE

static
UsdImagingMetalEngine*
_InitEngine(const SdfPath& rootPath,
            const SdfPathVector& excludedPaths,
            const SdfPathVector& invisedPaths,
            const SdfPath& delegateID = SdfPath::AbsoluteRootPath())
{
    if (UsdImagingMetalEngine::IsHydraEnabled()) {
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

/*virtual*/
void
UsdImagingMetal::PrepareBatch(const UsdPrim& root,
                              const UsdImagingMetalRenderParams& params)
{
    _engine->PrepareBatch(root, params);
}

/*virtual*/
void
UsdImagingMetal::RenderBatch(const SdfPathVector& paths,
                             const UsdImagingMetalRenderParams& params) {
    _engine->RenderBatch(paths, params);
}

/*virtual*/
void
UsdImagingMetal::Render(const UsdPrim& root,
                        const UsdImagingMetalRenderParams& params)
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
UsdImagingMetal::GetRendererDisplayName(TfToken const &id) const
{
    return _engine->GetRendererDisplayName(id);
}

/* virtual */
TfToken
UsdImagingMetal::GetCurrentRendererId() const
{
    return _engine->GetCurrentRendererId();
}

/* virtual */
bool
UsdImagingMetal::SetRendererPlugin(TfToken const &id)
{
    return _engine->SetRendererPlugin(id);
}

/* virtual */
TfTokenVector
UsdImagingMetal::GetRendererAovs() const
{
    return _engine->GetRendererAovs();
}

/* virtual */
bool
UsdImagingMetal::SetRendererAov(TfToken const &id)
{
    return _engine->SetRendererAov(id);
}

/* virtual */
UsdImagingMetalRendererSettingsList
UsdImagingMetal::GetRendererSettingsList() const
{
    return _engine->GetRendererSettingsList();
}

/* virtual */
VtValue
UsdImagingMetal::GetRendererSetting(TfToken const& id) const
{
    return _engine->GetRendererSetting(id);
}

/* virtual */
void
UsdImagingMetal::SetRendererSetting(TfToken const& id,
                                    VtValue const& value)
{
    _engine->SetRendererSetting(id, value);
}


bool
UsdImagingMetal::TestIntersection(
    const GfMatrix4d &viewMatrix,
    const GfMatrix4d &projectionMatrix,
    const GfMatrix4d &worldToLocalSpace,
    const UsdPrim& root, 
    const UsdImagingMetalRenderParams& params,
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
    const UsdImagingMetalRenderParams& params,
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
    return _engine->GetResourceAllocation();
}

PXR_NAMESPACE_CLOSE_SCOPE

