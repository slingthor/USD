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
#include "pxr/imaging/glf/glew.h"
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
#if defined(ARCH_GFX_METAL)
#include "pxr/imaging/hdSt/Metal/mslProgram.h"
#endif
#include "pxr/imaging/hdSt/resourceFactory.h"

#include <boost/functional/hash.hpp>

#include <string>
#include <sstream>

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PRIVATE_TOKENS(
    _tokens,
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
    size_t numShadows = useShadows ? _lightingContext->ComputeNumShadowsUsed() : 0;

    size_t hash = glslfxFile.Hash();
    boost::hash_combine(hash, numLights);
    boost::hash_combine(hash, useShadows);
    boost::hash_combine(hash, numShadows);

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
    size_t numShadows = useShadows ? _lightingContext->ComputeNumShadowsUsed() : 0;
    defineStream << "#define NUM_LIGHTS " << numLights<< "\n";
    defineStream << "#define USE_SHADOWS " << (int)(useShadows) << "\n";
    defineStream << "#define NUM_SHADOWS " << numShadows << "\n";
    if (useShadows) {
        bool const useBindlessShadowMaps =
            GarchSimpleShadowArray::GetBindlessShadowMapsEnabled();;
        defineStream << "#define USE_BINDLESS_SHADOW_TEXTURES "
                     << int(useBindlessShadowMaps) << "\n";
    }

    return defineStream.str() + source;
}

/* virtual */
void
HdxSimpleLightingShader::SetCamera(GfMatrix4d const &worldToViewMatrix,
                                          GfMatrix4d const &projectionMatrix)
{
    _lightingContext->SetCamera(worldToViewMatrix, projectionMatrix);
}

static void _BindToMetal(
    MSL_ShaderBindingMap const &bindingMap,
    TfToken &bindTextureName,
    TfToken &bindSamplerName,
    GarchTextureGPUHandle const &textureHandle,
    GarchSamplerGPUHandle const &samplerHandle)
{
    MSL_ShaderBinding const* const textureBinding = MSL_FindBinding(
        bindingMap,
        bindTextureName,
        kMSL_BindingType_Texture,
        0xFFFFFFFF,
        0);
    if(!textureBinding)
    {
        TF_FATAL_CODING_ERROR("Could not bind a texture to the shader?!");
    }

    HdBinding::Type type = textureBinding->_binding.GetType();
    
    MtlfMetalContext::GetMetalContext()->SetTexture(
        textureBinding->_index,
        textureHandle,
        bindTextureName,
        textureBinding->_stage);

    static std::string samplerName("samplerBind_" + _tokens->domeLightIrradiance.GetString());
    static TfToken samplerNameToken(samplerName, TfToken::Immortal);

    MSL_ShaderBinding const* const samplerBinding = MSL_FindBinding(
        bindingMap,
        bindSamplerName,
        kMSL_BindingType_Sampler,
        0xFFFFFFFF,
        0);

    if(!samplerBinding)
    {
        TF_FATAL_CODING_ERROR("Could not bind a sampler to the shader?!");
    }
    MtlfMetalContext::GetMetalContext()->SetSampler(
        samplerBinding->_index,
        samplerHandle,
        bindSamplerName,
        samplerBinding->_stage);
}

