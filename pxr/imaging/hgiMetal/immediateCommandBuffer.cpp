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

#include "pxr/imaging/hgiMetal/diagnostic.h"
#include "pxr/imaging/hgiMetal/immediateCommandBuffer.h"
#include "pxr/imaging/hgiMetal/blitEncoder.h"
#include "pxr/imaging/hgiMetal/graphicsEncoder.h"
#include "pxr/imaging/hgiMetal/texture.h"
#include "pxr/imaging/hgi/graphicsEncoderDesc.h"
#include "pxr/base/tf/diagnostic.h"
#include "pxr/base/trace/trace.h"
#include <memory>

PXR_NAMESPACE_OPEN_SCOPE

struct HgiMetalDescriptorCacheItem {
    HgiGraphicsEncoderDesc descriptor;
    uint32_t framebuffer = 0;
}; 

std::ostream& operator<<(
    std::ostream& out,
    const HgiMetalImmediateCommandBuffer& cmdBuf)
{
    out << "HgiMetalImmediateCommandBuffer: {"
        << "descriptor cache: { ";

    for (HgiMetalDescriptorCacheItem const * d : cmdBuf._descriptorCache) {
        out << d->descriptor;
    }

    out << "}}";
    return out;
}


static HgiMetalDescriptorCacheItem*
_CreateDescriptorCacheItem(const HgiGraphicsEncoderDesc& desc)
{
    HgiMetalDescriptorCacheItem* dci = new HgiMetalDescriptorCacheItem();
    dci->descriptor = desc;

    // Create framebuffer
//    glCreateFramebuffers(1, &dci->framebuffer);

    // Bind color attachments
    size_t numColorAttachments = desc.colorAttachments.size();
//    std::vector<GLenum> drawBuffers(numColorAttachments);

    //
    // Color attachments
    //
    for (size_t i=0; i<numColorAttachments; i++) {
        const HgiAttachmentDesc& attachment = desc.colorAttachments[i];
        HgiMetalTexture const* metalTexture = 
            static_cast<HgiMetalTexture const*>(attachment.texture);

        if (!TF_VERIFY(metalTexture, "Invalid attachment texture")) {
            continue;
        }

        id<MTLTexture> textureName = metalTexture->GetTextureId();
        if (!TF_VERIFY(textureName != nil, "Attachment not a texture")) {
            continue;
        }

//        glNamedFramebufferTexture(
//            dci->framebuffer,
//            GL_COLOR_ATTACHMENT0 + i,
//            textureName,
//            /*level*/ 0);
//
//        drawBuffers[i] = GL_COLOR_ATTACHMENT0 + i;
    }

//    glNamedFramebufferDrawBuffers(
//        dci->framebuffer,
//        numColorAttachments,
//        drawBuffers.data());

    //
    // Depth attachment
    //
    if (desc.depthAttachment.texture) {
        const HgiAttachmentDesc& attachment = desc.depthAttachment;
        HgiMetalTexture const* metalTexture =
            static_cast<HgiMetalTexture const*>(attachment.texture);

        id<MTLTexture> textureName = metalTexture->GetTextureId();

        if (TF_VERIFY(textureName != nil, "Attachment not a texture")) {
//            glNamedFramebufferTexture(
//                dci->framebuffer,
//                GL_DEPTH_ATTACHMENT,
//                textureName,
//                0); // level
        }
    }

    // Note that if color or depth is multi-sample, they both have to be for GL.
//    GLenum status = glCheckNamedFramebufferStatus(
//        dci->framebuffer,
//        GL_FRAMEBUFFER);
//    TF_VERIFY(status == GL_FRAMEBUFFER_COMPLETE);

    return dci;
}

static void
_DestroyDescriptorCacheItem(HgiMetalDescriptorCacheItem* dci)
{
    if (dci->framebuffer) {
//        TF_VERIFY(glIsFramebuffer(dci->framebuffer),
//            "Tried to free invalid framebuffer");
//
//        glDeleteFramebuffers(1, &dci->framebuffer);
        dci->framebuffer = 0;
    }

    delete dci;
}

