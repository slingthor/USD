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
#include "pxr/imaging/glf/glew.h"

#include "pxr/imaging/hdSt/GL/quadrangulateGL.h"

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

#include "pxr/imaging/garch/contextCaps.h"
#include "pxr/imaging/garch/glslfx.h"
#include "pxr/imaging/garch/resourceFactory.h"


#include "pxr/base/gf/vec2i.h"
#include "pxr/base/gf/vec4i.h"

PXR_NAMESPACE_OPEN_SCOPE


HdSt_QuadrangulateComputationGPUGL::HdSt_QuadrangulateComputationGPUGL(
                                                   HdSt_MeshTopology *topology,
                                                   TfToken const &sourceName,
                                                   HdType dataType,
                                                   SdfPath const &id)
    : HdSt_QuadrangulateComputationGPU(topology, sourceName, dataType, id)
{
}

void
HdSt_QuadrangulateComputationGPUGL::Execute(
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
    
#if defined(ARCH_GFX_METAL)
    // Emit error until we support this
    TF_CODING_ERROR("Metal Compute currently not supported");
#endif

    if (!glDispatchCompute)
        return;

    // select shader by datatype
    TfToken shaderToken =
        (HdGetComponentType(_dataType) == HdTypeFloat) ?
        HdStGLSLProgramTokens->quadrangulateFloat :
        HdStGLSLProgramTokens->quadrangulateDouble;

    HdStProgramSharedPtr computeProgram =
        HdStProgram::GetComputeProgram(shaderToken,
            static_cast<HdStResourceRegistry*>(resourceRegistry));
        
    if (!computeProgram) return;

    HdBufferArrayRangeSharedPtr range_ =
        boost::static_pointer_cast<HdBufferArrayRange> (range);

    // buffer resources for GPU computation
    HdBufferResourceSharedPtr primvar_ = range_->GetResource(_name);
    HdStBufferResourceSharedPtr primvar =
        boost::static_pointer_cast<HdStBufferResource> (primvar_);

    HdBufferArrayRangeSharedPtr quadrangulateTableRange_ =
        boost::static_pointer_cast<HdBufferArrayRange> (quadrangulateTableRange);

    HdBufferResourceSharedPtr quadrangulateTable_ =
        quadrangulateTableRange_->GetResource();
    HdStBufferResourceSharedPtr quadrangulateTable =
        boost::static_pointer_cast<HdStBufferResource> (quadrangulateTable_);

    // prepare uniform buffer for GPU computation
    struct Uniform {
        int vertexOffset;
        int quadInfoStride;
        int quadInfoOffset;
        int maxNumVert;
        int primvarOffset;
        int primvarStride;
        int numComponents;
    } uniform;

    int quadInfoStride = quadInfo->maxNumVert + 2;

    // coherent vertex offset in aggregated buffer array
    uniform.vertexOffset = range->GetOffset();
    // quadinfo offset/stride in aggregated adjacency table
    uniform.quadInfoStride = quadInfoStride;
    uniform.quadInfoOffset = quadrangulateTableRange->GetOffset();
    uniform.maxNumVert = quadInfo->maxNumVert;
    // interleaved offset/stride to points
    // note: this code (and the glsl smooth normal compute shader) assumes
    // components in interleaved vertex array are always same data type.
    // i.e. it can't handle an interleaved array which interleaves
    // float/double, float/int etc.
    const size_t componentSize =
        HdDataSizeOfType(HdGetComponentType(primvar->GetTupleType().type));
    uniform.primvarOffset = primvar->GetOffset() / componentSize;
    uniform.primvarStride = primvar->GetStride() / componentSize;
    uniform.numComponents =
        HdGetComponentCount(primvar->GetTupleType().type);

    // transfer uniform buffer
    GLuint ubo = computeProgram->GetGlobalUniformBuffer().GetId();
    GarchContextCaps const &caps = GarchResourceFactory::GetInstance()->GetContextCaps();
    // XXX: workaround for 319.xx driver bug of glNamedBufferDataEXT on UBO
    // XXX: move this workaround to renderContextCaps
    if (false && caps.directStateAccessEnabled) {
        glNamedBufferDataEXT(ubo, sizeof(uniform), &uniform, GL_STATIC_DRAW);
    } else {
        glBindBuffer(GL_UNIFORM_BUFFER, ubo);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(uniform), &uniform, GL_STATIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, primvar->GetId());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, quadrangulateTable->GetId());

    // dispatch compute kernel
    computeProgram->SetProgram();

    int numNonQuads = (int)quadInfo->numVerts.size();

    glDispatchCompute(numNonQuads, 1, 1);

    computeProgram->UnsetProgram();
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glBindBufferBase(GL_UNIFORM_BUFFER, 0, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);

    HD_PERF_COUNTER_ADD(HdPerfTokens->quadrangulatedVerts,
                        quadInfo->numAdditionalPoints);
}

PXR_NAMESPACE_CLOSE_SCOPE