/* virtual */
void
HdxSimpleLightingShader::BindResources(HdStProgram const &program,
                                       HdSt_ResourceBinder const &binder,
                                       HdRenderPassState const &state)
{
    static std::mutex _mutex;
    std::lock_guard<std::mutex> lock(_mutex);

    // XXX: we'd like to use HdSt_ResourceBinder instead of GarchBindingMap.
    //
    program.AssignUniformBindings(_bindingMap);
    _lightingContext->BindUniformBlocks(_bindingMap);

    program.AssignSamplerUnits(_bindingMap);
    _lightingContext->BindSamplers(_bindingMap);

    bool isOpenGL = HdStResourceFactory::GetInstance()->IsOpenGL();

#if defined(ARCH_GFX_OPENGL)
    GLuint programId = 0;
    
    if (isOpenGL) {
        HdStGLSLProgram const &glslProgram(
            dynamic_cast<const HdStGLSLProgram&>(program));
        programId = glslProgram.GetGLProgram();
    }
#endif
    
    HdStMSLProgram const &mslProgram(
        dynamic_cast<const HdStMSLProgram&>(program));

    for (auto const& light : _lightingContext->GetLights()){

        if (light.IsDomeLight()) {

            HdBinding irradianceBinding = 
                                binder.GetBinding(_tokens->domeLightIrradiance);
            if (irradianceBinding.GetType() == HdBinding::TEXTURE_2D) {
                int samplerUnit = irradianceBinding.GetTextureUnit();
                
                if (isOpenGL) {
#if defined(ARCH_GFX_OPENGL)
                    uint32_t textureId = uint32_t(light.GetIrradianceId());
                    glActiveTexture(GL_TEXTURE0 + samplerUnit);
                    glBindTexture(GL_TEXTURE_2D, (GLuint)textureId);
                	glBindSampler(samplerUnit, 0);
#endif
                }
                else {
                    static std::string textureName("textureBind_" + _tokens->domeLightIrradiance.GetString());
                    static TfToken textureNameToken(textureName, TfToken::Immortal);
                    static std::string samplerName("samplerBind_" + _tokens->domeLightIrradiance.GetString());
                    static TfToken samplerNameToken(samplerName, TfToken::Immortal);

                    _BindToMetal(
                        mslProgram.GetBindingMap(),
                        textureNameToken,
                        samplerNameToken,
                        light.GetIrradianceId(),
                        light.GetIrradianceSamplerId());
                }
            } 
            HdBinding prefilterBinding = 
                                binder.GetBinding(_tokens->domeLightPrefilter);
            if (prefilterBinding.GetType() == HdBinding::TEXTURE_2D) {
                int samplerUnit = prefilterBinding.GetTextureUnit();
                
                if (isOpenGL) {
#if defined(ARCH_GFX_OPENGL)
                    uint32_t textureId = uint32_t(light.GetPrefilterId());
                    glActiveTexture(GL_TEXTURE0 + samplerUnit);
                    glBindTexture(GL_TEXTURE_2D, (GLuint)textureId);
                	glBindSampler(samplerUnit, 0);
#endif
                }
                else {
                    static std::string textureName("textureBind_" + _tokens->domeLightPrefilter.GetString());
                    static TfToken textureNameToken(textureName, TfToken::Immortal);
                    static std::string samplerName("samplerBind_" + _tokens->domeLightPrefilter.GetString());
                    static TfToken samplerNameToken(samplerName, TfToken::Immortal);

                    _BindToMetal(
                        mslProgram.GetBindingMap(),
                        textureNameToken,
                        samplerNameToken,
                        light.GetPrefilterId(),
                        light.GetPrefilterSamplerId());
                }
            } 
            HdBinding brdfBinding = binder.GetBinding(_tokens->domeLightBRDF);
            if (brdfBinding.GetType() == HdBinding::TEXTURE_2D) {
                int samplerUnit = brdfBinding.GetTextureUnit();
                
                if (isOpenGL) {
#if defined(ARCH_GFX_OPENGL)
                    uint32_t textureId = uint32_t(light.GetBrdfId());
                    glActiveTexture(GL_TEXTURE0 + samplerUnit);
                    glBindTexture(GL_TEXTURE_2D, (GLuint)textureId);
                	glBindSampler(samplerUnit, 0);
#endif
                }
                else {
                    static std::string textureName("textureBind_" + _tokens->domeLightBRDF.GetString());
                    static TfToken textureNameToken(textureName, TfToken::Immortal);
                    static std::string samplerName("samplerBind_" + _tokens->domeLightBRDF.GetString());
                    static TfToken samplerNameToken(samplerName, TfToken::Immortal);

                    _BindToMetal(
                        mslProgram.GetBindingMap(),
                        textureNameToken,
                        samplerNameToken,
                        light.GetBrdfId(),
                        light.GetBrdfSamplerId());
                }
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
HdxSimpleLightingShader::UnbindResources(HdStProgram const &program,
                                         HdSt_ResourceBinder const &binder,
                                         HdRenderPassState const &state)
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
    static std::mutex _mutex;
    std::lock_guard<std::mutex> lock(_mutex);

    bool haveDomeLight = false;
    for (auto const& light : _lightingContext->GetLights()) {

        if (light.IsDomeLight() && !haveDomeLight) {

            // For now we assume that the only simple light with a texture is
            // a domeLight (ignoring RectLights, and multiple domeLights)
            haveDomeLight = true;
            break;
        }
    }

    if (!haveDomeLight && _lightTextureParams.size()) {
        _lightTextureParams.clear();
    }

    if (haveDomeLight && !_lightTextureParams.size()) {
        for (auto const& light : _lightingContext->GetLights()) {

            if (light.IsDomeLight()) {

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
                break;
            }
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

