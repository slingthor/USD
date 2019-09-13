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
#include "pxr/imaging/hdx/simpleLightingShader.h"
#include "pxr/imaging/hdx/package.h"

#include "pxr/imaging/hd/binding.h"
#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hdSt/textureResource.h"
#include "pxr/imaging/hd/tokens.h"

#include "pxr/imaging/hf/perfLog.h"

#include "pxr/imaging/hio/glslfx.h"
#include "pxr/imaging/garch/bindingMap.h"
#include "pxr/imaging/garch/simpleLightingContext.h"

#include "pxr/base/tf/staticTokens.h"

#if defined(ARCH_GFX_OPENGL)
#include "pxr/imaging/hdSt/GL/glslProgram.h"
#endif
#include "pxr/imaging/hdSt/resourceFactory.h"

#include <boost/functional/hash.hpp>

#include <string>
#include <sstream>

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PRIVATE_TOKENS(
    _tokens,
    // (DomeLightTexture)  // textures in previewSurface.glslfx
    (domeLightIrradiance)
    (domeLightPrefilter) 
    (domeLightBRDF)
);


HdxSimpleLightingShader::HdxSimpleLightingShader()
    : _lightingContext(GarchSimpleLightingContext::New())
    , _bindingMap(GarchBindingMap::New())
    , _useLighting(true)
{
    _lightingContext->InitUniformBlockBindings(_bindingMap);
    _lightingContext->InitSamplerUnitBindings(_bindingMap);

    _glslfx.reset(new HioGlslfx(HdxPackageSimpleLightingShader()));
}

HdxSimpleLightingShader::~HdxSimpleLightingShader()
{
}

/* virtual */
HdxSimpleLightingShader::ID
HdxSimpleLightingShader::ComputeHash() const
{
    HD_TRACE_FUNCTION();

    TfToken glslfxFile = HdxPackageSimpleLightingShader();
    size_t numLights = _useLighting ? _lightingContext->GetNumLightsUsed() : 0;
    bool useShadows = _useLighting ? _lightingContext->GetUseShadows() : false;

    size_t hash = glslfxFile.Hash();
    boost::hash_combine(hash, numLights);
    boost::hash_combine(hash, useShadows);

    return (ID)hash;
}

/* virtual */
std::string
HdxSimpleLightingShader::GetSource(TfToken const &shaderStageKey) const
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    std::string source = _glslfx->GetSource(shaderStageKey);

    if (source.empty()) return source;

    std::stringstream defineStream;
    size_t numLights = _useLighting ? _lightingContext->GetNumLightsUsed() : 0;
    bool useShadows = _useLighting ? _lightingContext->GetUseShadows() : false;
    defineStream << "#define NUM_LIGHTS " << numLights<< "\n";
    defineStream << "#define USE_SHADOWS " << (int)(useShadows) << "\n";

    return defineStream.str() + source;
}

/* virtual */
void
HdxSimpleLightingShader::SetCamera(GfMatrix4d const &worldToViewMatrix,
                                          GfMatrix4d const &projectionMatrix)
{
    _lightingContext->SetCamera(worldToViewMatrix, projectionMatrix);
}

/* virtual */
void
HdxSimpleLightingShader::BindResources(HdSt_ResourceBinder const &binder,
                                       HdStProgram const &program)
{
    static std::mutex _mutex;
    std::lock_guard<std::mutex> lock(_mutex);

    // XXX: we'd like to use HdSt_ResourceBinder instead of GarchBindingMap.
    //
    program.AssignUniformBindings(_bindingMap);
    _lightingContext->BindUniformBlocks(_bindingMap);

    program.AssignSamplerUnits(_bindingMap);
    _lightingContext->BindSamplers(_bindingMap);

#if defined(ARCH_GFX_OPENGL)
    GLuint programId = 0;
    
    bool isOpenGL = HdStResourceFactory::GetInstance()->IsOpenGL();
    if (isOpenGL) {
        HdStGLSLProgram const &glslProgram(
            dynamic_cast<const HdStGLSLProgram&>(program));
        programId = glslProgram.GetGLProgram();
    }
#endif

    for (auto const& light : _lightingContext->GetLights()){

        if (light.IsDomeLight()) {

            HdBinding irradianceBinding = 
                                binder.GetBinding(_tokens->domeLightIrradiance);
            if (irradianceBinding.GetType() == HdBinding::TEXTURE_2D) {
                int samplerUnit = irradianceBinding.GetTextureUnit();
                
                uint32_t textureId = uint32_t(light.GetIrradianceId());
                uint32_t samplerId = uint32_t(light.GetSamplerId());
#if defined(ARCH_GFX_OPENGL)
                if (isOpenGL) {
                    glActiveTexture(GL_TEXTURE0 + samplerUnit);
                    glBindTexture(GL_TEXTURE_2D, (GLuint)textureId);
                    glBindSampler(samplerUnit, (unsigned int)samplerId);
                    
                    glProgramUniform1i(programId, irradianceBinding.GetLocation(),
                                        samplerUnit);
                }
#endif
            } 
            HdBinding prefilterBinding = 
                                binder.GetBinding(_tokens->domeLightPrefilter);
            if (prefilterBinding.GetType() == HdBinding::TEXTURE_2D) {
                int samplerUnit = prefilterBinding.GetTextureUnit();
                
                uint32_t textureId = uint32_t(light.GetPrefilterId());
                uint32_t samplerId = uint32_t(light.GetSamplerId());
#if defined(ARCH_GFX_OPENGL)
                if (isOpenGL) {
                    glActiveTexture(GL_TEXTURE0 + samplerUnit);
                    glBindTexture(GL_TEXTURE_2D, (GLuint)textureId);
                    glBindSampler(samplerUnit, (unsigned int)samplerId);
                    
                    glProgramUniform1i(programId, prefilterBinding.GetLocation(),
                                        samplerUnit);
                }
#endif
            } 
            HdBinding brdfBinding = binder.GetBinding(_tokens->domeLightBRDF);
            if (brdfBinding.GetType() == HdBinding::TEXTURE_2D) {
                int samplerUnit = brdfBinding.GetTextureUnit();
                
                uint32_t textureId = uint32_t(light.GetBrdfId());
                uint32_t samplerId = uint32_t(light.GetSamplerId());
#if defined(ARCH_GFX_OPENGL)
                if (isOpenGL) {
                    glActiveTexture(GL_TEXTURE0 + samplerUnit);
                    glBindTexture(GL_TEXTURE_2D, (GLuint)textureId);
                    glBindSampler(samplerUnit, (unsigned int)samplerId);
                    
                    glProgramUniform1i(programId, brdfBinding.GetLocation(),
                                       samplerUnit);
                }
#endif
            }
        }
    }
#if defined(ARCH_GFX_OPENGL)
    if (isOpenGL) {
        glActiveTexture(GL_TEXTURE0);
    }
#endif
    binder.BindShaderResources(this);
}

