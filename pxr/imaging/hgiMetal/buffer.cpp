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
#include <Metal/Metal.h>

#include "pxr/base/arch/defines.h"

#include "pxr/imaging/hgiMetal/hgi.h"
#include "pxr/imaging/hgiMetal/diagnostic.h"
#include "pxr/imaging/hgiMetal/buffer.h"


PXR_NAMESPACE_OPEN_SCOPE

HgiMetalBuffer::HgiMetalBuffer(HgiMetal *hgi, HgiBufferDesc const & desc)
    : HgiBuffer(desc)
    , _bufferId(nil)
{

    if (desc.byteSize == 0) {
        TF_CODING_ERROR("Buffers must have a non-zero length");
    }

    MTLResourceOptions resourceOptions =
        MTLResourceStorageModeDefault|MTLResourceCPUCacheModeDefaultCache;
    
    if (desc.initialData) {
        _bufferId = [hgi->GetDevice() newBufferWithBytes:desc.initialData
                                                  length:desc.byteSize
                                                  options:resourceOptions];
    }
    else {
        _bufferId = [hgi->GetDevice() newBufferWithLength:desc.byteSize
                                                  options:resourceOptions];
    }
    
    _descriptor.initialData = nullptr;

    _bufferId.label =
        [NSString stringWithUTF8String:_descriptor.debugName.c_str()];
}

HgiMetalBuffer::~HgiMetalBuffer()
{
    if (_bufferId != nil) {
        [_bufferId release];
        _bufferId = nil;
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
