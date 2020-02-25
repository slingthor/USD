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

#include "pxr/imaging/hdSt/Metal/bufferResourceMetal.h"
#include "pxr/imaging/hdSt/Metal/vboMemoryBufferMetal.h"
#include "pxr/imaging/hdSt/resourceFactory.h"
#include "pxr/imaging/mtlf/mtlDevice.h"

#include <boost/make_shared.hpp>
#include <vector>

#include "pxr/base/arch/hash.h"
#include "pxr/base/tf/diagnostic.h"
#include "pxr/base/tf/envSetting.h"
#include "pxr/base/tf/iterator.h"
#include "pxr/base/tf/enum.h"

#include "pxr/imaging/hdSt/bufferRelocator.h"
#include "pxr/imaging/hd/engine.h"
#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/tokens.h"

#include "pxr/imaging/hf/perfLog.h"

PXR_NAMESPACE_OPEN_SCOPE

// ---------------------------------------------------------------------------
//  _StripedBufferArray
// ---------------------------------------------------------------------------
HdStVBOMemoryBufferMetal::HdStVBOMemoryBufferMetal(
    TfToken const &role,
    HdBufferSpecVector const &bufferSpecs,
    HdBufferArrayUsageHint usageHint)
    : HdStVBOMemoryManager::_StripedBufferArray(
        role, bufferSpecs, usageHint)
{
}

HdStVBOMemoryBufferMetal::~HdStVBOMemoryBufferMetal()
{
    _DeallocateResources();
}

