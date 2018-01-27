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
#include "pxr/imaging/mtlf/mtlDevice.h"

#include "pxr/imaging/hd/Metal/persistentBufferMetal.h"

#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/renderContextCaps.h"

#include "pxr/imaging/hf/perfLog.h"

PXR_NAMESPACE_OPEN_SCOPE


HdPersistentBufferMetal::HdPersistentBufferMetal(
    TfToken const &role, size_t dataSize, void* data)
    : HdResourceMetal(role)
    , _mappedAddress(0)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    id<MTLBuffer> newId = nil;
    
    newId = [MtlfMetalContext::GetMetalContext()->device newBufferWithBytes:data length:dataSize options:MTLResourceStorageModeManaged];
    _mappedAddress = [newId contents];

    SetAllocation(newId, dataSize);
}

HdPersistentBufferMetal::~HdPersistentBufferMetal()
{
    id<MTLBuffer> buffer = id<MTLBuffer>(GetMetalId());
    if (buffer) {
        [buffer release];
    }
    buffer = nil;
    SetAllocation(buffer, 0);
}

PXR_NAMESPACE_CLOSE_SCOPE

