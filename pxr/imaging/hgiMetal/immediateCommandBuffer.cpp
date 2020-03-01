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
    if (!TF_VERIFY(desc.colorAttachmentDescs.size() <= maxColorAttachments,
        "Too many color attachments for Metal frambuffer"))
    {
        return nullptr;
    }

    HgiMetalGraphicsEncoder* encoder(
        new HgiMetalGraphicsEncoder(_commandBuffer, desc));

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
