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
// uniformBlock.cpp
//
#include "pxr/imaging/mtlf/mtlDevice.h"

#include "pxr/imaging/mtlf/uniformBlock.h"
#include "pxr/imaging/mtlf/bindingMap.h"

PXR_NAMESPACE_OPEN_SCOPE


MtlfUniformBlock::MtlfUniformBlock() :
    _buffer(0), _size(0)
{
    TF_FATAL_CODING_ERROR("Not Implemented");
//    glGenBuffers(1, &_buffer);
}

MtlfUniformBlock::~MtlfUniformBlock()
{
    TF_FATAL_CODING_ERROR("Not Implemented");
//    if (_buffer) glDeleteBuffers(1, &_buffer);
}

void
MtlfUniformBlock::Bind(GarchBindingMapPtr const & bindingMap,
                       std::string const & identifier)
{
    TF_FATAL_CODING_ERROR("Not Implemented");

    if (!bindingMap) return;
//    int binding = bindingMap->GetUniformBinding(identifier);
//    glBindBufferBase(GL_UNIFORM_BUFFER, binding, _buffer);
}

void
MtlfUniformBlock::Update(const void *data, int size)
{
    TF_FATAL_CODING_ERROR("Not Implemented");
    /*
    glBindBuffer(GL_UNIFORM_BUFFER, _buffer);
    if (_size != size) {
        glBufferData(GL_UNIFORM_BUFFER, size, NULL, GL_STATIC_DRAW);
        _size = size;
    }
    if (size > 0) {
        // Bug 95969 BufferSubData w/ size == 0 should be a noop but
        // raises errors on some NVIDIA drivers.
        glBufferSubData(GL_UNIFORM_BUFFER, 0, size, data);
    }
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
     */
}

PXR_NAMESPACE_CLOSE_SCOPE

