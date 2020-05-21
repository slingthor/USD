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
#include "pxr/imaging/hdSt/simpleLightingShader.h"
#include "pxr/imaging/hdSt/resourceRegistry.h"
#include "pxr/imaging/hdSt/package.h"
#include "pxr/imaging/hdSt/tokens.h"

#include "pxr/imaging/hdSt/Metal/mslProgram.h"
#include "pxr/imaging/hgiMetal/texture.h"

#include "pxr/base/tf/token.h"

PXR_NAMESPACE_OPEN_SCOPE


HdSt_DomeLightComputationGPUMetal::HdSt_DomeLightComputationGPUMetal(
    const TfToken & shaderToken,
    HgiTextureHandle const& sourceGLTextureName,
    HdStSimpleLightingShaderPtr const &lightingShader,
    unsigned int numLevels,
    unsigned int level,
    float roughness)
    : HdSt_DomeLightComputationGPU(
       shaderToken,
       dynamic_cast<HgiMetalTexture*>(sourceGLTextureName.Get())->GetTextureId(),
       lightingShader,
       numLevels,
       level,
       roughness)
{
}

GarchTextureGPUHandle
HdSt_DomeLightComputationGPUMetal::_CreateGLTexture(
    const int32_t width, const int32_t height) const
{
    id<MTLDevice> device = MtlfMetalContext::GetMetalContext()->currentDevice;
    MTLTextureDescriptor* desc =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA16Float
                                                           width:width
                                                          height:height
                                                       mipmapped:NO];
    desc.mipmapLevelCount = _numLevels;
    desc.resourceOptions = MTLResourceStorageModeDefault;
    desc.usage = MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite;
    return [device newTextureWithDescriptor:desc];
}

void
HdSt_DomeLightComputationGPUMetal::_Execute(HdStProgramSharedPtr computeProgram)
{
    if (_level != 0) {
        // Metal generates mipmaps along with the top level
        return;
    }

    HdStSimpleLightingShaderSharedPtr const shader = _lightingShader.lock();
    if (!TF_VERIFY(shader)) {
        return;
    }

    MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();
    HdStMSLProgramSharedPtr const &mslProgram(std::dynamic_pointer_cast<HdStMSLProgram>(computeProgram));

    // Get texture name from lighting shader.

    id<MTLTexture> dstGLTextureName = shader->GetGLTextureName(_shaderToken);

    // Get size of source texture
    id<MTLTexture> sourceTexture = _sourceGLTextureName;
    uint32_t srcWidth  = sourceTexture.width;
    uint32_t srcHeight = sourceTexture.height;

    // Size of texture to be created.
    const uint32_t width  = srcWidth  / 2;
    const uint32_t height = srcHeight / 2;
    
    // Computation for level 0 is responsible for freeing/allocating
    // the texture.
    if (_level == 0) {
        if (dstGLTextureName) {
            // Free previously allocated texture.
            [dstGLTextureName release];
        }

        // Create new texture.
        dstGLTextureName = _CreateGLTexture(width, height);
        // And set on shader.
        shader->SetGLTextureName(_shaderToken, dstGLTextureName);
    }

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
    MTLSize threadsPerGrid   = MTLSizeMake(([dstGLTextureName width] + (threadgroupCount.width - 1)) / threadgroupCount.width,
                                           ([dstGLTextureName height] + (threadgroupCount.height - 1)) / threadgroupCount.height,
                                           1);

    id<MTLCommandBuffer> commandBuffer = [context->gpus.commandQueue commandBuffer];
    id<MTLComputeCommandEncoder> computeEncoder = [commandBuffer computeCommandEncoder];
    
    [computeEncoder setComputePipelineState:pipelineState];
    [computeEncoder useResource:dstGLTextureName usage:MTLResourceUsageWrite];
    
    context->SetComputeEncoderState(computeEncoder);

    [computeEncoder setBytes:(const void *)&_uniforms
                      length:sizeof(_uniforms)
                     atIndex:0];
    
    [computeEncoder setTexture:sourceTexture
                       atIndex:0];
    [computeEncoder setTexture:dstGLTextureName
                       atIndex:1];

    [computeEncoder dispatchThreadgroups:threadsPerGrid threadsPerThreadgroup:threadgroupCount];
    
    [computeEncoder endEncoding];
    
    if (_numLevels > 1) {
        // Generate the rest of the mip chain
        id<MTLBlitCommandEncoder> blitEncoder = [commandBuffer blitCommandEncoder];
        
        [blitEncoder generateMipmapsForTexture:dstGLTextureName];
        [blitEncoder endEncoding];
    }
    
    [commandBuffer commit];
}

PXR_NAMESPACE_CLOSE_SCOPE
