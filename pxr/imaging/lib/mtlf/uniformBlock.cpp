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


MtlfUniformBlock::MtlfUniformBlock(char const *label) :
    _buffer(nil), _size(0)
{
}

MtlfUniformBlock::~MtlfUniformBlock()
{
    if (_buffer) {
        [_buffer release];
        _buffer = nil;
    }
}

void
MtlfUniformBlock::Bind(GarchBindingMapPtr const & bindingMap,
                       std::string const & identifier)
{
    if (!bindingMap)
        return;

    MtlfBindingMap::MTLFBindingIndex mtlfBindingIndex(bindingMap->GetUniformBinding(identifier));
    if(!mtlfBindingIndex.isLinked)
        return; //We're trying to bind a buffer that the shader doesn't know about. We should ignore this.
    MtlfMetalContext::GetMetalContext()->SetUniformBuffer(mtlfBindingIndex.index, _buffer, TfToken(identifier), (MSL_ProgramStage)mtlfBindingIndex.stage);
}

void
MtlfUniformBlock::Update(const void *data, int size)
{
    if (_buffer == nil) {
        id<MTLDevice> device = MtlfMetalContext::GetMetalContext()->device;
        _buffer = [device newBufferWithBytes:data length:size options:MTLResourceStorageModeManaged];
    }
    
    //METAL TODO
    
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

