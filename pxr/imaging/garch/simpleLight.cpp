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
/// \file simpleLight.cpp

#include "pxr/imaging/garch/simpleLight.h"

PXR_NAMESPACE_OPEN_SCOPE


GarchSimpleLight::GarchSimpleLight(GfVec4f const & position) :
    _ambient(0.2, 0.2, 0.2, 1.0),
    _diffuse(1.0, 1.0, 1.0, 1.0),
    _specular(1.0, 1.0, 1.0, 1.0),
    _position(position[0], position[1], position[2], 1.0),
    _spotDirection(0.0, 0.0, -1.0),
    _spotCutoff(180.0),
    _spotFalloff(0.0),
    _attenuation(1.0, 0.0, 0.0),
    _isCameraSpaceLight(false),
    _hasIntensity(true),
    _hasShadow(false),
    _shadowResolution(512),
    _shadowBias(0.0),
    _shadowBlur(0.0),
    _shadowIndexStart(0),
    _shadowIndexEnd(0),
    _transform(GfMatrix4d().SetIdentity()),
    _shadowMatrices(std::vector<GfMatrix4d>(1, GfMatrix4d().SetIdentity())),
    _isDomeLight(false),
    _id()
{
}

GarchSimpleLight::~GarchSimpleLight() = default;

GfMatrix4d const &
GarchSimpleLight::GetTransform() const
{
    return _transform;
}

void
GarchSimpleLight::SetTransform(GfMatrix4d const & mat)
{
    _transform = mat;
}

GfVec4f const &
GarchSimpleLight::GetAmbient() const
{
    return _ambient;
}

void
GarchSimpleLight::SetAmbient(GfVec4f const & ambient)
{
    _ambient = ambient;
}


GfVec4f const &
GarchSimpleLight::GetDiffuse() const
{
    return _diffuse;
}

void
GarchSimpleLight::SetDiffuse(GfVec4f const & diffuse)
{
    _diffuse = diffuse;
}


GfVec4f const &
GarchSimpleLight::GetSpecular() const
{
    return _specular;
}

void
GarchSimpleLight::SetSpecular(GfVec4f const & specular)
{
    _specular = specular;
}

GfVec4f const &
GarchSimpleLight::GetPosition() const
{
    return _position;
}

void
GarchSimpleLight::SetPosition(GfVec4f const & position)
{
    _position = position;
}

GfVec3f const &
GarchSimpleLight::GetSpotDirection() const
{
    return _spotDirection;
}

void
GarchSimpleLight::SetSpotDirection(GfVec3f const & spotDirection)
{
    _spotDirection = spotDirection;
}

float const &
GarchSimpleLight::GetSpotCutoff() const
{
    return _spotCutoff;
}

void
GarchSimpleLight::SetSpotCutoff(float const & spotCutoff)
{
    _spotCutoff = spotCutoff;
}

float const &
GarchSimpleLight::GetSpotFalloff() const
{
    return _spotFalloff;
}

void
GarchSimpleLight::SetSpotFalloff(float const & spotFalloff)
{
    _spotFalloff = spotFalloff;
}

GfVec3f const &
GarchSimpleLight::GetAttenuation() const
{
    return _attenuation;
}

void
GarchSimpleLight::SetAttenuation(GfVec3f const & attenuation)
{
    _attenuation = attenuation;
}

void
GarchSimpleLight::SetHasIntensity(bool hasIntensity)
{
    _hasIntensity = hasIntensity;
}

bool
GarchSimpleLight::HasIntensity() const
{
    return _hasIntensity;
}

bool
GarchSimpleLight::HasShadow() const
{
    return _hasShadow;
}

void
GarchSimpleLight::SetHasShadow(bool hasShadow)
{
    _hasShadow = hasShadow;
}

int
GarchSimpleLight::GetShadowResolution() const
{
    return _shadowResolution;
}

void
GarchSimpleLight::SetShadowResolution(int resolution)
{
    _shadowResolution = resolution;
}

float
GarchSimpleLight::GetShadowBias() const
{
    return _shadowBias;
}

void
GarchSimpleLight::SetShadowBias(float bias)
{
    _shadowBias = bias;
}

