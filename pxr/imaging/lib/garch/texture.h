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
#ifndef GARCH_TEXTURE_H
#define GARCH_TEXTURE_H

/// \file garch/texture.h

#include "pxr/pxr.h"
#include "pxr/imaging/garch/api.h"
#include "pxr/imaging/garch/image.h"
#include "pxr/base/tf/declarePtrs.h"
#include "pxr/base/tf/refPtr.h"
#include "pxr/base/tf/staticTokens.h"
#include "pxr/base/tf/weakPtr.h"
#include "pxr/base/vt/dictionary.h"

#include "pxr/imaging/garch/gl.h"

#if defined(ARCH_OS_WINDOWS)
typedef uint64_t GLuint64;
#endif

#if defined(ARCH_GFX_METAL)
#include <Metal/Metal.h>
#endif

#include <map>
#include <string>
#include <vector>
#include <boost/noncopyable.hpp>

PXR_NAMESPACE_OPEN_SCOPE


#define GARCH_TEXTURE_TOKENS                      \
    (texels)                                    \
    (layout)

TF_DECLARE_PUBLIC_TOKENS(GarchTextureTokens, GARCH_API, GARCH_TEXTURE_TOKENS);

TF_DECLARE_WEAK_AND_REF_PTRS(GarchTexture);

struct GarchTextureGPUHandle {
    GarchTextureGPUHandle() {
        handle = 0;
    }
    GarchTextureGPUHandle(GarchTextureGPUHandle const & _gpuHandle) {
        handle = _gpuHandle.handle;
    }
    
    void Clear() { handle = 0; }
    bool IsSet() const { return handle != 0; }

    // OpenGL
    GarchTextureGPUHandle(GLuint const _handle) {
        handle = _handle;
    }
    GarchTextureGPUHandle(GLuint64 const _handle) {
        handle = _handle;
    }
    GarchTextureGPUHandle& operator =(GLuint const _handle) {
        handle = _handle;
        return *this;
    }
    GarchTextureGPUHandle& operator =(GLuint64 const _handle) {
        handle = _handle;
        return *this;
    }
    operator GLuint() const { return (GLuint)handle; }
    operator GLuint64() const { return handle; }

#if defined(ARCH_GFX_METAL)
    // Metal
    GarchTextureGPUHandle(id<MTLTexture> const _handle) {
        handle = (__bridge uint64_t)_handle;
    }
    GarchTextureGPUHandle& operator =(id<MTLTexture> const _handle) {
        handle = (__bridge uint64_t)_handle;
        return *this;
    }
    operator id<MTLTexture>() const { return (__bridge id<MTLTexture>)handle; }
#endif
    
    uint64_t handle;
};

struct GarchSamplerGPUHandle {
    GarchSamplerGPUHandle() {
        handle = 0;
    }
    GarchSamplerGPUHandle(GarchSamplerGPUHandle const & _gpuHandle) {
        handle = _gpuHandle.handle;
    }
    
    void Clear() { handle = 0; }
    bool IsSet() const { return handle != 0; }
    
    // OpenGL
    GarchSamplerGPUHandle(GLuint const _handle) {
        handle = _handle;
    }
    GarchSamplerGPUHandle(GLuint64 const _handle) {
        handle = _handle;
    }
    GarchSamplerGPUHandle& operator =(GLuint const _handle) {
        handle = _handle;
        return *this;
    }
    GarchSamplerGPUHandle& operator =(GLuint64 const _handle) {
        handle = _handle;
        return *this;
    }
    operator GLuint() const { return (GLuint)handle; }
    operator GLuint64() const { return handle; }
    
#if defined(ARCH_GFX_METAL)
    // Metal
    GarchSamplerGPUHandle(id<MTLSamplerState> const _handle) {
        handle = (__bridge uint64_t)_handle;
    }
    GarchSamplerGPUHandle& operator =(id<MTLSamplerState> const _handle) {
        handle = (__bridge uint64_t)_handle;
        return *this;
    }
    operator id<MTLSamplerState>() const { return (__bridge id<MTLSamplerState>)handle; }
#endif
    
    uint64_t handle;
};

