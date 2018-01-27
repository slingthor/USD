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
#ifndef GARCH_SIMPLE_LIGHT_H
#define GARCH_SIMPLE_LIGHT_H

/// \file garch/simpleLight.h

#include "pxr/pxr.h"
#include "pxr/imaging/garch/api.h"
#include "pxr/base/gf/matrix4d.h"
#include "pxr/base/gf/vec3f.h"
#include "pxr/base/gf/vec4f.h"
#include "pxr/usd/sdf/path.h"

PXR_NAMESPACE_OPEN_SCOPE


class GarchSimpleLight {
public:
    GARCH_API
    GarchSimpleLight(GfVec4f const & position = GfVec4f(0.0, 0.0, 0.0, 1.0));
    GARCH_API
    virtual ~GarchSimpleLight();

    GARCH_API
    virtual GfMatrix4d const & GetTransform() const;
    GARCH_API
    virtual void SetTransform(GfMatrix4d const & mat);

    GARCH_API
    virtual GfVec4f const & GetAmbient() const;
    GARCH_API
    virtual void SetAmbient(GfVec4f const & ambient);

    GARCH_API
    virtual GfVec4f const & GetDiffuse() const;
    GARCH_API
    virtual void SetDiffuse(GfVec4f const & diffuse);

    GARCH_API
    virtual GfVec4f const & GetSpecular() const;
    GARCH_API
    virtual void SetSpecular(GfVec4f const & specular);

    GARCH_API
    virtual GfVec4f const & GetPosition() const;
    GARCH_API
    virtual void SetPosition(GfVec4f const & position);

    GARCH_API
    virtual GfVec3f const & GetSpotDirection() const;
    GARCH_API
    virtual void SetSpotDirection(GfVec3f const & spotDirection);

    GARCH_API
    virtual float const & GetSpotCutoff() const;
    GARCH_API
    virtual void SetSpotCutoff(float const & spotCutoff);

    GARCH_API
    virtual float const & GetSpotFalloff() const;
    GARCH_API
    virtual void SetSpotFalloff(float const & spotFalloff);

    GARCH_API
    virtual GfVec3f const & GetAttenuation() const;
    GARCH_API
    virtual void SetAttenuation(GfVec3f const & attenuation);

    GARCH_API
    virtual GfMatrix4d const & GetShadowMatrix() const;
    GARCH_API
    virtual void SetShadowMatrix(GfMatrix4d const &matrix);

    GARCH_API
    virtual int GetShadowResolution() const;
    GARCH_API
    virtual void SetShadowResolution(int resolution);

    GARCH_API
    virtual float GetShadowBias() const;
    GARCH_API
    virtual void SetShadowBias(float bias);

    GARCH_API
    virtual float GetShadowBlur() const;
    GARCH_API
    virtual void SetShadowBlur(float blur);

    GARCH_API
    virtual int GetShadowIndex() const;
    GARCH_API
    virtual void SetShadowIndex(int shadowIndex);

    GARCH_API
    virtual bool HasShadow() const;
    GARCH_API
    virtual void SetHasShadow(bool hasShadow);

    GARCH_API
    virtual bool IsCameraSpaceLight() const;
    GARCH_API
    virtual void SetIsCameraSpaceLight(bool isCameraSpaceLight);

    GARCH_API
    virtual SdfPath const & GetID() const;
    GARCH_API
    virtual void SetID(SdfPath const & id);

    GARCH_API
    virtual bool operator ==(GarchSimpleLight const & other) const;
    GARCH_API
    virtual bool operator !=(GarchSimpleLight const & other) const;

private:
    GARCH_API
    friend std::ostream & operator <<(std::ostream &out,
                                      const GarchSimpleLight& v);
    GfVec4f _ambient;
    GfVec4f _diffuse;
    GfVec4f _specular;
    GfVec4f _position;
    GfVec3f _spotDirection;
    float _spotCutoff;
    float _spotFalloff;
    GfVec3f _attenuation;
    bool _isCameraSpaceLight;

    bool _hasShadow;
    int _shadowResolution;
    float _shadowBias;
    float _shadowBlur;
    int _shadowIndex;

    GfMatrix4d _transform;
    GfMatrix4d _shadowMatrix;

    SdfPath _id;
};

// VtValue requirements
GARCH_API
std::ostream& operator<<(std::ostream& out, const GarchSimpleLight& v);

typedef std::vector<class GarchSimpleLight> GarchSimpleLightVector;

// VtValue requirements
GARCH_API
std::ostream& operator<<(std::ostream& out, const GarchSimpleLightVector& pv);


PXR_NAMESPACE_CLOSE_SCOPE

#endif
