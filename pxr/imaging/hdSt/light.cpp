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

#include "pxr/imaging/hdSt/light.h"
#include "pxr/imaging/hdSt/tokens.h"
#include "pxr/imaging/hdSt/textureResource.h"
#include "pxr/imaging/hdSt/resourceFactory.h"
#include "pxr/imaging/hdSt/domeLightComputations.h"
#include "pxr/imaging/hdSt/resourceRegistry.h"

#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/sceneDelegate.h"
#include "pxr/imaging/hd/rprimCollection.h"
#include "pxr/imaging/hd/renderIndex.h"

#include "pxr/base/gf/matrix4d.h"

#include "pxr/imaging/garch/contextCaps.h"
#include "pxr/imaging/garch/resourceFactory.h"

#include "pxr/base/tf/staticTokens.h"

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PRIVATE_TOKENS(
    _tokens,
    (domeLightIrradiance)
    (domeLightPrefilter) 
    (domeLightBRDF)
);

HdStLight::HdStLight(SdfPath const &id, TfToken const &lightType)
    : HdLight(id),
    _lightType(lightType)
{
    _irradianceTexture.Clear();
    _prefilterTexture.Clear();
    _brdfTexture.Clear();

    _irradianceSampler.Clear();
    _prefilterSampler.Clear();
    _brdfSampler.Clear();
}

HdStLight::~HdStLight()
{
    bool isOpenGL = HdStResourceFactory::GetInstance()->IsOpenGL();
#if defined(ARCH_GFX_OPENGL)
    if (isOpenGL) {
        uint32_t t[] = { _irradianceTexture, _prefilterTexture, _brdfTexture };
        glDeleteTextures(sizeof(t) / sizeof(t[0]), t);
    }
#endif
#if defined(ARCH_GFX_METAL)
    if (!isOpenGL) {
        MtlfMultiTexture mt = _irradianceTexture;
        mt.release();
        _irradianceTexture = mt;
        
        mt = _prefilterTexture;
        mt.release();
        _prefilterTexture = mt;
        
        mt = _brdfTexture;
        mt.release();
        _brdfTexture = mt;

        MtlfMultiSampler ms = _irradianceSampler;
        ms.release();
        _irradianceSampler = ms;
        
        ms = _prefilterSampler;
        ms.release();
        _prefilterSampler = ms;
        
        ms = _brdfSampler;
        ms.release();
        _brdfSampler = ms;
    }
#endif
}

GarchSimpleLight
HdStLight::_ApproximateAreaLight(SdfPath const &id, 
                                 HdSceneDelegate *sceneDelegate)
{
    // Get the color of the light
    GfVec3f hdc = sceneDelegate->GetLightParamValue(id, HdStLightTokens->color)
            .Get<GfVec3f>();

    // Extract intensity
    float intensity = 
        sceneDelegate->GetLightParamValue(id, HdLightTokens->intensity)
            .Get<float>();

    // Extract the exposure of the light
    float exposure = 
        sceneDelegate->GetLightParamValue(id, HdLightTokens->exposure)
            .Get<float>();
    intensity *= powf(2.0f, GfClamp(exposure, -50.0f, 50.0f));

    // Calculate the final color of the light
    GfVec4f c(hdc[0]*intensity, hdc[1]*intensity, hdc[2]*intensity, 1.0f); 

    // Get the transform of the light
    GfMatrix4d transform = _params[HdTokens->transform].Get<GfMatrix4d>();
    GfVec3d hdp = transform.ExtractTranslation();
    GfVec4f p = GfVec4f(hdp[0], hdp[1], hdp[2], 1.0f);

    // Create the Glf Simple Light object that will be used by the rest
    // of the pipeline. No support for shadows for this translated light.
    GarchSimpleLight l;
    l.SetPosition(p);
    l.SetDiffuse(c);
    l.SetHasShadow(false);
    return l;
}

