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
#include "pxr/imaging/glf/glew.h"
#include "pxr/imaging/hdSt/glUtils.h"
#include "pxr/imaging/hdSt/tokens.h"

#include "pxr/imaging/hgi/hgi.h"
#include "pxr/imaging/hgi/blitCmds.h"
#include "pxr/imaging/hgi/blitCmdsOps.h"

#include "pxr/imaging/garch/contextCaps.h"
#include "pxr/imaging/garch/resourceFactory.h"

#include "pxr/base/gf/vec2d.h"
#include "pxr/base/gf/vec2f.h"
#include "pxr/base/gf/vec2i.h"
#include "pxr/base/gf/vec3d.h"
#include "pxr/base/gf/vec3f.h"
#include "pxr/base/gf/vec3i.h"
#include "pxr/base/gf/vec4d.h"
#include "pxr/base/gf/vec4f.h"
#include "pxr/base/gf/vec4i.h"
#include "pxr/base/gf/matrix4f.h"
#include "pxr/base/gf/matrix4d.h"
#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/tokens.h"
#include "pxr/base/vt/array.h"
#include "pxr/base/tf/envSetting.h"
#include "pxr/base/tf/iterator.h"

// APPLE METAL:
#import "pxr/imaging/hgiMetal/buffer.h"

PXR_NAMESPACE_OPEN_SCOPE


// ---------------------------------------------------------------------------

void
HdStBufferRelocator::AddRange(GLintptr readOffset,
                              GLintptr writeOffset,
                              GLsizeiptr copySize)
{
    _CopyUnit unit(readOffset, writeOffset, copySize);
    if (_queue.empty() || (!_queue.back().Concat(unit))) {
        _queue.push_back(unit);
    }
}

void
HdStBufferRelocator::Commit(HgiBlitCmds* blitCmds)
{
    HgiBufferGpuToGpuOp blitOp;
    blitOp.gpuSourceBuffer = _srcBuffer;
    blitOp.gpuDestinationBuffer = _dstBuffer;

    TF_FOR_ALL (it, _queue) {
        blitOp.sourceByteOffset = it->readOffset;
        blitOp.byteSize = it->copySize;
        blitOp.destinationByteOffset = it->writeOffset;

        blitCmds->CopyBufferGpuToGpu(blitOp);

        // APPLE METAL: We need to do this copy host side, otherwise later
        // cpu copies into OTHER parts of this destination buffer see some of
        // our GPU copied range trampled by alignment of the blit. The Metal
        // spec says bytes outside of the range may be copied when calling
        // didModifyRange
        HgiMetalBuffer* buffer = static_cast<HgiMetalBuffer*>(_srcBuffer.Get());
        if ([buffer->GetBufferId() storageMode] == MTLStorageModeManaged) {
            memcpy((char*)_dstBuffer->GetCPUStagingAddress() + it->writeOffset,
                   (char*)_srcBuffer->GetCPUStagingAddress() + it->readOffset,
                   it->copySize);
        }
    }
    HD_PERF_COUNTER_ADD(HdStPerfTokens->copyBufferGpuToGpu,
                        (double)_queue.size());

    _queue.clear();
}

PXR_NAMESPACE_CLOSE_SCOPE

