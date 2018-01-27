//
// Copyright 2017 Pixar
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
#ifndef HD_BUFFER_RESOURCE_METAL_H
#define HD_BUFFER_RESOURCE_METAL_H

#include "pxr/pxr.h"
#include "pxr/imaging/hd/api.h"
#include "pxr/imaging/hd/version.h"
#include "pxr/imaging/hd/bufferResource.h"
#include "pxr/imaging/mtlf/mtlDevice.h"

#include "pxr/base/tf/token.h"

#include <boost/shared_ptr.hpp>
#include <cstddef>
#include <utility>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE


class HdBufferResourceMetal;

typedef boost::shared_ptr<HdBufferResourceMetal> HdBufferResourceMetalSharedPtr;

typedef std::vector<
    std::pair<TfToken, HdBufferResourceMetalSharedPtr> > HdBufferResourceMetalNamedList;

/// \class HdBufferResourceGL
///
/// A specific type of HdBufferResource (GPU resource) representing an 
/// OpenGL buffer object.
///
class HdBufferResourceMetal : public HdBufferResource {
public:
    HD_API
    HdBufferResourceMetal(TfToken const &role,
                     int glDataType,
                     short numComponents,
                     int arraySize,
                     int offset,
                     int stride);
    HD_API
    ~HdBufferResourceMetal();

    /// Sets the OpenGL name/identifier for this resource and its size.
    /// also caches the gpu address of the buffer.
    HD_API
    virtual void SetAllocation(HdBufferResourceGPUHandle buffer, size_t size);

    /// Returns the Metal object for this GPU resource
    virtual HdBufferResourceGPUHandle GetId() const { return (__bridge HdBufferResourceGPUHandle)_id; }

    /// Returns the gpu address (if available. otherwise returns 0).
    uint64_t GetGPUAddress() const { return _gpuAddr; }

    /// Returns the texture buffer view
    HD_API
    HdBufferResourceGPUHandle GetTextureBuffer();

    HD_API
    virtual void CopyData(size_t vboOffset, size_t dataSize, void const *data);

    HD_API
    virtual VtValue ReadBuffer(int glDataType,
                               int numComponents,
                               int arraySize,
                               int vboOffset,
                               int stride,
                               int numElements);

    HD_API
    virtual uint8_t const* GetBufferContents() const;
    
private:
    uint64_t        _gpuAddr;
    id<MTLTexture>  _texId;
    id<MTLBuffer>   _id;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif //HD_BUFFER_RESOURCE_METAL_H
