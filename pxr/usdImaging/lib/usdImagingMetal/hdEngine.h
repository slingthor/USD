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

/// \file hdEngine.h

#ifndef USDIMAGINGMETAL_HDENGINE_H
#define USDIMAGINGMETAL_HDENGINE_H

#include "pxr/pxr.h"

#include "pxr/imaging/mtlf/mtlDevice.h"

#include "pxr/usdImaging/usdImagingMetal/api.h"
#include "pxr/usdImaging/usdImagingMetal/engine.h"
#include "pxr/usdImaging/usdImaging/delegate.h"

#include "pxr/imaging/hd/version.h"
#include "pxr/imaging/hd/engine.h"

#include "pxr/imaging/hdx/rendererPlugin.h"
#include "pxr/imaging/hdx/selectionTracker.h"
#include "pxr/imaging/hdx/taskController.h"

#include "pxr/base/tf/declarePtrs.h"

#include <boost/shared_ptr.hpp>

PXR_NAMESPACE_OPEN_SCOPE


TF_DECLARE_WEAK_AND_REF_PTRS(GarchSimpleLightingContext);

class HdRenderIndex;
typedef boost::shared_ptr<class UsdImagingMetalHdEngine>
                                        UsdImagingMetalHdEngineSharedPtr;
typedef std::vector<UsdImagingMetalHdEngineSharedPtr>
                                        UsdImagingMetalHdEngineSharedPtrVector;
typedef std::vector<UsdPrim> UsdPrimVector;

class UsdImagingMetalHdEngine : public UsdImagingMetalEngine
{
public:
    // Important! Call UsdImagingMetalHdEngine::IsDefaultPluginAvailable() before
    // construction; if no plugins are available, the class will only
    // get halfway constructed.
    USDIMAGINGMETAL_API
    UsdImagingMetalHdEngine(const SdfPath& rootPath,
                            const SdfPathVector& excludedPaths,
                            const SdfPathVector& invisedPaths=SdfPathVector(),
                            const SdfPath& delegateID = SdfPath::AbsoluteRootPath());

    USDIMAGINGMETAL_API
    static bool IsDefaultPluginAvailable();

    USDIMAGINGMETAL_API
    virtual ~UsdImagingMetalHdEngine();

    USDIMAGINGMETAL_API
    HdRenderIndex *GetRenderIndex() const;

    USDIMAGINGMETAL_API
    virtual void InvalidateBuffers() override;

    USDIMAGINGMETAL_API
    static void PrepareBatch(
        const UsdImagingMetalHdEngineSharedPtrVector& engines,
        const UsdPrimVector& rootPrims,
        const std::vector<UsdTimeCode>& times,
        RenderParams params);

    USDIMAGINGMETAL_API
    virtual void PrepareBatch(const UsdPrim& root, RenderParams params) override;
    USDIMAGINGMETAL_API
    virtual void RenderBatch(const SdfPathVector& paths, RenderParams params) override;

    USDIMAGINGMETAL_API
    virtual void Render(const UsdPrim& root, RenderParams params) override;

    // Core rendering function: just draw, don't update anything.
    USDIMAGINGMETAL_API
    void Render(RenderParams params);

    USDIMAGINGMETAL_API
    virtual void SetCameraState(const GfMatrix4d& viewMatrix,
                                const GfMatrix4d& projectionMatrix,
                                const GfVec4d& viewport) override;

    USDIMAGINGMETAL_API
    virtual void SetLightingStateFromOpenGL() override;

    USDIMAGINGMETAL_API
    virtual void SetLightingState(GarchSimpleLightingContextPtr const &src) override;

    USDIMAGINGMETAL_API
    virtual void SetLightingState(GarchSimpleLightVector const &lights,
                                  GarchSimpleMaterial const &material,
                                  GfVec4f const &sceneAmbient) override;

    USDIMAGINGMETAL_API
    virtual void SetRootTransform(GfMatrix4d const& xf) override;

    USDIMAGINGMETAL_API
    virtual void SetRootVisibility(bool isVisible) override;

    USDIMAGINGMETAL_API
    virtual void SetSelected(SdfPathVector const& paths) override;

    USDIMAGINGMETAL_API
    virtual void ClearSelected() override;
    USDIMAGINGMETAL_API
    virtual void AddSelected(SdfPath const &path, int instanceIndex) override;

