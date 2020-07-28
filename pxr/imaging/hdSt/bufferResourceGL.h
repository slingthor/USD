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
#ifndef PXR_IMAGING_HD_ST_BUFFER_RESOURCE_GL_H
#define PXR_IMAGING_HD_ST_BUFFER_RESOURCE_GL_H

#include "pxr/pxr.h"
#include "pxr/imaging/hdSt/api.h"
#include "pxr/imaging/hd/version.h"
#include "pxr/imaging/hd/bufferResource.h"

#include "pxr/imaging/hgi/buffer.h"

#include "pxr/base/tf/token.h"

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

using HdStBufferResourceGLSharedPtr =
    std::shared_ptr<class HdStBufferResourceGL>;

using HdStBufferResourceGLNamedPair =
    std::pair<TfToken, HdStBufferResourceGLSharedPtr>;
using HdStBufferResourceGLNamedList =
    std::vector<HdStBufferResourceGLNamedPair>;

class Hgi;

/// \class HdStBufferResourceGL
///
/// A specific type of HdBufferResource (GPU resource) representing an 
/// OpenGL buffer object.
///
class HdStBufferResourceGL final : public HdBufferResource
{
public:
    HDST_API
    HdStBufferResourceGL(TfToken const &role,
                         HdTupleType tupleType,
                         int offset,
                         int stride);
    HDST_API
    ~HdStBufferResourceGL();
    
    /// Sets the OpenGL name/identifier for this resource and its size.
    /// also caches the gpu address of the buffer.
    HDST_API
    void SetAllocation(HgiBufferHandle const& id, size_t size);
    
    // APPLE METAL: Multibuffering support.
    void SetAllocations(HgiBufferHandle const& id0,
                        HgiBufferHandle const& id1,
                        HgiBufferHandle const& id2,
                        size_t size);

    /// Returns the Hgi id for this GPU resource
    HgiBufferHandle& GetId() { return _ids[_activeBuffer]; }
    
    // APPLE METAL: Multibuffering support.
    /// Returns the Metal object at the triple buffer index for this GPU resource
    HgiBufferHandle& GetId(int32_t const index) {
        return _ids[index];
    }
    
    // APPLE METAL: Multibuffering support.
    HDST_API
    virtual void CopyData(Hgi* hgi,
                          size_t vboOffset,
                          size_t dataSize,
                          void const *data);
    
    // APPLE METAL: Multibuffering support.
    HDST_API
    virtual VtValue ReadBuffer(Hgi* hgi,
                               HdTupleType tupleType,
                               int vboOffset,
                               int stride,
                               int numElements);

    /// Returns the gpu address (if available. otherwise returns 0).
    uint64_t GetGPUAddress() const {
        return _gpuAddr[_activeBuffer];
    }
    
    // APPLE METAL: Multibuffering support.
    static constexpr int32_t MULTIBUFFERING = 3;

private:
    // APPLE METAL: Multibuffering support.
    uint64_t _gpuAddr[MULTIBUFFERING];
    HgiBufferHandle _ids[MULTIBUFFERING];
    int32_t _lastFrameModified;
    int32_t _activeBuffer;
    bool _firstFrameBeingFilled;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_IMAGING_HD_ST_BUFFER_RESOURCE_GL_H
