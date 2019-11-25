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
/// \file bindingMap.cpp

#include "pxr/imaging/mtlf/mtlDevice.h"

#include "pxr/imaging/mtlf/bindingMap.h"
#include "pxr/base/tf/stl.h"
#include "pxr/base/tf/type.h"

PXR_NAMESPACE_OPEN_SCOPE


int
MtlfBindingMap::GetSamplerUnit(std::string const & name)
{
    return GetSamplerUnit(TfToken(name));
}

int
MtlfBindingMap::GetSamplerUnit(TfToken const & name)
{
    //METAL TODO: I'd really like to make this function assert if it's going to return a non-linked index.
    //But of course this "Get" function is being used for its side-effects.... so I can't.
    MTLFBindingIndex samplerUnit;
    if (!TfMapLookup(_samplerBindings, name, &samplerUnit)) {
        // XXX error check < MAX_TEXTURE_IMAGE_UNITS
        samplerUnit.index = _samplerBindings.size();
        _samplerBindings[name] = samplerUnit.asInt;
    }
    return samplerUnit.asInt;
}

int
MtlfBindingMap::GetAttributeIndex(std::string const & name)
{
    return GetAttributeIndex(TfToken(name));
}

int
MtlfBindingMap::GetAttributeIndex(TfToken const & name)
{
    int attribIndex = -1;
    if (!TfMapLookup(_attribBindings, name, &attribIndex)) {
        return -1;
    }
    return attribIndex;
}

void
MtlfBindingMap::AssignSamplerUnitsToProgram(GarchProgramGPUHandle program)
{
    TF_FATAL_CODING_ERROR("Not Implemented");
}

int
MtlfBindingMap::GetUniformBinding(std::string const & name)
{
    return GetUniformBinding(TfToken(name));
}

int
MtlfBindingMap::GetUniformBinding(TfToken const & name)
{
    MTLFBindingIndex binding;
    if (!TfMapLookup(_uniformBindings, name, &binding)) {
        binding.index = (int)_uniformBindings.size();
        _uniformBindings[name] = binding.asInt;
    }
    return binding.asInt;
}

bool
MtlfBindingMap::HasUniformBinding(std::string const & name) const
{
    return HasUniformBinding(TfToken(name));
}

bool
MtlfBindingMap::HasUniformBinding(TfToken const & name) const
{
    return (_uniformBindings.find(name) != _uniformBindings.end());
}

void
MtlfBindingMap::AssignUniformBindingsToProgram(GarchProgramGPUHandle program)
{
    TF_FATAL_CODING_ERROR("Not Implemented");
}

void
MtlfBindingMap::AddCustomBindings(GarchProgramGPUHandle program)
{
    TF_FATAL_CODING_ERROR("Not Implemented");
}

void
MtlfBindingMap::Debug() const
{
    printf("MtlfBindingMap\n");

    // sort for comparing baseline in testMtlfBindingMap
    std::map<TfToken, int> attribBindings, samplerBindings, uniformBindings;
    for (BindingMap::value_type const& p : _attribBindings ) {
        attribBindings.insert(p);
    }
    for (BindingMap::value_type const& p : _samplerBindings ) {
        samplerBindings.insert(p);
    }
    for (BindingMap::value_type const& p : _uniformBindings ) {
        uniformBindings.insert(p);
    }

    printf(" Attribute bindings\n");
    for (BindingMap::value_type const& p : attribBindings ) {
        printf("  %s : %d\n", p.first.GetText(), p.second);
    }
    printf(" Sampler bindings\n");
    for (BindingMap::value_type const& p : samplerBindings) {
        printf("  %s : %d\n", p.first.GetText(), p.second);
    }
    printf(" Uniform bindings\n");
    for (BindingMap::value_type const& p : uniformBindings) {
        printf("  %s : %d\n", p.first.GetText(), p.second);
    }
}


PXR_NAMESPACE_CLOSE_SCOPE