    USDIMAGINGMETAL_API
    virtual void SetSelectionColor(GfVec4f const& color) override;

    USDIMAGINGMETAL_API
    virtual SdfPath GetRprimPathFromPrimId(int primId) const override;

    USDIMAGINGMETAL_API
    virtual SdfPath GetPrimPathFromInstanceIndex(
        SdfPath const& protoPrimPath,
        int instanceIndex,
        int *absoluteInstanceIndex=NULL,
        SdfPath * rprimPath=NULL,
        SdfPathVector *instanceContext=NULL) override;

    USDIMAGINGMETAL_API
    virtual bool IsConverged() const override;

    USDIMAGINGMETAL_API
    virtual TfTokenVector GetRendererPlugins() const override;

    USDIMAGINGMETAL_API
    virtual std::string GetRendererPluginDesc(TfToken const &id) const override;

    USDIMAGINGMETAL_API
    virtual bool SetRendererPlugin(TfToken const &id) override;

    USDIMAGINGMETAL_API
    virtual TfTokenVector GetRendererAovs() const;
    
    USDIMAGINGMETAL_API
    virtual bool SetRendererAov(TfToken const& id);
    
    USDIMAGINGMETAL_API
    virtual bool TestIntersection(
        const GfMatrix4d &viewMatrix,
        const GfMatrix4d &projectionMatrix,
        const GfMatrix4d &worldToLocalSpace,
        const UsdPrim& root, 
        RenderParams params,
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
        RenderParams params,
        unsigned int pickResolution,
        PathTranslatorCallback pathTranslator,
        HitBatch *outHit) override;

    USDIMAGINGMETAL_API
    virtual VtDictionary GetResourceAllocation() const override;

private:
    // Helper functions for preparing multiple engines for
    // batched drawing.
    static void _PrepareBatch(const UsdImagingMetalHdEngineSharedPtrVector& engines,
                              const UsdPrimVector& rootPrims,
                              const std::vector<UsdTimeCode>& times,
                              const RenderParams& params);

    static void _Populate(const UsdImagingMetalHdEngineSharedPtrVector& engines,
                          const UsdPrimVector& rootPrims,
                          const RenderParams& params);
    static void _SetTimes(const UsdImagingMetalHdEngineSharedPtrVector& engines,
                          const UsdPrimVector& rootPrims,
                          const std::vector<UsdTimeCode>& times,
                          const RenderParams& params);

    // These functions factor batch preparation into separate steps so they
    // can be reused by both the vectorized and non-vectorized API.
    bool _CanPrepareBatch(const UsdPrim& root, const RenderParams& params);
    void _PreSetTime(const UsdPrim& root, const RenderParams& params);
    void _PostSetTime(const UsdPrim& root, const RenderParams& params);

    // Create a hydra collection given root paths and render params.
    // Returns true if the collection was updated.
    static bool _UpdateHydraCollection(HdRprimCollection *collection,
                          SdfPathVector const& roots,
                          UsdImagingMetalEngine::RenderParams const& params,
                          TfTokenVector *renderTags);
    static HdxRenderTaskParams _MakeHydraRenderParams(
                          UsdImagingMetalEngine::RenderParams const& params);

    // This function disposes of: the render index, the render plugin,
    // the task controller, and the usd imaging delegate.
    void _DeleteHydraResources();

    HdEngine _engine;

    HdRenderIndex *_renderIndex;

    HdxSelectionTrackerSharedPtr _selTracker;
    HdRprimCollection _renderCollection;
    HdRprimCollection _intersectCollection;

    SdfPath const _delegateID;
    UsdImagingDelegate *_delegate;
    
    HdxRendererPlugin *_renderPlugin;
    HdxTaskController *_taskController;
    
    GarchSimpleLightingContextRefPtr _lightingContextForOpenGLState;
    
    // Data we want to live across render plugin switches:
    GfVec4f _selectionColor;
    
    SdfPath _rootPath;
    SdfPathVector _excludedPrimPaths;
    SdfPathVector _invisedPrimPaths;
    bool _isPopulated;
    
    TfTokenVector _renderTags;
    
    MTLRenderPassDescriptor* _mtlRenderPassDescriptor;
    
    MTLCaptureManager *sharedCaptureManager;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // USDIMAGINGMETAL_HDENGINE_H
