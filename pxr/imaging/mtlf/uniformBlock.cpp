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
    _size(0)
{
    _buffer = nil;
}

MtlfUniformBlock::~MtlfUniformBlock()
{
    if (_buffer) {
        MtlfMetalContext::GetMetalContext()->ReleaseMetalBuffer(_buffer);
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
    // Only recreate buffer if one doesn't already exist or the size has changed
    if (!_buffer || _buffer.length != size) {
        _buffer = MtlfMetalContext::GetMetalContext()->GetMetalBuffer(size, MTLResourceStorageModeDefault, data);
    } else if (_size > 0 && [_buffer respondsToSelector:@selector(didModifyRange:)]) {
        if([_buffer storageMode] == MTLStorageModeManaged) {
            NSRange range = NSMakeRange(0, size);
            memcpy(_buffer.contents, data, size);
            id<MTLResource> resource = _buffer;
            ARCH_PRAGMA_PUSH
            ARCH_PRAGMA_INSTANCE_METHOD_NOT_FOUND
            [resource didModifyRange:range];
            ARCH_PRAGMA_POP
        }
        else {
            TF_WARN("Unable to update Metal uniform block as it's storage mode is not managed");
        }
    } else {
        TF_WARN("Unable to update Metal uniform block as architecture does not support didModifyRange");
    }
    
    if (_size != size) {
//        glBufferData(GL_UNIFORM_BUFFER, size, NULL, GL_STATIC_DRAW);
        _size = size;
    }
}

PXR_NAMESPACE_CLOSE_SCOPE

