//
// Copyright 2019 Pixar
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

#include "pxr/imaging/garch/contextCaps.h"
#include "pxr/imaging/garch/resourceFactory.h"

#include "pxr/imaging/hdSt/domeLightComputations.h"
#include "pxr/imaging/hdSt/simpleLightingShader.h"
#include "pxr/imaging/hdSt/program.h"
#include "pxr/imaging/hdSt/resourceFactory.h"
#include "pxr/imaging/hdSt/resourceRegistry.h"
#include "pxr/imaging/hdSt/package.h"
#include "pxr/imaging/hdSt/tokens.h"

#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hf/perfLog.h"

#include "pxr/base/tf/token.h"

PXR_NAMESPACE_OPEN_SCOPE

HdSt_DomeLightComputationGPUSharedPtr
HdSt_DomeLightComputationGPU::New(
    const TfToken & shaderToken,
    HgiTextureHandle const& sourceGLTextureName,
    HdStSimpleLightingShaderPtr const &lightingShader,
    unsigned int numLevels,
    unsigned int level ,
    float roughness)
{
    return HdSt_DomeLightComputationGPUSharedPtr(
            HdStResourceFactory::GetInstance()->NewDomeLightComputationGPU(
                shaderToken, sourceGLTextureName, lightingShader, numLevels, level,
                roughness));
}

HdSt_DomeLightComputationGPU::HdSt_DomeLightComputationGPU(
    const TfToken &shaderToken,
    const GarchTextureGPUHandle sourceGLTextureName,
    HdStSimpleLightingShaderPtr const &lightingShader,
    const unsigned int numLevels,
    const unsigned int level, 
    const float roughness) 
  : _shaderToken(shaderToken),
    _lightingShader(lightingShader),
    _sourceGLTextureName(sourceGLTextureName),
    _numLevels(numLevels), 
    _level(level), 
    _roughness(roughness)
{
}

void
HdSt_DomeLightComputationGPU::Execute(HdBufferArrayRangeSharedPtr const &range,
                                      HdResourceRegistry * const resourceRegistry)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    bool hasDispatchCompute = GarchResourceFactory::GetInstance()->GetContextCaps().hasDispatchCompute;
    if (!hasDispatchCompute) {
        return;
    }

    HdStProgramSharedPtr const computeProgram = 
        HdStProgram::GetComputeProgram(
            HdStPackageDomeLightShader(), 
            _shaderToken,
            static_cast<HdStResourceRegistry*>(resourceRegistry));

    if (!TF_VERIFY(computeProgram)) {
        return;
    }

    _Execute(computeProgram);
}

PXR_NAMESPACE_CLOSE_SCOPE
