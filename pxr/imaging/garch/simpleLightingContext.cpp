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
/// \file simpleLightingContext.cpp

#include "pxr/imaging/garch/simpleLightingContext.h"
#include "pxr/imaging/garch/bindingMap.h"
#include "pxr/imaging/garch/contextCaps.h"
#include "pxr/imaging/garch/resourceFactory.h"
#include "pxr/imaging/garch/simpleLight.h"
#include "pxr/imaging/garch/simpleMaterial.h"
#include "pxr/imaging/garch/uniformBlock.h"

#include "pxr/base/arch/pragmas.h"
#include "pxr/base/tf/stringUtils.h"
#include "pxr/base/tf/staticData.h"
#include "pxr/base/tf/staticTokens.h"

#include <algorithm>
#include <iostream>
#include <string>

PXR_NAMESPACE_OPEN_SCOPE


TF_DEFINE_PRIVATE_TOKENS(
    _tokens,
    ((lightingUB, "Lighting"))
    ((shadowUB, "Shadow"))
    ((bindlessShadowUB, "BindlessShadowSamplers"))
    ((materialUB, "Material"))
    ((shadowSampler, "shadowTexture"))
    ((shadowCompareSampler, "shadowCompareTexture"))
);

// XXX:
// currently max number of lights are limited to 16 by
// GL_MAX_VARYING_VECTORS for having the varying attribute
//    out vec2 FshadowFilterWidth[NUM_LIGHTS];
// which is defined in simpleLighting.glslfx.
static const int _maxLightsUsed = 16;

/* static */
GarchSimpleLightingContextRefPtr
GarchSimpleLightingContext::New()
{
    return TfCreateRefPtr(
        GarchResourceFactory::GetInstance()->NewSimpleLightingContext());
}

GarchSimpleLightingContext::GarchSimpleLightingContext() :
    _shadows(TfCreateRefPtr(
        GarchSimpleShadowArray::New())),
    _worldToViewMatrix(1.0),
    _projectionMatrix(1.0),
    _sceneAmbient(0.01, 0.01, 0.01, 1.0),
    _useLighting(false),
    _useShadows(false),
    _useColorMaterialDiffuse(false),
    _lightingUniformBlockValid(false),
    _shadowUniformBlockValid(false),
    _materialUniformBlockValid(false)
{
}

GarchSimpleLightingContext::~GarchSimpleLightingContext()
{
}

void
GarchSimpleLightingContext::SetLights(GarchSimpleLightVector const & lights)
{
    _lights = lights;
    _lightingUniformBlockValid = false;
    _shadowUniformBlockValid = false;

    int numLights = GetNumLightsUsed();

    _useShadows = false;
    for (int i = 0;i < numLights; ++i) {
        if (_lights[i].HasShadow()) {
            _useShadows = true;
            break;
        }
    }
}

GarchSimpleLightVector &
GarchSimpleLightingContext::GetLights()
{
    return _lights;
}

int
GarchSimpleLightingContext::GetNumLightsUsed() const
{
    return std::min((int)_lights.size(), _maxLightsUsed);
}

int
GarchSimpleLightingContext::ComputeNumShadowsUsed() const
{
    int numShadows = 0;
    for (auto const& light : _lights) {
        if (light.HasShadow() && numShadows <= light.GetShadowIndexEnd()) {
            numShadows = light.GetShadowIndexEnd() + 1;
        }
    }
    return numShadows;
}

void
GarchSimpleLightingContext::SetShadows(GarchSimpleShadowArrayRefPtr const & shadows)
{
    _shadows = shadows;
    _shadowUniformBlockValid = false;
}

GarchSimpleShadowArrayRefPtr const &
GarchSimpleLightingContext::GetShadows()
{
    return _shadows;
}

void
GarchSimpleLightingContext::SetMaterial(GarchSimpleMaterial const & material)
{
    if (_material != material) {
        _material = material;
        _materialUniformBlockValid = false;
    }
}

GarchSimpleMaterial const &
GarchSimpleLightingContext::GetMaterial() const
{
    return _material;
}

