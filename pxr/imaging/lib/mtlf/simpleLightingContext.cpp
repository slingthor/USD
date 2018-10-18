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
/// \file simpleLightingContext.cpp

#include "pxr/imaging/mtlf/mtlDevice.h"

#include "pxr/imaging/garch/bindingMap.h"
#include "pxr/imaging/garch/resourceFactory.h"
#include "pxr/imaging/garch/simpleLight.h"
#include "pxr/imaging/garch/simpleMaterial.h"
#include "pxr/imaging/garch/uniformBlock.h"

#include "pxr/imaging/mtlf/simpleLightingContext.h"
#include "pxr/imaging/mtlf/bindingMap.h"

#include "pxr/base/arch/pragmas.h"
#include "pxr/base/tf/stringUtils.h"
#include "pxr/base/tf/staticData.h"
#include "pxr/base/tf/staticTokens.h"

#include <algorithm>
#include <iostream>
#include <string>

PXR_NAMESPACE_OPEN_SCOPE


TF_DEFINE_PRIVATE_TOKENS(
    _tokens,
    ((lightingUB, "Lighting"))
    ((shadowUB, "Shadow"))
    ((materialUB, "Material"))
    ((shadowSampler, "shadowTexture"))
    ((shadowCompareSampler, "shadowCompareTexture"))
    ((shadowSamplerMetalSampler, "shadowTextureSampler"))
    ((shadowCompareSamplerMetalSampler, "shadowCompareTextureSampler"))
);

// XXX:
// currently max number of lights are limited to 16 by
// GL_MAX_VARYING_VECTORS for having the varying attribute
//    out vec2 FshadowFilterWidth[NUM_LIGHTS];
// which is defined in simpleLighting.glslfx.
static const int _maxLightsUsed = 16;

MtlfSimpleLightingContext::MtlfSimpleLightingContext()
{
}

MtlfSimpleLightingContext::~MtlfSimpleLightingContext()
{
}

void
MtlfSimpleLightingContext::InitSamplerUnitBindings(GarchBindingMapPtr const &bindingMap) const
{
    GarchSimpleLightingContext::InitSamplerUnitBindings(bindingMap);

    bindingMap->GetSamplerUnit(_tokens->shadowSamplerMetalSampler);
    bindingMap->GetSamplerUnit(_tokens->shadowCompareSamplerMetalSampler);
}

void
MtlfSimpleLightingContext::BindSamplers(GarchBindingMapPtr const &bindingMap)
{
    MtlfBindingMap::MTLFBindingIndex shadowTexture(bindingMap->GetSamplerUnit(_tokens->shadowSampler));
    MtlfBindingMap::MTLFBindingIndex shadowCompareTexture(bindingMap->GetSamplerUnit(_tokens->shadowCompareSampler));
    MtlfBindingMap::MTLFBindingIndex shadowSampler(bindingMap->GetSamplerUnit(_tokens->shadowSamplerMetalSampler));
    MtlfBindingMap::MTLFBindingIndex shadowCompareSampler(bindingMap->GetSamplerUnit(_tokens->shadowCompareSamplerMetalSampler));

    MtlfMetalContext::GetMetalContext()->SetTexture(shadowTexture.index,
                                                    _shadows->GetShadowMapTexture(),
                                                    _tokens->shadowSampler,
                                                    MSL_ProgramStage(shadowTexture.stage));
    MtlfMetalContext::GetMetalContext()->SetSampler(shadowSampler.index,
                                                    _shadows->GetShadowMapDepthSampler(),
                                                    _tokens->shadowSampler,
                                                    MSL_ProgramStage(shadowSampler.stage));
    
    MtlfMetalContext::GetMetalContext()->SetTexture(shadowCompareTexture.index,
                                                    _shadows->GetShadowMapTexture(),
                                                    _tokens->shadowCompareSampler,
                                                    MSL_ProgramStage(shadowCompareTexture.stage));
    
    MtlfMetalContext::GetMetalContext()->SetSampler(shadowCompareSampler.index,
                                                    _shadows->GetShadowMapCompareSampler(),
                                                    _tokens->shadowCompareSampler,
                                                    MSL_ProgramStage(shadowCompareSampler.stage));
}

void
MtlfSimpleLightingContext::UnbindSamplers(GarchBindingMapPtr const &bindingMap)
{
}

void
MtlfSimpleLightingContext::SetStateFromOpenGL()
{
    TF_FATAL_CODING_ERROR("Not Implemented");
}

PXR_NAMESPACE_CLOSE_SCOPE

