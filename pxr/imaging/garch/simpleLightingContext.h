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
#ifndef GARCH_SIMPLE_LIGHTING_CONTEXT_H
#define GARCH_SIMPLE_LIGHTING_CONTEXT_H

/// \file garch/simpleLightingContext.h

#include "pxr/pxr.h"
#include "pxr/imaging/garch/api.h"
#include "pxr/imaging/garch/simpleLight.h"
#include "pxr/imaging/garch/simpleMaterial.h"
#include "pxr/imaging/garch/simpleShadowArray.h"

#include "pxr/base/gf/matrix4d.h"
#include "pxr/base/gf/vec4f.h"

#include "pxr/base/tf/declarePtrs.h"
#include "pxr/base/tf/refBase.h"
#include "pxr/base/tf/weakBase.h"
#include "pxr/base/tf/token.h"

#include <memory>

PXR_NAMESPACE_OPEN_SCOPE


TF_DECLARE_WEAK_AND_REF_PTRS(GarchBindingMap);
TF_DECLARE_WEAK_AND_REF_PTRS(GarchUniformBlock);
TF_DECLARE_WEAK_AND_REF_PTRS(GarchSimpleLightingContext);
TF_DECLARE_WEAK_AND_REF_PTRS(GarchSimpleShadowArray);

class GarchSimpleLightingContext : public TfRefBase, public TfWeakBase {
public:
    typedef GarchSimpleLightingContext This;

    GARCH_API
    static GarchSimpleLightingContextRefPtr New();

    GARCH_API
    void SetLights(GarchSimpleLightVector const & lights);
    GARCH_API
    GarchSimpleLightVector const & GetLights() const;

    // returns the effective number of lights taken into account
    // in composable/compatible shader constraints
    GARCH_API
    int GetNumLightsUsed() const;
    
    // returns the number of shadow maps needed, by summing shadow maps
    // allocated to each light.
    GARCH_API
    int ComputeNumShadowsUsed() const;

    GARCH_API
    void SetShadows(GarchSimpleShadowArrayRefPtr const & shadows);
    GARCH_API
    GarchSimpleShadowArrayRefPtr const & GetShadows() const;

    GARCH_API
    void SetMaterial(GarchSimpleMaterial const & material);
    GARCH_API
    GarchSimpleMaterial const & GetMaterial() const;

    GARCH_API
    void SetSceneAmbient(GfVec4f const & sceneAmbient);
    GARCH_API
    GfVec4f const & GetSceneAmbient() const;

    GARCH_API
    void SetCamera(GfMatrix4d const &worldToViewMatrix,
                   GfMatrix4d const &projectionMatrix);

    GARCH_API
    void SetUseLighting(bool val);
    GARCH_API
    bool GetUseLighting() const;

    // returns true if any light has shadow enabled.
    GARCH_API
    bool GetUseShadows() const;

    GARCH_API
    void SetUseColorMaterialDiffuse(bool val);
    GARCH_API
    bool GetUseColorMaterialDiffuse() const;

    GARCH_API
    virtual void InitUniformBlockBindings(GarchBindingMapPtr const &bindingMap) const;
    GARCH_API
    virtual void InitSamplerUnitBindings(GarchBindingMapPtr const &bindingMap) const;

    GARCH_API
    virtual void BindUniformBlocks(GarchBindingMapPtr const &bindingMap);
    GARCH_API
    virtual void BindSamplers(GarchBindingMapPtr const &bindingMap) = 0;

    GARCH_API
    virtual void UnbindSamplers(GarchBindingMapPtr const &bindingMap) = 0;

    GARCH_API
    virtual void SetStateFromOpenGL() = 0;

    /// \name Post Surface Lighting
    ///
    /// This context can provide additional shader source, currently
    /// used to implement post surface lighting, along with a hash
    /// to help de-duplicate use by client shader programs.
    ///
    /// @{

    GARCH_API
    size_t ComputeShaderSourceHash();

    GARCH_API
    std::string const & ComputeShaderSource(TfToken const &shaderStageKey);

    /// @}

protected:
    GARCH_API
    GarchSimpleLightingContext();
    GARCH_API
    ~GarchSimpleLightingContext();

    void _ComputePostSurfaceShaderState();
    void _BindPostSurfaceShaderParams(GarchBindingMapPtr const &bindingMap);

    GarchSimpleLightVector _lights;
    GarchSimpleShadowArrayRefPtr _shadows;

    GfMatrix4d _worldToViewMatrix;
    GfMatrix4d _projectionMatrix;

    GarchSimpleMaterial _material;
    GfVec4f _sceneAmbient;

    bool _useLighting;
    bool _useShadows;
    bool _useColorMaterialDiffuse;

    GarchUniformBlockRefPtr _lightingUniformBlock;
    GarchUniformBlockRefPtr _shadowUniformBlock;
    GarchUniformBlockRefPtr _materialUniformBlock;
    GarchUniformBlockRefPtr _bindlessShadowlUniformBlock;

    class _PostSurfaceShaderState;
        std::unique_ptr<_PostSurfaceShaderState> _postSurfaceShaderState;

    bool _lightingUniformBlockValid;
    bool _shadowUniformBlockValid;
    bool _materialUniformBlockValid;
    bool _postSurfaceShaderStateValid;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif // GARCH_SIMPLE_LIGHTING_CONTEXT_H