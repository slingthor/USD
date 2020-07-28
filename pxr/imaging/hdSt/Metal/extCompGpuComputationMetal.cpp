//
// Copyright 2017 Pixar
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

#include "pxr/imaging/hdSt/Metal/extCompGpuComputationMetal.h"
#include "pxr/imaging/hdSt/Metal/mslProgram.h"

#include "pxr/imaging/hdSt/bufferResourceGL.h"
#include "pxr/imaging/hdSt/extCompGpuComputationBufferSource.h"
#include "pxr/imaging/hdSt/extCompGpuPrimvarBufferSource.h"
#include "pxr/imaging/hdSt/extComputation.h"
#include "pxr/imaging/hdSt/resourceRegistry.h"

#include "pxr/imaging/hd/bufferArrayRange.h"
#include "pxr/imaging/hd/compExtCompInputSource.h"
#include "pxr/imaging/hd/extComputation.h"
#include "pxr/imaging/hd/extCompPrimvarBufferSource.h"
#include "pxr/imaging/hd/extCompCpuComputation.h"
#include "pxr/imaging/hd/sceneDelegate.h"
#include "pxr/imaging/hd/sceneExtCompInputSource.h"
#include "pxr/imaging/hd/vtBufferSource.h"

#include <limits>

PXR_NAMESPACE_OPEN_SCOPE


HdStExtCompGpuComputationMetal::HdStExtCompGpuComputationMetal(
        SdfPath const &id,
        HdStExtCompGpuComputationResourceSharedPtr const &resource,
        HdExtComputationPrimvarDescriptorVector const &compPrimvars,
        int dispatchCount,
        int elementCount)
 : HdStExtCompGpuComputation(
        id, resource, compPrimvars, dispatchCount, elementCount)
{
}

void
HdStExtCompGpuComputationMetal::_Execute(
     HdStProgramSharedPtr const &computeProgram,
     std::vector<int32_t> const &_uniforms,
     HdStBufferArrayRangeGLSharedPtr outputBar)
{
    MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();
    HdStMSLProgramSharedPtr const &mslProgram(std::dynamic_pointer_cast<HdStMSLProgram>(computeProgram));
    
    unsigned long immutableBufferMask = 0;//(1 << 0) | (1 << 2) | (1 << 3);

    context->FlushBuffers();

    id<MTLCommandBuffer> commandBuffer = [context->gpus.commandQueue commandBuffer];
    id<MTLComputeCommandEncoder> computeEncoder = [commandBuffer computeCommandEncoder];
    
    id<MTLFunction> computeFunction = mslProgram->GetComputeFunction();
    id<MTLComputePipelineState> pipelineState =
        context->GetComputeEncoderState(
            computeFunction, 4, 0, immutableBufferMask,
            @"HdStExtCompGpuComputationMetal pipeline state");

    [computeEncoder setComputePipelineState:pipelineState];
    
    context->SetComputeEncoderState(computeEncoder);
    
    [computeEncoder setBytes:(const void *)&_uniforms[0]
                      length:sizeof(int32_t) * _uniforms.size()
                     atIndex:4];

    int maxThreadsPerThreadgroup = [pipelineState threadExecutionWidth];
    int const maxThreadsPerGroup = 32;
    if (maxThreadsPerThreadgroup > maxThreadsPerGroup) {
        maxThreadsPerThreadgroup = maxThreadsPerGroup;
    }
    
    MTLSize threadgroupCount =
        MTLSizeMake(fmin(maxThreadsPerThreadgroup, GetDispatchCount()), 1, 1);

    [computeEncoder dispatchThreads:MTLSizeMake(GetDispatchCount(), 1, 1)
              threadsPerThreadgroup:threadgroupCount];
    
    [computeEncoder endEncoding];
    [commandBuffer commit];
}


PXR_NAMESPACE_CLOSE_SCOPE
