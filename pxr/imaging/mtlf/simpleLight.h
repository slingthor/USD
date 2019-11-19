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
#ifndef MTLF_SIMPLE_LIGHT_H
#define MTLF_SIMPLE_LIGHT_H

/// \file mtlf/simpleLight.h

#include "pxr/pxr.h"
#include "pxr/imaging/mtlf/api.h"
#include "pxr/base/gf/matrix4d.h"
#include "pxr/base/gf/vec3f.h"
#include "pxr/base/gf/vec4f.h"
#include "pxr/usd/sdf/path.h"

PXR_NAMESPACE_OPEN_SCOPE


class MtlfSimpleLight final {
public:
    MTLF_API
    MtlfSimpleLight(GfVec4f const & position = GfVec4f(0.0, 0.0, 0.0, 1.0));
    MTLF_API
    ~MtlfSimpleLight();

    MTLF_API
    GfMatrix4d const & GetTransform() const;
    MTLF_API
    void SetTransform(GfMatrix4d const & mat);

    MTLF_API
    GfVec4f const & GetAmbient() const;
    MTLF_API
    void SetAmbient(GfVec4f const & ambient);

    MTLF_API
    GfVec4f const & GetDiffuse() const;
    MTLF_API
    void SetDiffuse(GfVec4f const & diffuse);

    MTLF_API
    GfVec4f const & GetSpecular() const;
    MTLF_API
    void SetSpecular(GfVec4f const & specular);

    MTLF_API
    GfVec4f const & GetPosition() const;
    MTLF_API
    void SetPosition(GfVec4f const & position);

    MTLF_API
    GfVec3f const & GetSpotDirection() const;
    MTLF_API
    void SetSpotDirection(GfVec3f const & spotDirection);

    MTLF_API
    float const & GetSpotCutoff() const;
    MTLF_API
    void SetSpotCutoff(float const & spotCutoff);

    MTLF_API
    float const & GetSpotFalloff() const;
    MTLF_API
    void SetSpotFalloff(float const & spotFalloff);

    MTLF_API
    GfVec3f const & GetAttenuation() const;
    MTLF_API
    void SetAttenuation(GfVec3f const & attenuation);

    MTLF_API
    GfMatrix4d const & GetShadowMatrix() const;
    MTLF_API
    void SetShadowMatrix(GfMatrix4d const &matrix);

    MTLF_API
    int GetShadowResolution() const;
    MTLF_API
    void SetShadowResolution(int resolution);

    MTLF_API
    float GetShadowBias() const;
    MTLF_API
    void SetShadowBias(float bias);

    MTLF_API
    float GetShadowBlur() const;
    MTLF_API
    void SetShadowBlur(float blur);

    MTLF_API
    int GetShadowIndex() const;
    MTLF_API
    void SetShadowIndex(int shadowIndex);

    MTLF_API
    bool HasShadow() const;
    MTLF_API
    void SetHasShadow(bool hasShadow);

    MTLF_API
    bool IsCameraSpaceLight() const;
    MTLF_API
    void SetIsCameraSpaceLight(bool isCameraSpaceLight);

    MTLF_API
    SdfPath const & GetID() const;
    MTLF_API
    void SetID(SdfPath const & id);

    MTLF_API
    bool operator ==(MtlfSimpleLight const & other) const;
    MTLF_API
    bool operator !=(MtlfSimpleLight const & other) const;

private:
    MTLF_API
    friend std::ostream & operator <<(std::ostream &out,
                                      const MtlfSimpleLight& v);
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
MTLF_API
std::ostream& operator<<(std::ostream& out, const MtlfSimpleLight& v);

typedef std::vector<class MtlfSimpleLight> MtlfSimpleLightVector;

// VtValue requirements
MTLF_API
std::ostream& operator<<(std::ostream& out, const MtlfSimpleLightVector& pv);


PXR_NAMESPACE_CLOSE_SCOPE

#endif  // MTLF_SIMPLE_LIGHT_H
