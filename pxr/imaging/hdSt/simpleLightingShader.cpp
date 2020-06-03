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
#include "pxr/imaging/hdSt/simpleLightingShader.h"
#include "pxr/imaging/hdSt/textureResource.h"
#include "pxr/imaging/hdSt/textureIdentifier.h"
#include "pxr/imaging/hdSt/subtextureIdentifier.h"
#include "pxr/imaging/hdSt/textureObject.h"
#include "pxr/imaging/hdSt/textureHandle.h"
#include "pxr/imaging/hdSt/package.h"
#include "pxr/imaging/hdSt/materialParam.h"
#include "pxr/imaging/hdSt/resourceBinder.h"
#include "pxr/imaging/hdSt/resourceRegistry.h"
#include "pxr/imaging/hdSt/domeLightComputations.h"

#include "pxr/imaging/hd/sceneDelegate.h"
#include "pxr/imaging/hd/binding.h"
#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/tokens.h"

#include "pxr/imaging/hf/perfLog.h"

#include "pxr/imaging/hio/glslfx.h"
#include "pxr/imaging/hgi/enums.h"
#include "pxr/imaging/hgi/texture.h"

#include "pxr/imaging/garch/bindingMap.h"

#include "pxr/base/tf/staticTokens.h"

#if defined(PXR_OPENGL_SUPPORT_ENABLED)
#include "pxr/imaging/hdSt/GL/glslProgram.h"
#include "pxr/imaging/hgiGL/conversions.h"
#endif
#if defined(PXR_METAL_SUPPORT_ENABLED)
#include "pxr/imaging/hdSt/Metal/mslProgram.h"
#include "pxr/imaging/hgiMetal/conversions.h"
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


HdStSimpleLightingShader::HdStSimpleLightingShader() 
    : _lightingContext(GarchSimpleLightingContext::New())
    , _bindingMap(GarchBindingMap::New())
    , _useLighting(true)
    , _glslfx(std::make_unique<HioGlslfx>(HdStPackageSimpleLightingShader()))
{
    _lightingContext->InitUniformBlockBindings(_bindingMap);
    _lightingContext->InitSamplerUnitBindings(_bindingMap);
    _isOpenGL = HdStResourceFactory::GetInstance()->IsOpenGL();
}

HdStSimpleLightingShader::~HdStSimpleLightingShader()
{
#if defined(PXR_OPENGL_SUPPORT_ENABLED)
    if (_isOpenGL) {
        uint32_t t[] = { _domeLightIrradianceGLName, _domeLightPrefilterGLName, _domeLightBrdfGLName };
        uint32_t s[] = { _domeLightIrradianceGLSampler, _domeLightPrefilterGLSampler, _domeLightBrdfGLSampler };
        glDeleteTextures(sizeof(t) / sizeof(t[0]), t);
        glDeleteTextures(sizeof(s) / sizeof(s[0]), s);
    }
#endif
#if defined(PXR_METAL_SUPPORT_ENABLED)
    if (!_isOpenGL) {
        [_domeLightIrradianceGLName release];
        [_domeLightPrefilterGLName release];
        [_domeLightBrdfGLName release];

        [_domeLightIrradianceGLSampler release];
        [_domeLightPrefilterGLSampler release];
        [_domeLightBrdfGLSampler release];
    }
#endif
}

/* virtual */
HdStSimpleLightingShader::ID
HdStSimpleLightingShader::ComputeHash() const
{
    HD_TRACE_FUNCTION();

    const TfToken glslfxFile = HdStPackageSimpleLightingShader();
    const size_t numLights =
        _useLighting ? _lightingContext->GetNumLightsUsed() : 0;
    const bool useShadows =
        _useLighting ? _lightingContext->GetUseShadows() : false;
    const size_t numShadows =
        useShadows ? _lightingContext->ComputeNumShadowsUsed() : 0;

    size_t hash = glslfxFile.Hash();
    boost::hash_combine(hash, numLights);
    boost::hash_combine(hash, useShadows);
    boost::hash_combine(hash, numShadows);

    return (ID)hash;
}

/* virtual */
std::string
HdStSimpleLightingShader::GetSource(TfToken const &shaderStageKey) const
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    const std::string source = _glslfx->GetSource(shaderStageKey);

    if (source.empty()) return source;

    std::stringstream defineStream;
    const size_t numLights =
        _useLighting ? _lightingContext->GetNumLightsUsed() : 0;
    const bool useShadows =
        _useLighting ? _lightingContext->GetUseShadows() : false;
    const size_t numShadows =
        useShadows ? _lightingContext->ComputeNumShadowsUsed() : 0;
    defineStream << "#define NUM_LIGHTS " << numLights<< "\n";
    defineStream << "#define USE_SHADOWS " << (int)(useShadows) << "\n";
    defineStream << "#define NUM_SHADOWS " << numShadows << "\n";
    if (useShadows) {
        const bool useBindlessShadowMaps =
            GarchSimpleShadowArray::GetBindlessShadowMapsEnabled();;
        defineStream << "#define USE_BINDLESS_SHADOW_TEXTURES "
                     << int(useBindlessShadowMaps) << "\n";
    }

    return defineStream.str() + source;
}

