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
#include "pxr/imaging/hdSt/api.h"
#include "pxr/imaging/hdSt/bufferResource.h"
#include "pxr/imaging/mtlf/mtlDevice.h"

#include "pxr/base/tf/token.h"

#include <boost/shared_ptr.hpp>
#include <cstddef>
#include <utility>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE


class HdStBufferResourceMetal;

typedef boost::shared_ptr<HdStBufferResourceMetal> HdStBufferResourceMetalSharedPtr;

typedef std::vector<
    std::pair<TfToken, HdStBufferResourceMetalSharedPtr> > HdStBufferResourceMetalNamedList;

/// \class HdStBufferResourceGL
///
/// A specific type of HdBufferResource (GPU resource) representing an 
/// OpenGL buffer object.
///
class HdStBufferResourceMetal : public HdStBufferResource {
public:
    HDST_API
    HdStBufferResourceMetal(TfToken const &role,
                            HdTupleType tupleType,
                            int offset,
                            int stride);
    HDST_API
    virtual ~HdStBufferResourceMetal();

    /// Sets the OpenGL name/identifier for this resource and its size.
    /// also caches the gpu address of the buffer.
    HDST_API
    virtual void SetAllocation(HdBufferResourceGPUHandle buffer, size_t size) override;

    /// Returns the Metal object for this GPU resource
    HDST_API
    virtual HdBufferResourceGPUHandle GetId() const { return (__bridge HdBufferResourceGPUHandle)_id; }

    /// Returns the gpu address (if available. otherwise returns 0).
    HDST_API
    virtual uint64_t GetGPUAddress() const override { return _gpuAddr; }

    /// Returns the texture buffer view
    HDST_API
    virtual GarchTextureGPUHandle GetTextureBuffer() override;

    HDST_API
    virtual void CopyData(size_t vboOffset, size_t dataSize, void const *data) override;

    HDST_API
    virtual VtValue ReadBuffer(HdTupleType tupleType,
                               int vboOffset,
                               int stride,
                               int numElements) override;

    HDST_API
    virtual uint8_t const* GetBufferContents() const override;
    
private:
    uint64_t        _gpuAddr;
    id<MTLTexture>  _texId;
    id<MTLBuffer>   _id;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif //HD_BUFFER_RESOURCE_METAL_H
