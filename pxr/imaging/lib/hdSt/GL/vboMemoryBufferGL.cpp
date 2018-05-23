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
#include "pxr/imaging/hdSt/GL/vboMemoryBufferGL.h"

#include <boost/make_shared.hpp>
#include <vector>

#include "pxr/base/arch/hash.h"
#include "pxr/base/tf/diagnostic.h"
#include "pxr/base/tf/envSetting.h"
#include "pxr/base/tf/iterator.h"
#include "pxr/base/tf/enum.h"

#include "pxr/imaging/hdSt/bufferRelocator.h"
#include "pxr/imaging/hdSt/bufferResource.h"
#include "pxr/imaging/hdSt/glUtils.h"
#include "pxr/imaging/hdSt/renderContextCaps.h"
#include "pxr/imaging/hdSt/GL/glConversions.h"
#include "pxr/imaging/hd/engine.h"
#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/tokens.h"

#include "pxr/imaging/hf/perfLog.h"

PXR_NAMESPACE_OPEN_SCOPE

// ---------------------------------------------------------------------------
//  _StripedBufferArray
// ---------------------------------------------------------------------------
HdStVBOMemoryBufferGL::HdStVBOMemoryBufferGL(
    TfToken const &role,
    HdBufferSpecVector const &bufferSpecs,
    bool isImmutable)
    : HdStVBOMemoryManager::_StripedBufferArray(role, bufferSpecs, isImmutable)
{
}

void
HdStVBOMemoryBufferGL::Reallocate(
    std::vector<HdBufferArrayRangeSharedPtr> const &ranges,
    HdBufferArraySharedPtr const &curRangeOwner)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    // XXX: make sure glcontext
    HdStRenderContextCaps const &caps = HdStRenderContextCaps::GetInstance();

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
        HdBufferResourceSharedPtr const &bres = resources[bresIdx].second;
        HdBufferResourceSharedPtr const &curRes =
                curRangeOwner_->GetResources()[bresIdx].second;

        int bytesPerElement = HdDataSizeOfTupleType(bres->GetTupleType());
        TF_VERIFY(bytesPerElement > 0);
        GLsizeiptr bufferSize = bytesPerElement * _totalCapacity;

        // allocate new one
        // curId and oldId will be different when we are adopting ranges
        // from another buffer array.
        HdResourceGPUHandle newId;
        HdResourceGPUHandle oldId(bres->GetId());
        HdResourceGPUHandle curId(curRes->GetId());

        if(glGenBuffers) {
            GLuint nid = 0;
            glGenBuffers(1, &nid);
            
            if (ARCH_LIKELY(caps.directStateAccessEnabled)) {
                glNamedBufferDataEXT(nid,
                                     bufferSize, NULL, GL_STATIC_DRAW);
            } else {
                glBindBuffer(GL_ARRAY_BUFFER, nid);
                glBufferData(GL_ARRAY_BUFFER,
                             bufferSize, NULL, GL_STATIC_DRAW);
                glBindBuffer(GL_ARRAY_BUFFER, 0);
            }

            newId = nid;

            // if old buffer exists, copy unchanged data
            if (curId) {
                std::vector<size_t>::iterator newOffsetIt = newOffsets.begin();

                // pre-pass to combine consecutive buffer range relocation
                boost::scoped_ptr<HdStBufferRelocator> relocator(
                     HdStBufferRelocator::New(curId, newId));
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
                    int oldSize = range->GetCapacity();
                    int newSize = range->GetNumElements();
                    GLsizeiptr copySize =
                        std::min(oldSize, newSize) * bytesPerElement;
                    int oldOffset = range->GetOffset();
                    if (copySize > 0) {
                        GLintptr readOffset = oldOffset * bytesPerElement;
                        GLintptr writeOffset = *newOffsetIt * bytesPerElement;

                        relocator->AddRange(readOffset, writeOffset, copySize);
                    }
                    ++newOffsetIt;
                }

                // buffer copy
                relocator->Commit();
            }

            if (oldId) {
                // delete old buffer
                GLuint oid = (GLuint)(uint64_t)oldId;
                glDeleteBuffers(1, &oid);
            }
        } else {
            // for unit test
            static GLuint id = 1;
            newId = id++;
        }

        // update id of buffer resource
        bres->SetAllocation(newId, bufferSize);
    }

    // update ranges
    for (size_t idx = 0; idx < ranges.size(); ++idx) {
        HdStVBOMemoryManager::_StripedBufferArrayRangeSharedPtr range =
            boost::static_pointer_cast<HdStVBOMemoryManager::_StripedBufferArrayRange>(ranges[idx]);
        if (!range) {
            TF_CODING_ERROR("_StripedBufferArrayRange expired unexpectedly.");
            continue;
        }
        range->SetOffset(newOffsets[idx]);
        range->SetCapacity(range->GetNumElements());
    }
    _needsReallocation = false;
    _needsCompaction = false;

    // increment version to rebuild dispatch buffers.
    IncrementVersion();
}

void
HdStVBOMemoryBufferGL::_DeallocateResources()
{
    TF_FOR_ALL (it, GetResources()) {
        HdResourceGPUHandle oldId = it->second->GetId();
        if (oldId) {
            if (glDeleteBuffers) {
                GLuint oid = oldId;
                glDeleteBuffers(1, &oid);
            }
            it->second->SetAllocation(HdResourceGPUHandle(), 0);
        }
    }
}

PXR_NAMESPACE_CLOSE_SCOPE

