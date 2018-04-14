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
#include "pxr/imaging/hdSt/GL/vboSimpleMemoryBufferGL.h"

#include "pxr/base/tf/diagnostic.h"
#include "pxr/base/tf/envSetting.h"
#include "pxr/base/tf/iterator.h"

#include "pxr/imaging/hdSt/bufferRelocator.h"
#include "pxr/imaging/hdSt/bufferResource.h"
#include "pxr/imaging/hdSt/GL/glConversions.h"
#include "pxr/imaging/hdSt/glUtils.h"
#include "pxr/imaging/hdSt/renderContextCaps.h"

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
//  HdStVBOSimpleMemoryBufferGL
// ---------------------------------------------------------------------------
HdStVBOSimpleMemoryBufferGL::HdStVBOSimpleMemoryBufferGL(
    TfToken const &role,
    HdBufferSpecVector const &bufferSpecs)
    : HdStVBOSimpleMemoryManager::_SimpleBufferArray(role, bufferSpecs)
{
}

void
HdStVBOSimpleMemoryBufferGL::Reallocate(
    std::vector<HdBufferArrayRangeSharedPtr> const & ranges,
    HdBufferArraySharedPtr const &curRangeOwner)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    // XXX: make sure glcontext
    HdStRenderContextCaps const &caps = HdStRenderContextCaps::GetInstance();

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
    
    TF_FOR_ALL (bresIt, GetResources()) {
        HdBufferResourceSharedPtr const &bres = bresIt->second;

        // XXX:Arrays: We should use HdDataSizeOfTupleType() here, to
        // add support for array types.
        int bytesPerElement = HdDataSizeOfType(bres->GetTupleType().type);
        GLsizeiptr bufferSize = bytesPerElement * numElements;

        if (glGenBuffers) {
            // allocate new one
            void *newId = NULL;
            void *oldId = bres->GetId();

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
            newId = (void*)(uint64_t)nid;

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
            GLsizeiptr copySize = std::min(oldSize, newSize) * bytesPerElement;
            if (copySize > 0) {
                HD_PERF_COUNTER_INCR(HdPerfTokens->glCopyBufferSubData);

                if (caps.copyBufferEnabled) {
                    if (ARCH_LIKELY(caps.directStateAccessEnabled)) {
                        glNamedCopyBufferSubDataEXT((GLint)(uint64_t)oldId, *(GLuint*)&newId, 0, 0, copySize);
                    } else {
                        glBindBuffer(GL_COPY_READ_BUFFER, (GLint)(uint64_t)oldId);
                        glBindBuffer(GL_COPY_WRITE_BUFFER, (GLint)(uint64_t)newId);
                        glCopyBufferSubData(GL_COPY_READ_BUFFER,
                                            GL_COPY_WRITE_BUFFER, 0, 0, copySize);
                        glBindBuffer(GL_COPY_READ_BUFFER, 0);
                        glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
                    }
                } else {
                    // driver issues workaround
                    std::vector<char> data(copySize);
                    glBindBuffer(GL_ARRAY_BUFFER, (GLint)(uint64_t)oldId);
                    glGetBufferSubData(GL_ARRAY_BUFFER, 0, copySize, &data[0]);
                    glBindBuffer(GL_ARRAY_BUFFER, (GLint)(uint64_t)newId);
                    glBufferSubData(GL_ARRAY_BUFFER, 0, copySize, &data[0]);
                    glBindBuffer(GL_ARRAY_BUFFER, 0);
                }
            }

            // delete old buffer
            if (oldId) {
                GLuint oid = (GLint)(uint64_t)oldId;
                glDeleteBuffers(1, &oid);
            }

            bres->SetAllocation(newId, bufferSize);
        } else {
            // for unit test
            static int id = 1;
            bres->SetAllocation((void*)(uint64_t)id++, bufferSize);
        }
    }

    _capacity = numElements;
    _needsReallocation = false;

    // increment version to rebuild dispatch buffers.
    IncrementVersion();
}

void
HdStVBOSimpleMemoryBufferGL::_DeallocateResources()
{
    TF_FOR_ALL (it, GetResources()) {
        void *oldId = it->second->GetId();
        if (oldId) {
            GLuint oid = (GLint)(uint64_t)oldId;
            glDeleteBuffers(1, &oid);
            it->second->SetAllocation(0, 0);
        }
    }
}

PXR_NAMESPACE_CLOSE_SCOPE

