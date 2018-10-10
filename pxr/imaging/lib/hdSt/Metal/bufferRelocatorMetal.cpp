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

#include "pxr/base/tf/iterator.h"
#include "pxr/base/vt/array.h"

PXR_NAMESPACE_OPEN_SCOPE
HdStBufferRelocatorMetal::HdStBufferRelocatorMetal(HdResourceGPUHandle srcBuffer, HdResourceGPUHandle dstBuffer)
{
    _srcBuffer = srcBuffer;
    _dstBuffer = dstBuffer;
}

void
HdStBufferRelocatorMetal::Commit()
{
    if (_queue.empty())
        return;

    MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();
    context->CreateCommandBuffer(METALWORKQUEUE_RESOURCE);
    context->LabelCommandBuffer(@"HdStBufferRelocatorMetal::Commit()", METALWORKQUEUE_RESOURCE);
    id<MTLBlitCommandEncoder> blitEncoder = context->GetBlitEncoder(METALWORKQUEUE_RESOURCE);
    
    TF_FOR_ALL (it, _queue) {
        [blitEncoder copyFromBuffer:_srcBuffer
                       sourceOffset:it->readOffset
                           toBuffer:_dstBuffer
                  destinationOffset:it->writeOffset
                               size:it->copySize];
    }
    context->ReleaseEncoder(true, METALWORKQUEUE_RESOURCE);
    context->CommitCommandBuffer(false, false, METALWORKQUEUE_RESOURCE);

    HD_PERF_COUNTER_ADD(HdPerfTokens->glCopyBufferSubData,
                        (double)_queue.size());

    _queue.clear();
}

PXR_NAMESPACE_CLOSE_SCOPE

