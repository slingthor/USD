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
#include "pxr/imaging/mtlf/mtlDevice.h"

#include "pxr/imaging/hdSt/Metal/quadrangulateMetal.h"

#include "pxr/imaging/hdSt/bufferResource.h"
#include "pxr/imaging/hdSt/program.h"
#include "pxr/imaging/hdSt/meshTopology.h"
#include "pxr/imaging/hdSt/resourceRegistry.h"
#include "pxr/imaging/hdSt/tokens.h"

#include "pxr/imaging/hd/bufferArrayRange.h"
#include "pxr/imaging/hd/meshUtil.h"
#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/types.h"
#include "pxr/imaging/hd/vtBufferSource.h"

#include "pxr/imaging/garch/glslfx.h"

#include "pxr/base/gf/vec2i.h"
#include "pxr/base/gf/vec4i.h"

PXR_NAMESPACE_OPEN_SCOPE

HdSt_QuadrangulateComputationGPUMetal::HdSt_QuadrangulateComputationGPUMetal(
                                                                       HdSt_MeshTopology *topology,
                                                                       TfToken const &sourceName,
                                                                       HdType dataType,
                                                                       SdfPath const &id)
: HdSt_QuadrangulateComputationGPU(topology, sourceName, dataType, id)
{
}

void
HdSt_QuadrangulateComputationGPUMetal::Execute(
    HdBufferArrayRangeSharedPtr const &range,
    HdResourceRegistry *resourceRegistry)
{
    if (!TF_VERIFY(_topology))
        return;

    HD_TRACE_FUNCTION();
    HD_PERF_COUNTER_INCR(HdPerfTokens->quadrangulateGPU);

    // if this topology doesn't contain non-quad faces, quadInfoRange is null.
    HdBufferArrayRangeSharedPtr const &quadrangulateTableRange =
        _topology->GetQuadrangulateTableRange();
    if (!quadrangulateTableRange) return;

    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    HdQuadInfo const *quadInfo = _topology->GetQuadInfo();
    if (!quadInfo) {
        TF_CODING_ERROR("QuadInfo is null.");
        return;
    }

    TF_FATAL_CODING_ERROR("Not Implemented");

    HD_PERF_COUNTER_ADD(HdPerfTokens->quadrangulatedVerts,
                        quadInfo->numAdditionalPoints);
}

PXR_NAMESPACE_CLOSE_SCOPE

