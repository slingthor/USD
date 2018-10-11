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

#include "pxr/imaging/hdSt/Metal/interleavedMemoryBufferMetal.h"
#include "pxr/imaging/hdSt/bufferResource.h"
#include "pxr/imaging/hdSt/bufferRelocator.h"
#include "pxr/imaging/hdSt/GL/glUtils.h"
#include "pxr/imaging/hdSt/GL/glConversions.h"

#include <boost/make_shared.hpp>
#include <boost/scoped_ptr.hpp>
#include <vector>

#include "pxr/base/arch/hash.h"
#include "pxr/base/tf/diagnostic.h"
#include "pxr/base/tf/enum.h"
#include "pxr/base/tf/iterator.h"

#include "pxr/imaging/hd/engine.h"
#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/tokens.h"

#include "pxr/imaging/hf/perfLog.h"

using namespace boost;

PXR_NAMESPACE_OPEN_SCOPE

// ---------------------------------------------------------------------------
//  _StripedInterleavedBuffer
// ---------------------------------------------------------------------------

HdStStripedInterleavedBufferMetal::HdStStripedInterleavedBufferMetal(
    TfToken const &role,
    HdBufferSpecVector const &bufferSpecs,
    int bufferOffsetAlignment = 0,
    int structAlignment = 0,
    size_t maxSize = 0,
    TfToken const &garbageCollectionPerfToken = HdPerfTokens->garbageCollectedUbo)
    : HdStInterleavedMemoryManager::_StripedInterleavedBuffer(
        role, bufferSpecs, bufferOffsetAlignment, structAlignment, maxSize, garbageCollectionPerfToken)
{
    // Empty
}

void
HdStStripedInterleavedBufferMetal::Reallocate(
    std::vector<HdBufferArrayRangeSharedPtr> const &ranges,
    HdBufferArraySharedPtr const &curRangeOwner)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    // XXX: make sure glcontext

    HD_PERF_COUNTER_INCR(HdPerfTokens->vboRelocated);

    // Calculate element count
    size_t elementCount = 0;
    TF_FOR_ALL (it, ranges) {
        HdBufferArrayRangeSharedPtr const &range = *it;
        if (!range) {
            TF_CODING_ERROR("Expired range found in the reallocation list");
        }
        elementCount += (*it)->GetNumElements();
    }
    size_t totalSize = elementCount * _stride;

    // update range list (should be done before early exit)
    _SetRangeList(ranges);

    // If there is no data to reallocate, it is the caller's responsibility to
    // deallocate the underlying resource. 
    //
    // XXX: There is an issue here if the caller does not deallocate
    // after this return, we will hold onto unused GPU resources until the next
    // reallocation. Perhaps we should free the buffer here to avoid that
    // situation.
    if (totalSize == 0)
        return;

    // resize each BufferResource
    // all HdBufferSources are sharing same VBO

    // allocate new one
    // curId and oldId will be different when we are adopting ranges
    // from another buffer array.

    id<MTLBuffer> newId = nil;
    id<MTLBuffer> oldId = GetResources().begin()->second->GetId();
    
    HdStInterleavedMemoryManager::_StripedInterleavedBufferSharedPtr curRangeOwner_ =
    boost::static_pointer_cast<_StripedInterleavedBuffer> (curRangeOwner);
    
    id<MTLBuffer> curId = curRangeOwner_->GetResources().begin()->second->GetId();
    newId = MtlfMetalContext::GetMetalContext()->GetMetalBuffer(totalSize, MTLResourceStorageModeManaged);
    
    // if old buffer exists, copy unchanged data
    if (curId) {
        int index = 0;
        
        size_t rangeCount = GetRangeCount();
        
        // pre-pass to combine consecutive buffer range relocation
        boost::scoped_ptr<HdStBufferRelocator> relocator(HdStBufferRelocator::New(curId, newId));
        for (size_t rangeIdx = 0; rangeIdx < rangeCount; ++rangeIdx) {
            HdStInterleavedMemoryManager::_StripedInterleavedBufferRangeSharedPtr range = _GetRangeSharedPtr(rangeIdx);
            
            if (!range) {
                TF_CODING_ERROR("_StripedInterleavedBufferRange expired "
                                "unexpectedly.");
                continue;
            }
            int oldIndex = range->GetIndex();
            if (oldIndex >= 0) {
                // copy old data
                GLintptr readOffset = oldIndex * _stride;
                GLintptr writeOffset = index * _stride;
                GLsizeiptr copySize = _stride * range->GetNumElements();
                
                relocator->AddRange(readOffset, writeOffset, copySize);
            }
            
            range->SetIndex(index);
            index += range->GetNumElements();
        }
        
        // buffer copy
        relocator->Commit();
        
    } else {
        // just set index
        int index = 0;
        
        size_t rangeCount = GetRangeCount();
        for (size_t rangeIdx = 0; rangeIdx < rangeCount; ++rangeIdx) {
            HdStInterleavedMemoryManager::_StripedInterleavedBufferRangeSharedPtr range = _GetRangeSharedPtr(rangeIdx);
            if (!range) {
                TF_CODING_ERROR("_StripedInterleavedBufferRange expired "
                                "unexpectedly.");
                continue;
            }
            
            range->SetIndex(index);
            index += range->GetNumElements();
        }
    }
    // delete old buffer
    if(oldId != nil) {
        MtlfMetalContext::GetMetalContext()->ReleaseMetalBuffer(oldId);
    }
    
    // update id to all buffer resources
    TF_FOR_ALL(it, GetResources()) {
        it->second->SetAllocation((__bridge HdResourceGPUHandle)newId, totalSize);
    }

    _needsReallocation = false;
    _needsCompaction = false;

    // increment version to rebuild dispatch buffers.
    IncrementVersion();
}

void
HdStStripedInterleavedBufferMetal::_DeallocateResources()
{
    HdBufferResourceSharedPtr resource = GetResource();
    if (resource) {
        id<MTLBuffer> _id = resource->GetId();
        if (_id != nil) {
            MtlfMetalContext::GetMetalContext()->ReleaseMetalBuffer(_id);
            resource->SetAllocation(nil, 0);
        }
    }
}

PXR_NAMESPACE_CLOSE_SCOPE

