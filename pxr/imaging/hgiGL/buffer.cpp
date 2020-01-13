//
// Copyright 2019 Pixar
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
#include <GL/glew.h>
#include "pxr/imaging/hgiGL/diagnostic.h"
#include "pxr/imaging/hgiGL/buffer.h"


PXR_NAMESPACE_OPEN_SCOPE

HgiGLBuffer::HgiGLBuffer(HgiBufferDesc const & desc)
    : HgiBuffer(desc)
    , _bufferId(0)
{

    if (desc.length == 0) {
        TF_CODING_ERROR("Buffers must have a non-zero length");
    }
    
    if (desc.usage & HgiBufferUsageVertexData) {
        _target = GL_ARRAY_BUFFER;
    } else if (desc.usage & HgiBufferUsageIndices) {
        _target = GL_ELEMENT_ARRAY_BUFFER;
    } else if (desc.usage & HgiBufferUsageUniforms) {
        _target = GL_UNIFORM_BUFFER;
    } else {
        TF_CODING_ERROR("Unknown HgTextureUsage bit");
    }

    _length = desc.length;

    glGenBuffers(1, &_bufferId);
    glBindBuffer(_target, _bufferId);
    glBufferData(_target, desc.length, NULL, GL_STATIC_DRAW);
    glBindBuffer(_target, 0);

    HGIGL_POST_PENDING_GL_ERRORS();
}

HgiGLBuffer::~HgiGLBuffer()
{
    if (_bufferId > 0) {
        glDeleteBuffers(1, &_bufferId);
        _bufferId = 0;
    }

    HGIGL_POST_PENDING_GL_ERRORS();
}

void HgiGLBuffer::Copy(void const *data, size_t offset, size_t size) {
    
    glBindBuffer(_target, _bufferId);
    
    if (offset || size != _length) {
        if (glBufferSubData != NULL) {
            glBufferSubData(_target, offset, size, data);
        }
        else {
            TF_CODING_ERROR("glBufferSubData is not available");
        }
    }
    else {
        glBufferData(_target, size, data, GL_STATIC_DRAW);
    }

    glBindBuffer(_target, 0);
    
    HGIGL_POST_PENDING_GL_ERRORS();
}

PXR_NAMESPACE_CLOSE_SCOPE
