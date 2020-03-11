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

#include "pxr/imaging/mtlf/mtlDevice.h"
#include "pxr/imaging/hdSt/Metal/mslProgram.h"

#include "pxr/imaging/garch/resourceFactory.h"
#include "pxr/imaging/garch/contextCaps.h"

#include "pxr/imaging/hdSt/program.h"
#include "pxr/imaging/hdSt/resourceRegistry.h"
#include "pxr/imaging/hdSt/tokens.h"

#include "pxr/imaging/hdSt/Metal/flatNormalsMetal.h"

#include "pxr/imaging/hd/bufferArrayRange.h"
#include "pxr/imaging/hd/bufferResource.h"
#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/tokens.h"
#include "pxr/imaging/hd/vtBufferSource.h"

#include "pxr/imaging/hf/perfLog.h"

#include "pxr/base/vt/array.h"

#include "pxr/base/gf/vec3d.h"
#include "pxr/base/gf/vec3f.h"
#include "pxr/base/tf/token.h"

PXR_NAMESPACE_OPEN_SCOPE


HdSt_FlatNormalsComputationMetal::HdSt_FlatNormalsComputationMetal(
    HdBufferArrayRangeSharedPtr const &topologyRange,
    HdBufferArrayRangeSharedPtr const &vertexRange,
    int numFaces, TfToken const &srcName, TfToken const &dstName,
    HdType srcDataType, bool packed)
    : HdSt_FlatNormalsComputationGPU(
        topologyRange,
        vertexRange,
        numFaces, srcName, dstName,
        srcDataType, packed)
{
    // empty
}

void
HdSt_FlatNormalsComputationMetal::_Execute(
    HdStProgramSharedPtr computeProgram,
    Uniform const& uniform,
    HdBufferResourceSharedPtr points,
    HdBufferResourceSharedPtr normals,
    HdBufferResourceSharedPtr indices,
    HdBufferResourceSharedPtr primitiveParam,
    int numPrims)
{
    
    if(numPrims == 0) return;
    
    MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();
    HdStMSLProgramSharedPtr const &mslProgram(boost::dynamic_pointer_cast<HdStMSLProgram>(computeProgram));
    
    //  All but the normals are immutable
    unsigned long immutableBufferMask = (1 << 0) | (1 << 2) | (1 << 3) | (1 << 4);

    id<MTLBuffer> const& pointsBuffer = points->GetId();
    id<MTLBuffer> const& normalsBuffer = normals->GetId();
    id<MTLBuffer> const& indicesBuffer = indices->GetId();
    id<MTLBuffer> const& primitiveParamBuffer = primitiveParam->GetId();
    
    context->FlushBuffers();
    context->PrepareBufferFlush();
    
    id<MTLCommandBuffer> commandBuffer = [context->gpus.commandQueue commandBuffer];
    id<MTLComputeCommandEncoder> computeEncoder = [commandBuffer computeCommandEncoder];
    
    id<MTLFunction> computeFunction = mslProgram->GetComputeFunction();
    id<MTLComputePipelineState> pipelineState =
        context->GetComputeEncoderState(computeFunction, 4, 0, immutableBufferMask,
                                        @"GPU Flat Normals pipeline state");
    
    [computeEncoder setComputePipelineState:pipelineState];
    [computeEncoder setBuffer:pointsBuffer    offset:0 atIndex:0];
    [computeEncoder setBuffer:normalsBuffer   offset:0 atIndex:1];
    [computeEncoder setBuffer:indicesBuffer   offset:0 atIndex:2];
    [computeEncoder setBuffer:primitiveParamBuffer offset:0 atIndex:3];
    [computeEncoder setBytes:(const void *)&uniform length:sizeof(uniform) atIndex:4];

    int maxThreadsPerThreadgroup = [pipelineState threadExecutionWidth];
    int const maxThreadsPerGroup = 32;
    if (maxThreadsPerThreadgroup > maxThreadsPerGroup) {
        maxThreadsPerThreadgroup = maxThreadsPerGroup;
    }
    
    MTLSize threadgroupCount =
        MTLSizeMake(fmin(maxThreadsPerThreadgroup, numPrims), 1, 1);

    [computeEncoder dispatchThreads:MTLSizeMake(numPrims, 1, 1)
              threadsPerThreadgroup:threadgroupCount];

    [computeEncoder endEncoding];
    [commandBuffer commit];
}

PXR_NAMESPACE_CLOSE_SCOPE
