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
#ifndef HD_RESOURCE_H
#define HD_RESOURCE_H

#include "pxr/pxr.h"
#include "pxr/base/arch/defines.h"
#include "pxr/base/tf/token.h"

#include "pxr/imaging/hd/api.h"
#include "pxr/imaging/hd/version.h"

#include "pxr/imaging/garch/gl.h"

#if defined(ARCH_GFX_METAL)
#include <Metal/Metal.h>
#endif

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>

#include <cstddef>

PXR_NAMESPACE_OPEN_SCOPE


typedef boost::shared_ptr<class HdResource> HdResourceSharedPtr;

struct HdResourceGPUHandle {

#if defined(ARCH_GFX_USE_TRIPLE_BUFFERING)
    static int const numHandles = 3;
#else
    static int const numHandles = 1;
#endif

    HdResourceGPUHandle() {
        for (int i = 0; i < numHandles; i++) {
            handle[i] = 0;
        }
    }
    HdResourceGPUHandle(HdResourceGPUHandle const & _gpuHandle) {
        for (int i = 0; i < numHandles; i++) {
            handle[i] = _gpuHandle.handle[i];
        }
    }
    
    void Clear() {
        for (int i = 0; i < numHandles; i++) {
            handle[i] = 0;
        }
    }
    bool IsSet() const { return handle[0] != 0; }

#if defined(ARCH_GFX_OPENGL)
    // OpenGL
    HdResourceGPUHandle(GLuint const _handle) {
        handle[0] = (void*)uint64_t(_handle);
#if defined(ARCH_GFX_USE_TRIPLE_BUFFERING)
        handle[1] = NULL;
        handle[2] = NULL;
#endif
    }
    HdResourceGPUHandle& operator =(GLuint const _handle) {
        handle[0] = (void*)uint64_t(_handle);
#if defined(ARCH_GFX_USE_TRIPLE_BUFFERING)
        handle[1] = NULL;
        handle[2] = NULL;
#endif
        return *this;
    }
    operator GLuint() const { return (GLuint)uint64_t(handle[0]); }
    
    bool operator !=(HdResourceGPUHandle const _handle) const {
        return handle[0] != _handle;
    }
    
    bool operator ==(HdResourceGPUHandle const _handle) const {
        return handle[0] == _handle;
    }
    
    bool operator <(HdResourceGPUHandle const _handle) const {
        return handle[0] < _handle;
    }
    
    bool operator >(HdResourceGPUHandle const _handle) const {
        return handle[0] > _handle;
    }
#endif // ARCH_GFX_OPENGL

#if defined(ARCH_GFX_METAL)
    // Metal
    HdResourceGPUHandle(id<MTLBuffer> const _handle) {
        handle[0] = (__bridge void*)_handle;
#if defined(ARCH_GFX_USE_TRIPLE_BUFFERING)
        handle[1] = NULL;
        handle[2] = NULL;
#endif
    }
    HdResourceGPUHandle& operator =(id<MTLBuffer> const _handle) {
        handle[0] = (__bridge void*)_handle;
#if defined(ARCH_GFX_USE_TRIPLE_BUFFERING)
        handle[1] = NULL;
        handle[2] = NULL;
#endif
        return *this;
    }
    operator id<MTLBuffer>() const { return (__bridge id<MTLBuffer>)handle[0]; }

#if defined(ARCH_GFX_USE_TRIPLE_BUFFERING)
    // Metal - triple buffered
    HdResourceGPUHandle(id<MTLBuffer> const _handle0,
                        id<MTLBuffer> const _handle1,
                        id<MTLBuffer> const _handle2) {
        handle[0] = (__bridge void*)_handle0;
        handle[1] = (__bridge void*)_handle1;
        handle[2] = (__bridge void*)_handle2;
    }
#endif // ARCH_GFX_TRIPLE_BUFFER
    id<MTLBuffer> operator[](const int index) const { return (__bridge id<MTLBuffer>)handle[index]; }

#endif // ARCH_GFX_METAL
    
    // Storage
    void* handle[numHandles];
};
/// \class HdResource
///
/// Base class for all GPU resource objects.
///
class HdResource : boost::noncopyable 
{
public:
    HD_API
    HdResource(TfToken const & role);
    HD_API
    virtual ~HdResource();

    /// Returns the role of the GPU data in this resource.
    TfToken const & GetRole() const {return _role;}

    /// Returns the size of the resource allocated in the GPU
    HD_API
    size_t GetSize() const {return _size;}
    
    /// Sets the OpenGL name/identifier for this resource and its size.
    /// also caches the gpu address of the buffer.
    HD_API
    virtual void SetAllocation(HdResourceGPUHandle id, size_t size) = 0;
    
    /// The graphics API name/identifier for this resource
    HD_API
    virtual HdResourceGPUHandle GetId() const = 0;

protected:
    /// Stores the size of the resource allocated in the GPU
    HD_API
    void SetSize(size_t size);

private:
    const TfToken _role;
    size_t _size;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif //HD_RESOURCE_H
