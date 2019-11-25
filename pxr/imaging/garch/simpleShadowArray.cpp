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
/// \file simpleShadowArray.cpp

#include "pxr/imaging/garch/simpleShadowArray.h"
#include "pxr/imaging/garch/resourceFactory.h"

#include "pxr/base/gf/vec2i.h"
#include "pxr/base/gf/vec4d.h"

PXR_NAMESPACE_OPEN_SCOPE

GarchSimpleShadowArray* GarchSimpleShadowArray::New(GfVec2i const & size, size_t numLayers)
{
    return GarchResourceFactory::GetInstance()->NewSimpleShadowArray(size, numLayers);
}

GarchSimpleShadowArray::GarchSimpleShadowArray(GfVec2i const & size,
                                           size_t numLayers) :
    _size(size),
    _numLayers(numLayers),
    _viewMatrix(_numLayers),
    _projectionMatrix(_numLayers),
    _shadowDepthSampler(),
    _shadowCompareSampler()
{
}

GarchSimpleShadowArray::~GarchSimpleShadowArray()
{
}

GfVec2i
GarchSimpleShadowArray::GetSize() const
{
    return _size;
}

void
GarchSimpleShadowArray::SetSize(GfVec2i const & size)
{
    if (_size != size) {
        _size = size;
    }
}

size_t
GarchSimpleShadowArray::GetNumLayers() const
{
    return _numLayers;
}

void
GarchSimpleShadowArray::SetNumLayers(size_t numLayers)
{
    if (_numLayers != numLayers) {
        _viewMatrix.resize(numLayers, GfMatrix4d().SetIdentity());
        _projectionMatrix.resize(numLayers, GfMatrix4d().SetIdentity());
        _numLayers = numLayers;
    }
}

GfMatrix4d
GarchSimpleShadowArray::GetViewMatrix(size_t index) const
{
    if (!TF_VERIFY(index < _viewMatrix.size())) {
        return GfMatrix4d(1.0);
    }

    return _viewMatrix[index];
}

void
GarchSimpleShadowArray::SetViewMatrix(size_t index, GfMatrix4d const & matrix)
{
    if (!TF_VERIFY(index < _viewMatrix.size())) {
        return;
    }

    _viewMatrix[index] = matrix;
}

GfMatrix4d
GarchSimpleShadowArray::GetProjectionMatrix(size_t index) const
{
    if (!TF_VERIFY(index < _projectionMatrix.size())) {
        return GfMatrix4d(1.0);
    }

    return _projectionMatrix[index];
}

void
GarchSimpleShadowArray::SetProjectionMatrix(size_t index, GfMatrix4d const & matrix)
{
    if (!TF_VERIFY(index < _projectionMatrix.size())) {
        return;
    }

    _projectionMatrix[index] = matrix;
}

GfMatrix4d
GarchSimpleShadowArray::GetWorldToShadowMatrix(size_t index) const
{
    GfMatrix4d size = GfMatrix4d().SetScale(GfVec3d(0.5, 0.5, 0.5));
    GfMatrix4d center = GfMatrix4d().SetTranslate(GfVec3d(0.5, 0.5, 0.5));
    return GetViewMatrix(index) * GetProjectionMatrix(index) * size * center;
}

GarchTextureGPUHandle
GarchSimpleShadowArray::GetShadowMapTexture() const
{
    return _texture;
}

GarchSamplerGPUHandle
GarchSimpleShadowArray::GetShadowMapDepthSampler() const
{
    return _shadowDepthSampler;
}

GarchSamplerGPUHandle
GarchSimpleShadowArray::GetShadowMapCompareSampler() const
{
    return _shadowCompareSampler;
}

PXR_NAMESPACE_CLOSE_SCOPE