void
HdStVBOMemoryBufferMetal::Reallocate(
    std::vector<HdBufferArrayRangeSharedPtr> const &ranges,
    HdBufferArraySharedPtr const &curRangeOwner)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    // XXX: make sure glcontext
    GarchContextCaps const &caps = GarchResourceFactory::GetInstance()->GetContextCaps();

    HD_PERF_COUNTER_INCR(HdPerfTokens->vboRelocated);

    HdStVBOMemoryManager::_StripedBufferArraySharedPtr curRangeOwner_ =
        boost::static_pointer_cast<_StripedBufferArray> (curRangeOwner);

    if (!TF_VERIFY(GetResources().size() ==
                      curRangeOwner_->GetResources().size())) {
        TF_CODING_ERROR("Resource mismatch when reallocating buffer array");
        return;
    }

    if (TfDebug::IsEnabled(HD_SAFE_MODE)) {
        HdStBufferResourceNamedList::size_type bresIdx = 0;
        TF_FOR_ALL(bresIt, GetResources()) {
            TF_VERIFY(curRangeOwner_->GetResources()[bresIdx++].second ==
                      curRangeOwner_->GetResource(bresIt->first));
        }
    }

    // count up total elements and update new offsets
    size_t totalNumElements = 0;
    std::vector<size_t> newOffsets;
    newOffsets.reserve(ranges.size());

    TF_FOR_ALL (it, ranges) {
        HdBufferArrayRangeSharedPtr const &range = *it;
        if (!range) {
            TF_CODING_ERROR("Expired range found in the reallocation list");
            continue;
        }

        // save new offset
        newOffsets.push_back(totalNumElements);

        // XXX: always tightly pack for now.
        totalNumElements += range->GetNumElements();
    }

    // update range list (should be done before early exit)
    _SetRangeList(ranges);

    // If there is no data to reallocate, it is the caller's responsibility to
    // deallocate the underlying resource. 
    //
    // XXX: There is an issue here if the caller does not deallocate
    // after this return, we will hold onto unused GPU resources until the next
    // reallocation. Perhaps we should free the buffer here to avoid that
    // situation.
    if (totalNumElements == 0)
        return;

    _totalCapacity = totalNumElements;

    // resize each BufferResource
    HdBufferResourceNamedList const& resources = GetResources();
    for (size_t bresIdx=0; bresIdx<resources.size(); ++bresIdx) {
        HdStBufferResourceMetalSharedPtr const &bres =
            boost::static_pointer_cast<HdStBufferResourceMetal>(resources[bresIdx].second);
        HdStBufferResourceMetalSharedPtr const &curRes =
            boost::static_pointer_cast<HdStBufferResourceMetal>(curRangeOwner_->GetResources()[bresIdx].second);

        int const bytesPerElement = HdDataSizeOfTupleType(bres->GetTupleType());
        TF_VERIFY(bytesPerElement > 0);
        size_t const bufferSize = bytesPerElement * _totalCapacity;

        // allocate new one
        // curId and oldId will be different when we are adopting ranges
        // from another buffer array.
        HdResourceGPUHandle newId[3];
        HdResourceGPUHandle oldId[3];
        HdResourceGPUHandle curId[3];

        MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();
        
        for (int i = 0; i < 3; i++) {
            
            // Disable triple buffering
            if (i == 0) {
                newId[i] = context->GetMetalBuffer(bufferSize, MTLResourceStorageModeDefault);
            }
            else {
                newId[i].Clear();
            }

            oldId[i] = bres->GetIdAtIndex(i);
            curId[i] = curRes->GetIdAtIndex(i);
        }
        // if old buffer exists, copy unchanged data
        if (curId[0].IsSet()) {
            std::vector<size_t>::iterator newOffsetIt = newOffsets.begin();

            // pre-pass to combine consecutive buffer range relocation
            HdStBufferRelocator* relocator[3];
            
            for (int i = 0; i < 3; i++) {
                int const curIndex = curId[i].IsSet() ? i : 0;
                if (newId[i].IsSet())
                    relocator[i] = HdStResourceFactory::GetInstance()->NewBufferRelocator(curId[curIndex], newId[i]);
                else
                    relocator[i] = NULL;
            }

            TF_FOR_ALL (it, ranges) {
                HdStVBOMemoryManager::_StripedBufferArrayRangeSharedPtr range =
                    boost::static_pointer_cast<HdStVBOMemoryManager::_StripedBufferArrayRange>(*it);
                if (!range) {
                    TF_CODING_ERROR("_StripedBufferArrayRange "
                                    "expired unexpectedly.");
                    continue;
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
                size_t const copySize =
                    std::min(oldSize, newSize) * bytesPerElement;
                int const oldOffset = range->GetElementOffset();
                if (copySize > 0) {
                    ptrdiff_t const readOffset = oldOffset * bytesPerElement;
                    ptrdiff_t const writeOffset = *newOffsetIt * bytesPerElement;

                    for (int i = 0; i < 3; i++) {
                        if (relocator[i]) {
                            relocator[i]->AddRange(readOffset, writeOffset, copySize);
                        }
                    }
                }
                ++newOffsetIt;
            }

            // buffer copy
            for (int i = 0; i < 3; i++) {
                if (relocator[i]) {
                    relocator[i]->Commit();
                    delete relocator[i];
                }
            }
        }

        for (int i = 0; i < 3; i++) {
            if (oldId[i].IsSet()) {
                // delete old buffer
                MtlfMetalContext::GetMetalContext()->ReleaseMetalBuffer(oldId[i]);
            }
        }

        // update id of buffer resource
        bres->SetAllocations(newId[0], newId[1], newId[2], bufferSize);
    }

    // update ranges
    for (size_t idx = 0; idx < ranges.size(); ++idx) {
        HdStVBOMemoryManager::_StripedBufferArrayRangeSharedPtr range =
            boost::static_pointer_cast<HdStVBOMemoryManager::_StripedBufferArrayRange>(ranges[idx]);
        if (!range) {
            TF_CODING_ERROR("_StripedBufferArrayRange expired unexpectedly.");
            continue;
        }
        range->SetElementOffset(newOffsets[idx]);
        range->SetCapacity(range->GetNumElements());
    }
    _needsReallocation = false;
    _needsCompaction = false;

    // increment version to rebuild dispatch buffers.
    IncrementVersion();
}

void
HdStVBOMemoryBufferMetal::_DeallocateResources()
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