void
GarchSimpleLightingContext::SetSceneAmbient(GfVec4f const & sceneAmbient)
{
    if (_sceneAmbient != sceneAmbient) {
        _sceneAmbient = sceneAmbient;
        _materialUniformBlockValid = false;
    }
}

GfVec4f const &
GarchSimpleLightingContext::GetSceneAmbient() const
{
    return _sceneAmbient;
}

void
GarchSimpleLightingContext::SetCamera(GfMatrix4d const &worldToViewMatrix,
                                     GfMatrix4d const &projectionMatrix)
{
    if (_worldToViewMatrix != worldToViewMatrix) {
        _worldToViewMatrix = worldToViewMatrix;
        _lightingUniformBlockValid = false;
        _shadowUniformBlockValid = false;
    }
    _projectionMatrix = projectionMatrix;
}

void
GarchSimpleLightingContext::SetUseLighting(bool val)
{
    if (_useLighting != val) {
        _useLighting = val;
        _lightingUniformBlockValid = false;
    }
}

bool
GarchSimpleLightingContext::GetUseLighting() const
{
    return _useLighting;
}

bool
GarchSimpleLightingContext::GetUseShadows() const
{
    return _useShadows;
}

void
GarchSimpleLightingContext::SetUseColorMaterialDiffuse(bool val)
{
    if (_useColorMaterialDiffuse != val) {
        _lightingUniformBlockValid = false;
        _useColorMaterialDiffuse = val;
    }
}

bool
GarchSimpleLightingContext::GetUseColorMaterialDiffuse() const
{
    return _useColorMaterialDiffuse;
}

void
GarchSimpleLightingContext::InitUniformBlockBindings(
        GarchBindingMapPtr const &bindingMap) const
{
    // populate uniform bindings (XXX: need better API)
    bindingMap->GetUniformBinding(_tokens->lightingUB);
    bindingMap->GetUniformBinding(_tokens->shadowUB);
    bindingMap->GetUniformBinding(_tokens->materialUB);
    
    if (GarchSimpleShadowArray::GetBindlessShadowMapsEnabled()) {
        bindingMap->GetUniformBinding(_tokens->bindlessShadowUB);
    }
}

void
GarchSimpleLightingContext::InitSamplerUnitBindings(
        GarchBindingMapPtr const &bindingMap) const
{
    if (!GarchSimpleShadowArray::GetBindlessShadowMapsEnabled()) {
        bindingMap->GetSamplerUnit(_tokens->shadowSampler);
        bindingMap->GetSamplerUnit(_tokens->shadowCompareSampler);
    }
}

inline void
setVec3(float *dst, GfVec3f const & vec)
{
    dst[0] = vec[0];
    dst[1] = vec[1];
    dst[2] = vec[2];
}

inline static void
setVec4(float *dst, GfVec4f const &vec)
{
    dst[0] = vec[0];
    dst[1] = vec[1];
    dst[2] = vec[2];
    dst[3] = vec[3];
}

inline static void
setMatrix(float *dst, GfMatrix4d const & mat)
{
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            dst[i*4+j] = (float)mat[i][j];
}

