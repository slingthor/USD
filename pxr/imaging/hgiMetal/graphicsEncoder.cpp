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
#include "pxr/imaging/hgi/graphicsEncoderDesc.h"
#include "pxr/imaging/hgiMetal/buffer.h"
#include "pxr/imaging/hgiMetal/conversions.h"
#include "pxr/imaging/hgiMetal/diagnostic.h"
#include "pxr/imaging/hgiMetal/graphicsEncoder.h"
#include "pxr/imaging/hgiMetal/pipeline.h"
#include "pxr/imaging/hgiMetal/resourceBindings.h"
#include "pxr/imaging/hgiMetal/texture.h"

#include "pxr/base/arch/defines.h"

PXR_NAMESPACE_OPEN_SCOPE

HgiMetalGraphicsEncoder::HgiMetalGraphicsEncoder(
    id<MTLCommandBuffer> commandBuffer,
    HgiGraphicsEncoderDesc const& desc)
    : HgiGraphicsEncoder()
{
    TF_VERIFY(desc.width>0 && desc.height>0);
    TF_VERIFY(desc.colorTextures.size() == desc.colorAttachmentDescs.size());
    
    MTLRenderPassDescriptor *renderPassDescriptor =
        [[MTLRenderPassDescriptor alloc] init];

    // Color attachments
    for (size_t i=0; i<desc.colorAttachmentDescs.size(); i++) {
        HgiAttachmentDesc const &hgiColorAttachment =
            desc.colorAttachmentDescs[i];
        MTLRenderPassColorAttachmentDescriptor *metalColorAttachment =
            renderPassDescriptor.colorAttachments[i];
        
        if (@available(ios 8.0, *)) {
            metalColorAttachment.loadAction = MTLLoadActionLoad;
        }
        else {
            metalColorAttachment.loadAction =
                HgiMetalConversions::GetAttachmentLoadOp(
                    hgiColorAttachment.loadOp);
        }

        metalColorAttachment.storeAction =
            HgiMetalConversions::GetAttachmentStoreOp(
                hgiColorAttachment.storeOp);
        if (hgiColorAttachment.loadOp == HgiAttachmentLoadOpClear) {
            GfVec4f const& clearColor = hgiColorAttachment.clearValue;
            metalColorAttachment.clearColor =
                MTLClearColorMake(
                    clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
        }
        
        HgiMetalTexture *colorTexture =
            static_cast<HgiMetalTexture*>(desc.colorTextures[i].Get());

        TF_VERIFY(
            colorTexture->GetDescriptor().format == hgiColorAttachment.format);
        metalColorAttachment.texture = colorTexture->GetTextureId();
    }

    // Depth attachment
    if (desc.depthTexture) {
        HgiAttachmentDesc const &hgiDepthAttachment =
            desc.depthAttachmentDesc;
        MTLRenderPassDepthAttachmentDescriptor *metalDepthAttachment =
            renderPassDescriptor.depthAttachment;

        metalDepthAttachment.loadAction =
            HgiMetalConversions::GetAttachmentLoadOp(
                hgiDepthAttachment.loadOp);;
        metalDepthAttachment.storeAction =
            HgiMetalConversions::GetAttachmentStoreOp(
                hgiDepthAttachment.storeOp);
        
        metalDepthAttachment.clearDepth = hgiDepthAttachment.clearValue[0];
        
        HgiMetalTexture *depthTexture =
            static_cast<HgiMetalTexture*>(desc.depthTexture.Get());
        
        TF_VERIFY(
            depthTexture->GetDescriptor().format == hgiDepthAttachment.format);
        metalDepthAttachment.texture = depthTexture->GetTextureId();
    }

    _encoder = [commandBuffer
        renderCommandEncoderWithDescriptor:renderPassDescriptor];
    [renderPassDescriptor release];
}

HgiMetalGraphicsEncoder::~HgiMetalGraphicsEncoder()
{
}

void
HgiMetalGraphicsEncoder::EndEncoding()
{
    [_encoder endEncoding];
}

void
HgiMetalGraphicsEncoder::SetViewport(GfVec4i const& vp)
{
    double x = vp[0];
    double y = vp[1];
    double w = vp[2];
    double h = vp[3];
    [_encoder setViewport:(MTLViewport){x, y, w, h, 0.0, 1.0}];
}

void
HgiMetalGraphicsEncoder::SetScissor(GfVec4i const& sc)
{
    uint32_t x = sc[0];
    uint32_t y = sc[1];
    uint32_t w = sc[2];
    uint32_t h = sc[3];
    [_encoder setScissorRect:(MTLScissorRect){x, y, w, h}];
}

void
HgiMetalGraphicsEncoder::BindPipeline(HgiPipelineHandle pipeline)
{
    if (HgiMetalPipeline* p = static_cast<HgiMetalPipeline*>(pipeline.Get())) {
        p->BindPipeline(_encoder);
    }
}

void
HgiMetalGraphicsEncoder::BindResources(HgiResourceBindingsHandle r)
{
    if (HgiMetalResourceBindings* rb=
        static_cast<HgiMetalResourceBindings*>(r.Get()))
    {
        rb->BindResources(_encoder);
    }
}

void
HgiMetalGraphicsEncoder::BindVertexBuffers(
    uint32_t firstBinding,
    HgiBufferHandleVector const& vertexBuffers,
    std::vector<uint32_t> const& byteOffsets)
{
    TF_VERIFY(byteOffsets.size() == vertexBuffers.size());
    TF_VERIFY(byteOffsets.size() == vertexBuffers.size());

    for (size_t i=0; i<vertexBuffers.size(); i++) {
        HgiBufferHandle bufHandle = vertexBuffers[i];
        HgiMetalBuffer* buf = static_cast<HgiMetalBuffer*>(bufHandle.Get());
        HgiBufferDesc const& desc = buf->GetDescriptor();

        TF_VERIFY(desc.usage & HgiBufferUsageVertex);
        
        [_encoder setVertexBuffer:buf->GetBufferId()
                           offset:byteOffsets[i]
                          atIndex:firstBinding + i];
    }
}

void
HgiMetalGraphicsEncoder::DrawIndexed(
    HgiBufferHandle const& indexBuffer,
    uint32_t indexCount,
    uint32_t indexBufferByteOffset,
    uint32_t vertexOffset,
    uint32_t instanceCount,
    uint32_t firstInstance)
{
    TF_VERIFY(instanceCount>0);

    HgiMetalBuffer* indexBuf = static_cast<HgiMetalBuffer*>(indexBuffer.Get());
    HgiBufferDesc const& indexDesc = indexBuf->GetDescriptor();

    // We assume 32bit indices: GL_UNSIGNED_INT
    TF_VERIFY(indexDesc.usage & HgiBufferUsageIndex32);
    
    [_encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                         indexCount:indexCount
                          indexType:MTLIndexTypeUInt32
                        indexBuffer:indexBuf->GetBufferId()
                  indexBufferOffset:indexBufferByteOffset];
}

void
HgiMetalGraphicsEncoder::PushDebugGroup(const char* label)
{
    HGIMETAL_DEBUG_LABEL(_encoder, label)
}

void
HgiMetalGraphicsEncoder::PopDebugGroup()
{
}


PXR_NAMESPACE_CLOSE_SCOPE
