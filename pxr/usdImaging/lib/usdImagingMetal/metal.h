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

/// \file usdImagingMetal/metal.h

#ifndef USDIMAGINGMETAL_METAL_H
#define USDIMAGINGMETAL_METAL_H

#include "pxr/pxr.h"
#include "pxr/usdImaging/usdImagingMetal/api.h"
#include "pxr/usdImaging/usdImagingMetal/engine.h"

#include <boost/shared_ptr.hpp>

PXR_NAMESPACE_OPEN_SCOPE


class SdfPath;

typedef std::vector<SdfPath> SdfPathVector;
typedef std::vector<UsdPrim> UsdPrimVector;

typedef boost::shared_ptr<class UsdImagingMetalEngine> UsdImagingMetalEngineSharedPtr;
typedef boost::shared_ptr<class UsdImagingMetal> UsdImagingMetalSharedPtr;
typedef std::vector<UsdImagingMetalSharedPtr> UsdImagingMetalSharedPtrVector;

/// \class UsdImagingMetal
///
/// Convenience class that abstracts whether we are rendering via
/// a high-performance Hd render engine, or a simple vbo renderer that can
/// run on old openGl versions.
///
/// The first time a UsdImagingMetal is created in a process, we decide whether
/// it and all subsequently created objects will use Hd if:
/// \li the machine's hardware and insalled openGl are sufficient
/// \li the environment variable HD_ENABLED is unset, or set to "1"
/// \li any hydra renderer plugin can be found
/// 
/// So, to disable Hd rendering for testing purposes, set HD_ENABLED to "0"
///
class UsdImagingMetal : public UsdImagingMetalEngine {
public:

    USDIMAGINGMETAL_API
    UsdImagingMetal();
    USDIMAGINGMETAL_API
    UsdImagingMetal(const SdfPath& rootPath,
                 const SdfPathVector& excludedPaths,
                 const SdfPathVector& invisedPaths=SdfPathVector(),
                 const SdfPath& delegateID = SdfPath::AbsoluteRootPath());

    USDIMAGINGMETAL_API
    virtual ~UsdImagingMetal();

    /// Prepares a sub-index delegate for drawing.
    ///
    /// This can be called many times for different sub-indexes (prim paths)
    /// over the stage, and then all rendered together with a call to
    /// RenderBatch()
    USDIMAGINGMETAL_API
    virtual void PrepareBatch(const UsdPrim& root,
                              const UsdImagingMetalRenderParams& params) override;

    /// Draws all sub-indices indentified by \p paths.  Presumes that each
    /// sub-index has already been prepared for drawing by calling
    /// PrepareBatch()
    USDIMAGINGMETAL_API
    virtual void RenderBatch(const SdfPathVector& paths,
                             const UsdImagingMetalRenderParams& params) override;

    /// Render everything at and beneath \p root, using the configuration in
    /// \p params
    ///
    /// If this is the first call to Render(), \p root will become the limiting
    /// root for all future calls to Render().  That is, you can call Render()
    /// again on \p root or any descendant of \p root, but not on any parent,
    /// sibling, or cousin of \p root.
    USDIMAGINGMETAL_API
    virtual void Render(const UsdPrim& root,
                        const UsdImagingMetalRenderParams& params) override;

    USDIMAGINGMETAL_API
    virtual void InvalidateBuffers();

    USDIMAGINGMETAL_API
    virtual void SetCameraState(const GfMatrix4d& viewMatrix,
                                const GfMatrix4d& projectionMatrix,
                                const GfVec4d& viewport);

    /// Helper function to extract lighting state from opengl and then
    /// call SetLights.
    USDIMAGINGMETAL_API
    virtual void SetLightingStateFromOpenGL();

    /// Copy lighting state from another lighting context.
    USDIMAGINGMETAL_API
    virtual void SetLightingState(GarchSimpleLightingContextPtr const &src);

    /// Set lighting state
    USDIMAGINGMETAL_API
    virtual void SetLightingState(GarchSimpleLightVector const &lights,
                                  GarchSimpleMaterial const &material,
                                  GfVec4f const &sceneAmbient);

    USDIMAGINGMETAL_API
    virtual void SetRootTransform(GfMatrix4d const& xf);

    USDIMAGINGMETAL_API
    virtual void SetRootVisibility(bool isVisible);

    /// Set the paths for selection highlighting. Note that these paths may 
    /// include prefix root paths, which will be expanded internally.
    USDIMAGINGMETAL_API
    virtual void SetSelected(SdfPathVector const& paths);

    USDIMAGINGMETAL_API
    virtual void ClearSelected();
    USDIMAGINGMETAL_API
    virtual void AddSelected(SdfPath const &path, int instanceIndex);

    /// Set the color for selection highlighting.
    USDIMAGINGMETAL_API
    virtual void SetSelectionColor(GfVec4f const& color);

    USDIMAGINGMETAL_API
    virtual SdfPath GetRprimPathFromPrimId(int primId) const;

    USDIMAGINGMETAL_API
    virtual SdfPath GetPrimPathFromInstanceIndex(
        const SdfPath& protoPrimPath,
        int instanceIndex,
        int *absoluteInstanceIndex = NULL,
        SdfPath * rprimPath=NULL,
        SdfPathVector *instanceContext=NULL);

    USDIMAGINGMETAL_API
    virtual bool IsConverged() const;

    USDIMAGINGMETAL_API
    virtual TfTokenVector GetRendererPlugins() const;

    USDIMAGINGMETAL_API
    virtual std::string GetRendererDisplayName(TfToken const &id) const override;
    
    USDIMAGINGMETAL_API
    virtual TfToken GetCurrentRendererId() const override;

    USDIMAGINGMETAL_API
    virtual bool SetRendererPlugin(TfToken const &id);

    USDIMAGINGMETAL_API
    virtual TfTokenVector GetRendererAovs() const;
    
    USDIMAGINGMETAL_API
    virtual bool SetRendererAov(TfToken const &id);

    USDIMAGINGMETAL_API
    virtual bool TestIntersection(
        const GfMatrix4d &viewMatrix,
        const GfMatrix4d &projectionMatrix,
        const GfMatrix4d &worldToLocalSpace,
        const UsdPrim& root, 
        const UsdImagingMetalRenderParams& params,
        GfVec3d *outHitPoint,
        SdfPath *outHitPrimPath = NULL,
        SdfPath *outHitInstancerPath = NULL,
        int *outHitInstanceIndex = NULL,
        int *outHitElementIndex = NULL) override;

    USDIMAGINGMETAL_API
    virtual bool TestIntersectionBatch(
        const GfMatrix4d &viewMatrix,
        const GfMatrix4d &projectionMatrix,
        const GfMatrix4d &worldToLocalSpace,
        const SdfPathVector& paths, 
        const UsdImagingMetalRenderParams& params,
        unsigned int pickResolution,
        PathTranslatorCallback pathTranslator,
        HitBatch *outHit) override;

    USDIMAGINGMETAL_API
    virtual VtDictionary GetResourceAllocation() const;

    /// Returns the list of renderer settings.
    USDIMAGINGMETAL_API
    virtual UsdImagingMetalRendererSettingsList GetRendererSettingsList() const;
    
    /// Gets a renderer setting's current value.
    USDIMAGINGMETAL_API
    virtual VtValue GetRendererSetting(TfToken const& id) const;
    
    /// Sets a renderer setting's value.
    USDIMAGINGMETAL_API
    virtual void SetRendererSetting(TfToken const& id,
                                    VtValue const& value);
    

private:
    UsdImagingMetalEngineSharedPtr _engine;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif // USDIMAGINGMETAL_METAL_H
