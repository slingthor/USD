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

#include "pxr/imaging/hd/bufferArrayRange.h"
#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/vertexAdjacency.h"
#include "pxr/imaging/hd/vtBufferSource.h"

#include "pxr/imaging/hf/perfLog.h"

#include "pxr/base/vt/array.h"

#include "pxr/base/gf/vec3d.h"
#include "pxr/base/gf/vec3f.h"
#include "pxr/base/tf/token.h"

PXR_NAMESPACE_OPEN_SCOPE


HdSt_SmoothNormalsComputationGL::HdSt_SmoothNormalsComputationGL(
    Hd_VertexAdjacency const *adjacency,
    TfToken const &srcName, TfToken const &dstName,
    HdType srcDataType, HdType dstDataType)
    : HdSt_SmoothNormalsComputationGPU(adjacency, srcName, dstName, srcDataType, dstDataType)
{
    if (srcDataType != HdTypeFloatVec3 && srcDataType != HdTypeDoubleVec3) {
        TF_CODING_ERROR(
            "Unsupported points type %s for computing smooth normals",
            TfEnum::GetName(srcDataType).c_str());
        _srcDataType = HdTypeInvalid;
    }
    if (dstDataType != HdTypeFloatVec3 && dstDataType != HdTypeDoubleVec3 &&
        dstDataType != HdTypeInt32_2_10_10_10_REV) {
        TF_CODING_ERROR(
            "Unsupported normals type %s for computing smooth normals",
            TfEnum::GetName(dstDataType).c_str());
        _dstDataType = HdTypeInvalid;
    }
}

void
HdSt_SmoothNormalsComputationGL::_Execute(
    HdStProgramSharedPtr computeProgram,
    Uniform const &uniform,
    HdBufferResourceSharedPtr points,
    HdBufferResourceSharedPtr normals,
    HdBufferResourceSharedPtr adjacency,
    int numPoints)
{
    if (!glDispatchCompute)
        return;

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
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, points->GetId());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, normals->GetId());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, adjacency->GetId());

    // dispatch compute kernel
    computeProgram->SetProgram();

    glDispatchCompute(numPoints, 1, 1);

    computeProgram->UnsetProgram();
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glBindBufferBase(GL_UNIFORM_BUFFER, 0, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, 0);
}


PXR_NAMESPACE_CLOSE_SCOPE

