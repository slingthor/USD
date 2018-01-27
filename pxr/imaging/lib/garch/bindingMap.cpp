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

#include "pxr/imaging/garch/bindingMap.h"

#include "pxr/base/tf/stl.h"
#include "pxr/base/tf/type.h"

PXR_NAMESPACE_OPEN_SCOPE

GarchBindingMapRefPtr GarchBindingMap::New()
{
    TF_CODING_ERROR("Not Implemented");
    return TfNullPtr;//TfCreateRefPtr(new GarchBindingMap());
}

int
GarchBindingMap::GetSamplerUnit(std::string const & name)
{
    return GetSamplerUnit(TfToken(name));
}

int
GarchBindingMap::GetSamplerUnit(TfToken const & name)
{
    int samplerUnit = -1;
    if (!TfMapLookup(_samplerBindings, name, &samplerUnit)) {
        // XXX error check < MAX_TEXTURE_IMAGE_UNITS
        samplerUnit = _samplerBindings.size();
        _samplerBindings[name] = samplerUnit;
    }
    TF_VERIFY(samplerUnit >= 0);
    return samplerUnit;
}

int
GarchBindingMap::GetAttributeIndex(std::string const & name)
{
    return GetAttributeIndex(TfToken(name));
}

int
GarchBindingMap::GetAttributeIndex(TfToken const & name)
{
    int attribIndex = -1;
    if (!TfMapLookup(_attribBindings, name, &attribIndex)) {
        return -1;
    }
    return attribIndex;
}

int
GarchBindingMap::GetUniformBinding(std::string const & name)
{
    return GetUniformBinding(TfToken(name));
}

int
GarchBindingMap::GetUniformBinding(TfToken const & name)
{
    int binding = -1;
    if (!TfMapLookup(_uniformBindings, name, &binding)) {
        binding = (int)_uniformBindings.size();
        _uniformBindings[name] = binding;
    }
    TF_VERIFY(binding >= 0);
    return binding;
}

bool
GarchBindingMap::HasUniformBinding(std::string const & name) const
{
    return HasUniformBinding(TfToken(name));
}

bool
GarchBindingMap::HasUniformBinding(TfToken const & name) const
{
    return (_uniformBindings.find(name) != _uniformBindings.end());
}

PXR_NAMESPACE_CLOSE_SCOPE

