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


HgiMetalImmediateCommandBuffer::HgiMetalImmediateCommandBuffer(
    id<MTLDevice> device,
    id<MTLCommandQueue> commandQueue)
: _device(device)
, _commandQueue(commandQueue)
, _commandBuffer(nil)
, _workToFlush(false)
{
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

    _workToFlush = true;
    HgiMetalGraphicsEncoder* encoder(
        new HgiMetalGraphicsEncoder(_commandBuffer, desc));

    return HgiGraphicsEncoderUniquePtr(encoder);
}

HgiBlitEncoderUniquePtr
HgiMetalImmediateCommandBuffer::CreateBlitEncoder()
{
    _workToFlush = true;
    return HgiBlitEncoderUniquePtr(new HgiMetalBlitEncoder(this));
}

id<MTLCommandBuffer>
HgiMetalImmediateCommandBuffer::GetCommandBuffer()
{
    _workToFlush = true;
    return _commandBuffer;
}

void
HgiMetalImmediateCommandBuffer::StartFrame()
{
    if ([[MTLCaptureManager sharedCaptureManager] isCapturing]) {
        // We need to grab a new command buffer otherwise the previous one
        // (if it was allocated at the end of the last frame) won't appear in
        // this frame's capture, and it will confuse us!
        _workToFlush = false;
        [_commandBuffer release];

        _commandBuffer = [_commandQueue commandBuffer];
        [_commandBuffer retain];
    }
}

void
HgiMetalImmediateCommandBuffer::BlockUntilCompleted()
{
    if (!_workToFlush) {
        return;
    }

    [_commandBuffer commit];
    [_commandBuffer waitUntilCompleted];
    [_commandBuffer release];

    _commandBuffer = [_commandQueue commandBuffer];
    [_commandBuffer retain];
    _workToFlush = false;
}

void
HgiMetalImmediateCommandBuffer::BlockUntilSubmitted()
{
    if (!_workToFlush) {
        return;
    }

    [_commandBuffer commit];
    [_commandBuffer waitUntilScheduled];
    [_commandBuffer release];

    _commandBuffer = [_commandQueue commandBuffer];
    [_commandBuffer retain];
    _workToFlush = false;
}

PXR_NAMESPACE_CLOSE_SCOPE