/// \class GarchTexture
///
/// Represents a texture object in Garch.
///
/// A texture is typically defined by reading texture image data from an image
/// file but a texture might also represent an attachment of a draw target.
///
class GarchTexture : public TfRefBase, public TfWeakBase, boost::noncopyable {
public:
    /// \class Binding
    ///
    /// A texture has one or more bindings which describe how the different
    /// aspects of the texture should be bound in order to allow shader
    /// access. Most textures will have a single binding for the role
    /// "texels", but some textures might need multiple bindings, e.g. a
    /// ptexTexture will have an additional binding for the role "layout".
    ///
    struct Binding {
        Binding(TfToken name, TfToken role, GLenum target,
                GarchTextureGPUHandle textureId, GarchSamplerGPUHandle samplerId)
            : name(name)
            , role(role)
            , target(target)
            , textureId(textureId)
            , samplerId(samplerId) { }

            TfToken name;
            TfToken role;
            GLenum target;
            GarchTextureGPUHandle textureId;
            GarchSamplerGPUHandle samplerId;
    };
    typedef std::vector<Binding> BindingVector;

    GARCH_API
    virtual ~GarchTexture() = 0;

    /// Returns the bindings to use this texture for the shader resource
    /// named \a identifier. If \a samplerId is specified, the bindings
    /// returned will use this samplerId for resources which can be sampled.
    virtual BindingVector GetBindings(TfToken const & identifier,
                                      GarchSamplerGPUHandle samplerId = GarchSamplerGPUHandle()) const = 0;

    /// Amount of memory used to store the texture
    GARCH_API
    size_t GetMemoryUsed() const;
    
    /// Amount of memory the user wishes to allocate to the texture
    GARCH_API
    size_t GetMemoryRequested() const;
    
    /// Returns the graphics API texture object for the texture.
    GARCH_API
    virtual GarchTextureGPUHandle GetTextureName() const = 0;

    /// Specify the amount of memory the user wishes to allocate to the texture
    GARCH_API
    void SetMemoryRequested(size_t targetMemory);

    virtual VtDictionary GetTextureInfo() const = 0;

    GARCH_API
    virtual bool IsMinFilterSupported(GLenum filter);

    GARCH_API
    virtual bool IsMagFilterSupported(GLenum filter);

    /// static reporting function
    GARCH_API
    static size_t GetTextureMemoryAllocated();
    

    /// Returns an identifier that can be used to determine when the
    /// contents of this texture (i.e. its image data) has changed.
    ///
    /// The contents of most textures will be immutable for the lifetime
    /// of the texture. However, the contents of the texture attachments
    /// of a draw target change when the draw target is updated.
    GARCH_API
    size_t GetContentsID() const;

    GARCH_API
    GarchImage::ImageOriginLocation GetOriginLocation() const;

    GARCH_API
    bool IsOriginLowerLeft() const;

protected:
    GARCH_API
    GarchTexture();

    GARCH_API
    GarchTexture(GarchImage::ImageOriginLocation originLocation);

    GARCH_API
    void _SetMemoryUsed(size_t size);
    
    GARCH_API
    virtual void _OnSetMemoryRequested(size_t targetMemory);

    GARCH_API
    void _UpdateContentsID();
    
private:
    size_t _memoryUsed;
    size_t _memoryRequested;
    size_t _contentsID;
    GarchImage::ImageOriginLocation _originLocation;
};

class GarchTextureFactoryBase : public TfType::FactoryBase {
public:
    virtual GarchTextureRefPtr New(const TfToken& texturePath,
                                   GarchImage::ImageOriginLocation originLocation) const = 0;
    virtual GarchTextureRefPtr New(const TfTokenVector& texturePaths,
                                   GarchImage::ImageOriginLocation originLocation) const = 0;
};

template <class T>
class GarchTextureFactory : public GarchTextureFactoryBase {
public:
    virtual GarchTextureRefPtr New(const TfToken& texturePath,
                                   GarchImage::ImageOriginLocation originLocation =
                                                GarchImage::OriginUpperLeft) const
    {
        return T::New(texturePath);
    }

    virtual GarchTextureRefPtr New(const TfTokenVector& texturePaths,
                                   GarchImage::ImageOriginLocation originLocation =
                                                GarchImage::OriginUpperLeft) const
    {
        return TfNullPtr;
    }
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif  // GARCH_TEXTURE_H
