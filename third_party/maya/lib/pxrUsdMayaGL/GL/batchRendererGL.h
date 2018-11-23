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
#ifndef PXRUSDMAYAGL_BATCH_RENDERER_GL_H
#define PXRUSDMAYAGL_BATCH_RENDERER_GL_H

/// \file pxrUsdMayaGL/GL/batchRendererGL.h

#include "pxr/pxr.h"
#include "pxrUsdMayaGL/api.h"
#include "pxrUsdMayaGL/renderParams.h"
#include "pxrUsdMayaGL/sceneDelegate.h"
#include "pxrUsdMayaGL/shapeAdapter.h"
#include "pxrUsdMayaGL/softSelectHelper.h"

#include "pxrUsdMayaGL/batchRenderer.h"

#include "usdMaya/diagnosticDelegate.h"
#include "usdMaya/notice.h"
#include "usdMaya/util.h"

#include "pxr/base/gf/matrix4d.h"
#include "pxr/base/gf/vec3f.h"
#include "pxr/base/gf/vec4d.h"
#include "pxr/base/tf/singleton.h"
#include "pxr/base/tf/weakBase.h"
#include <pxr/imaging/glf/resourceFactory.h>
#include "pxr/imaging/hd/engine.h"
#include "pxr/imaging/hd/renderIndex.h"
#include "pxr/imaging/hd/rprimCollection.h"
#include "pxr/imaging/hdSt/renderDelegate.h"
#include "pxr/imaging/hdx/intersector.h"
#include "pxr/imaging/hdx/selectionTracker.h"
#include "pxr/usd/sdf/path.h"

#include <maya/M3dView.h>
#include <maya/MDrawContext.h>
#include <maya/MDrawRequest.h>
#include <maya/MObjectHandle.h>
#include <maya/MMessage.h>
#include <maya/MSelectInfo.h>
#include <maya/MSelectionContext.h>
#include <maya/MTypes.h>
#include <maya/MUserData.h>

#include <memory>
#include <utility>
#include <unordered_map>
#include <unordered_set>


PXR_NAMESPACE_OPEN_SCOPE

/// UsdMayaGLBatchRenderer is a singleton that shapes can use to get consistent
/// batched drawing via Hydra in Maya, regardless of legacy viewport or
/// Viewport 2.0 usage.
///
/// Typical usage is as follows:
///
/// Objects that manage drawing and selection of Maya shapes (e.g. classes
/// derived from \c MPxSurfaceShapeUI or \c MPxDrawOverride) should construct
/// and maintain a PxrMayaHdShapeAdapter. Those objects should call
/// AddShapeAdapter() to add their shape for batched drawing and selection.
///
/// In preparation for drawing, the shape adapter should be synchronized to
/// populate it with data from its shape and from the viewport display state.
/// A user data object should also be created/obtained for the shape by calling
/// the shape adapter's GetMayaUserData() method.
///
/// In the draw stage, Draw() must be called for each draw request to complete
/// the render.
///
/// Draw/selection management objects should be sure to call
/// RemoveShapeAdapter() (usually in the destructor) when they no longer wish
/// for their shape to participate in batched drawing and selection.
///
class UsdMayaGLBatchRendererGL
        : public UsdMayaGLBatchRenderer
{
protected:

    PXRUSDMAYAGL_API
    virtual HdEngine& _GetEngine() override {
        return _hdEngine;
    }

    /// Private helper function to render the given list of render items.
    /// Note that this doesn't set lighting, so if you need to update the
    /// lighting from the scene, you need to do that beforehand.
    PXRUSDMAYAGL_API
    virtual void _Render(
                     const GfMatrix4d& worldToViewMatrix,
                     const GfMatrix4d& projectionMatrix,
                     const GfVec4d& viewport,
                     const std::vector<UsdMayaGLBatchRenderer::_RenderItem>& items) override;

protected:

    friend class TfSingleton<UsdMayaGLBatchRendererGL>;

    PXRUSDMAYAGL_API
    UsdMayaGLBatchRendererGL();

    PXRUSDMAYAGL_API
    virtual ~UsdMayaGLBatchRendererGL();

    /// The HdEngine
    HdEngine _hdEngine;
    
    /// The low level API resource factory to use for creation of GPU resources
    GlfResourceFactory _resourceFactory;
};


PXR_NAMESPACE_CLOSE_SCOPE


#endif
