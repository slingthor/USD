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
#include <boost/python/class.hpp>
#include <boost/python/def.hpp>
#include <boost/python/tuple.hpp>
#include <boost/python.hpp>
#include <boost/python/converter/from_python.hpp>

#include "pxr/usdImaging/usdImagingMetal/metal.h"

#include "pxr/usdImaging/usdImaging/delegate.h"

#include "pxr/usd/usd/prim.h"
#include "pxr/base/tf/pyContainerConversions.h"
#include "pxr/base/tf/pyEnum.h"
#include "pxr/base/tf/pyResultConversions.h"

using namespace std;
using namespace boost::python;
using namespace boost;

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

static boost::python::tuple
_TestIntersection(
    UsdImagingMetal & self,
    const GfMatrix4d &viewMatrix,
    const GfMatrix4d &projectionMatrix,
    const GfMatrix4d &worldToLocalSpace,
    const UsdPrim& root, 
    UsdImagingMetal::RenderParams params)
{
    GfVec3d hitPoint;
    SdfPath hitPrimPath;
    SdfPath hitInstancerPath;
    int hitInstanceIndex;
    int hitElementIndex;

    self.TestIntersection(
        viewMatrix,
        projectionMatrix,
        worldToLocalSpace,
        root,
        params,
        &hitPoint,
        &hitPrimPath,
        &hitInstancerPath,
        &hitInstanceIndex,
        &hitElementIndex);

    return boost::python::make_tuple(hitPoint, hitPrimPath, hitInstancerPath, hitInstanceIndex, hitElementIndex);
}

static boost::python::tuple
_GetPrimPathFromInstanceIndex(
    UsdImagingMetal & self,
    const SdfPath& protoPrimPath,
    int instanceIndex)
{
    int absoluteInstanceIndex = 0;
    SdfPath path = self.GetPrimPathFromInstanceIndex(protoPrimPath,
                                                     instanceIndex,
                                                     &absoluteInstanceIndex);
    return boost::python::make_tuple(path, absoluteInstanceIndex);
}

static void
_SetLightingState(UsdImagingMetal &self, GarchSimpleLightVector const &lights,
                  GarchSimpleMaterial const &material, GfVec4f const &sceneAmbient)
{
    self.SetLightingState(lights, material, sceneAmbient);
}

} // anonymous namespace 

