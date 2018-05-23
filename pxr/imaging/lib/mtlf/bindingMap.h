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

/// \file mtlf/bindingMap.h

#include "pxr/pxr.h"
#include "pxr/imaging/mtlf/api.h"
#include "pxr/imaging/garch/gl.h"
#include "pxr/imaging/garch/bindingMap.h"
#include "pxr/base/tf/stringUtils.h"
#include "pxr/base/tf/token.h"

#include "pxr/base/tf/hashmap.h"

PXR_NAMESPACE_OPEN_SCOPE

TF_DECLARE_WEAK_AND_REF_PTRS(MtlfBindingMap);

class MtlfBindingMap : public GarchBindingMap {
public:
    typedef TfHashMap<TfToken, int, TfToken::HashFunctor> BindingMap;

    MTLF_API
    virtual int GetSamplerUnit(std::string const &name) override;
    MTLF_API
    virtual int GetSamplerUnit(TfToken const & name) override;

    // If GetAttributeIndex is called with an unknown
    // attribute token they return -1
    MTLF_API
    virtual int GetAttributeIndex(std::string const & name) override;
    MTLF_API
    virtual int GetAttributeIndex(TfToken const & name) override;

    MTLF_API
    virtual int GetUniformBinding(std::string const & name) override;
    MTLF_API
    virtual int GetUniformBinding(TfToken const & name) override;

    MTLF_API
    virtual bool HasUniformBinding(std::string const & name) const override;
    MTLF_API
    virtual bool HasUniformBinding(TfToken const & name) const override;

    MTLF_API
    virtual void AssignSamplerUnitsToProgram(GarchProgramGPUHandle program) override;

    MTLF_API
    virtual void AssignUniformBindingsToProgram(GarchProgramGPUHandle program) override;

    MTLF_API
    virtual void AddCustomBindings(GarchProgramGPUHandle program) override;

    MTLF_API
    virtual void Debug() const override;
    
protected:
    MtlfBindingMap() {}
    
    friend class MtlfResourceFactory;

private:
    void _AddActiveAttributeBindings(GarchProgramGPUHandle program);
    void _AddActiveUniformBindings(GarchProgramGPUHandle program);
    void _AddActiveUniformBlockBindings(GarchProgramGPUHandle program);
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif  // MTLF_BINDING_MAP_H
