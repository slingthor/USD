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

#if defined(PXR_METAL_SUPPORT_ENABLED)
#include "pxr/imaging/mtlf/mtlDevice.h"
#endif

#include <cstddef>
#include <memory>

PXR_NAMESPACE_OPEN_SCOPE


using HdResourceSharedPtr = std::shared_ptr<class HdResource>;

struct HdResourceGPUHandle {

    void Clear() {
        handle = 0;
    }
    bool IsSet() const { return handle != 0; }

#if defined(PXR_OPENGL_SUPPORT_ENABLED)
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


#if defined(PXR_METAL_SUPPORT_ENABLED)
    HdResourceGPUHandle() {
        buffer = nil;
    }
    HdResourceGPUHandle(HdResourceGPUHandle const & _gpuHandle) {
        buffer = _gpuHandle.buffer;
    }

    HdResourceGPUHandle(id<MTLBuffer> const _handle) {
        buffer = _handle;
    }
    HdResourceGPUHandle& operator =(id<MTLBuffer> const _handle) {
        buffer = _handle;
        return *this;
    }
    operator id<MTLBuffer>() const { return buffer; }

#endif // PXR_METAL_SUPPORT_ENABLED

    // Storage
    union {
        void* handle;
#if defined(PXR_METAL_SUPPORT_ENABLED)
        id<MTLBuffer> buffer;
#endif
    };
};
/// \class HdResource
///
/// Base class for all GPU resource objects.
///
class HdResource
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

protected:
    /// Stores the size of the resource allocated in the GPU
    HD_API
    void SetSize(size_t size);

private:

    // Don't allow copies.
    HdResource(const HdResource &) = delete;
    HdResource &operator=(const HdResource &) = delete;


    const TfToken _role;
    size_t _size;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif //PXR_IMAGING_HD_RESOURCE_H
