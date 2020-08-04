//
// Copyright 2020 Pixar
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
#include "pxr/imaging/hgiGL/ops.h"
#include "pxr/imaging/hgiGL/buffer.h"
#include "pxr/imaging/hgiGL/computePipeline.h"
#include "pxr/imaging/hgiGL/conversions.h"
#include "pxr/imaging/hgiGL/diagnostic.h"
#include "pxr/imaging/hgiGL/graphicsCmds.h"
#include "pxr/imaging/hgiGL/graphicsPipeline.h"
#include "pxr/imaging/hgiGL/resourceBindings.h"
#include "pxr/imaging/hgiGL/shaderProgram.h"
#include "pxr/imaging/hgiGL/texture.h"

PXR_NAMESPACE_OPEN_SCOPE


HgiGLOpsFn
HgiGLOps::PushDebugGroup(const char* label)
{
    // Make copy of string string since the lamda will execute later.
    std::string lbl = label;

    return [lbl] {
        #if defined(GL_KHR_debug)
        if (GLEW_KHR_debug) {
            glPushDebugGroup(GL_DEBUG_SOURCE_THIRD_PARTY, 0, -1, lbl.c_str());
        }
        #endif
    };
}

HgiGLOpsFn
HgiGLOps::PopDebugGroup()
{
    return [] {
        #if defined(GL_KHR_debug)
        if (GLEW_KHR_debug) {
            glPopDebugGroup();
        }
        #endif
    };
}

HgiGLOpsFn
HgiGLOps::CopyTextureGpuToCpu(HgiTextureGpuToCpuOp const& copyOp)
{
    return [copyOp] {
        HgiTextureHandle texHandle = copyOp.gpuSourceTexture;
        HgiGLTexture* srcTexture = static_cast<HgiGLTexture*>(texHandle.Get());

        if (!TF_VERIFY(srcTexture && srcTexture->GetTextureId(),
            "Invalid texture handle")) {
            return;
        }

        if (copyOp.destinationBufferByteSize == 0) {
            TF_WARN("The size of the data to copy was zero (aborted)");
            return;
        }

        HgiTextureDesc const& texDesc = srcTexture->GetDescriptor();

    if (!TF_VERIFY(texDesc.layerCount > copyOp.sourceTexelOffset[2],
        "Trying to copy an invalid texture layer/slice")) {
        return;
    }

        GLenum glFormat = 0;
        GLenum glPixelType = 0;

        if (texDesc.usage & HgiTextureUsageBitsDepthTarget) {
            TF_VERIFY(texDesc.format == HgiFormatFloat32 ||
                      texDesc.format == HgiFormatFloat32UInt8);
            // XXX: Copy only the depth component. To copy stencil, we'd need
            // to set the format to GL_STENCIL_INDEX separately..
            glFormat = GL_DEPTH_COMPONENT;
            glPixelType = GL_FLOAT;
        } else if (texDesc.usage & HgiTextureUsageBitsStencilTarget) {
            TF_WARN("Copying a stencil-only texture is unsupported currently\n"
                   );
            return;
        } else {
            HgiGLConversions::GetFormat(
                texDesc.format,
                &glFormat,
                &glPixelType);
        }

        if (HgiIsCompressed(texDesc.format)) {
            TF_CODING_ERROR(
                "Copying from compressed GPU texture not supported.");
            return;
        }
        
        // Make sure writes are finished before we read from the texture
        glMemoryBarrier(GL_ALL_BARRIER_BITS);

        glGetTextureSubImage(
            srcTexture->GetTextureId(),
            copyOp.mipLevel,
            copyOp.sourceTexelOffset[0], // x offset
            copyOp.sourceTexelOffset[1], // y offset
            copyOp.sourceTexelOffset[2], // z offset (depth or layer)
            texDesc.dimensions[0], // width
            texDesc.dimensions[1], // height
            texDesc.dimensions[2], // layerCnt or depth
            glFormat,
            glPixelType,
            copyOp.destinationBufferByteSize,
            copyOp.cpuDestinationBuffer);

        HGIGL_POST_PENDING_GL_ERRORS();
    };
}

