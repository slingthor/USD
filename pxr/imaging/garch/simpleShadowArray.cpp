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
#include "pxr/base/tf/debug.h"
#include "pxr/base/tf/envSetting.h"

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_ENV_SETTING(GARCH_ENABLE_BINDLESS_SHADOW_TEXTURES, false,
                      "Enable use of bindless shadow maps");

GarchSimpleShadowArray* GarchSimpleShadowArray::New()
{
    return GarchResourceFactory::GetInstance()->NewSimpleShadowArray();
}

GarchSimpleShadowArray::GarchSimpleShadowArray() :
    _size(0),
    _numLayers(0),
    // common state
    _framebuffer(),
    _shadowCompareSampler()
{
}

GarchSimpleShadowArray::~GarchSimpleShadowArray()
{
}

/*static*/
bool
GarchSimpleShadowArray::GetBindlessShadowMapsEnabled()
{
    // Note: We do not test the GL context caps for the availability of the
    // bindless texture and int64 extensions.
    static bool usingBindlessShadowMaps =
        TfGetEnvSetting(GARCH_ENABLE_BINDLESS_SHADOW_TEXTURES);

    return usingBindlessShadowMaps;
}

// --------- (public) Bindful API ----------
void
GarchSimpleShadowArray::SetSize(GfVec2i const & size)
{
    if (GetBindlessShadowMapsEnabled()) {
        TF_CODING_ERROR("Using bindful API %s when bindless "
            "shadow maps are enabled\n", TF_FUNC_NAME().c_str());
        return;
    }
    if (_size != size) {
        _FreeBindfulTextures();
        _size = size;
    }
}

void
GarchSimpleShadowArray::SetNumLayers(size_t numLayers)
{
    if (GetBindlessShadowMapsEnabled()) {
        TF_CODING_ERROR("Using bindful API %s when bindless "
            "shadow maps are enabled\n", TF_FUNC_NAME().c_str());
        return;
    }

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
    if (GetBindlessShadowMapsEnabled()) {
        TF_CODING_ERROR("Using bindful API in %s when bindless "
            "shadow maps are enabled\n",  TF_FUNC_NAME().c_str());
        return GarchTextureGPUHandle();
    }
    return _bindfulTexture;
}

GarchSamplerGPUHandle
GarchSimpleShadowArray::GetShadowMapDepthSampler() const
{
    if (GetBindlessShadowMapsEnabled()) {
        TF_CODING_ERROR("Using bindful API in %s when bindless "
            "shadow maps are enabled\n",  TF_FUNC_NAME().c_str());
        return GarchSamplerGPUHandle();
    }
    return _shadowDepthSampler;
}

GarchSamplerGPUHandle
GarchSimpleShadowArray::GetShadowMapCompareSampler() const
{
    if (GetBindlessShadowMapsEnabled()) {
        TF_CODING_ERROR("Using bindful API in %s when bindless "
            "shadow maps are enabled\n",  TF_FUNC_NAME().c_str());
        return GarchSamplerGPUHandle();
    }
    return _shadowCompareSampler;
}

// --------- (public) Bindless API ----------
void
GarchSimpleShadowArray::SetShadowMapResolutions(
    std::vector<GfVec2i> const& resolutions)
{
    if (_resolutions == resolutions) {
        return;
    }

    _resolutions = resolutions;

    _FreeBindlessTextures();

    size_t numShadowMaps = _resolutions.size();
    if (_viewMatrix.size() != numShadowMaps ||
        _projectionMatrix.size() != numShadowMaps) {
        _viewMatrix.resize(numShadowMaps, GfMatrix4d().SetIdentity());
        _projectionMatrix.resize(numShadowMaps, GfMatrix4d().SetIdentity());
    }

}

std::vector<uint64_t> const&
GarchSimpleShadowArray::GetBindlessShadowMapHandles() const
{
    return _bindlessTextureHandles;
}

// --------- (public) Common API ----------
size_t
GarchSimpleShadowArray::GetNumShadowMapPasses() const
{
    // In both the bindful and bindless cases, we require one pass per shadow
    // map.
    if (GetBindlessShadowMapsEnabled()) {
        return _resolutions.size();
    } else {
        return _numLayers;
    }
}

GfVec2i
GarchSimpleShadowArray::GetShadowMapSize(size_t index) const
{
    GfVec2i shadowMapSize(0);
    if (GetBindlessShadowMapsEnabled()) {
        if (TF_VERIFY(index < _resolutions.size())) {
            shadowMapSize = _resolutions[index];
        }
    } else {
        // In the bindful case, all shadow map textures use the same size.
        shadowMapSize = _size;
    }

    return shadowMapSize;
}

// --------- private helpers ----------
bool
GarchSimpleShadowArray::_ShadowMapExists() const
{
    return GetBindlessShadowMapsEnabled() ? !_bindlessTextures.empty() :
                                             _bindfulTexture.IsSet();
}

PXR_NAMESPACE_CLOSE_SCOPE