/* virtual */
void
HdStSimpleLightingShader::SetCamera(GfMatrix4d const &worldToViewMatrix,
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
    
    MtlfMetalContext::GetMetalContext()->SetTexture(
        textureBinding->_index,
        textureHandle,
        bindTextureName,
        textureBinding->_stage);

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

static
bool
_HasDomeLight(GarchSimpleLightingContextRefPtr const &ctx)
{
    for (auto const& light : ctx->GetLights()){
        if (light.IsDomeLight()) {
            return true;
        }
    }
    return false;
}

static
void
_BindTextureAndSampler(HdStProgram const &program,
                       HdSt_ResourceBinder const &binder,
                       TfToken const &token,
                       const GarchTextureGPUHandle glName,
                       const GarchSamplerGPUHandle glSampler)
{
    const HdBinding binding = binder.GetBinding(token);
    if (binding.GetType() != HdBinding::TEXTURE_2D) {
        return;
    }
    
    bool isOpenGL = HdStResourceFactory::GetInstance()->IsOpenGL();
    
    if (isOpenGL) {
#if defined(PXR_OPENGL_SUPPORT_ENABLED)
        const int samplerUnit = binding.GetTextureUnit();
        glActiveTexture(GL_TEXTURE0 + samplerUnit);
        glBindTexture(GL_TEXTURE_2D, glName);
        glBindSampler(samplerUnit, glSampler);
#endif
    }
    else {
        std::string textureName("textureBind_" + token.GetString());
        TfToken textureNameToken(textureName, TfToken::Immortal);
        std::string samplerName("samplerBind_" + token.GetString());
        TfToken samplerNameToken(samplerName, TfToken::Immortal);

        HdStMSLProgram const &mslProgram(
            dynamic_cast<const HdStMSLProgram&>(program));

        _BindToMetal(
            mslProgram.GetBindingMap(),
            textureNameToken,
            samplerNameToken,
            glName,
            glSampler);
    }
}

/* virtual */
void
HdStSimpleLightingShader::BindResources(HdStProgram const &program,
                                        HdSt_ResourceBinder const &binder,
                                        HdRenderPassState const &state)
{
    static std::mutex _mutex;
    std::lock_guard<std::mutex> lock(_mutex);

    // XXX: we'd like to use HdSt_ResourceBinder instead of GlfBindingMap.
    //
    program.AssignUniformBindings(_bindingMap);
    _lightingContext->BindUniformBlocks(_bindingMap);

    program.AssignSamplerUnits(_bindingMap);
    _lightingContext->BindSamplers(_bindingMap);

    if(_HasDomeLight(_lightingContext)) {
        _BindTextureAndSampler(program,
                               binder,
                               _tokens->domeLightIrradiance,
                               _domeLightIrradianceGLName,
                               _domeLightIrradianceGLSampler);
        _BindTextureAndSampler(program,
                               binder,
                               _tokens->domeLightPrefilter,
                               _domeLightPrefilterGLName,
                               _domeLightPrefilterGLSampler);
        _BindTextureAndSampler(program,
                               binder,
                               _tokens->domeLightBRDF,
                               _domeLightBrdfGLName,
                               _domeLightBrdfGLSampler);
    }
    if (_isOpenGL) {
#if defined(PXR_OPENGL_SUPPORT_ENABLED)
        glActiveTexture(GL_TEXTURE0);
#endif
    }
    binder.BindShaderResources(this);
}

/* virtual */
void
HdStSimpleLightingShader::UnbindResources(HdStProgram const &program,
                                         HdSt_ResourceBinder const &binder,
                                         HdRenderPassState const &state)
{
    // XXX: we'd like to use HdSt_ResourceBinder instead of GlfBindingMap.
    //
    _lightingContext->UnbindSamplers(_bindingMap);
    
    if (HdStResourceFactory::GetInstance()->IsOpenGL()) {
#if defined(PXR_OPENGL_SUPPORT_ENABLED)
        if(_HasDomeLight(_lightingContext)) {
            GarchTextureGPUHandle t;
            GarchSamplerGPUHandle s;
            _BindTextureAndSampler(program,
                                   binder,
                                   _tokens->domeLightIrradiance,
                                   t,
                                   s);
            _BindTextureAndSampler(program,
                                   binder,
                                   _tokens->domeLightPrefilter,
                                   t,
                                   s);
            _BindTextureAndSampler(program,
                                   binder,
                                   _tokens->domeLightBRDF,
                                   t,
                                   s);
        }

        glActiveTexture(GL_TEXTURE0);
#endif
    }
}

/*virtual*/
void
HdStSimpleLightingShader::AddBindings(HdBindingRequestVector *customBindings)
{
    // For now we assume that the only simple light with a texture is
    // a domeLight (ignoring RectLights, and multiple domeLights)

    static std::mutex _mutex;
    std::lock_guard<std::mutex> lock(_mutex);

    bool haveDomeLight = _HasDomeLight(_lightingContext);
    if(!haveDomeLight  && _lightTextureParams.size()) {
        _lightTextureParams.clear();
    }
    if (haveDomeLight && !_lightTextureParams.size()) {
        // irradiance map
        _lightTextureParams.push_back(
            HdSt_MaterialParam(
                HdSt_MaterialParam::ParamTypeTexture,
                    _tokens->domeLightIrradiance,
                    VtValue(GfVec4f(0.0)),
                    TfTokenVector(),
                    HdTextureType::Uv));
        // prefilter map
        _lightTextureParams.push_back(
            HdSt_MaterialParam(
                HdSt_MaterialParam::ParamTypeTexture,
                    _tokens->domeLightPrefilter,
                    VtValue(GfVec4f(0.0)),
                    TfTokenVector(),
                    HdTextureType::Uv));
        // BRDF texture
        _lightTextureParams.push_back(
        HdSt_MaterialParam(
            HdSt_MaterialParam::ParamTypeTexture,
                _tokens->domeLightBRDF,
                VtValue(GfVec4f(0.0)),
                TfTokenVector(),
                HdTextureType::Uv));
    }
}

HdSt_MaterialParamVector const& 
HdStSimpleLightingShader::GetParams() const 
{
    return _lightTextureParams;
}

void
HdStSimpleLightingShader::SetLightingStateFromOpenGL()
{
    _lightingContext->SetStateFromOpenGL();
}

void
HdStSimpleLightingShader::SetLightingState(
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

void
HdStSimpleLightingShader::AllocateTextureHandles(HdSceneDelegate *const delegate)
{
    SdfAssetPath path;

    for (auto const& light : _lightingContext->GetLights()){
        if (light.IsDomeLight()) {
            path = light.GetDomeLightTextureFile();
        }
    }

    const std::string &resolvedPath = path.GetResolvedPath();
    if (resolvedPath.empty()) {
        _domeLightTextureHandle = nullptr;
        return;
    }

    if (_domeLightTextureHandle) {
        HdStTextureIdentifier const &textureId =
            _domeLightTextureHandle->GetTextureObject()->GetTextureIdentifier();
        if (textureId.GetFilePath() != resolvedPath) {
            return;
        }
    }

    HdStResourceRegistry * const resourceRegistry =
        dynamic_cast<HdStResourceRegistry*>(
            delegate->GetRenderIndex().GetResourceRegistry().get());
    if (!TF_VERIFY(resourceRegistry)) {
        return;
    }
        
    const HdStTextureIdentifier textureId(
        TfToken(resolvedPath),
        std::make_unique<HdStUvOrientationSubtextureIdentifier>(
            /* flipVertically = */ true));

    static const HdSamplerParameters samplerParameters{
        HdWrapRepeat, HdWrapRepeat, HdWrapRepeat,
        HdMinFilterLinear, HdMagFilterLinear};

    _domeLightTextureHandle = resourceRegistry->AllocateTextureHandle(
        textureId, 
        HdTextureType::Uv,
        samplerParameters,
        /* targetMemory = */ 0,
        /* createBindlessHandle = */ false,
        shared_from_this());
}

void
HdStSimpleLightingShader::AddResourcesFromTextures(ResourceContext &ctx) const
{
    if (!_domeLightTextureHandle) {
        // No dome lights, bail.
        return;
    }
    
    // Get the GL name of the environment map that
    // was loaded during commit.
    HdStTextureObjectSharedPtr const &textureObject =
        _domeLightTextureHandle->GetTextureObject();
    HdStUvTextureObject const *uvTextureObject =
        dynamic_cast<HdStUvTextureObject*>(
            textureObject.get());
    if (!TF_VERIFY(uvTextureObject)) {
        return;
    }
    HgiTextureHandle const& texture = uvTextureObject->GetTexture();
    if (!TF_VERIFY(texture)) {
        return;
    }
    
    // TEMP - work around for continuous building of textures
    if (_domeLightIrradianceGLSampler.IsSet()) {
        return;
    }

    // Non-const weak pointer of this
    HdStSimpleLightingShaderPtr const thisShader =
        std::dynamic_pointer_cast<HdStSimpleLightingShader>(
            std::const_pointer_cast<HdStShaderCode, const HdStShaderCode>(
                shared_from_this()));

    // Irriadiance map computations.
    ctx.AddComputation(
        nullptr,
        HdSt_DomeLightComputationGPU::New(
            _tokens->domeLightIrradiance,
            texture,
            thisShader));
    
    static const GLuint numPrefilterLevels = 5;

    // Prefilter map computations. mipLevel = 0 allocates texture.
    for (unsigned int mipLevel = 0; mipLevel < numPrefilterLevels; ++mipLevel) {
        const float roughness =
            (float)mipLevel / (float)(numPrefilterLevels - 1);

        ctx.AddComputation(
            nullptr,
            HdSt_DomeLightComputationGPU::New(
                _tokens->domeLightPrefilter, 
                texture,
                thisShader,
                numPrefilterLevels,
                mipLevel,
                roughness));
    }

    // Brdf map computation
    ctx.AddComputation(
        nullptr,
        HdSt_DomeLightComputationGPU::New(
            _tokens->domeLightBRDF,
            texture,
            thisShader));
}

static
void
_CreateSampler(GarchSamplerGPUHandle & samplerName,
               const HgiSamplerAddressMode wrapMode,
               const HgiMipFilter mipFilter)
{
    if (samplerName.IsSet()) {
        return;
    }
    
    bool isOpenGL = HdStResourceFactory::GetInstance()->IsOpenGL();
    if (isOpenGL) {
#if defined(PXR_OPENGL_SUPPORT_ENABLED)
        uint32_t s;
        glGenSamplers(1, &s);
        glSamplerParameteri(s, GL_TEXTURE_WRAP_S, HgiGLConversions::GetSamplerAddressMode(wrapMode));
        glSamplerParameteri(s, GL_TEXTURE_WRAP_T, HgiGLConversions::GetSamplerAddressMode(wrapMode));
        glSamplerParameteri(s, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glSamplerParameteri(s, GL_TEXTURE_MIN_FILTER,
            HgiGLConversions::GetMinFilter(HgiSamplerFilterLinear, mipFilter));
        samplerName = s;
#endif
    }
    else {
#if defined(PXR_METAL_SUPPORT_ENABLED)
        MTLSamplerDescriptor* samplerDescriptor = [[MTLSamplerDescriptor alloc] init];
        samplerDescriptor.sAddressMode = HgiMetalConversions::GetSamplerAddressMode(wrapMode);
        samplerDescriptor.tAddressMode = samplerDescriptor.sAddressMode;
        samplerDescriptor.minFilter = HgiMetalConversions::GetMinMagFilter(HgiSamplerFilterLinear);
        samplerDescriptor.magFilter = samplerDescriptor.minFilter;
        samplerDescriptor.mipFilter = HgiMetalConversions::GetMipFilter(mipFilter);
        
        id<MTLDevice> device = MtlfMetalContext::GetMetalContext()->currentDevice;
        samplerName = [device newSamplerStateWithDescriptor:samplerDescriptor];
#endif
    }
}

void
HdStSimpleLightingShader::_CreateSamplersIfNecessary()
{
    _CreateSampler(_domeLightIrradianceGLSampler,
                   HgiSamplerAddressModeRepeat,
                   HgiMipFilterNotMipmapped);
    _CreateSampler(_domeLightPrefilterGLSampler,
                   HgiSamplerAddressModeRepeat,
                   HgiMipFilterLinear);
    _CreateSampler(_domeLightBrdfGLSampler,
                   HgiSamplerAddressModeClampToEdge,
                   HgiMipFilterNotMipmapped);
}

void
HdStSimpleLightingShader::SetGLTextureName(
    const TfToken &token, const GarchTextureGPUHandle glName)
{
    _CreateSamplersIfNecessary();

    if (token == _tokens->domeLightIrradiance) {
        _domeLightIrradianceGLName = glName;
    } else if (token == _tokens->domeLightPrefilter) {
        _domeLightPrefilterGLName = glName;
    } else if (token == _tokens->domeLightBRDF) {
        _domeLightBrdfGLName = glName;
    } else {
        TF_CODING_ERROR("Unsupported texture token %s", token.GetText());
    }
}

GarchTextureGPUHandle
HdStSimpleLightingShader::GetGLTextureName(const TfToken &token) const
{
    if (token == _tokens->domeLightIrradiance) {
        return _domeLightIrradianceGLName;
    }
    if (token == _tokens->domeLightPrefilter) {
        return _domeLightPrefilterGLName;
    }
    if (token == _tokens->domeLightBRDF) {
        return _domeLightBrdfGLName;
    }
    TF_CODING_ERROR("Unsupported texture token %s", token.GetText());
    return GarchTextureGPUHandle();
}

PXR_NAMESPACE_CLOSE_SCOPE

