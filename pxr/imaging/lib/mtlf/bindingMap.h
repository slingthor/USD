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
#ifndef MTLF_BINDING_MAP_H
#define MTLF_BINDING_MAP_H

/// \file glf/bindingMap.h

#include "pxr/pxr.h"
#include "pxr/imaging/mtlf/api.h"
#include "pxr/imaging/garch/gl.h"
#include "pxr/imaging/garch/bindingMap.h"
#include "pxr/base/tf/refBase.h"
#include "pxr/base/tf/stringUtils.h"
#include "pxr/base/tf/token.h"
#include "pxr/base/tf/weakBase.h"

#include "pxr/base/tf/hashmap.h"

PXR_NAMESPACE_OPEN_SCOPE


class MtlfBindingMap : public GarchBindingMap {
public:
    typedef TfHashMap<TfToken, int, TfToken::HashFunctor> BindingMap;

    MTLF_API
    int GetSamplerUnit(std::string const &name);
    MTLF_API
    int GetSamplerUnit(TfToken const & name);

    // If GetAttributeIndex is called with an unknown
    // attribute token they return -1
    MTLF_API
    int GetAttributeIndex(std::string const & name);
    MTLF_API
    int GetAttributeIndex(TfToken const & name);

    MTLF_API
    int GetUniformBinding(std::string const & name);
    MTLF_API
    int GetUniformBinding(TfToken const & name);

    MTLF_API
    bool HasUniformBinding(std::string const & name) const;
    MTLF_API
    bool HasUniformBinding(TfToken const & name) const;

    int GetNumSamplerBindings() const {
        return (int)_samplerBindings.size();
    }

    void ClearAttribBindings() {
        _attribBindings.clear();
    }

    void AddAttribBinding(TfToken const &name, int location) {
        _attribBindings[name] = location;
    }

    BindingMap const &GetAttributeBindings() const {
        return _attribBindings;
    }

    MTLF_API
    void AssignSamplerUnitsToProgram(GLuint program);

    MTLF_API
    void AssignUniformBindingsToProgram(GLuint program);

    MTLF_API
    void AddCustomBindings(GLuint program);

    MTLF_API
    void Debug() const;
    
protected:
    MtlfBindingMap() {}
    
    friend class MtlfResourceFactory;

private:
    void _AddActiveAttributeBindings(GLuint program);
    void _AddActiveUniformBindings(GLuint program);
    void _AddActiveUniformBlockBindings(GLuint program);

    BindingMap _attribBindings;
    BindingMap _samplerBindings;
    BindingMap _uniformBindings;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif  // MTLF_BINDING_MAP_H
