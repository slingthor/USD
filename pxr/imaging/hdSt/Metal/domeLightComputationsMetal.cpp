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
#include "pxr/imaging/hdSt/dynamicUvTextureObject.h"
#include "pxr/imaging/hdSt/textureHandle.h"
#include "pxr/imaging/hdSt/tokens.h"

#include "pxr/imaging/hdSt/Metal/glslProgramMetal.h"
#include "pxr/imaging/hgiMetal/texture.h"

#include "pxr/base/tf/token.h"

PXR_NAMESPACE_OPEN_SCOPE


HdSt_DomeLightComputationGPUMetal::HdSt_DomeLightComputationGPUMetal(
    const TfToken & shaderToken,
    HdStSimpleLightingShaderPtr const &lightingShader,
    unsigned int numLevels,
    unsigned int level,
    float roughness)
    : HdSt_DomeLightComputationGPU(
       shaderToken,
       lightingShader,
       numLevels,
       level,
       roughness)
{
}

GarchTextureGPUHandle
HdSt_DomeLightComputationGPUMetal::_GetGlTextureName(const HgiTexture * const hgiTexture)
{
    const HgiMetalTexture * const metalTexture =
        dynamic_cast<const HgiMetalTexture*>(hgiTexture);
    if (!metalTexture) {
        TF_CODING_ERROR(
            "Texture in dome light computation is not HgiMetalTexture");
        return GarchTextureGPUHandle();
    }
    const GarchTextureGPUHandle textureName = metalTexture->GetTextureId();
    if (!textureName.IsSet()) {
        TF_CODING_ERROR(
            "Texture in dome light computation has zero GL name");
    }
    return textureName;
}

void
HdSt_DomeLightComputationGPUMetal::_Execute(HdStGLSLProgramSharedPtr computeProgram)
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
    HdStGLSLProgramMSLSharedPtr const &mslProgram(std::dynamic_pointer_cast<HdStGLSLProgramMSL>(computeProgram));

    // Size of source texture (the dome light environment map)
    GfVec3i srcDim;
    // GL name of source texture
    GarchTextureGPUHandle srcGLTextureName;
    if (!_GetSrcTextureDimensionsAndGLName(
            shader, &srcDim, &srcGLTextureName)) {
        return;
    }

    // Size of texture to be created.
    const GLint width  = srcDim[0] / 2;
    const GLint height = srcDim[1] / 2;

    // Get texture object from lighting shader that this
    // computation is supposed to populate
    HdStTextureHandleSharedPtr const &dstTextureHandle =
        shader->GetTextureHandle(_shaderToken);

    if (!TF_VERIFY(dstTextureHandle)) {
        return;
    }

    HdStDynamicUvTextureObject * const dstUvTextureObject =
      dynamic_cast<HdStDynamicUvTextureObject*>(
          dstTextureHandle->GetTextureObject().get());
    if (!TF_VERIFY(dstUvTextureObject)) {
        return;
    }

    if (_level == 0) {
        // Level zero is in charge of actually creating the
        // GPU resource.
        HgiTextureDesc desc;
        desc.debugName = _shaderToken.GetText();
        desc.format = HgiFormatFloat16Vec4;
        desc.dimensions = GfVec3i(width, height, 1);
        desc.layerCount = 1;
        desc.mipLevels = _numLevels;
        desc.usage =
            HgiTextureUsageBitsShaderRead | HgiTextureUsageBitsShaderWrite;
        _FillPixelsByteSize(&desc);
        dstUvTextureObject->CreateTexture(desc);
    }

    const GarchTextureGPUHandle dstGLTextureName = _GetGlTextureName(
        dstUvTextureObject->GetTexture().Get());

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
    
    [computeEncoder setTexture:srcGLTextureName
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
