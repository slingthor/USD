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
#ifndef GARCH_SIMPLE_MATERIAL_H
#define GARCH_SIMPLE_MATERIAL_H

/// \file garch/simpleMaterial.h

#include "pxr/pxr.h"
#include "pxr/imaging/garch/api.h"
#include "pxr/base/gf/vec4f.h"

PXR_NAMESPACE_OPEN_SCOPE


class GarchSimpleMaterial final {
public:
    GARCH_API
    GarchSimpleMaterial();
    GARCH_API
    ~GarchSimpleMaterial();

    GARCH_API
    GfVec4f const & GetAmbient() const;
    GARCH_API
    void SetAmbient(GfVec4f const & ambient);

    GARCH_API
    GfVec4f const & GetDiffuse() const;
    GARCH_API
    void SetDiffuse(GfVec4f const & diffuse);

    GARCH_API
    GfVec4f const & GetSpecular() const;
    GARCH_API
    void SetSpecular(GfVec4f const & specular);

    GARCH_API
    GfVec4f const & GetEmission() const;
    GARCH_API
    void SetEmission(GfVec4f const & specular);

    GARCH_API
    double GetShininess() const;
    GARCH_API
    void SetShininess(double specular);

    GARCH_API
    bool operator ==(GarchSimpleMaterial const & other) const;
    GARCH_API
    bool operator !=(GarchSimpleMaterial const & other) const;

private:
    GfVec4f _ambient;
    GfVec4f _diffuse;
    GfVec4f _specular;
    GfVec4f _emission;
    double _shininess;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif // GARCH_SIMPLE_MATERIAL_H