void
GarchSimpleLightingContext::BindUniformBlocks(GarchBindingMapPtr const &bindingMap)
{
    if (!_lightingUniformBlock)
        _lightingUniformBlock = GarchResourceFactory::GetInstance()->NewUniformBlock("_lightingUniformBlock");
    if (!_shadowUniformBlock)
        _shadowUniformBlock = GarchResourceFactory::GetInstance()->NewUniformBlock("_shadowUniformBlock");
    if (!_materialUniformBlock)
        _materialUniformBlock = GarchResourceFactory::GetInstance()->NewUniformBlock("_materialUniformBlock");

    const bool usingBindlessShadowMaps =
        GarchSimpleShadowArray::GetBindlessShadowMapsEnabled();
    
    if (usingBindlessShadowMaps && !_bindlessShadowlUniformBlock) {
        _bindlessShadowlUniformBlock =
            GarchResourceFactory::GetInstance()->NewUniformBlock("_bindlessShadowUniformBlock");
    }

    bool bAlwaysNeedsBinding = GarchResourceFactory::GetInstance()->GetContextCaps().alwaysNeedsBinding;
    
    bool shadowExists = false;
    if ((!_lightingUniformBlockValid || !_shadowUniformBlockValid) &&
        ((_lights.size() > 0) || bAlwaysNeedsBinding)) {
        int numLights = GetNumLightsUsed();
        int numShadows = ComputeNumShadowsUsed();

        // 16byte aligned
        struct LightSource {
            float position[4];
            float ambient[4];
            float diffuse[4];
            float specular[4];
            float spotDirection[4];
            float spotCutoff;
            float spotFalloff;
            float padding[2];
            float attenuation[4];
            float worldToLightTransform[16];
            int32_t shadowIndexStart;
            int32_t shadowIndexEnd;
            int32_t hasShadow;
            int32_t isIndirectLight;
        };

        struct Lighting {
            int32_t useLighting;
            int32_t useColorMaterialDiffuse;
            int32_t padding[2];
            ARCH_PRAGMA_PUSH
            ARCH_PRAGMA_ZERO_SIZED_STRUCT
            LightSource lightSource[0];
            ARCH_PRAGMA_POP
        };

        // 16byte aligned
        struct ShadowMatrix {
            float viewToShadowMatrix[16];
            float shadowToViewMatrix[16];
            float blur;
            float bias;
            float padding[2];
        };

        struct Shadow {
            ARCH_PRAGMA_PUSH
            ARCH_PRAGMA_ZERO_SIZED_STRUCT
            ShadowMatrix shadow[0];
            ARCH_PRAGMA_POP
        };
        
        // Use a uniform buffer block for the array of 64bit bindless handles.
        //
        // glf/shaders/simpleLighting.glslfx uses a uvec2 array instead of
        // uint64_t.
        // Note that uint64_t has different padding rules depending on the
        // layout: std140 results in 128bit alignment, while shared (default)
        // results in 64bit alignment.
        struct PaddedHandle {
            uint64_t handle;
            //uint64_t padding; // Skip padding since we don't need it.
        };

        struct BindlessShadowSamplers {
            ARCH_PRAGMA_PUSH
            ARCH_PRAGMA_ZERO_SIZED_STRUCT
            PaddedHandle shadowCompareTextures[0];
            ARCH_PRAGMA_POP
        };

        size_t lightingSize;
        size_t shadowSize;

        if (numLights == 0) {
            lightingSize = sizeof(Lighting) + sizeof(LightSource);
        }
        else {
            lightingSize = sizeof(Lighting) + sizeof(LightSource) * numLights;
        }
        
        if (numShadows == 0) {
            shadowSize = sizeof(ShadowMatrix);
        }
        else {
            shadowSize = sizeof(ShadowMatrix) * numShadows;
        }
        
        Lighting *lightingData = (Lighting *)alloca(lightingSize);
        Shadow *shadowData = (Shadow *)alloca(shadowSize);

        memset(shadowData, 0, shadowSize);
        memset(lightingData, 0, lightingSize);

        BindlessShadowSamplers *bindlessHandlesData = nullptr;
        size_t bindlessHandlesSize = 0;
        if (usingBindlessShadowMaps) {
            bindlessHandlesSize = sizeof(PaddedHandle) * numShadows;
            bindlessHandlesData =
                (BindlessShadowSamplers*)alloca(bindlessHandlesSize);
            memset(bindlessHandlesData, 0, bindlessHandlesSize);
        }
        
        GfMatrix4d viewToWorldMatrix = _worldToViewMatrix.GetInverse();

        lightingData->useLighting = _useLighting;
        lightingData->useColorMaterialDiffuse = _useColorMaterialDiffuse;

        for (int i = 0; _useLighting && i < numLights; ++i) {
            GarchSimpleLight const &light = _lights[i];

            setVec4(lightingData->lightSource[i].position,
                    light.GetPosition() * _worldToViewMatrix);
            setVec4(lightingData->lightSource[i].diffuse, light.GetDiffuse());
            setVec4(lightingData->lightSource[i].ambient, light.GetAmbient());
            setVec4(lightingData->lightSource[i].specular, light.GetSpecular());
            setVec3(lightingData->lightSource[i].spotDirection,
                    _worldToViewMatrix.TransformDir(light.GetSpotDirection()));
            setVec3(lightingData->lightSource[i].attenuation,
                    light.GetAttenuation());
            lightingData->lightSource[i].spotCutoff = light.GetSpotCutoff();
            lightingData->lightSource[i].spotFalloff = light.GetSpotFalloff();
            setMatrix(lightingData->lightSource[i].worldToLightTransform,
                      light.GetTransform().GetInverse());
            lightingData->lightSource[i].hasShadow = light.HasShadow();
            lightingData->lightSource[i].isIndirectLight = light.IsDomeLight();

            if (lightingData->lightSource[i].hasShadow) {
                int shadowIndexStart = light.GetShadowIndexStart();
                lightingData->lightSource[i].shadowIndexStart =
                    shadowIndexStart;
                int shadowIndexEnd = light.GetShadowIndexEnd();
                lightingData->lightSource[i].shadowIndexEnd = shadowIndexEnd;

                for (int shadowIndex = shadowIndexStart;
                     shadowIndex <= shadowIndexEnd; ++shadowIndex) {
                    GfMatrix4d viewToShadowMatrix = viewToWorldMatrix *
                        _shadows->GetWorldToShadowMatrix(shadowIndex);
                    GfMatrix4d shadowToViewMatrix =
                        viewToShadowMatrix.GetInverse();

                    shadowData->shadow[shadowIndex].bias = light.GetShadowBias();
                    shadowData->shadow[shadowIndex].blur = light.GetShadowBlur();
                    
                    setMatrix(
                        shadowData->shadow[shadowIndex].viewToShadowMatrix,
                        viewToShadowMatrix);
                    setMatrix(
                        shadowData->shadow[shadowIndex].shadowToViewMatrix,
                        shadowToViewMatrix);
                }

                shadowExists = true;
            }
        }

        _lightingUniformBlock->Update(lightingData, lightingSize);
        _lightingUniformBlockValid = true;

        if (shadowExists || bAlwaysNeedsBinding) {
            _shadowUniformBlock->Update(shadowData, shadowSize);
            _shadowUniformBlockValid = true;
            
            if (usingBindlessShadowMaps) {
                std::vector<uint64_t> const& shadowMapHandles =
                    _shadows->GetBindlessShadowMapHandles();

                for (size_t i = 0; i < shadowMapHandles.size(); i++) {
                    bindlessHandlesData->shadowCompareTextures[i].handle
                        = shadowMapHandles[i];
                }

                _bindlessShadowlUniformBlock->Update(
                    bindlessHandlesData, bindlessHandlesSize);
            }
        }
    }

    _lightingUniformBlock->Bind(bindingMap, _tokens->lightingUB);

    if (shadowExists || bAlwaysNeedsBinding) {
        _shadowUniformBlock->Bind(bindingMap, _tokens->shadowUB);
        
        if (usingBindlessShadowMaps) {
            _bindlessShadowlUniformBlock->Bind(
                bindingMap, _tokens->bindlessShadowUB);
        }
    }

    if (!_materialUniformBlockValid) {
        // has to be matched with the definition of simpleLightingShader.glslfx
        struct Material {
            float ambient[4];
            float diffuse[4];
            float specular[4];
            float emission[4];
            float sceneColor[4];  // XXX: should be separated?
            float shininess;
            float padding[3];
        } materialData;

        memset(&materialData, 0, sizeof(materialData));

        setVec4(materialData.ambient, _material.GetAmbient());
        setVec4(materialData.diffuse, _material.GetDiffuse());
        setVec4(materialData.specular, _material.GetSpecular());
        setVec4(materialData.emission, _material.GetEmission());
        materialData.shininess = _material.GetShininess();
        setVec4(materialData.sceneColor, _sceneAmbient);

        _materialUniformBlock->Update(&materialData, sizeof(materialData));
        _materialUniformBlockValid = true;
    }

    _materialUniformBlock->Bind(bindingMap, _tokens->materialUB);
}

PXR_NAMESPACE_CLOSE_SCOPE

