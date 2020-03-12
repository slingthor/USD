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
#include <Metal/Metal.h>

#include "pxr/imaging/hgiMetal/blitEncoder.h"
#include "pxr/imaging/hgiMetal/buffer.h"
#include "pxr/imaging/hgiMetal/immediateCommandBuffer.h"
#include "pxr/imaging/hgiMetal/conversions.h"
#include "pxr/imaging/hgiMetal/diagnostic.h"
#include "pxr/imaging/hgiMetal/texture.h"
#include "pxr/imaging/hgi/blitEncoderOps.h"
#include "pxr/imaging/hgi/types.h"

#include "pxr/base/arch/defines.h"

PXR_NAMESPACE_OPEN_SCOPE

HgiMetalBlitEncoder::HgiMetalBlitEncoder(
    HgiMetalImmediateCommandBuffer* cmdBuf)
    : HgiBlitEncoder()
    , _commandBuffer(cmdBuf)
{
    id<MTLDevice> device = _commandBuffer->GetDevice();
    _blitEncoder = [_commandBuffer->GetCommandBuffer() blitCommandEncoder];
}

HgiMetalBlitEncoder::~HgiMetalBlitEncoder()
{
}

void
HgiMetalBlitEncoder::EndEncoding()
{
    [_blitEncoder endEncoding];
}

void
HgiMetalBlitEncoder::PushDebugGroup(const char* label)
{
    HGIMETAL_DEBUG_LABEL(_blitEncoder, label)
}

void
HgiMetalBlitEncoder::PopDebugGroup()
{
}

void 
HgiMetalBlitEncoder::CopyTextureGpuToCpu(
    HgiTextureGpuToCpuOp const& copyOp)
{
    HgiTextureHandle texHandle = copyOp.gpuSourceTexture;
    HgiMetalTexture* srcTexture = static_cast<HgiMetalTexture*>(texHandle.Get());

    if (!TF_VERIFY(srcTexture && srcTexture->GetTextureId(),
        "Invalid texture handle")) {
        return;
    }

    if (copyOp.destinationBufferByteSize == 0) {
        TF_WARN("The size of the data to copy was zero (aborted)");
        return;
    }

    HgiTextureDesc const& texDesc = srcTexture->GetDescriptor();

    uint32_t layerCnt = copyOp.startLayer + copyOp.numLayers;
    if (!TF_VERIFY(texDesc.layerCount >= layerCnt,
        "Texture has less layers than attempted to be copied")) {
        return;
    }

    MTLPixelFormat metalFormat = MTLPixelFormatInvalid;

    if (texDesc.usage & HgiTextureUsageBitsColorTarget) {
        metalFormat = HgiMetalConversions::GetPixelFormat(texDesc.format);
    } else if (texDesc.usage & HgiTextureUsageBitsDepthTarget) {
        TF_VERIFY(texDesc.format == HgiFormatFloat32);
        metalFormat = MTLPixelFormatDepth32Float;
    } else {
        TF_CODING_ERROR("Unknown HgTextureUsage bit");
    }

    id<MTLDevice> device = _commandBuffer->GetDevice();

#if defined(ARCH_OS_MACOS)
    MTLResourceOptions options = MTLResourceStorageModeManaged;
#else
    MTLResourceOptions options = MTLResourceStorageModeShared;
#endif

    size_t bytesPerPixel = HgiDataSizeOfFormat(texDesc.format);
    id<MTLBuffer> cpuBuffer =
        [device newBufferWithBytesNoCopy:copyOp.cpuDestinationBuffer
                                  length:copyOp.destinationBufferByteSize
                                 options:options
                             deallocator:nil];

    MTLOrigin origin = MTLOriginMake(
        copyOp.sourceTexelOffset[0],
        copyOp.sourceTexelOffset[1],
        copyOp.sourceTexelOffset[2]);
    MTLSize size = MTLSizeMake(
        texDesc.dimensions[0] - copyOp.sourceTexelOffset[0],
        texDesc.dimensions[1] - copyOp.sourceTexelOffset[1],
        texDesc.dimensions[2] - copyOp.sourceTexelOffset[2]);
    
    MTLBlitOption blitOptions = MTLBlitOptionNone;

    [_blitEncoder copyFromTexture:srcTexture->GetTextureId()
                      sourceSlice:0
                      sourceLevel:copyOp.startLayer
                     sourceOrigin:origin
                       sourceSize:size
                         toBuffer:cpuBuffer
                destinationOffset:0
           destinationBytesPerRow:(bytesPerPixel * texDesc.dimensions[0])
         destinationBytesPerImage:(bytesPerPixel * texDesc.dimensions[0] *
                                   texDesc.dimensions[1] *
                                   texDesc.dimensions[2])
                          options:blitOptions];
#if defined(ARCH_OS_MACOS)
    [_blitEncoder synchronizeResource:cpuBuffer];
#endif
    [cpuBuffer release];
}

void HgiMetalBlitEncoder::CopyBufferCpuToGpu(
    HgiBufferCpuToGpuOp const& copyOp)
{
    if (copyOp.byteSize == 0 ||
        !copyOp.cpuSourceBuffer ||
        !copyOp.gpuDestinationBuffer)
    {
        return;
    }

    HgiMetalBuffer* metalBuffer = static_cast<HgiMetalBuffer*>(
        copyOp.gpuDestinationBuffer.Get());

    // Offset into the src buffer
    const char* src = ((const char*) copyOp.cpuSourceBuffer) +
        copyOp.sourceByteOffset;

    // Offset into the dst buffer
    size_t dstOffset = copyOp.destinationByteOffset;
    uint8_t *dst = static_cast<uint8_t*>([metalBuffer->GetBufferId() contents]);
    memcpy(dst + dstOffset, src, copyOp.byteSize);
#if defined(ARCH_OS_MACOS)
    [metalBuffer->GetBufferId()
        didModifyRange:NSMakeRange(dstOffset, copyOp.byteSize)];
#endif
}

void 
HgiMetalBlitEncoder::ResolveImage(
    HgiResolveImageOp const& resolveOp)
{
    // This is totally temporary and only works because MSAA is actually
    // disabled in HgiMetalTexture at present.

    // Gather source and destination textures
    HgiMetalTexture* metalSrcTexture = static_cast<HgiMetalTexture*>(
        resolveOp.source.Get());
    HgiMetalTexture* metalDstTexture = static_cast<HgiMetalTexture*>(
        resolveOp.destination.Get());

    if (!metalSrcTexture || !metalDstTexture) {
        TF_CODING_ERROR("No textures provided for resolve");
        return;
    }

    [_blitEncoder copyFromTexture:metalSrcTexture->GetTextureId()
                        toTexture:metalDstTexture->GetTextureId()];
}


PXR_NAMESPACE_CLOSE_SCOPE
