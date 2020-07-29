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

PXR_NAMESPACE_OPEN_SCOPE

bool
HdStGLUtils::GetShaderCompileStatus(GLuint shader, std::string * reason)
{
#if defined(PXR_OPENGL_SUPPORT_ENABLED)
    // glew has to be initialized
    if (!glGetShaderiv) return true;

    GLint status = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (reason) {
        GLint infoLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLength);
        if (infoLength > 0) {
            char *infoLog = new char[infoLength];;
            glGetShaderInfoLog(shader, infoLength, NULL, infoLog);
            reason->assign(infoLog, infoLength);
            delete[] infoLog;
        }
    }
    return (status == GL_TRUE);
#else
    return true;
#endif
}

bool
HdStGLUtils::GetProgramLinkStatus(GLuint program, std::string * reason)
{
#if defined(PXR_OPENGL_SUPPORT_ENABLED)
    // glew has to be initialized
    if (!glGetProgramiv) return true;

    GLint status = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (reason) {
        GLint infoLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLength);
        if (infoLength > 0) {
            char *infoLog = new char[infoLength];;
            glGetProgramInfoLog(program, infoLength, NULL, infoLog);
            reason->assign(infoLog, infoLength);
            delete[] infoLog;
        }
    }
    return (status == GL_TRUE);
#else
    return true;
#endif
}

// ---------------------------------------------------------------------------

void
HdStGLBufferRelocator::AddRange(GLintptr readOffset,
                              GLintptr writeOffset,
                              GLsizeiptr copySize)
{
    _CopyUnit unit(readOffset, writeOffset, copySize);
    if (_queue.empty() || (!_queue.back().Concat(unit))) {
        _queue.push_back(unit);
    }
}

void
HdStGLBufferRelocator::Commit(Hgi* hgi)
{
    HgiBufferGpuToGpuOp blitOp;
    blitOp.gpuSourceBuffer = _srcBuffer;
    blitOp.gpuDestinationBuffer = _dstBuffer;

    // Use blit work to record resource copy commands.
    HgiBlitCmdsUniquePtr blitCmds = hgi->CreateBlitCmds();
    
    TF_FOR_ALL (it, _queue) {
        blitOp.sourceByteOffset = it->readOffset;
        blitOp.byteSize = it->copySize;
        blitOp.destinationByteOffset = it->writeOffset;

        blitCmds->CopyBufferGpuToGpu(blitOp);
    }
    hgi->SubmitCmds(blitCmds.get());

    HD_PERF_COUNTER_ADD(HdPerfTokens->glCopyBufferSubData,
                        (double)_queue.size());

    _queue.clear();
}

PXR_NAMESPACE_CLOSE_SCOPE