GarchSimpleLight
HdStLight::_PrepareDomeLight(
    SdfPath const &id, 
    HdSceneDelegate *sceneDelegate)
{
    // get/load the environment map texture resource
    GarchTextureGPUHandle textureId;
    VtValue textureResourceValue = sceneDelegate->GetLightParamValue(id, 
                                            HdLightTokens->textureResource);
        
    TF_VERIFY(textureResourceValue.IsHolding<HdTextureResourceSharedPtr>());
    if (textureResourceValue.IsHolding<HdTextureResourceSharedPtr>()) {
        
        _textureResource = boost::dynamic_pointer_cast<HdStTextureResource>(
                    textureResourceValue.Get<HdTextureResourceSharedPtr>());

        // texture resource would be empty if the path could not be resolved
        if (_textureResource) {

            // Use the texture resource (environment map) to pre-compute 
            // the necessary maps (irradiance, pre-filtered, BRDF LUT)
            textureId = _textureResource->GetTexelsTextureId();

            HdRenderIndex& index = sceneDelegate->GetRenderIndex();
            HdStResourceRegistry* hdStResourceRegistry =
                static_cast<HdStResourceRegistry*>(
                    index.GetResourceRegistry().get());

            // Schedule texture computations
            _SetupComputations(textureId, hdStResourceRegistry);
        }
    } 

    VtValue transform = sceneDelegate->GetLightParamValue(
                                                id, HdTokens->transform);
    
    // Create the Garch Simple Light object that will be used by the rest
    // of the pipeline. No support for shadows for dome light.
    GarchSimpleLight l;
    l.SetHasShadow(false);
    l.SetIsDomeLight(true);
    l.SetIrradianceId(_irradianceTexture);
    l.SetPrefilterId(_prefilterTexture);
    l.SetBrdfId(_brdfTexture);
    l.SetIrradianceSamplerId(_irradianceSampler);
    l.SetPrefilterSamplerId(_prefilterSampler);
    l.SetBrdfSamplerId(_brdfSampler);
    if (transform.IsHolding<GfMatrix4d>()) {
        l.SetTransform(transform.UncheckedGet<GfMatrix4d>());
    }
    return l;
}

void 
HdStLight::_SetupComputations(
    GarchTextureGPUHandle const &sourceTexture, 
    HdStResourceRegistry *resourceRegistry)
{
    bool isOpenGL = HdStResourceFactory::GetInstance()->IsOpenGL();
    GarchContextCaps const &caps =
        GarchResourceFactory::GetInstance()->GetContextCaps();
    // verify that the GL version supports compute shaders
    if (isOpenGL && caps.apiVersion < 430) {
        TF_WARN("Need OpenGL version 4.30 or higher to use DomeLight");
        return;
    }
    
    // get the width and height of the source texture
    int textureWidth = 0, textureHeight = 0;
    if (isOpenGL) {
#if defined(ARCH_GFX_OPENGL)
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sourceTexture);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &textureWidth);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT,
                                &textureHeight);