/* virtual */
void
HdxSimpleLightingShader::UnbindResources(HdSt_ResourceBinder const &binder,
                                         HdStProgram const &program)
{
    // XXX: we'd like to use HdSt_ResourceBinder instead of GlfBindingMap.
    //
    _lightingContext->UnbindSamplers(_bindingMap);
    
#if defined(ARCH_GFX_OPENGL)
    if (HdStResourceFactory::GetInstance()->IsOpenGL()) {
       for (auto const& light : _lightingContext->GetLights()){

            if (light.IsDomeLight()) {

                HdBinding irradianceBinding =
                                    binder.GetBinding(_tokens->domeLightIrradiance);
                if (irradianceBinding.GetType() == HdBinding::TEXTURE_2D) {
                    int samplerUnit = irradianceBinding.GetTextureUnit();
                    glActiveTexture(GL_TEXTURE0 + samplerUnit);
                    glBindTexture(GL_TEXTURE_2D, 0);
                    
                    glBindSampler(samplerUnit, 0);
                }
                HdBinding prefilterBinding =
                                    binder.GetBinding(_tokens->domeLightPrefilter);
                if (prefilterBinding.GetType() == HdBinding::TEXTURE_2D) {
                    int samplerUnit = prefilterBinding.GetTextureUnit();
                    glActiveTexture(GL_TEXTURE0 + samplerUnit);
                    glBindTexture(GL_TEXTURE_2D, 0);
                    
                    glBindSampler(samplerUnit, 0);
                }
                HdBinding brdfBinding = binder.GetBinding(_tokens->domeLightBRDF);
                if (brdfBinding.GetType() == HdBinding::TEXTURE_2D) {
                    int samplerUnit = brdfBinding.GetTextureUnit();
                    glActiveTexture(GL_TEXTURE0 + samplerUnit);
                    glBindTexture(GL_TEXTURE_2D, 0);
                    
                    glBindSampler(samplerUnit, 0);
                }
            }
        }
        glActiveTexture(GL_TEXTURE0);
    }
#endif
}

/*virtual*/
void
HdxSimpleLightingShader::AddBindings(HdBindingRequestVector *customBindings)
{
    _lightTextureParams.clear();

    bool haveDomeLight = false;
    for (auto const& light : _lightingContext->GetLights()) { 

        if (light.IsDomeLight() && !haveDomeLight) {

            // For now we assume that the only simple light with a texture is
            // a domeLight (ignoring RectLights, and multiple domeLights)
            haveDomeLight = true;

            // irradiance map
            _lightTextureParams.push_back(
                    HdMaterialParam(HdMaterialParam::ParamTypeTexture,
                    _tokens->domeLightIrradiance,
                    VtValue(GfVec4f(0.0)),
                    SdfPath(),
                    TfTokenVector(),
                    HdTextureType::Uv));
            // prefilter map
            _lightTextureParams.push_back(
                    HdMaterialParam(HdMaterialParam::ParamTypeTexture,
                    _tokens->domeLightPrefilter,
                    VtValue(GfVec4f(0.0)),
                    SdfPath(),
                    TfTokenVector(),
                    HdTextureType::Uv));
            // BRDF texture
            _lightTextureParams.push_back(
                    HdMaterialParam(HdMaterialParam::ParamTypeTexture,
                    _tokens->domeLightBRDF,
                    VtValue(GfVec4f(0.0)),
                    SdfPath(),
                    TfTokenVector(),
                    HdTextureType::Uv));
        }
    }
}

HdMaterialParamVector const& 
HdxSimpleLightingShader::GetParams() const 
{
    return _lightTextureParams;
}

void
HdxSimpleLightingShader::SetLightingStateFromOpenGL()
{
    _lightingContext->SetStateFromOpenGL();
}

void
HdxSimpleLightingShader::SetLightingState(
    GarchSimpleLightingContextPtr const &src)
{
    if (src) {
        _useLighting = true;
        _lightingContext->SetUseLighting(!src->GetLights().empty());
        _lightingContext->SetLights(src->GetLights());
        _lightingContext->SetMaterial(src->GetMaterial());
        _lightingContext->SetSceneAmbient(src->GetSceneAmbient());
        _lightingContext->SetShadows(src->GetShadows());
    } else {
        // XXX:
        // if src is null, turn off lights (this is temporary used for shadowmap drawing).
        // see GprimUsdBaseIcBatch::Draw()
        _useLighting = false;
    }
}

PXR_NAMESPACE_CLOSE_SCOPE

