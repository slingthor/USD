//
// Copyright 2018 Pixar
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
#include "pxr/usdImaging/usdImagingGL/renderParams.h"

using namespace pxr;

UsdImagingGLRenderParams::UsdImagingGLRenderParams() :
    frame(UsdTimeCode::Default()),
    complexity(1.0),
    drawMode(HdStDrawMode::DRAW_SHADED_SMOOTH),
    showGuides(false),
    showProxy(true),
    showRender(false),
    forceRefresh(false),
    flipFrontFacing(false),
    cullStyle(UsdImagingGLCullStyle::CULL_STYLE_NOTHING),
    enableIdRender(false),
    enableLighting(true),
    enableSampleAlphaToCoverage(false),
    applyRenderState(true),
    gammaCorrectColors(true),
    highlight(false),
    overrideColor(.0f, .0f, .0f, .0f),
    wireframeColor(.0f, .0f, .0f, .0f),
    alphaThreshold(-1),
    clipPlanes(),
    enableSceneMaterials(true),
    enableUsdDrawModes(true),
    clearColor(0,0,0,1),
    renderResolution(100,100)
{
#if defined(ARCH_GFX_METAL)
    mtlRenderPassDescriptorForNativeMetal = nil;
#endif
}