HgiGLOpsFn
HgiGLOps::CopyTextureCpuToGpu(HgiTextureCpuToGpuOp const& copyOp)
{
    return [copyOp] {
        HgiTextureDesc const& desc =
            copyOp.gpuDestinationTexture->GetDescriptor();

        GLenum internalFormat = 0;
        GLenum format = 0;
        GLenum type = 0;

        HgiGLConversions::GetFormat(desc.format,&format,&type,&internalFormat);

        const bool isCompressed = HgiIsCompressed(desc.format);
        GfVec3i const& offsets = copyOp.destinationTexelOffset;
        GfVec3i const& dimensions = desc.dimensions;

        HgiGLTexture* dstTexture = static_cast<HgiGLTexture*>(
            copyOp.gpuDestinationTexture.Get());

        switch(desc.type) {
        case HgiTextureType2D:
            if (isCompressed) {
                glCompressedTextureSubImage2D(
                    dstTexture->GetTextureId(),
                    copyOp.mipLevel,
                    offsets[0], offsets[1],
                    dimensions[0], dimensions[1],
                    format,
                    copyOp.bufferByteSize,
                    copyOp.cpuSourceBuffer);
            } else {
                glTextureSubImage2D(
                    dstTexture->GetTextureId(),
                    copyOp.mipLevel,
                    offsets[0], offsets[1],
                    dimensions[0], dimensions[1],
                    format,
                    type,
                    copyOp.cpuSourceBuffer);
            }
            break;
        case HgiTextureType3D:
            if (isCompressed) {
                glCompressedTextureSubImage3D(
                    dstTexture->GetTextureId(),
                    copyOp.mipLevel,
                    offsets[0], offsets[1], offsets[2],
                    dimensions[0], dimensions[1], dimensions[2],
                    format,
                    copyOp.bufferByteSize,
                    copyOp.cpuSourceBuffer);
            } else {
                glTextureSubImage3D(
                    dstTexture->GetTextureId(),
                    copyOp.mipLevel,
                    offsets[0], offsets[1], offsets[2],
                    dimensions[0], dimensions[1], dimensions[2],
                    format,
                    type,
                    copyOp.cpuSourceBuffer);
            }
            break;
        default:
            TF_CODING_ERROR("Unsupported HgiTextureType enum value");
            break;
        }

        // Make sure the copy is finished before reads from texture.
        glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);

        HGIGL_POST_PENDING_GL_ERRORS();
    };
}

HgiGLOpsFn
HgiGLOps::CopyBufferGpuToGpu(HgiBufferGpuToGpuOp const& copyOp)
{
    return [copyOp] {
        HgiBufferHandle const& srcBufHandle = copyOp.gpuSourceBuffer;

        if (!TF_VERIFY(srcBufHandle && srcBufHandle->GetRawResource(),
            "Invalid source buffer handle")) {
            return;
        }

        HgiBufferHandle const& dstBufHandle = copyOp.gpuDestinationBuffer;

        if (!TF_VERIFY(dstBufHandle && dstBufHandle->GetRawResource(),
            "Invalid destination buffer handle")) {
            return;
        }

        if (copyOp.byteSize == 0) {
            TF_WARN("The size of the data to copy was zero (aborted)");
            return;
        }

        glCopyNamedBufferSubData(srcBufHandle->GetRawResource(),
                                 dstBufHandle->GetRawResource(),
                                 copyOp.sourceByteOffset,
                                 copyOp.destinationByteOffset,
                                 copyOp.byteSize);
    };
}

HgiGLOpsFn 
HgiGLOps::CopyBufferCpuToGpu(HgiBufferCpuToGpuOp const& copyOp)
{
    return [copyOp] {
        if (copyOp.byteSize == 0 ||
            !copyOp.cpuSourceBuffer ||
            !copyOp.gpuDestinationBuffer)
        {
            return;
        }
        
        HgiBufferHandle const& dstBufHandle = copyOp.gpuDestinationBuffer;

        // Offset into the src buffer
        const char* src = ((const char*) copyOp.cpuSourceBuffer) +
            copyOp.sourceByteOffset;

        // Offset into the dst buffer
        GLintptr dstOffset = copyOp.destinationByteOffset;

        glNamedBufferSubData(
            dstBufHandle->GetRawResource(),
            dstOffset,
            copyOp.byteSize,
            src);

        // Make sure the copy is finished before reads from buffer.
        glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);

        HGIGL_POST_PENDING_GL_ERRORS();
    };
}

HgiGLOpsFn
HgiGLOps::SetViewport(GfVec4i const& vp)
{
    return [vp] {
        glViewport(vp[0], vp[1], vp[2], vp[3]);
    };
}

HgiGLOpsFn
HgiGLOps::SetScissor(GfVec4i const& sc)
{
    return [sc] {
        glScissor(sc[0], sc[1], sc[2], sc[3]);
    };
}

