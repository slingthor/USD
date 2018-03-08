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

#include "pxr/imaging/hd/GL/bufferRelocatorGL.h"
#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/tokens.h"

#include "pxr/imaging/hdSt/renderContextCaps.h"

#include "pxr/base/vt/array.h"

PXR_NAMESPACE_OPEN_SCOPE

HdBufferRelocatorGL::HdBufferRelocatorGL(HdBufferResourceGPUHandle srcBuffer, HdBufferResourceGPUHandle dstBuffer)
{
    _srcBuffer = (GLuint)(uint64_t)srcBuffer;
    _dstBuffer = (GLuint)(uint64_t)dstBuffer;
}

void
HdBufferRelocatorGL::Commit()
{
    HdRenderContextCaps const &caps = HdRenderContextCaps::GetInstance();

    if (caps.copyBufferEnabled) {
        // glCopyBuffer
        if (!caps.directStateAccessEnabled) {
            glBindBuffer(GL_COPY_READ_BUFFER, (GLint)(uint64_t)_srcBuffer);
            glBindBuffer(GL_COPY_WRITE_BUFFER, (GLint)(uint64_t)_dstBuffer);
        }

        TF_FOR_ALL (it, _queue) {
            if (ARCH_LIKELY(caps.directStateAccessEnabled)) {
                glNamedCopyBufferSubDataEXT((GLint)(uint64_t)_srcBuffer,
                                            (GLint)(uint64_t)_dstBuffer,
                                            it->readOffset,
                                            it->writeOffset,
                                            it->copySize);
            } else {
                glCopyBufferSubData(GL_COPY_READ_BUFFER,
                                    GL_COPY_WRITE_BUFFER,
                                    it->readOffset,
                                    it->writeOffset,
                                    it->copySize);
            }
        }
        HD_PERF_COUNTER_ADD(HdPerfTokens->glCopyBufferSubData,
                            (double)_queue.size());

        if (!caps.directStateAccessEnabled) {
            glBindBuffer(GL_COPY_READ_BUFFER, 0);
            glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
        }
    } else {
        // read back to CPU and send it to GPU again
        // (workaround for a driver crash)
        TF_FOR_ALL (it, _queue) {
            std::vector<char> data(it->copySize);
            glBindBuffer(GL_ARRAY_BUFFER, (GLint)(uint64_t)_srcBuffer);
            glGetBufferSubData(GL_ARRAY_BUFFER, it->readOffset, it->copySize,
                               &data[0]);
            glBindBuffer(GL_ARRAY_BUFFER, (GLint)(uint64_t)_dstBuffer);
            glBufferSubData(GL_ARRAY_BUFFER, it->writeOffset, it->copySize,
                            &data[0]);
        }
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    _queue.clear();
}

PXR_NAMESPACE_CLOSE_SCOPE

