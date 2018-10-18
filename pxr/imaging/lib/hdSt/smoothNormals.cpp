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
#include "pxr/imaging/glf/glew.h"

#include "pxr/imaging/garch/contextCaps.h"
#include "pxr/imaging/garch/resourceFactory.h"

#include "pxr/imaging/hdSt/bufferResource.h"
#include "pxr/imaging/hdSt/program.h"
#include "pxr/imaging/hdSt/resourceRegistry.h"
#include "pxr/imaging/hdSt/tokens.h"

#include "pxr/imaging/hdSt/GL/smoothNormalsGL.h"
#if defined(ARCH_GFX_METAL)
#include "pxr/imaging/hdSt/Metal/smoothNormalsMetal.h"
#endif

#include "pxr/imaging/hd/bufferArrayRange.h"
#include "pxr/imaging/hd/engine.h"
#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/vertexAdjacency.h"
#include "pxr/imaging/hd/vtBufferSource.h"

#include "pxr/imaging/hf/perfLog.h"

#include "pxr/base/vt/array.h"

#include "pxr/base/gf/vec3d.h"
#include "pxr/base/gf/vec3f.h"
#include "pxr/base/tf/token.h"

PXR_NAMESPACE_OPEN_SCOPE


HdSt_SmoothNormalsComputationGPU *HdSt_SmoothNormalsComputationGPU::New(
    Hd_VertexAdjacency const *adjacency,
    TfToken const &srcName, TfToken const &dstName,
    HdType srcDataType, bool packed)
{
    HdEngine::RenderAPI api = HdEngine::GetRenderAPI();
    switch(api)
    {
        case HdEngine::OpenGL:
            return new HdSt_SmoothNormalsComputationGL(adjacency, srcName, dstName, srcDataType, packed);
#if defined(ARCH_GFX_METAL)
        case HdEngine::Metal:
            return new HdSt_SmoothNormalsComputationMetal(adjacency, srcName, dstName, srcDataType, packed);
#endif
        default:
            TF_FATAL_CODING_ERROR("No HdSt_SmoothNormalsComputationGPU for this API");
    }
    return NULL;
}

HdSt_SmoothNormalsComputationGPU::HdSt_SmoothNormalsComputationGPU(
    Hd_VertexAdjacency const *adjacency,
    TfToken const &srcName, TfToken const &dstName,
    HdType srcDataType, bool packed)
    : _adjacency(adjacency), _srcName(srcName), _dstName(dstName)
    , _srcDataType(srcDataType)
{
    if (srcDataType != HdTypeFloatVec3 && srcDataType != HdTypeDoubleVec3) {
        TF_CODING_ERROR(
            "Unsupported points type %s for computing smooth normals",
            TfEnum::GetName(srcDataType).c_str());
        _srcDataType = HdTypeInvalid;
    }
    _dstDataType = packed ? HdTypeInt32_2_10_10_10_REV : _srcDataType;
}

void
HdSt_SmoothNormalsComputationGPU::Execute(
    HdBufferArrayRangeSharedPtr const &range,
    HdResourceRegistry *resourceRegistry)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    if (_srcDataType == HdTypeInvalid)
        return;

    TF_VERIFY(_adjacency);
    HdBufferArrayRangeSharedPtr const &adjacencyRange =
        _adjacency->GetAdjacencyRange();
    TF_VERIFY(adjacencyRange);

    // select shader by datatype
    TfToken shaderToken;
    if (_srcDataType == HdTypeFloatVec3) {
        if (_dstDataType == HdTypeFloatVec3) {
            shaderToken = HdStGLSLProgramTokens->smoothNormalsFloatToFloat;
        } else if (_dstDataType == HdTypeInt32_2_10_10_10_REV) {
            shaderToken = HdStGLSLProgramTokens->smoothNormalsFloatToPacked;
        }
    } else if (_srcDataType == HdTypeDoubleVec3) {
        if (_dstDataType == HdTypeDoubleVec3) {
            shaderToken = HdStGLSLProgramTokens->smoothNormalsDoubleToDouble;
        } else if (_dstDataType == HdTypeInt32_2_10_10_10_REV) {
            shaderToken = HdStGLSLProgramTokens->smoothNormalsDoubleToPacked;
        }
    }
    if (!TF_VERIFY(!shaderToken.IsEmpty())) return;

    HdStProgramSharedPtr computeProgram
        = HdStProgram::GetComputeProgram(shaderToken,
            static_cast<HdStResourceRegistry*>(resourceRegistry));
    if (!computeProgram) return;

    // buffer resources for GPU computation
    HdBufferResourceSharedPtr points = range->GetResource(_srcName);
    HdBufferResourceSharedPtr normals = range->GetResource(_dstName);
    HdBufferResourceSharedPtr adjacency = adjacencyRange->GetResource();

    // prepare uniform buffer for GPU computation
    Uniform uniform;

    // coherent vertex offset in aggregated buffer array
    uniform.vertexOffset = range->GetOffset();
    // adjacency offset/stride in aggregated adjacency table
    uniform.adjacencyOffset = adjacencyRange->GetOffset();
    // interleaved offset/stride to points
    // note: this code (and the glsl smooth normal compute shader) assumes
    // components in interleaved vertex array are always same data type.
    // i.e. it can't handle an interleaved array which interleaves
    // float/double, float/int etc.
    //
    // The offset and stride values we pass to the shader are in terms
    // of indexes, not bytes, so we must convert the HdBufferResource
    // offset/stride (which are in bytes) to counts of float[]/double[]
    // entries.
    const size_t pointComponentSize =
        HdDataSizeOfType(HdGetComponentType(points->GetTupleType().type));
    uniform.pointsOffset = points->GetOffset() / pointComponentSize;
    uniform.pointsStride = points->GetStride() / pointComponentSize;
    // interleaved offset/stride to normals
    const size_t normalComponentSize =
        HdDataSizeOfType(HdGetComponentType(normals->GetTupleType().type));
    uniform.normalsOffset = normals->GetOffset() / normalComponentSize;
    uniform.normalsStride = normals->GetStride() / normalComponentSize;

    // The number of points is based off the size of the output,
    // However, the number of points in the adjacency table
    // is computed based off the largest vertex indexed from
    // to topology (aka topology->ComputeNumPoints).
    //
    // Therefore, we need to clamp the number of points
    // to the number of entries in the adjancency table.
    int numDestPoints = range->GetNumElements();
    int numSrcPoints = _adjacency->GetNumPoints();

    int numPoints = std::min(numSrcPoints, numDestPoints);
    
    _Execute(computeProgram, uniform, points, normals, adjacency, numPoints);
}

void
HdSt_SmoothNormalsComputationGPU::GetBufferSpecs(HdBufferSpecVector *specs) const
{
    specs->emplace_back(_dstName, HdTupleType {_dstDataType, 1});
}


PXR_NAMESPACE_CLOSE_SCOPE

