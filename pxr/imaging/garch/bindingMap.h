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
#ifndef GARCH_BINDING_MAP_H
#define GARCH_BINDING_MAP_H

/// \file garch/bindingMap.h

#include "pxr/pxr.h"
#include "pxr/imaging/garch/api.h"
#include "pxr/imaging/garch/gl.h"
#include "pxr/base/tf/declarePtrs.h"
#include "pxr/base/tf/hashmap.h"
#include "pxr/base/tf/refBase.h"
#include "pxr/base/tf/stringUtils.h"
#include "pxr/base/tf/token.h"
#include "pxr/base/tf/weakBase.h"

#if defined(PXR_METAL_SUPPORT_ENABLED)
#include <Metal/Metal.h>
#endif

PXR_NAMESPACE_OPEN_SCOPE

TF_DECLARE_WEAK_AND_REF_PTRS(GarchBindingMap);

// XXX Move this somewhere else
struct GarchProgramGPUHandle {
    GarchProgramGPUHandle() {
        handle = 0;
    }
    GarchProgramGPUHandle(GarchProgramGPUHandle const & _gpuHandle) {
        handle = _gpuHandle.handle;
    }
    
    void Clear() { handle = 0; }
    bool IsSet() const { return handle != 0; }
    
    // OpenGL
    GarchProgramGPUHandle(GLuint const _handle) {
        handle = _handle;
    }
    GarchProgramGPUHandle& operator =(GLuint const _handle) {
        handle = _handle;
        return *this;
    }
    operator GLuint() const { return (GLuint)handle; }

#if defined(PXR_METAL_SUPPORT_ENABLED)
    // Metal
    GarchProgramGPUHandle(id<MTLFunction> const _handle) {
        handle = (__bridge uint64_t)_handle;
    }
    GarchProgramGPUHandle& operator =(id<MTLFunction> const _handle) {
        handle = (__bridge uint64_t)_handle;
        return *this;
    }
    operator id<MTLFunction>() const { return (__bridge id<MTLFunction>)handle; }
#endif
    
    uint64_t handle;
};

class GarchBindingMap : public TfRefBase, public TfWeakBase {
public:
    typedef TfHashMap<TfToken, int, TfToken::HashFunctor> AttribBindingMap;
    typedef TfHashMap<TfToken, int, TfToken::HashFunctor> SamplerBindingMap;
    typedef TfHashMap<TfToken, int, TfToken::HashFunctor> UniformBindingMap;

    /// Returns a new instance.
    GARCH_API
    static GarchBindingMapRefPtr New();

    GARCH_API
    virtual int GetSamplerUnit(std::string const &name);
    GARCH_API
    virtual int GetSamplerUnit(TfToken const & name);

    // If GetAttributeIndex is called with an unknown
    // attribute token they return -1
    GARCH_API
    virtual int GetAttributeIndex(std::string const & name);
    GARCH_API
    virtual int GetAttributeIndex(TfToken const & name);

    GARCH_API
    virtual int GetUniformBinding(std::string const & name);
    GARCH_API
    virtual int GetUniformBinding(TfToken const & name);

    GARCH_API
    virtual bool HasUniformBinding(std::string const & name) const;
    GARCH_API
    virtual bool HasUniformBinding(TfToken const & name) const;

    virtual int GetNumSamplerBindings() const {
        return (int)_samplerBindings.size();
    }

    virtual void ClearAttribBindings() {
        _attribBindings.clear();
    }

    /// \name Sampler and UBO Bindings
    ///
    /// Sampler units and uniform block bindings are reset and will be
    /// assigned sequentially starting from the specified baseIndex.
    /// This allows other subsystems to claim sampler units and uniform
    /// block bindings before additional indices are assigned by this
    /// binding map.
    ///
    /// @{

    void ResetSamplerBindings(int baseIndex) {
        _samplerBindings.clear();
        _samplerBindingBaseIndex = baseIndex;
    }

    void ResetUniformBindings(int baseIndex) {
        _uniformBindings.clear();
        _uniformBindingBaseIndex = baseIndex;
    }

    /// @}

    virtual void AddAttribBinding(TfToken const &name, int location) {
        _attribBindings[name] = location;
    }

    virtual AttribBindingMap const &GetAttributeBindings() const {
        return _attribBindings;
    }

    GARCH_API
    virtual void AssignSamplerUnitsToProgram(GarchProgramGPUHandle program) = 0;

    GARCH_API
    virtual void AssignUniformBindingsToProgram(GarchProgramGPUHandle program) = 0;

    GARCH_API
    virtual void AddCustomBindings(GarchProgramGPUHandle program) = 0;

    GARCH_API
    virtual void Debug() const = 0;

protected:
    
    GarchBindingMap()
          : _samplerBindingBaseIndex(0)
          , _uniformBindingBaseIndex(0)
          { }

    AttribBindingMap _attribBindings;
    SamplerBindingMap _samplerBindings;
    UniformBindingMap _uniformBindings;

    int _samplerBindingBaseIndex;
    int _uniformBindingBaseIndex;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif  // GARCH_BINDING_MAP_H