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

PXR_NAMESPACE_OPEN_SCOPE

TF_DECLARE_WEAK_AND_REF_PTRS(GarchBindingMap);

class GarchBindingMap : public TfRefBase, public TfWeakBase {
public:
    typedef TfHashMap<TfToken, int, TfToken::HashFunctor> BindingMap;

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

    virtual void AddAttribBinding(TfToken const &name, int location) {
        _attribBindings[name] = location;
    }

    virtual BindingMap const &GetAttributeBindings() const {
        return _attribBindings;
    }

    GARCH_API
    virtual void AssignSamplerUnitsToProgram(GLuint program) = 0;

    GARCH_API
    virtual void AssignUniformBindingsToProgram(GLuint program) = 0;

    GARCH_API
    virtual void AddCustomBindings(GLuint program) = 0;

    GARCH_API
    virtual void Debug() const = 0;

private:

    BindingMap _attribBindings;
    BindingMap _samplerBindings;
    BindingMap _uniformBindings;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif  // GARCH_BINDING_MAP_H
