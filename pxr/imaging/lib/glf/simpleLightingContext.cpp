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

#include "pxr/imaging/glf/glew.h"

#include "pxr/imaging/garch/bindingMap.h"
#include "pxr/imaging/garch/resourceFactory.h"
#include "pxr/imaging/garch/simpleLight.h"
#include "pxr/imaging/garch/simpleMaterial.h"
#include "pxr/imaging/garch/uniformBlock.h"

#include "pxr/imaging/glf/simpleLightingContext.h"
#include "pxr/imaging/glf/diagnostic.h"
#include "pxr/imaging/glf/package.h"

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
    ((materialUB, "Material"))
    ((shadowSampler, "shadowTexture"))
    ((shadowCompareSampler, "shadowCompareTexture"))
);

GlfSimpleLightingContext::GlfSimpleLightingContext()
{
}

GlfSimpleLightingContext::~GlfSimpleLightingContext()
{
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
GlfSimpleLightingContext::BindSamplers(GarchBindingMapPtr const &bindingMap)
{
    int shadowSampler = bindingMap->GetSamplerUnit(_tokens->shadowSampler);
    int shadowCompareSampler = bindingMap->GetSamplerUnit(_tokens->shadowCompareSampler);
    
    GLuint _shadowMapTexture = _shadows->GetShadowMapTexture();
    
    GLuint _shadowMapDepthSampler = (GLuint)(uint64_t)_shadows->GetShadowMapDepthSampler();
    GLuint _shadowMapCompareSampler = (GLuint)(uint64_t)_shadows->GetShadowMapCompareSampler();

    glActiveTexture(GL_TEXTURE0 + shadowSampler);
    glBindTexture(GL_TEXTURE_2D_ARRAY, _shadowMapTexture);
    glBindSampler(shadowSampler, _shadowMapDepthSampler);

    glActiveTexture(GL_TEXTURE0 + shadowCompareSampler);
    glBindTexture(GL_TEXTURE_2D_ARRAY, _shadowMapTexture);
    glBindSampler(shadowCompareSampler, _shadowMapCompareSampler);

    glActiveTexture(GL_TEXTURE0);
}

void
GlfSimpleLightingContext::UnbindSamplers(GarchBindingMapPtr const &bindingMap)
{
    int shadowSampler = bindingMap->GetSamplerUnit(_tokens->shadowSampler);
    int shadowCompareSampler = bindingMap->GetSamplerUnit(_tokens->shadowCompareSampler);

    glActiveTexture(GL_TEXTURE0 + shadowSampler);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    glBindSampler(shadowSampler, 0);

    glActiveTexture(GL_TEXTURE0 + shadowCompareSampler);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    glBindSampler(shadowCompareSampler, 0);

    glActiveTexture(GL_TEXTURE0);
}

void
GlfSimpleLightingContext::SetStateFromOpenGL()
{
    // import classic GL light's parameters into shaded lights
    SetUseLighting(glIsEnabled(GL_LIGHTING) == GL_TRUE);

    GfMatrix4d worldToViewMatrix;
    glGetDoublev(GL_MODELVIEW_MATRIX, worldToViewMatrix.GetArray());
    GfMatrix4d viewToWorldMatrix = worldToViewMatrix.GetInverse();

    GLint nLights = 0;
    glGetIntegerv(GL_MAX_LIGHTS, &nLights);

    GarchSimpleLightVector lights;
    lights.reserve(nLights);

    GarchSimpleLight light;
    for(int i = 0; i < nLights; ++i)
    {
        int lightName = GL_LIGHT0 + i;
        if (glIsEnabled(lightName)) {
            GLfloat position[4], color[4];

            glGetLightfv(lightName, GL_POSITION, position);
            light.SetPosition(GfVec4f(position)*viewToWorldMatrix);
            
            glGetLightfv(lightName, GL_AMBIENT, color);
            light.SetAmbient(GfVec4f(color));
            
            glGetLightfv(lightName, GL_DIFFUSE, color);
            light.SetDiffuse(GfVec4f(color));
            
            glGetLightfv(lightName, GL_SPECULAR, color);
            light.SetSpecular(GfVec4f(color));

            GLfloat spotDirection[3];
            glGetLightfv(lightName, GL_SPOT_DIRECTION, spotDirection);
            light.SetSpotDirection(
                viewToWorldMatrix.TransformDir(GfVec3f(spotDirection)));

            GLfloat floatValue;

            glGetLightfv(lightName, GL_SPOT_CUTOFF, &floatValue);
            light.SetSpotCutoff(floatValue);

            glGetLightfv(lightName, GL_SPOT_EXPONENT, &floatValue);
            light.SetSpotFalloff(floatValue);

            GfVec3f attenuation;
            glGetLightfv(lightName, GL_CONSTANT_ATTENUATION, &floatValue);
            attenuation[0] = floatValue;

            glGetLightfv(lightName, GL_LINEAR_ATTENUATION, &floatValue);
            attenuation[1] = floatValue;

            glGetLightfv(lightName, GL_QUADRATIC_ATTENUATION, &floatValue);
            attenuation[2] = floatValue;

            light.SetAttenuation(attenuation);

            lights.push_back(light);
        }
    }

    SetLights(lights);

    GarchSimpleMaterial material;

    GLfloat color[4], shininess;
    glGetMaterialfv(GL_FRONT, GL_AMBIENT, color);
    material.SetAmbient(GfVec4f(color));
    glGetMaterialfv(GL_FRONT, GL_DIFFUSE, color);
    material.SetDiffuse(GfVec4f(color));
    glGetMaterialfv(GL_FRONT, GL_SPECULAR, color);
    material.SetSpecular(GfVec4f(color));
    glGetMaterialfv(GL_FRONT, GL_EMISSION, color);
    material.SetEmission(GfVec4f(color));
    glGetMaterialfv(GL_FRONT, GL_SHININESS, &shininess);
    // clamp to 0.0001, since pow(0,0) is undefined in GLSL.
    shininess = std::max(0.0001f, shininess);
    material.SetShininess(shininess);

    SetMaterial(material);

    GfVec4f sceneAmbient;
    glGetFloatv(GL_LIGHT_MODEL_AMBIENT, &sceneAmbient[0]);
    SetSceneAmbient(sceneAmbient);
}

PXR_NAMESPACE_CLOSE_SCOPE