HgiGLOpsFn
HgiGLOps::BindPipeline(HgiGraphicsPipelineHandle pipeline)
{
    return [pipeline] {
        if (HgiGLGraphicsPipeline* p = static_cast<HgiGLGraphicsPipeline*>(pipeline.Get())) {
            p->BindPipeline();
        }
    };
}

HgiGLOpsFn
HgiGLOps::BindPipeline(HgiComputePipelineHandle pipeline)
{
    return [pipeline] {
        if (HgiGLComputePipeline* p = static_cast<HgiGLComputePipeline*>(pipeline.Get())) {
            p->BindPipeline();
        }
    };
}

HgiGLOpsFn
HgiGLOps::BindResources(HgiResourceBindingsHandle res)
{
    return [res] {
        if (HgiGLResourceBindings* rb =
            static_cast<HgiGLResourceBindings*>(res.Get()))
        {
            rb->BindResources();
        }
    };
}

HgiGLOpsFn
HgiGLOps::SetConstantValues(
    HgiGraphicsPipelineHandle pipeline,
    HgiShaderStage stages,
    uint32_t bindIndex,
    uint32_t byteSize,
    const void* data)
{
    return [pipeline, bindIndex, byteSize, data] {
        HgiGLShaderProgram* glProgram =
            static_cast<HgiGLShaderProgram*>(
                pipeline->GetDescriptor().shaderProgram.Get());
        uint32_t ubo = glProgram->GetUniformBuffer(byteSize);
        glNamedBufferData(ubo, byteSize, data, GL_STATIC_DRAW);
        glBindBufferBase(GL_UNIFORM_BUFFER, bindIndex, ubo);
    };
}

HgiGLOpsFn
HgiGLOps::SetConstantValues(
    HgiComputePipelineHandle pipeline,
    uint32_t bindIndex,
    uint32_t byteSize,
    const void* data)
{
    return [pipeline, bindIndex, byteSize, data] {
        HgiGLShaderProgram* glProgram =
            static_cast<HgiGLShaderProgram*>(
                pipeline->GetDescriptor().shaderProgram.Get());
        uint32_t ubo = glProgram->GetUniformBuffer(byteSize);
        glNamedBufferData(ubo, byteSize, data, GL_STATIC_DRAW);
        glBindBufferBase(GL_UNIFORM_BUFFER, bindIndex, ubo);
    };
}

HgiGLOpsFn
HgiGLOps::BindVertexBuffers(
    uint32_t firstBinding,
    HgiBufferHandleVector const& vertexBuffers,
    std::vector<uint32_t> const& byteOffsets)
{
    return [firstBinding, vertexBuffers, byteOffsets] {
        TF_VERIFY(byteOffsets.size() == vertexBuffers.size());
        TF_VERIFY(byteOffsets.size() == vertexBuffers.size());

        // XXX use glBindVertexBuffers to bind all VBs in one go.
        for (size_t i=0; i<vertexBuffers.size(); i++) {
            HgiBufferHandle const& bufHandle = vertexBuffers[i];
            HgiBufferDesc const& desc = bufHandle->GetDescriptor();

            TF_VERIFY(desc.usage & HgiBufferUsageVertex);

            glBindVertexBuffer(
                firstBinding + i,
                bufHandle->GetRawResource(),
                byteOffsets[i],
                desc.vertexStride);
        }

        HGIGL_POST_PENDING_GL_ERRORS();
    };
}

HgiGLOpsFn
HgiGLOps::DrawIndexed(
    HgiPrimitiveType primitiveType,
    HgiBufferHandle const& indexBuffer,
    uint32_t indexCount,
    uint32_t indexBufferByteOffset,
    uint32_t vertexOffset,
    uint32_t instanceCount,
    uint32_t firstInstance)
{
    return [primitiveType, indexBuffer, indexCount, indexBufferByteOffset,
        vertexOffset, instanceCount, firstInstance] {
        TF_VERIFY(instanceCount>0);

        HgiBufferDesc const& indexDesc = indexBuffer->GetDescriptor();

        // We assume 32bit indices: GL_UNSIGNED_INT
        TF_VERIFY(indexDesc.usage & HgiBufferUsageIndex32);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer->GetRawResource());

        glDrawElementsInstancedBaseVertex(
            HgiGLConversions::GetPrimitiveType(primitiveType),
            indexCount,
            GL_UNSIGNED_INT,
            (void*)(uintptr_t(indexBufferByteOffset)),
            instanceCount,
            vertexOffset);

        HGIGL_POST_PENDING_GL_ERRORS();
    };
}

