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

#include "pxr/imaging/hdSt/program.h"
#include "pxr/imaging/hdSt/resourceRegistry.h"
#include "pxr/imaging/hdSt/flatNormals.h"
#include "pxr/imaging/hdSt/tokens.h"

#include "pxr/imaging/hd/bufferArrayRange.h"
#include "pxr/imaging/hd/bufferResource.h"
#include "pxr/imaging/hd/engine.h"
#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/tokens.h"
#include "pxr/imaging/hd/vtBufferSource.h"

#include "pxr/imaging/hf/perfLog.h"

#include "pxr/base/vt/array.h"

#include "pxr/base/gf/vec3d.h"
#include "pxr/base/gf/vec3f.h"
#include "pxr/base/tf/token.h"

PXR_NAMESPACE_OPEN_SCOPE

HdSt_FlatNormalsComputationGPU::HdSt_FlatNormalsComputationGPU(
    HdBufferArrayRangeSharedPtr const &topologyRange,
    HdBufferArrayRangeSharedPtr const &vertexRange,
    int numFaces, TfToken const &srcName, TfToken const &dstName,
    HdType srcDataType, bool packed)
    : _topologyRange(topologyRange), _vertexRange(vertexRange)
    , _numFaces(numFaces), _srcName(srcName), _dstName(dstName)
    , _srcDataType(srcDataType)
{
    if (srcDataType != HdTypeFloatVec3 && srcDataType != HdTypeDoubleVec3) {
        TF_CODING_ERROR(
            "Unsupported points type %s for computing flat normals",
            TfEnum::GetName(srcDataType).c_str());
        _srcDataType = HdTypeInvalid;
    }
    _dstDataType = packed ? HdTypeInt32_2_10_10_10_REV : _srcDataType;
}

int
HdSt_FlatNormalsComputationGPU::GetNumOutputElements() const
{
    return _numFaces;
}

void
HdSt_FlatNormalsComputationGPU::Execute(
    HdBufferArrayRangeSharedPtr const &range,
    HdResourceRegistry *resourceRegistry)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    if (_srcDataType == HdTypeInvalid)
        return;

    // buffer resources for GPU computation
    HdBufferResourceSharedPtr points = _vertexRange->GetResource(_srcName);
    HdBufferResourceSharedPtr normals = range->GetResource(_dstName);
    HdBufferResourceSharedPtr indices = _topologyRange->GetResource(
        HdTokens->indices);
    HdBufferResourceSharedPtr primitiveParam = _topologyRange->GetResource(
        HdTokens->primitiveParam);

    // select shader by datatype
    TfToken shaderToken;
    int indexArity = HdGetComponentCount(indices->GetTupleType().type);
    if (indexArity == 3) {
        if (_srcDataType == HdTypeFloatVec3) {
            if (_dstDataType == HdTypeFloatVec3) {
                shaderToken = HdStGLSLProgramTokens->flatNormalsTriFloatToFloat;
            } else if (_dstDataType == HdTypeInt32_2_10_10_10_REV) {
                shaderToken = HdStGLSLProgramTokens->flatNormalsTriFloatToPacked;
            }
        } else if (_srcDataType == HdTypeDoubleVec3) {
            if (_dstDataType == HdTypeDoubleVec3) {
                shaderToken = HdStGLSLProgramTokens->flatNormalsTriDoubleToDouble;
            } else if (_dstDataType == HdTypeInt32_2_10_10_10_REV) {
                shaderToken = HdStGLSLProgramTokens->flatNormalsTriDoubleToPacked;
            }
        }
    } else if (indexArity == 4) {
        if (_srcDataType == HdTypeFloatVec3) {
            if (_dstDataType == HdTypeFloatVec3) {
                shaderToken = HdStGLSLProgramTokens->flatNormalsQuadFloatToFloat;
            } else if (_dstDataType == HdTypeInt32_2_10_10_10_REV) {
                shaderToken = HdStGLSLProgramTokens->flatNormalsQuadFloatToPacked;
            }
        } else if (_srcDataType == HdTypeDoubleVec3) {
            if (_dstDataType == HdTypeDoubleVec3) {
                shaderToken = HdStGLSLProgramTokens->flatNormalsQuadDoubleToDouble;
            } else if (_dstDataType == HdTypeInt32_2_10_10_10_REV) {
                shaderToken = HdStGLSLProgramTokens->flatNormalsQuadDoubleToPacked;
            }
        }
    }
    if (!TF_VERIFY(!shaderToken.IsEmpty())) return;

    HdStProgramSharedPtr computeProgram
        = HdStProgram::GetComputeProgram(shaderToken,
            static_cast<HdStResourceRegistry*>(resourceRegistry));
    if (!computeProgram) return;

    computeProgram->SetProgram();

    // prepare uniform buffer for GPU computation
    Uniform uniform;

    // coherent vertex offset in aggregated buffer array
    uniform.vertexOffset = _vertexRange->GetOffset();
    // coherent element offset in aggregated buffer array
    uniform.elementOffset = range->GetOffset();
    // coherent topology offset in aggregated buffer array
    uniform.topologyOffset = _topologyRange->GetOffset();
    // interleaved offset/stride to points
    // note: this code (and the glsl flat normal compute shader) assumes
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

    const size_t indexComponentSize =
        HdDataSizeOfType(HdGetComponentType(indices->GetTupleType().type));
    uniform.indexOffset = indices->GetOffset() / indexComponentSize;
    uniform.indexStride = indices->GetStride() / indexComponentSize;

    const size_t pParamComponentSize =
        HdDataSizeOfType(HdGetComponentType(primitiveParam->GetTupleType().type));
    uniform.pParamOffset = primitiveParam->GetOffset() / pParamComponentSize;
    uniform.pParamStride = primitiveParam->GetStride() / pParamComponentSize;

    _Execute(computeProgram, uniform, points,
             normals, indices, primitiveParam, _topologyRange->GetNumElements());

    computeProgram->UnsetProgram();
}

void
HdSt_FlatNormalsComputationGPU::GetBufferSpecs(HdBufferSpecVector *specs) const
{
    specs->emplace_back(_dstName, HdTupleType {_dstDataType, 1});
}


PXR_NAMESPACE_CLOSE_SCOPE