float
GarchSimpleLight::GetShadowBlur() const
{
    return _shadowBlur;
}

void
GarchSimpleLight::SetShadowBlur(float blur)
{
    _shadowBlur = blur;
}

int
GarchSimpleLight::GetShadowIndexStart() const
{
    return _shadowIndexStart;
}

void
GarchSimpleLight::SetShadowIndexStart(int shadowStart)
{
    _shadowIndexStart = shadowStart;
}

int
GarchSimpleLight::GetShadowIndexEnd() const
{
    return _shadowIndexEnd;
}

void
GarchSimpleLight::SetShadowIndexEnd(int shadowEnd)
{
    _shadowIndexEnd = shadowEnd;
}

std::vector<GfMatrix4d> const &
GarchSimpleLight::GetShadowMatrices() const
{
    return _shadowMatrices;
}

void
GarchSimpleLight::SetShadowMatrices(std::vector<GfMatrix4d> const & matrices)
{
    _shadowMatrices = matrices;
}

bool
GarchSimpleLight::IsCameraSpaceLight() const
{
    return _isCameraSpaceLight;
}

void
GarchSimpleLight::SetIsCameraSpaceLight(bool isCameraSpaceLight)
{
    _isCameraSpaceLight = isCameraSpaceLight;
}

SdfPath const &
GarchSimpleLight::GetID() const
{
    return _id;
}

void GarchSimpleLight::SetID(SdfPath const & id)
{
    _id = id;
}

bool
GarchSimpleLight::IsDomeLight() const
{
    return _isDomeLight;
}

void
GarchSimpleLight::SetIsDomeLight(bool isDomeLight)
{
    _isDomeLight = isDomeLight;
}

const SdfAssetPath &
GarchSimpleLight::GetDomeLightTextureFile() const
{
    return _domeLightTextureFile;
}

void
GarchSimpleLight::SetDomeLightTextureFile(const SdfAssetPath &path)
{
    _domeLightTextureFile = path;
}

// -------------------------------------------------------------------------- //
// VtValue requirements
// -------------------------------------------------------------------------- //

bool
GarchSimpleLight::operator==(const GarchSimpleLight& other) const
{
    return  _ambient == other._ambient
        &&  _diffuse == other._diffuse
        &&  _specular == other._specular
        &&  _position == other._position
        &&  _spotDirection == other._spotDirection
        &&  _spotCutoff == other._spotCutoff
        &&  _spotFalloff == other._spotFalloff
        &&  _attenuation == other._attenuation
        &&  _hasIntensity == other._hasIntensity
        &&  _hasShadow == other._hasShadow
        &&  _shadowResolution == other._shadowResolution
        &&  _shadowBias == other._shadowBias
        &&  _shadowBlur == other._shadowBlur
        &&  _shadowIndexStart == other._shadowIndexStart
        &&  _shadowIndexEnd == other._shadowIndexEnd
        &&  _transform == other._transform
        &&  _shadowMatrices == other._shadowMatrices
        &&  _isCameraSpaceLight == other._isCameraSpaceLight
        &&  _isDomeLight == other._isDomeLight
        &&  _domeLightTextureFile == other._domeLightTextureFile
        &&  _id == other._id;
}

bool
GarchSimpleLight::operator!=(const GarchSimpleLight& other) const
{
    return !(*this == other);
}

std::ostream& operator<<(std::ostream& out, const GarchSimpleLight& v)
{
    out << v._ambient
        << v._diffuse
        << v._specular
        << v._position
        << v._spotDirection
        << v._spotCutoff
        << v._spotFalloff
        << v._attenuation
        << v._hasIntensity
        << v._hasShadow
        << v._shadowResolution
        << v._shadowBias
        << v._shadowBlur
        << v._shadowIndexStart
        << v._shadowIndexEnd
        << v._transform
        << v._isCameraSpaceLight
        << v._isDomeLight
        << v._domeLightTextureFile
        << v._id;
    for (auto const& m : v._shadowMatrices) {
        out << m;
    }
    return out;
}

std::ostream&
operator<<(std::ostream& out, const GarchSimpleLightVector& pv)
{
    return out;
}

PXR_NAMESPACE_CLOSE_SCOPE

