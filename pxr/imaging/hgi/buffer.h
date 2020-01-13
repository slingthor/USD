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
#ifndef PXR_IMAGING_HGI_BUFFER_H
#define PXR_IMAGING_HGI_BUFFER_H

#include "pxr/pxr.h"
#include "pxr/base/gf/vec3i.h"
#include "pxr/imaging/hgi/api.h"
#include "pxr/imaging/hgi/enums.h"
#include "pxr/imaging/hgi/types.h"


PXR_NAMESPACE_OPEN_SCOPE

struct HgiBufferDesc;


///
/// \class HgiBuffer
///
/// Represents a graphics platform independent GPU buffer resource.
///
/// Base class for Hgi buffers.
/// To the client (HdSt) buffer resources are referred to via
/// opaque, stateless handles (HgBufferHandle).
///
class HgiBuffer {
public:
    HGI_API
    HgiBuffer(HgiBufferDesc const& desc);

    HGI_API
    virtual ~HgiBuffer();
    
    HGI_API
    virtual void Copy(void const *data, size_t offset, size_t size) = 0;

private:
    HgiBuffer() = delete;
    HgiBuffer & operator=(const HgiBuffer&) = delete;
    HgiBuffer(const HgiBuffer&) = delete;
};

typedef HgiBuffer* HgiBufferHandle;



/// \struct HgiBufferDesc
///
/// Describes the properties needed to create a GPU buffer.
///
/// <ul>
/// <li>length:
///   The size of the buffer in bytes.</li>
/// </ul>
///
struct HgiBufferDesc {
    HgiBufferDesc()
    : usage(HgiBufferUsageUniforms)
    , length(0)
    {}

    HgiBufferUsage usage;
    size_t length;
};

HGI_API
bool operator==(
    const HgiBufferDesc& lhs,
    const HgiBufferDesc& rhs);

HGI_API
inline bool operator!=(
    const HgiBufferDesc& lhs,
    const HgiBufferDesc& rhs);


PXR_NAMESPACE_CLOSE_SCOPE

#endif
