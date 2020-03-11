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

#include "pxr/imaging/garch/contextCaps.h"
#include "pxr/imaging/garch/resourceFactory.h"

#include "pxr/imaging/mtlf/mtlDevice.h"
#include "pxr/imaging/hdSt/Metal/bufferResourceMetal.h"
#include "pxr/imaging/hdSt/Metal/vboSimpleMemoryBufferMetal.h"

#include "pxr/base/tf/diagnostic.h"
#include "pxr/base/tf/envSetting.h"
#include "pxr/base/tf/iterator.h"

#include "pxr/imaging/hdSt/vboSimpleMemoryManager.h"

#include "pxr/imaging/hd/bufferArrayRange.h"
#include "pxr/imaging/hd/bufferSource.h"
#include "pxr/imaging/hd/engine.h"
#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/tokens.h"

#include "pxr/imaging/hf/perfLog.h"

#include <atomic>

#include <boost/functional/hash.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

using namespace boost;

PXR_NAMESPACE_OPEN_SCOPE


// ---------------------------------------------------------------------------
//  HdStVBOSimpleMemoryBufferMetal
// ---------------------------------------------------------------------------


// ---------------------------------------------------------------------------
//  _SimpleBufferArray
// ---------------------------------------------------------------------------
HdStVBOSimpleMemoryBufferMetal::HdStVBOSimpleMemoryBufferMetal(
    TfToken const &role,
    HdBufferSpecVector const &bufferSpecs,
    HdBufferArrayUsageHint usageHint)
    : HdStVBOSimpleMemoryManager::_SimpleBufferArray(role, bufferSpecs, usageHint)
{
    // Empty
}

HdStVBOSimpleMemoryBufferMetal::~HdStVBOSimpleMemoryBufferMetal() {
    _DeallocateResources();
}


void
HdStVBOSimpleMemoryBufferMetal::Reallocate(
    std::vector<HdBufferArrayRangeSharedPtr> const & ranges,
    HdBufferArraySharedPtr const &curRangeOwner)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    GarchContextCaps const &caps = GarchResourceFactory::GetInstance()->GetContextCaps();

    HD_PERF_COUNTER_INCR(HdPerfTokens->vboRelocated);

    if (!TF_VERIFY(curRangeOwner == shared_from_this())) {
        TF_CODING_ERROR("HdStVBOSimpleMemoryManager can't reassign ranges");
        return;
    }

    if (ranges.size() > 1) {
        TF_CODING_ERROR("HdStVBOSimpleMemoryManager can't take multiple ranges");
        return;
    }
    _SetRangeList(ranges);

    HdStVBOSimpleMemoryManager::_SimpleBufferArrayRangeSharedPtr range = _GetRangeSharedPtr();

    if (!range) {
        TF_CODING_ERROR("_SimpleBufferArrayRange expired unexpectedly.");
        return;
    }
    int numElements = range->GetNumElements();

    MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();

    /*
        MTL_FIXE - Ideally we wouldn't be creating and committing a command buffer here but we'd need some extra call
        to know when all reallocates had been performed so could commit them. However, if this is only an initialisation step it's probably OK.
     */
    id<MTLCommandBuffer> commandBuffer;
    id<MTLBlitCommandEncoder> blitEncoder;

    commandBuffer = [context->gpus.commandQueue commandBuffer];
    blitEncoder = [commandBuffer blitCommandEncoder];
    
    TF_FOR_ALL (bresIt, GetResources()) {
        HdStBufferResourceMetalSharedPtr const &bres =
            boost::static_pointer_cast<HdStBufferResourceMetal>(bresIt->second);

        // XXX:Arrays: We should use HdDataSizeOfTupleType() here, to
        // add support for array types.
        int const bytesPerElement = HdDataSizeOfType(bres->GetTupleType().type);
        size_t const bufferSize = bytesPerElement * numElements;

        // allocate new one
        id<MTLBuffer> newId[3];
        id<MTLBuffer> oldId[3];

        for (int i = 0; i < 3; i++) {
            oldId[i] = bres->GetIdAtIndex(i);
            
#if defined(ARCH_OS_MACOS)
            if (i == 0)
#else
            // Triple buffer everything
            if (true)
#endif
            {
                if (bufferSize) {
                    newId[i] = context->GetMetalBuffer(bufferSize, MTLResourceStorageModeDefault);
                }
                else {
                    // Dummy buffer - 0 byte buffers are invalid
                    newId[i] = context->GetMetalBuffer(256, MTLResourceStorageModeDefault);
                }
            }
            else {
                newId[i] = nil;
            }
        }

        // copy the range. There are three cases:
        //
        // 1. src length (capacity) == dst length (numElements)
        //   Copy the entire range
        //
        // 2. src length < dst length
        //   Enlarging the range. This typically happens when
        //   applying quadrangulation/subdivision to populate
        //   additional data at the end of source data.
        //
        // 3. src length > dst length
        //   Shrinking the range. When the garbage collection
        //   truncates ranges.
        //
        int const oldSize = range->GetCapacity();
        int const newSize = range->GetNumElements();
        size_t const copySize = std::min(oldSize, newSize) * bytesPerElement;
        if (copySize > 0) {
            HD_PERF_COUNTER_INCR(HdPerfTokens->glCopyBufferSubData);

            for (int i = 0; i < 3; i++) {
                if (newId[i]) {
                    [blitEncoder copyFromBuffer:oldId[i]
                                      sourceOffset:0
                                          toBuffer:newId[i]
                                 destinationOffset:0
                                              size:copySize];
                }
            }
        }

        // delete old buffer
        for (int i = 0; i < 3; i++) {
            if (oldId[i]) {
                context->ReleaseMetalBuffer(oldId[i]);
            }
        }

        bres->SetAllocations(newId[0], newId[1], newId[2], bufferSize);
    }
    
    [blitEncoder endEncoding];
    [commandBuffer commit];

    _capacity = numElements;
    _needsReallocation = false;

    // increment version to rebuild dispatch buffers.
    IncrementVersion();
}

void
HdStVBOSimpleMemoryBufferMetal::_DeallocateResources()
{
    TF_FOR_ALL (it, GetResources()) {
        HdStBufferResourceMetalSharedPtr const &bres =
            boost::static_pointer_cast<HdStBufferResourceMetal>(it->second);

        for (int i = 0; i < 3; i++) {
            HdResourceGPUHandle oldId(bres->GetIdAtIndex(i));
            if (oldId.IsSet()) {
                MtlfMetalContext::GetMetalContext()->ReleaseMetalBuffer(oldId);
            }
        }
        bres->SetAllocations(HdResourceGPUHandle(), HdResourceGPUHandle(), HdResourceGPUHandle(), 0);
    }
}

PXR_NAMESPACE_CLOSE_SCOPE

