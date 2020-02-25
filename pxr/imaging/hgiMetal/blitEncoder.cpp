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
#include "pxr/imaging/hgiMetal/conversions.h"
#include "pxr/imaging/hgiMetal/diagnostic.h"
#include "pxr/imaging/hgiMetal/texture.h"
#include "pxr/imaging/hgi/blitEncoderOps.h"

PXR_NAMESPACE_OPEN_SCOPE

HgiMetalBlitEncoder::HgiMetalBlitEncoder(
    HgiMetalImmediateCommandBuffer* cmdBuf)
    : HgiBlitEncoder()
    , _commandBuffer(cmdBuf)
{

}

HgiMetalBlitEncoder::~HgiMetalBlitEncoder()
{
}

void
HgiMetalBlitEncoder::EndEncoding()
{
}

void
HgiMetalBlitEncoder::PushDebugGroup(const char* label)
{
}

void
HgiMetalBlitEncoder::PopDebugGroup()
{
}

void 
HgiMetalBlitEncoder::CopyTextureGpuToCpu(
    HgiTextureGpuToCpuOp const& copyOp)
{
    HgiMetalTexture* srcTexture =
        static_cast<HgiMetalTexture*>(copyOp.gpuSourceTexture);

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
        metalFormat = HgiMetalConversions::GetFormat(texDesc.format);
    } else if (texDesc.usage & HgiTextureUsageBitsDepthTarget) {
        TF_VERIFY(texDesc.format == HgiFormatFloat32);
        metalFormat = MTLPixelFormatDepth32Float;
    } else {
        TF_CODING_ERROR("Unknown HgTextureUsage bit");
    }

    // Make sure writes are finished before we read from the texture
    //
    // XXX If we issue all the right commands, this barrier would have already
    // been issued by HdSt, but for now we do it here. This may introduce a
    // unneccesairy performance hit, so we should remove this when we
    // fully record fence/barrier/sempahores in command buffers / RenderPasses.
    //
/*
    glMemoryBarrier(GL_ALL_BARRIER_BITS);

    glGetTextureSubImage(
        srcTextureGL->GetTextureId(),
        0, // mip level
        copyOp.sourceByteOffset[0], // x offset
        copyOp.sourceByteOffset[1], // y offset
        copyOp.sourceByteOffset[2], // z offset
        copyOp.dimensions[0], // width
        copyOp.dimensions[1], // height
        copyOp.dimensions[2], // layerCnt
        glFormat,
        glPixelType,
        copyOp.destinationBufferByteSize,
        copyOp.cpuDestinationBuffer);
*/
    /*
    id<MTLDevice> device = nil;
     
    context->CreateCommandBuffer();
    context->LabelCommandBuffer(@"Get Image");
    id<MTLBlitCommandEncoder> blitEncoder = context->GetBlitEncoder();
    
    MtlfMetalContext::MtlfMultiBuffer const &cpuBuffer = context->GetMetalBuffer((bytesPerPixel * width * height), MTLResourceStorageModeDefault);
    
    [blitEncoder copyFromTexture:texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0) sourceSize:MTLSizeMake(width, height, 1) toBuffer:cpuBuffer.forCurrentGPU() destinationOffset:0 destinationBytesPerRow:(bytesPerPixel * width) destinationBytesPerImage:(bytesPerPixel * width * height) options:blitOptions];
#if defined(ARCH_OS_MACOS)
    [blitEncoder synchronizeResource:cpuBuffer.forCurrentGPU()];
#endif

    context->ReleaseEncoder(true);
    context->CommitCommandBufferForThread(false, true);

    memcpy(buffer, [cpuBuffer.forCurrentGPU() contents], bytesPerPixel * width * height);
    context->ReleaseMetalBuffer(cpuBuffer);
     */
}

