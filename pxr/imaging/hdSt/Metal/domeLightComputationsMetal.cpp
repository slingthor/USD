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

#include "pxr/imaging/hdSt/Metal/domeLightComputationsMetal.h"
#include "pxr/imaging/hdSt/resourceRegistry.h"
#include "pxr/imaging/hdSt/package.h"
#include "pxr/imaging/hdSt/tokens.h"

#include "pxr/imaging/hdSt/Metal/mslProgram.h"

#include "pxr/base/tf/token.h"

PXR_NAMESPACE_OPEN_SCOPE


HdSt_DomeLightComputationGPUMetal::HdSt_DomeLightComputationGPUMetal(
    TfToken token,
    GarchTextureGPUHandle const &sourceId,
    GarchTextureGPUHandle const &destId,
    int width, int height, unsigned int numLevels, unsigned int level, 
    float roughness)
    : HdSt_DomeLightComputationGPU(token, sourceId, destId,
        width, height, numLevels, level, roughness)
{
}


void
HdSt_DomeLightComputationGPUMetal::_Execute(HdStProgramSharedPtr computeProgram)
{
    MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();
    HdStMSLProgramSharedPtr const &mslProgram(std::dynamic_pointer_cast<HdStMSLProgram>(computeProgram));

    struct Uniforms {
        Uniforms(float _roughness, int _level)
        : roughness(_roughness)
        , level(_level)
        , rowOffset(0)
        {}

        float roughness;
        int level;
        int rowOffset;
    } _uniforms(_roughness, _level);

    id<MTLFunction> computeFunction = mslProgram->GetComputeFunction();
    id<MTLComputePipelineState> pipelineState =
        context->GetComputeEncoderState(computeFunction, 1, 2, 1,
            @"HdSt_DomeLightComputationGPUMetal pipeline state");

    NSUInteger exeWidth = [pipelineState threadExecutionWidth];
    NSUInteger maxThreadsPerThreadgroup = [pipelineState maxTotalThreadsPerThreadgroup];
    MTLSize threadgroupCount = MTLSizeMake(exeWidth, maxThreadsPerThreadgroup / exeWidth, 1);
    MTLSize threadsPerGrid   = MTLSizeMake(([_destTextureId width] + (threadgroupCount.width - 1)) / threadgroupCount.width,
                                           1,
                                           1);

    int numRows = ([_destTextureId height] + (threadgroupCount.height - 1)) / threadgroupCount.height;
    
    for (int i = 0; i < numRows; i++) {
        id<MTLCommandBuffer> commandBuffer = [context->gpus.commandQueue commandBuffer];
        id<MTLComputeCommandEncoder> computeEncoder = [commandBuffer computeCommandEncoder];
        
        [computeEncoder setComputePipelineState:pipelineState];
        
        context->SetComputeEncoderState(computeEncoder);

        _uniforms.rowOffset = i * threadgroupCount.height;
        [computeEncoder setBytes:(const void *)&_uniforms
                          length:sizeof(_uniforms)
                         atIndex:0];
        
        [computeEncoder setTexture:_sourceTextureId
                           atIndex:0];
        [computeEncoder setTexture:_destTextureId
                           atIndex:1];

        [computeEncoder dispatchThreadgroups:threadsPerGrid threadsPerThreadgroup:threadgroupCount];
        
        [computeEncoder endEncoding];
        [commandBuffer commit];
    }
    
    if (_numLevels > 1) {
        id<MTLCommandBuffer> commandBuffer = [context->gpus.commandQueue commandBuffer];

        // Generate the rest of the mip chain
        id<MTLBlitCommandEncoder> blitEncoder = [commandBuffer blitCommandEncoder];
        
        [blitEncoder generateMipmapsForTexture:_destTextureId];
        [blitEncoder endEncoding];

        [commandBuffer commit];
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
