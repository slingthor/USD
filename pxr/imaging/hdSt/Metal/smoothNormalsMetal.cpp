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
#include "pxr/imaging/mtlf/mtlDevice.h"
#include "pxr/imaging/mtlf/OSDMetalContext.h"
#include "pxr/imaging/hdSt/Metal/mslProgram.h"

#include "pxr/imaging/garch/contextCaps.h"
#include "pxr/imaging/garch/resourceFactory.h"

#include "pxr/imaging/hdSt/bufferResourceGL.h"
#include "pxr/imaging/hdSt/program.h"
#include "pxr/imaging/hdSt/resourceRegistry.h"
#include "pxr/imaging/hdSt/tokens.h"

#include "pxr/imaging/hdSt/Metal/smoothNormalsMetal.h"

#include "pxr/imaging/hgiMetal/buffer.h"

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


HdSt_SmoothNormalsComputationMetal::HdSt_SmoothNormalsComputationMetal(
    Hd_VertexAdjacency const *adjacency,
    TfToken const &srcName,
    TfToken const &dstName,
    HdType srcDataType,
    bool packed)
    : HdSt_SmoothNormalsComputationGPU(adjacency, srcName, dstName, srcDataType, packed)
{
    if (srcDataType != HdTypeFloatVec3 && srcDataType != HdTypeDoubleVec3) {
        TF_CODING_ERROR(
            "Unsupported points type %s for computing smooth normals",
            TfEnum::GetName(srcDataType).c_str());
        _srcDataType = HdTypeInvalid;
    }
}

void
HdSt_SmoothNormalsComputationMetal::_Execute(
    HdStProgramSharedPtr computeProgram,
    Uniform const &uniform,
    HdStBufferResourceGLSharedPtr points,
    HdStBufferResourceGLSharedPtr normals,
    HdStBufferResourceGLSharedPtr adjacency,
    int numPoints)
{
    
    if(numPoints == 0) return;
    
    MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();
    
    // temp fix for Storm recompiling the shader every frame due to resource
    // management bug. Remove static in the future
    static HdStMSLProgramSharedPtr mslProgram;
    mslProgram = std::dynamic_pointer_cast<HdStMSLProgram>(computeProgram);

    // Only the normals are writebale
    unsigned long immutableBufferMask = (1 << 0) | (1 << 2) | (1 << 3);

    id<MTLBuffer> pointsBuffer = HgiMetalBuffer::MTLBuffer(points->GetId());
    id<MTLBuffer> normalsBuffer = HgiMetalBuffer::MTLBuffer(normals->GetId());
    id<MTLBuffer> adjacencyBuffer = HgiMetalBuffer::MTLBuffer(adjacency->GetId());

    context->FlushBuffers();
    context->PrepareBufferFlush();
    
    id<MTLCommandBuffer> commandBuffer = [context->gpus.commandQueue commandBuffer];
    id<MTLComputeCommandEncoder> computeEncoder = [commandBuffer computeCommandEncoder];
    
    id<MTLFunction> computeFunction = mslProgram->GetComputeFunction();
    id<MTLComputePipelineState> pipelineState =
        context->GetComputeEncoderState(computeFunction, 4, 0, immutableBufferMask,
                                        @"GPU Smooth Normals pipeline state");

    [computeEncoder setComputePipelineState:pipelineState];
    [computeEncoder setBuffer:pointsBuffer    offset:0 atIndex:0];
    [computeEncoder setBuffer:normalsBuffer   offset:0 atIndex:1];
    [computeEncoder setBuffer:adjacencyBuffer offset:0 atIndex:2];
    [computeEncoder setBytes:(const void *)&uniform
                      length:sizeof(uniform)
                     atIndex:3];
    
    int maxThreadsPerThreadgroup = [pipelineState threadExecutionWidth];
    int const maxThreadsPerGroup = 32;
    if (maxThreadsPerThreadgroup > maxThreadsPerGroup) {
        maxThreadsPerThreadgroup = maxThreadsPerGroup;
    }

    MTLSize threadgroupCount =
        MTLSizeMake(fmin(maxThreadsPerThreadgroup, numPoints), 1, 1);
    
    [computeEncoder dispatchThreads:MTLSizeMake(numPoints, 1, 1)
              threadsPerThreadgroup:threadgroupCount];
    
    [computeEncoder endEncoding];
    [commandBuffer commit];
}


PXR_NAMESPACE_CLOSE_SCOPE

