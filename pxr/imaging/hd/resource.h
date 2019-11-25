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
#ifndef PXR_IMAGING_HD_RESOURCE_H
#define PXR_IMAGING_HD_RESOURCE_H

#include "pxr/pxr.h"
#include "pxr/base/arch/defines.h"
#include "pxr/base/tf/token.h"

#include "pxr/imaging/hd/api.h"
#include "pxr/imaging/hd/version.h"

#include "pxr/imaging/garch/gl.h"

#if defined(ARCH_GFX_METAL)
#include "pxr/imaging/mtlf/mtlDevice.h"
#endif

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>

#include <cstddef>

PXR_NAMESPACE_OPEN_SCOPE


typedef boost::shared_ptr<class HdResource> HdResourceSharedPtr;

struct HdResourceGPUHandle {

    void Clear() {
        handle = 0;
    }
    bool IsSet() const { return handle != 0; }

#if defined(ARCH_GFX_OPENGL)
    // OpenGL
    HdResourceGPUHandle(GLuint const _handle) {
        handle = (void*)uint64_t(_handle);
    }
    HdResourceGPUHandle& operator =(GLuint const _handle) {
        handle = (void*)uint64_t(_handle);
        return *this;
    }
    operator GLuint() const { return (GLuint)uint64_t(handle); }
#endif // ARCH_GFX_OPENGL
    
    bool operator !=(HdResourceGPUHandle const _handle) const {
        return handle != _handle.handle;
    }
    
    bool operator ==(HdResourceGPUHandle const _handle) const {
        return handle == _handle.handle;
    }
    
    bool operator <(HdResourceGPUHandle const _handle) const {
        return handle < _handle.handle;
    }
    
    bool operator >(HdResourceGPUHandle const _handle) const {
        return handle > _handle.handle;
    }


#if defined(ARCH_GFX_METAL)
    HdResourceGPUHandle() {
        multiBuffer.Clear();
    }
    HdResourceGPUHandle(HdResourceGPUHandle const & _gpuHandle) {
        multiBuffer = _gpuHandle.multiBuffer;
    }

    HdResourceGPUHandle(MtlfMetalContext::MtlfMultiBuffer const _handle) {
        multiBuffer = _handle;
    }
    HdResourceGPUHandle& operator =(MtlfMetalContext::MtlfMultiBuffer const _handle) {
        multiBuffer = _handle;
        return *this;
    }
    operator MtlfMetalContext::MtlfMultiBuffer() const { return multiBuffer; }

#endif // ARCH_GFX_METAL

    // Storage
    union {
        void* handle;
#if defined(ARCH_GFX_METAL)
        MtlfMetalContext::MtlfMultiBuffer multiBuffer;
#endif
    };
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

#endif //PXR_IMAGING_HD_RESOURCE_H
