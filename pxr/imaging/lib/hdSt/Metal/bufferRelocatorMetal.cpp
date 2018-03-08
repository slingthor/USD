//
// Copyright 2016 Pixar
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

#include "pxr/imaging/mtlf/mtlDevice.h"
#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/tokens.h"

#include "pxr/imaging/hdSt/Metal/bufferRelocatorMetal.h"

#include "pxr/base/vt/array.h"

PXR_NAMESPACE_OPEN_SCOPE
HdStBufferRelocatorMetal::HdStBufferRelocatorMetal(HdBufferResourceGPUHandle srcBuffer, HdBufferResourceGPUHandle dstBuffer)
{
    _srcBuffer = (__bridge id<MTLBuffer>)srcBuffer;
    _dstBuffer = (__bridge id<MTLBuffer>)dstBuffer;
}

void
HdStBufferRelocatorMetal::Commit()
{
    if (_queue.empty())
        return;

    id<MTLCommandBuffer> commandBuffer = [MtlfMetalContext::GetMetalContext()->commandQueue commandBuffer];
    id<MTLBlitCommandEncoder> blitEncoder = [commandBuffer blitCommandEncoder];

    TF_FOR_ALL (it, _queue) {
        [blitEncoder copyFromBuffer:(__bridge id<MTLBuffer>)_srcBuffer
                       sourceOffset:it->readOffset
                           toBuffer:(__bridge id<MTLBuffer>)_dstBuffer
                  destinationOffset:it->writeOffset
                               size:it->copySize];
    }
    [blitEncoder endEncoding];
    [commandBuffer commit];

    HD_PERF_COUNTER_ADD(HdPerfTokens->glCopyBufferSubData,
                        (double)_queue.size());

    _queue.clear();
}

PXR_NAMESPACE_CLOSE_SCOPE

