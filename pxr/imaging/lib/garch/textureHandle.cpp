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
/// \file GLTextureObject.cpp

// 

#include "pxr/imaging/garch/textureHandle.h"
#include "pxr/imaging/garch/texture.h"
#include "pxr/imaging/garch/textureRegistry.h"

#include "pxr/base/tf/fileUtils.h"
#include "pxr/base/tf/diagnostic.h"
#include "pxr/base/trace/trace.h"

#include <iostream>

PXR_NAMESPACE_OPEN_SCOPE


GarchTextureHandleRefPtr
GarchTextureHandle::New(GarchTextureRefPtr texture)
{
    return TfCreateRefPtr(new GarchTextureHandle(texture));
}

GarchTextureHandle::GarchTextureHandle(GarchTextureRefPtr texture) :
    _texture(texture)
{
}

GarchTextureHandle::~GarchTextureHandle()
{
}

void
GarchTextureHandle::AddMemoryRequest(size_t targetMemory)
{
    _requestedMemories[targetMemory]++;
    _ComputeMemoryRequirement();
}

void
GarchTextureHandle::DeleteMemoryRequest(size_t targetMemory)
{
    std::map<size_t, size_t>::iterator it =
        _requestedMemories.find(targetMemory);

    if (it != _requestedMemories.end()) {
        it->second--;
        if(it->second == 0){
            _requestedMemories.erase(it);
        }
    }

    if (_requestedMemories.empty()) {
        // nobody refers this texture.
        GarchTextureRegistry::GetInstance().RequiresGarbageCollection();
    } else {
        _ComputeMemoryRequirement();
    }
}

void
GarchTextureHandle::_ComputeMemoryRequirement()
{
    // adjust resolution to the maximum requirement
    size_t maxTargetMemory = 0;
    if (!_requestedMemories.empty()) {
        maxTargetMemory = _requestedMemories.rbegin()->first;
    }

    _texture->SetMemoryRequested(maxTargetMemory);

}

PXR_NAMESPACE_CLOSE_SCOPE