#endif
    }
    else {
#if defined(ARCH_GFX_METAL)
        id<MTLTexture> tex = sourceTexture.multiTexture.forCurrentGPU();
        textureWidth = [tex width];
        textureHeight = [tex height];
#endif
    }
    
    if (!textureWidth || !textureHeight) {
        TF_WARN("DomeLight source texture has a zero dimension");
        return;
    }

    // initialize the 3 textures and add computations to the resource registry
    GLuint numLevels = 1, numPrefilterLevels = 5, level = 0;
    // make the computed textures half the size of the given environment map
    textureHeight = textureHeight/2;
    textureWidth = textureWidth/2;

    // Diffuse Irradiance
    if (isOpenGL) {
#if defined(ARCH_GFX_OPENGL)
        uint32_t t;
        glGenTextures(1, &t);
        _irradianceTexture = t;
        glBindTexture(GL_TEXTURE_2D, _irradianceTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexStorage2D(GL_TEXTURE_2D, numLevels, GL_RGBA16F, textureWidth,
                        textureHeight);
#endif
    }
    else
    {
#if defined(ARCH_GFX_METAL)
        id<MTLDevice> device = MtlfMetalContext::GetMetalContext()->currentDevice;
        MTLTextureDescriptor* desc =
            [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA16Float
                                                               width:textureWidth
                                                              height:textureHeight
                                                           mipmapped:NO];
        desc.resourceOptions = MTLResourceStorageModeDefault;
        desc.usage = MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite;
        _irradianceTexture = MtlfMultiTexture(desc);
        
        MTLSamplerDescriptor* samplerDescriptor = [[MTLSamplerDescriptor alloc] init];
        samplerDescriptor.sAddressMode = MTLSamplerAddressModeRepeat;
        samplerDescriptor.tAddressMode = MTLSamplerAddressModeRepeat;
        samplerDescriptor.minFilter = MTLSamplerMinMagFilterLinear;
        samplerDescriptor.magFilter = MTLSamplerMinMagFilterLinear;
        _irradianceSampler = MtlfMultiSampler(samplerDescriptor);
#endif
    }

    // Add Computation 
    HdSt_DomeLightComputationGPUSharedPtr irradianceComputation(
        HdSt_DomeLightComputationGPU::New(_tokens->domeLightIrradiance, 
            sourceTexture, _irradianceTexture, textureWidth, textureHeight,
            numLevels, level));
    resourceRegistry->AddComputation(nullptr, irradianceComputation);

    // PreFilter
    if (isOpenGL) {
#if defined(ARCH_GFX_OPENGL)
        uint32_t t;
        glGenTextures(1, &t);
        _prefilterTexture = t;
        glBindTexture(GL_TEXTURE_2D, _prefilterTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                        GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexStorage2D(GL_TEXTURE_2D, numPrefilterLevels, GL_RGBA16F,
                        textureWidth, textureHeight);
#endif
    }
    else
    {
#if defined(ARCH_GFX_METAL)
        id<MTLDevice> device = MtlfMetalContext::GetMetalContext()->currentDevice;
        MTLTextureDescriptor* desc =
            [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA16Float
                                                               width:textureWidth
                                                              height:textureHeight
                                                           mipmapped:YES];
        desc.resourceOptions = MTLResourceStorageModeDefault;
        desc.mipmapLevelCount = numPrefilterLevels;
        desc.usage = MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite;
        _prefilterTexture = MtlfMultiTexture(desc);
        
        MTLSamplerDescriptor* samplerDescriptor = [[MTLSamplerDescriptor alloc] init];
        samplerDescriptor.sAddressMode = MTLSamplerAddressModeRepeat;
        samplerDescriptor.tAddressMode = MTLSamplerAddressModeRepeat;
        samplerDescriptor.minFilter = MTLSamplerMinMagFilterLinear;
        samplerDescriptor.magFilter = MTLSamplerMinMagFilterLinear;
        samplerDescriptor.mipFilter = MTLSamplerMipFilterLinear;
        _prefilterSampler = MtlfMultiSampler(samplerDescriptor);
#endif
    }

    if (caps.hasMipLevelTextureWrite) {
        // Add Computation for each of the mipLevels
        for (unsigned int mipLevel = 0; mipLevel < numPrefilterLevels; ++mipLevel) {

            float roughness = (float)mipLevel / (float)(numPrefilterLevels - 1);
            HdSt_DomeLightComputationGPUSharedPtr preFilterComputation(
                    HdSt_DomeLightComputationGPU::New(_tokens->domeLightPrefilter,
                    sourceTexture, _prefilterTexture, textureWidth, textureHeight,
                    numPrefilterLevels, mipLevel, roughness));
            hdStResourceRegistry->AddComputation(nullptr, preFilterComputation);
        }
    }
    else {
        float roughness = 0.0f;
        HdSt_DomeLightComputationGPUSharedPtr preFilterComputation(
                HdSt_DomeLightComputationGPU::New(_tokens->domeLightPrefilter,
                sourceTexture, _prefilterTexture, textureWidth, textureHeight,
                numPrefilterLevels, 0, roughness));
        resourceRegistry->AddComputation(nullptr, preFilterComputation);
    }

    // BRDF LUT
    if (isOpenGL) {
#if defined(ARCH_GFX_OPENGL)
        uint32_t t;
        glGenTextures(1, &t);
        _brdfTexture = t;
        glBindTexture(GL_TEXTURE_2D, _brdfTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexStorage2D(GL_TEXTURE_2D, numLevels, GL_RGBA16F,
                        textureHeight, textureHeight);
#endif
    }
    else
    {
#if defined(ARCH_GFX_METAL)
        id<MTLDevice> device = MtlfMetalContext::GetMetalContext()->currentDevice;
        MTLTextureDescriptor* desc =
            [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA16Float
                                                               width:textureHeight
                                                              height:textureHeight
                                                           mipmapped:NO];
        desc.resourceOptions = MTLResourceStorageModeDefault;
        desc.usage = MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite;
        _brdfTexture = MtlfMultiTexture(desc);
        
        MTLSamplerDescriptor* samplerDescriptor = [[MTLSamplerDescriptor alloc] init];
        samplerDescriptor.sAddressMode = MTLSamplerAddressModeClampToEdge;
        samplerDescriptor.tAddressMode = MTLSamplerAddressModeClampToEdge;
        samplerDescriptor.minFilter = MTLSamplerMinMagFilterLinear;
        samplerDescriptor.magFilter = MTLSamplerMinMagFilterLinear;
        _brdfSampler = MtlfMultiSampler(samplerDescriptor);
#endif
    }

    // Add Computation 
    HdSt_DomeLightComputationGPUSharedPtr brdfComputation(
            HdSt_DomeLightComputationGPU::New(_tokens->domeLightBRDF, 
            sourceTexture, _brdfTexture, textureHeight, textureHeight, 
            numLevels, level));
    resourceRegistry->AddComputation(nullptr, brdfComputation);
}

/* virtual */
void
HdStLight::Sync(HdSceneDelegate *sceneDelegate,
                HdRenderParam   *renderParam,
                HdDirtyBits     *dirtyBits)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    TF_UNUSED(renderParam);

    SdfPath const &id = GetId();

    if (!TF_VERIFY(sceneDelegate != nullptr)) {
        return;
    }

    // HdStLight communicates to the scene graph and caches all interesting
    // values within this class. Later on Get() is called from 
    // TaskState (RenderPass) to perform aggregation/pre-computation, 
    // in order to make the shader execution efficient.

    // Change tracking
    HdDirtyBits bits = *dirtyBits;

    // Transform
    if (bits & DirtyTransform) {
        VtValue transform = sceneDelegate->Get(id, HdLightTokens->transform);
        if (transform.IsHolding<GfMatrix4d>()) {
            _params[HdLightTokens->transform] = transform;
        } else {
            _params[HdLightTokens->transform] = GfMatrix4d(1);
        }
    }

    // Lighting Params
    if (bits & DirtyParams) {
        if (_lightType == HdPrimTypeTokens->simpleLight) {
            _params[HdLightTokens->params] =
                sceneDelegate->Get(id, HdLightTokens->params);
        }
        else if (_lightType == HdPrimTypeTokens->domeLight) {
            _params[HdLightTokens->params] = 
                _PrepareDomeLight(id, sceneDelegate);
        }
        // If it is an area light we will extract the parameters and convert
        // them to a gl friendly representation. 
        else {
            _params[HdLightTokens->params] =
                _ApproximateAreaLight(id, sceneDelegate);
        }
    }

    // Shadow Params
    if (bits & DirtyShadowParams) {
        _params[HdLightTokens->shadowParams] =
                sceneDelegate->Get(id, HdLightTokens->shadowParams);
    }

    // Shadow Collection
    if (bits & DirtyCollection) {
        VtValue vtShadowCollection =
                sceneDelegate->Get(id, HdLightTokens->shadowCollection);

        // Optional
        if (vtShadowCollection.IsHolding<HdRprimCollection>()) {
            HdRprimCollection newCollection =
                vtShadowCollection.UncheckedGet<HdRprimCollection>();

            if (_params[HdLightTokens->shadowCollection] != newCollection) {
                _params[HdLightTokens->shadowCollection] = newCollection;

                HdChangeTracker& changeTracker =
                             sceneDelegate->GetRenderIndex().GetChangeTracker();

                changeTracker.MarkCollectionDirty(newCollection.GetName());
            }

        } else {
            _params[HdLightTokens->shadowCollection] = HdRprimCollection();
        }
    }

    *dirtyBits = Clean;
}

VtValue
HdStLight::Get(TfToken const &token) const
{
    VtValue val;
    TfMapLookup(_params, token, &val);
    return val;
}

/* virtual */
HdDirtyBits
HdStLight::GetInitialDirtyBitsMask() const
{
    // In the case of regular lights we want to sync all dirty bits, but
    // for area lights coming from the scenegraph we just want to extract
    // the Transform and Params for now.
    if (_lightType == HdPrimTypeTokens->simpleLight) {
        return AllDirty;
    } else {
        return (DirtyParams | DirtyTransform);
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
