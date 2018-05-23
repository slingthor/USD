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

#include "pxr/imaging/hdSt/GL/interleavedMemoryBufferGL.h"
#include "pxr/imaging/hdSt/bufferResource.h"
#include "pxr/imaging/hdSt/bufferRelocator.h"
#include "pxr/imaging/hdSt/renderContextCaps.h"
#include "pxr/imaging/hdSt/glUtils.h"
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
//  HdStStripedInterleavedBufferGL
// ---------------------------------------------------------------------------


HdStStripedInterleavedBufferGL::HdStStripedInterleavedBufferGL(
    TfToken const &role,
    HdBufferSpecVector const &bufferSpecs,
    int bufferOffsetAlignment = 0,
    int structAlignment = 0,
    size_t maxSize = 0,
    TfToken const &garbageCollectionPerfToken = HdPerfTokens->garbageCollectedUbo)
    : HdStInterleavedMemoryManager::_StripedInterleavedBuffer(
        role, bufferSpecs, bufferOffsetAlignment, structAlignment, maxSize, garbageCollectionPerfToken)
{
}

void
HdStStripedInterleavedBufferGL::Reallocate(
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

    GLuint newId = 0;
    HdResourceGPUHandle oldId = (GLuint)(uint64_t)GetResources().begin()->second->GetId();

    HdStInterleavedMemoryManager::_StripedInterleavedBufferSharedPtr curRangeOwner_ =
        boost::static_pointer_cast<HdStInterleavedMemoryManager::_StripedInterleavedBuffer> (curRangeOwner);

    HdResourceGPUHandle curId = (GLuint)(uint64_t)curRangeOwner_->GetResources().begin()->second->GetId();

    if (glGenBuffers) {
        glGenBuffers(1, &newId);

        HdStRenderContextCaps const &caps = HdStRenderContextCaps::GetInstance();
        if (caps.directStateAccessEnabled) {
            glNamedBufferDataEXT(newId, totalSize, /*data=*/NULL, GL_STATIC_DRAW);
        } else {
            glBindBuffer(GL_ARRAY_BUFFER, newId);
            glBufferData(GL_ARRAY_BUFFER, totalSize, /*data=*/NULL, GL_STATIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }

        // if old buffer exists, copy unchanged data
        if (curId) {
            int index = 0;

            size_t rangeCount = GetRangeCount();

            // pre-pass to combine consecutive buffer range relocation
            boost::scoped_ptr<HdStBufferRelocator> relocator(
                HdStBufferRelocator::New(curId, newId));
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
        if (oldId) {
            // delete old buffer
            GLuint oid = oldId;
            glDeleteBuffers(1, &oid);
        }
    } else {
        // for unit test
        static GLuint id = 1;
        newId = id++;
    }

    // update id to all buffer resources
    TF_FOR_ALL(it, GetResources()) {
        it->second->SetAllocation(newId, totalSize);
    }

    _needsReallocation = false;
    _needsCompaction = false;

    // increment version to rebuild dispatch buffers.
    IncrementVersion();
}

void
HdStStripedInterleavedBufferGL::_DeallocateResources()
{
    HdBufferResourceSharedPtr resource = GetResource();
    if (resource) {
        GLuint id = resource->GetId();
        if (id) {
            if (glDeleteBuffers) {
                glDeleteBuffers(1, &id);
            }
            resource->SetAllocation(HdResourceGPUHandle(), 0);
        }
    }
}

PXR_NAMESPACE_CLOSE_SCOPE