void 
HgiMetalBlitEncoder::ResolveImage(
    HgiResolveImageOp const& resolveOp)
{
    // Create framebuffers for resolve.
    /*
    uint32_t readFramebuffer;
    uint32_t writeFramebuffer;
    glCreateFramebuffers(1, &readFramebuffer);
    glCreateFramebuffers(1, &writeFramebuffer);

    // Gather source and destination textures
    HgiMetalTexture const* metalSrcTexture =
        static_cast<HgiMetalTexture const*>(resolveOp.source);
    HgiMetalTexture const* metalDstTexture =
        static_cast<HgiMetalTexture const*>(resolveOp.destination);

    if (!metalSrcTexture || !metalDstTexture) {
        TF_CODING_ERROR("No textures provided for resolve");            
        return;
    }

    id<MTLTexture> readAttachment = metalSrcTexture->GetTextureId();
    TF_VERIFY(glIsTexture(readAttachment), "Source is not a texture");
    id<MTLTexture> writeAttachment = metalDstTexture->GetTextureId();
    TF_VERIFY(glIsTexture(writeAttachment), "Destination is not a texture");

    // Update framebuffer bindings
    if (resolveOp.usage & HgiTextureUsageBitsDepthTarget) {
        // Depth-only, so no color attachments for read or write
        // Clear previous color attachment since all attachments must be
        // written to from fragment shader or texels will be undefined.

        GLenum drawBufs[1] = {GL_NONE};
        glNamedFramebufferDrawBuffers(
            readFramebuffer, 1, drawBufs);
        glNamedFramebufferDrawBuffers(
            writeFramebuffer, 1, drawBufs);

        glNamedFramebufferTexture(
            readFramebuffer, GL_COLOR_ATTACHMENT0, 0, 0);
        glNamedFramebufferTexture(
            writeFramebuffer, GL_COLOR_ATTACHMENT0, 0, 0);

        glNamedFramebufferTexture(
            readFramebuffer,
            GL_DEPTH_ATTACHMENT,
            readAttachment,
            0);
        glNamedFramebufferTexture(
            writeFramebuffer,
            GL_DEPTH_ATTACHMENT,
            writeAttachment,
            0);
    } else {
        // Color-only, so no depth attachments for read or write.
        // Clear previous depth attachment since all attachments must be
        // written to from fragment shader or texels will be undefined.

        GLenum drawBufs[1] = {GL_COLOR_ATTACHMENT0};
        glNamedFramebufferDrawBuffers(
            readFramebuffer, 1, drawBufs);
        glNamedFramebufferDrawBuffers(
            writeFramebuffer, 1, drawBufs);

        glNamedFramebufferTexture(
            readFramebuffer, GL_DEPTH_ATTACHMENT, 0, 0);
        glNamedFramebufferTexture(
            writeFramebuffer, GL_DEPTH_ATTACHMENT, 0, 0);

        glNamedFramebufferTexture(
            readFramebuffer, 
            GL_COLOR_ATTACHMENT0, 
            readAttachment, 
            0);
        glNamedFramebufferTexture(
            writeFramebuffer, 
            GL_COLOR_ATTACHMENT0, 
            writeAttachment,
            0);
    }

    GLenum status = glCheckNamedFramebufferStatus(readFramebuffer,
                                                  GL_READ_FRAMEBUFFER);
    TF_VERIFY(status == GL_FRAMEBUFFER_COMPLETE);

    status = glCheckNamedFramebufferStatus(writeFramebuffer,
                                           GL_DRAW_FRAMEBUFFER);
    TF_VERIFY(status == GL_FRAMEBUFFER_COMPLETE);

    // Resolve MSAA fbo to a regular fbo
    GLbitfield mask = (resolveOp.usage & HgiTextureUsageBitsDepthTarget) ? 
            GL_DEPTH_BUFFER_BIT : GL_COLOR_BUFFER_BIT;

    const GfVec4i& src = resolveOp.sourceRegion;
    const GfVec4i& dst = resolveOp.destinationRegion;

    // Bind resolve framebuffer
    GLint restoreRead, restoreWrite;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &restoreRead);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &restoreWrite);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, readFramebuffer); // MS
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, writeFramebuffer);// regular

    glBlitFramebuffer(
        src[0], src[1], src[2], src[3], 
        dst[0], dst[1], dst[2], dst[3], 
        mask, 
        GL_NEAREST);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, restoreRead);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, restoreWrite);

    glDeleteFramebuffers(1, &readFramebuffer);
    glDeleteFramebuffers(1, &writeFramebuffer);
*/
}


PXR_NAMESPACE_CLOSE_SCOPE
