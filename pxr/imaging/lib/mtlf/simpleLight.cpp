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

#include "pxr/imaging/mtlf/simpleLight.h"

#include "pxr/imaging/garch/gl.h"

PXR_NAMESPACE_OPEN_SCOPE


MtlfSimpleLight::MtlfSimpleLight(GfVec4f const & position) :
    _ambient(0.2, 0.2, 0.2, 1.0),
    _diffuse(1.0, 1.0, 1.0, 1.0),
    _specular(1.0, 1.0, 1.0, 1.0),
    _position(position[0], position[1], position[2], 1.0),
    _spotDirection(0.0, 0.0, -1.0),
    _spotCutoff(180.0),
    _spotFalloff(0.0),
    _attenuation(1.0, 0.0, 0.0),
    _isCameraSpaceLight(false),
    _hasShadow(false),
    _shadowResolution(512),
    _shadowBias(0.0),
    _shadowBlur(0.0),
    _shadowIndex(0),
    _transform(GfMatrix4d().SetIdentity()),
    _shadowMatrix(GfMatrix4d().SetIdentity()),
    _id()
{
}

MtlfSimpleLight::~MtlfSimpleLight()
{
}

GfMatrix4d const &
MtlfSimpleLight::GetTransform() const
{
    return _transform;
}

void
MtlfSimpleLight::SetTransform(GfMatrix4d const & mat)
{
    _transform = mat;
}

GfVec4f const &
MtlfSimpleLight::GetAmbient() const
{
    return _ambient;
}

void
MtlfSimpleLight::SetAmbient(GfVec4f const & ambient)
{
    _ambient = ambient;
}


GfVec4f const &
MtlfSimpleLight::GetDiffuse() const
{
    return _diffuse;
}

void
MtlfSimpleLight::SetDiffuse(GfVec4f const & diffuse)
{
    _diffuse = diffuse;
}


GfVec4f const &
MtlfSimpleLight::GetSpecular() const
{
    return _specular;
}

void
MtlfSimpleLight::SetSpecular(GfVec4f const & specular)
{
    _specular = specular;
}

GfVec4f const &
MtlfSimpleLight::GetPosition() const
{
    return _position;
}

void
MtlfSimpleLight::SetPosition(GfVec4f const & position)
{
    _position = position;
}

GfVec3f const &
MtlfSimpleLight::GetSpotDirection() const
{
    return _spotDirection;
}

void
MtlfSimpleLight::SetSpotDirection(GfVec3f const & spotDirection)
{
    _spotDirection = spotDirection;
}

float const &
MtlfSimpleLight::GetSpotCutoff() const
{
    return _spotCutoff;
}

void
MtlfSimpleLight::SetSpotCutoff(float const & spotCutoff)
{
    _spotCutoff = spotCutoff;
}

float const &
MtlfSimpleLight::GetSpotFalloff() const
{
    return _spotFalloff;
}

void
MtlfSimpleLight::SetSpotFalloff(float const & spotFalloff)
{
    _spotFalloff = spotFalloff;
}

GfVec3f const &
MtlfSimpleLight::GetAttenuation() const
{
    return _attenuation;
}

void
MtlfSimpleLight::SetAttenuation(GfVec3f const & attenuation)
{
    _attenuation = attenuation;
}

bool
MtlfSimpleLight::HasShadow() const
{
    return _hasShadow;
}

void
MtlfSimpleLight::SetHasShadow(bool hasShadow)
{
    _hasShadow = hasShadow;
}

int
MtlfSimpleLight::GetShadowResolution() const
{
    return _shadowResolution;
}

void
MtlfSimpleLight::SetShadowResolution(int resolution)
{
    _shadowResolution = resolution;
}

float
MtlfSimpleLight::GetShadowBias() const
{
    return _shadowBias;
}

void
MtlfSimpleLight::SetShadowBias(float bias)
{
    _shadowBias = bias;
}

float
MtlfSimpleLight::GetShadowBlur() const
{
    return _shadowBlur;
}

void
MtlfSimpleLight::SetShadowBlur(float blur)
{
    _shadowBlur = blur;
}

int
MtlfSimpleLight::GetShadowIndex() const
{
    return _shadowIndex;
}

void
MtlfSimpleLight::SetShadowIndex(int index)
{
    _shadowIndex = index;
}

GfMatrix4d const &
MtlfSimpleLight::GetShadowMatrix() const
{
    return _shadowMatrix;
}

void
MtlfSimpleLight::SetShadowMatrix(GfMatrix4d const & matrix)
{
    _shadowMatrix = matrix;
}

bool
MtlfSimpleLight::IsCameraSpaceLight() const
{
    return _isCameraSpaceLight;
}

void
MtlfSimpleLight::SetIsCameraSpaceLight(bool isCameraSpaceLight)
{
    _isCameraSpaceLight = isCameraSpaceLight;
}

SdfPath const &
MtlfSimpleLight::GetID() const
{
    return _id;
}

void MtlfSimpleLight::SetID(SdfPath const & id)
{
    _id = id;
}

// -------------------------------------------------------------------------- //
// VtValue requirements
// -------------------------------------------------------------------------- //

bool
MtlfSimpleLight::operator==(const MtlfSimpleLight& other) const
{
    return  _ambient == other._ambient
        &&  _diffuse == other._diffuse
        &&  _specular == other._specular
        &&  _position == other._position
        &&  _spotDirection == other._spotDirection
        &&  _spotCutoff == other._spotCutoff
        &&  _spotFalloff == other._spotFalloff
        &&  _attenuation == other._attenuation
        &&  _hasShadow == other._hasShadow
        &&  _shadowResolution == other._shadowResolution
        &&  _shadowBias == other._shadowBias
        &&  _shadowBlur == other._shadowBlur
        &&  _shadowIndex == other._shadowIndex
        &&  _transform == other._transform
        &&  _shadowMatrix == other._shadowMatrix
        &&  _isCameraSpaceLight == other._isCameraSpaceLight
        &&  _id == other._id;
}

bool
MtlfSimpleLight::operator!=(const MtlfSimpleLight& other) const
{
    return !(*this == other);
}

std::ostream& operator<<(std::ostream& out, const MtlfSimpleLight& v)
{
    out << v._ambient
        << v._diffuse
        << v._specular
        << v._position
        << v._spotDirection
        << v._spotCutoff
        << v._spotFalloff
        << v._attenuation
        << v._hasShadow
        << v._shadowResolution
        << v._shadowBias
        << v._shadowBlur
        << v._shadowIndex
        << v._transform
        << v._shadowMatrix
        << v._isCameraSpaceLight
        << v._id;
    return out;
}

std::ostream&
operator<<(std::ostream& out, const MtlfSimpleLightVector& pv)
{
    return out;
}

PXR_NAMESPACE_CLOSE_SCOPE