void wrapMetal()
{
    { 
        // Start a new scope so that DrawMode, CullStyle and RenderParams are under Metal.
        scope Metal = class_<UsdImagingMetal, boost::noncopyable>("GL",
                                        "UsdImaging Metal Renderer class")
            .def( init<>() )
            .def( init<const SdfPath &, const SdfPathVector&, const SdfPathVector& >() )
            .def("Render", &UsdImagingMetal::Render)
            .def("SetCameraState", &UsdImagingMetal::SetCameraState)
            .def("SetLightingStateFromOpenGL",
                 &UsdImagingMetal::SetLightingStateFromOpenGL)
            .def("SetLightingState", &_SetLightingState)
            .def("SetCameraStateFromOpenGL", &UsdImagingMetal::SetCameraStateFromOpenGL)
            .def("SetSelected", &UsdImagingMetal::SetSelected)
            .def("ClearSelected", &UsdImagingMetal::ClearSelected)
            .def("AddSelected", &UsdImagingMetal::AddSelected)
            .def("SetSelectionColor", &UsdImagingMetal::SetSelectionColor)
            .def("GetRprimPathFromPrimId", &UsdImagingMetal::GetRprimPathFromPrimId)
            .def("GetPrimPathFromInstanceIndex", &_GetPrimPathFromInstanceIndex)
            .def("TestIntersection", &_TestIntersection)
            .def("IsEnabledHydra", &UsdImagingMetal::IsEnabledHydra)
                .staticmethod("IsEnabledHydra")
            .def("IsConverged", &UsdImagingMetal::IsConverged)
            .def("GetRendererPlugins", &UsdImagingMetal::GetRendererPlugins,
                 return_value_policy< TfPySequenceToTuple >())
            .def("GetRendererPluginDesc", &UsdImagingMetal::GetRendererPluginDesc)
            .def("SetRendererPlugin", &UsdImagingMetal::SetRendererPlugin)
            .def("GetRendererAovs", &UsdImagingMetal::GetRendererAovs,
                 return_value_policy< TfPySequenceToTuple >())
            .def ("SetRendererAov", &UsdImagingMetal::SetRendererAov)
            .def("GetResourceAllocation", &UsdImagingMetal::GetResourceAllocation)
        ;

        // Wrap the constants.
        Metal.attr("ALL_INSTANCES") = UsdImagingDelegate::ALL_INSTANCES;

        // Wrap the DrawMode enum. Accessible as UsdImaging.GL.DrawMode
        enum_<UsdImagingMetal::DrawMode>("DrawMode")
            .value("DRAW_POINTS", UsdImagingMetal::DRAW_POINTS)
            .value("DRAW_WIREFRAME", UsdImagingMetal::DRAW_WIREFRAME)
            .value("DRAW_WIREFRAME_ON_SURFACE", UsdImagingMetal::DRAW_WIREFRAME_ON_SURFACE)
            .value("DRAW_SHADED_FLAT", UsdImagingMetal::DRAW_SHADED_FLAT)
            .value("DRAW_SHADED_SMOOTH", UsdImagingMetal::DRAW_SHADED_SMOOTH)
            .value("DRAW_GEOM_ONLY", UsdImagingMetal::DRAW_GEOM_ONLY)
            .value("DRAW_GEOM_FLAT", UsdImagingMetal::DRAW_GEOM_FLAT)
            .value("DRAW_GEOM_SMOOTH", UsdImagingMetal::DRAW_GEOM_SMOOTH)
        ;

        // Wrap the CullStyle enum. Accessible as UsdImaging.GL.CullStyle
        enum_<UsdImagingMetal::CullStyle>("CullStyle")
                .value("CULL_STYLE_NOTHING", UsdImagingMetal::CULL_STYLE_NOTHING)
                .value("CULL_STYLE_BACK", UsdImagingMetal::CULL_STYLE_BACK)
                .value("CULL_STYLE_FRONT", UsdImagingMetal::CULL_STYLE_FRONT)
                .value("CULL_STYLE_BACK_UNLESS_DOUBLE_SIDED", UsdImagingMetal::CULL_STYLE_BACK_UNLESS_DOUBLE_SIDED)
         ;

        // Wrap the RenderParams struct. Accessible as UsdImaging.GL.RenderParams
        typedef UsdImagingMetal::RenderParams Params;
        class_<UsdImagingMetal::RenderParams>("RenderParams",
                                        "Metal Renderer parameters")
            .def_readwrite("frame", &Params::frame)
            .def_readwrite("complexity", &Params::complexity)
            .def_readwrite("drawMode", &Params::drawMode)
            .def_readwrite("showGuides", &Params::showGuides)
            .def_readwrite("showRender", &Params::showRender)
            .def_readwrite("showProxy", &Params::showProxy)
            .def_readwrite("forceRefresh", &Params::forceRefresh)
            .def_readwrite("cullStyle", &Params::cullStyle)
            .def_readwrite("enableIdRender", &Params::enableIdRender)
            .def_readwrite("enableLighting", &Params::enableLighting)
            .def_readwrite("enableSampleAlphaToCoverage",
                &Params::enableSampleAlphaToCoverage)
            .def_readwrite("applyRenderState", &Params::applyRenderState)
            .def_readwrite("gammaCorrectColors", &Params::gammaCorrectColors)
            .def_readwrite("overrideColor", &Params::overrideColor)
            .def_readwrite("wireframeColor", &Params::wireframeColor)
            .def_readwrite("clipPlanes", &Params::clipPlanes)
            .def_readwrite("highlight", &Params::highlight)
            .def_readwrite("enableSceneMaterials",
                &Params::enableSceneMaterials)
            .def_readwrite("enableUsdDrawModes", &Params::enableUsdDrawModes)
        ;

        TfPyContainerConversions::from_python_sequence<
            std::vector<GarchSimpleLight>,
                TfPyContainerConversions::variable_capacity_policy>();
    }
}