static HgiMetalDescriptorCacheItem*
_AcquireDescriptorCacheItem(
    HgiGraphicsEncoderDesc const& desc,
    HgiMetalDescriptorCacheVec& descriptorCache)
{
    // We keep a small cache of descriptor / framebuffer combos since it is 
    // potentially an expensive state change to attach textures to Metal FBs.

    HgiMetalDescriptorCacheItem* dci = nullptr;

    // Look for our framebuffer in cache
    for (size_t i=0; i<descriptorCache.size(); i++) {
        HgiMetalDescriptorCacheItem* item = descriptorCache[i];
        if (desc == item->descriptor) {
            // If the Metal context is changed we cannot re-use the framebuffer
            // as framebuffers cannot be shared between contexts.
            //if (glIsFramebuffer(item->framebuffer))
            {
                dci = item;

                // Move descriptor to end of 'LRU cache' as it is still used.
                if (i < descriptorCache.size()) {
                    descriptorCache.erase(descriptorCache.begin() + i);
                    descriptorCache.push_back(dci);
                }
            }
            break;
        }
    }

    // Create a new descriptor cache item if it was not found
    if (!dci) {
        dci = _CreateDescriptorCacheItem(desc);
        descriptorCache.push_back(dci);

        // Destroy oldest descriptor / FB in LRU cache vector.
        // The size of the cache is small enough and we only store ptrs so we
        // use a vector instead of a linked list LRU.
        const size_t descriptorLRUsize = 32;
        if (descriptorCache.size() == descriptorLRUsize) {
            _DestroyDescriptorCacheItem(descriptorCache.front());
            descriptorCache.erase(descriptorCache.begin());
        }
    }

    return dci;
}

static void
_BindFramebuffer(HgiMetalDescriptorCacheItem* dci)
{
//    glBindFramebuffer(GL_FRAMEBUFFER, dci->framebuffer);

    // Apply LoadOps
    for (size_t i=0; i<dci->descriptor.colorAttachments.size(); i++) {
        HgiAttachmentDesc const& colorAttachment =
            dci->descriptor.colorAttachments[i];

        if (colorAttachment.loadOp == HgiAttachmentLoadOpClear) {
//            glClearBufferfv(GL_COLOR, i, colorAttachment.clearValue.data());
        }
    }

    HgiAttachmentDesc const& depthAttachment =
        dci->descriptor.depthAttachment;
    if (depthAttachment.texture && 
        depthAttachment.loadOp == HgiAttachmentLoadOpClear) {
//        glClearBufferfv(GL_DEPTH, 0, depthAttachment.clearValue.data());
    }
}

HgiMetalImmediateCommandBuffer::HgiMetalImmediateCommandBuffer(id<MTLDevice> device)
: _device(device)
, _commandQueue(nil)
, _commandBuffer(nil)
{
    _commandQueue = [_device newCommandQueue];
    _commandBuffer = [_commandQueue commandBuffer];
    [_commandBuffer retain];
}

HgiMetalImmediateCommandBuffer::~HgiMetalImmediateCommandBuffer()
{
    for (HgiMetalDescriptorCacheItem* dci : _descriptorCache) {
        _DestroyDescriptorCacheItem(dci);
    }

    [_commandBuffer release];
    [_commandQueue release];
}

HgiGraphicsEncoderUniquePtr
HgiMetalImmediateCommandBuffer::CreateGraphicsEncoder(
    HgiGraphicsEncoderDesc const& desc)
{
    TRACE_FUNCTION();

    if (!desc.HasAttachments()) {
        // XXX For now we do not emit a warning because we have to many
        // pieces that do not yet use Hgi fully.
        // TF_WARN("Encoder descriptor incomplete");
        return nullptr;
    }

    const size_t maxColorAttachments = 8;
    if (!TF_VERIFY(desc.colorAttachments.size() <= maxColorAttachments,
        "Too many color attachments for Metal frambuffer"))
    {
        return nullptr;
    }

    HgiMetalDescriptorCacheItem* dci =
        _AcquireDescriptorCacheItem(desc, _descriptorCache);

    _BindFramebuffer(dci);

    HgiMetalGraphicsEncoder* encoder(new HgiMetalGraphicsEncoder(desc));

    return HgiGraphicsEncoderUniquePtr(encoder);
}

HgiBlitEncoderUniquePtr
HgiMetalImmediateCommandBuffer::CreateBlitEncoder()
{
    return HgiBlitEncoderUniquePtr(new HgiMetalBlitEncoder(this));
}
    
void
HgiMetalImmediateCommandBuffer::FlushEncoders()
{
    [_commandBuffer commit];
    [_commandBuffer waitUntilCompleted];
    [_commandBuffer release];

    _commandBuffer = [_commandQueue commandBuffer];
    [_commandBuffer retain];
}

PXR_NAMESPACE_CLOSE_SCOPE
