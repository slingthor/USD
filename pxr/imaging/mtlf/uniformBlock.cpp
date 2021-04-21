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


MtlfUniformBlock::MtlfUniformBlock(char const *label)
    : _lastFrameModified(0)
    , _activeBuffer(0)
{
    for (int32_t i = 0; i < MULTIBUFFERING; i++) {
        _buffers[i] = nil;
    }
}

MtlfUniformBlock::~MtlfUniformBlock()
{
    for (int32_t i = 0; i < MULTIBUFFERING; i++) {
        if (_buffers[i]) {
            MtlfMetalContext::GetMetalContext()->ReleaseMetalBuffer(_buffers[i]);
            _buffers[i] = nil;
        }
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

    MtlfMetalContext::GetMetalContext()->SetUniformBuffer(mtlfBindingIndex.index, _buffers[_activeBuffer], TfToken(identifier), (MSL_ProgramStage)mtlfBindingIndex.stage);
}

void
MtlfUniformBlock::Update(const void *data, int size)
{
    int64_t currentFrame = MtlfMetalContext::GetMetalContext()->GetCurrentFrame();
    
    if (currentFrame != _lastFrameModified) {
        _activeBuffer++;
        _activeBuffer = (_activeBuffer < MULTIBUFFERING) ? _activeBuffer : 0;
        _lastFrameModified = currentFrame;
    }
    
    // Only recreate buffer if one doesn't already exist or the size has changed
    if (!_buffers[_activeBuffer] || _buffers[_activeBuffer].length != size) {
         if (_buffers[_activeBuffer]) {
             MtlfMetalContext::GetMetalContext()->ReleaseMetalBuffer(_buffers[_activeBuffer]);
         }
        _buffers[_activeBuffer] = MtlfMetalContext::GetMetalContext()->GetMetalBuffer(size, MTLResourceStorageModeDefault, data);
    } else if (size > 0) {
        NSRange range = NSMakeRange(0, size);
        memcpy(_buffers[_activeBuffer].contents, data, size);
        id<MTLResource> resource = _buffers[_activeBuffer];
        //We assume the buffer storage mode is managed for discrete GPUs
        //Apple silicon and intel based machines have shared buffers and don't need to perform didModifyRange
        if([_buffers[_activeBuffer] respondsToSelector:@selector(didModifyRange:)]){
            ARCH_PRAGMA_PUSH
            ARCH_PRAGMA_INSTANCE_METHOD_NOT_FOUND
            [resource didModifyRange:range];
            ARCH_PRAGMA_POP
        }
    }
}

PXR_NAMESPACE_CLOSE_SCOPE