HgiGLOpsFn
HgiGLOps::Dispatch(int dimX, int dimY)
{
    return [dimX, dimY] {
        glDispatchCompute(dimX, dimY, 1);

        // XXX We assume for now that compute outputs to a SSBO or Texture and
        // set both barriers. In the future we could try to get the client to
        // pass in more detailed barrier information or internally try to look
        // at the resource bindings to make barriers decisions.
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

        HGIGL_POST_PENDING_GL_ERRORS();
    };
}

HgiGLOpsFn
HgiGLOps::BindFramebufferOp(
    HgiGLDevice* device,
    HgiGraphicsCmdsDesc const& desc)
{
    return [device, desc] {
        TF_VERIFY(desc.HasAttachments(), "Missing attachments");

        uint32_t framebuffer = device->AcquireFramebuffer(desc);

        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

        bool blendEnabled = false;

        // Apply LoadOps and blend mode
        for (size_t i=0; i<desc.colorAttachmentDescs.size(); i++) {
            HgiAttachmentDesc const& colorAttachment =
                desc.colorAttachmentDescs[i];

            if (colorAttachment.loadOp == HgiAttachmentLoadOpClear) {
                glClearBufferfv(GL_COLOR, i, colorAttachment.clearValue.data());
            }

            blendEnabled |= colorAttachment.blendEnabled;

            GLenum srcColor = HgiGLConversions::GetBlendFactor(
                colorAttachment.srcColorBlendFactor);
            GLenum dstColor = HgiGLConversions::GetBlendFactor(
                colorAttachment.dstColorBlendFactor);

            GLenum srcAlpha = HgiGLConversions::GetBlendFactor(
                colorAttachment.srcAlphaBlendFactor);
            GLenum dstAlpha = HgiGLConversions::GetBlendFactor(
                colorAttachment.dstAlphaBlendFactor);

            GLenum colorOp = HgiGLConversions::GetBlendEquation(
                colorAttachment.colorBlendOp);
            GLenum alphaOp = HgiGLConversions::GetBlendEquation(
                colorAttachment.alphaBlendOp);

            glBlendFuncSeparatei(i, srcColor, dstColor, srcAlpha, dstAlpha);
            glBlendEquationSeparatei(i, colorOp, alphaOp);
        }

        HgiAttachmentDesc const& depthAttachment =
            desc.depthAttachmentDesc;
        if (desc.depthTexture &&
            depthAttachment.loadOp == HgiAttachmentLoadOpClear) {
            glClearBufferfv(GL_DEPTH, 0, depthAttachment.clearValue.data());
        }

        // Setup blending
        if (blendEnabled) {
            glEnable(GL_BLEND);
        } else {
            glDisable(GL_BLEND);
        }

        HGIGL_POST_PENDING_GL_ERRORS();
    };
}

HgiGLOpsFn
HgiGLOps::GenerateMipMaps(HgiTextureHandle const& texture)
{
    return [texture] {
        HgiGLTexture* glTex = static_cast<HgiGLTexture*>(texture.Get());
        if (glTex) {
            glGenerateTextureMipmap(glTex->GetTextureId());
            HGIGL_POST_PENDING_GL_ERRORS();
        }
    };
}

HgiGLOpsFn
HgiGLOps::ResolveFramebuffer(
    HgiGLDevice* device,
    HgiGraphicsCmdsDesc const &graphicsCmds)
{
    return [device, graphicsCmds] {
        const uint32_t resolvedFramebuffer = device->AcquireFramebuffer(
            graphicsCmds, /* resolved = */ true);
        if (!resolvedFramebuffer) {
            return;
        }

        const uint32_t framebuffer = device->AcquireFramebuffer(
            graphicsCmds);
        
        GLbitfield mask = 0;
        if (!graphicsCmds.colorResolveTextures.empty()) {
            mask |= GL_COLOR_BUFFER_BIT;
        }
        if (graphicsCmds.depthResolveTexture) {
            mask |= GL_DEPTH_BUFFER_BIT;
        }

        glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolvedFramebuffer);
        glBlitFramebuffer(0, 0, graphicsCmds.width, graphicsCmds.height,
                          0, 0, graphicsCmds.width, graphicsCmds.height,
                          mask,
                          GL_NEAREST);
    };
}

PXR_NAMESPACE_CLOSE_SCOPE
