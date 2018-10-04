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
#include "pxr/usdImaging/usdImaging/textureUtils.h"

#include "pxr/usdImaging/usdImaging/debugCodes.h"
#include "pxr/usdImaging/usdImaging/delegate.h"
#include "pxr/usdImaging/usdImaging/tokens.h"

#include "pxr/imaging/garch/glslfx.h"
#include "pxr/imaging/garch/resourceFactory.h"
#include "pxr/imaging/garch/textureHandle.h"
#include "pxr/imaging/garch/textureRegistry.h"

#include "pxr/imaging/hd/material.h"
#include "pxr/imaging/hd/tokens.h"
#include "pxr/imaging/hdSt/textureResource.h"

#include "pxr/base/tf/fileUtils.h"
#include "pxr/base/vt/value.h"

#include "pxr/usd/sdr/registry.h"
#include "pxr/usd/sdr/shaderNode.h"
#include "pxr/usd/sdr/shaderProperty.h"
#include "pxr/usd/usdHydra/tokens.h"
#include "pxr/usd/usdShade/shader.h"

PXR_NAMESPACE_OPEN_SCOPE


static HdWrap _GetWrap(UsdPrim const &usdPrim, const TfToken &wrapAttr)
{
    // The fallback, when the prim has no opinion is to use the metadata on
    // the texture.
    TfToken usdWrap = UsdHydraTokens->useMetadata;
    UsdShadeShader shader(usdPrim);

    if (shader) {
        if (auto wrapInput = shader.GetInput(wrapAttr)) {
            wrapInput.Get(&usdWrap);
        } else {
            // Get the default value from the shader registry if the input is
            // not authored on the shader prim.
            TfToken shaderId;
            shader.GetShaderId(&shaderId);
            if (!shaderId.IsEmpty()) {
                auto &shaderReg = SdrRegistry::GetInstance();
                if (SdrShaderNodeConstPtr sdrNode = 
                    shaderReg.GetShaderNodeByIdentifierAndType(shaderId, 
                                GarchGLSLFXTokens->glslfx)) {
                    if (const auto &sdrInput = 
                            sdrNode->GetShaderInput(wrapAttr)) {
                        VtValue wrapVal = sdrInput->GetDefaultValue();
                        if (wrapVal.IsHolding<TfToken>()) {
                            usdWrap = wrapVal.UncheckedGet<TfToken>();
                        }
                    }
                }
            }
        }
    }

    HdWrap hdWrap;
    if (usdWrap == UsdHydraTokens->clamp) {
        hdWrap = HdWrapClamp;
    } else if (usdWrap == UsdHydraTokens->repeat) {
        hdWrap = HdWrapRepeat;
    } else if (usdWrap == UsdHydraTokens->mirror) {
        hdWrap = HdWrapMirror;
    } else if (usdWrap == UsdHydraTokens->black) {
        hdWrap = HdWrapBlack;
    } else {
        if (usdWrap != UsdHydraTokens->useMetadata) {
            TF_WARN("Unknown wrap mode on prim %s: %s",
                    usdPrim.GetPath().GetText(),
                    usdWrap.GetText());
        }

        hdWrap = HdWrapUseMetadata;

        // For legacy reasons, there is two different behaviors for
        // useMetadata.  The deprecated HwUvTexture_1 shader nodes
        // use the legacy behavior, while new nodes should use the new
        // behavior.
        TfToken id;
        UsdAttribute attr = shader.GetIdAttr();
        if (attr.Get(&id)) {
            if (id == UsdHydraTokens->HwUvTexture_1) {
                hdWrap = HdWrapLegacy;
            }
        }
    }

    return hdWrap;
}

static HdWrap _GetWrapS(UsdPrim const &usdPrim)
{
    return _GetWrap(usdPrim, UsdHydraTokens->wrapS);
}

static HdWrap _GetWrapT(UsdPrim const &usdPrim)
{
    return _GetWrap(usdPrim, UsdHydraTokens->wrapT);
}

static HdMinFilter _GetMinFilter(UsdPrim const &usdPrim)
{
    // XXX: This default value should come from the registry
    TfToken minFilter("linear");
    UsdShadeShader shader(usdPrim);
    if (shader) {
        UsdAttribute attr = shader.GetInput(UsdHydraTokens->minFilter);
        if (attr) attr.Get(&minFilter);
    }
    HdMinFilter minFilterHd = 
                 (minFilter == UsdHydraTokens->nearest) ? HdMinFilterNearest
                 : (minFilter == UsdHydraTokens->nearestMipmapNearest) 
                                ? HdMinFilterNearestMipmapNearest
                 : (minFilter == UsdHydraTokens->nearestMipmapLinear) 
                                ? HdMinFilterNearestMipmapLinear
                 : (minFilter == UsdHydraTokens->linearMipmapNearest) 
                                ? HdMinFilterLinearMipmapNearest
                 : (minFilter == UsdHydraTokens->linearMipmapLinear) 
                                ? HdMinFilterLinearMipmapLinear
                 : HdMinFilterLinear; 
    return minFilterHd;
}

static HdMagFilter _GetMagFilter(UsdPrim const &usdPrim)
{
    // XXX: This default value should come from the registry
    TfToken magFilter("linear");
    UsdShadeShader shader(usdPrim);
    if (shader) {
        UsdAttribute attr = shader.GetInput(UsdHydraTokens->magFilter);
        if (attr) attr.Get(&magFilter);
    }
    HdMagFilter magFilterHd = 
                 (magFilter == UsdHydraTokens->nearest) ? HdMagFilterNearest
                 : HdMagFilterLinear; 
    return magFilterHd;
}

static float _GetMemoryLimit(UsdPrim const &usdPrim)
{
    // XXX: This default value should come from the registry
    float memoryLimit = 0.0f;
    UsdShadeShader shader(usdPrim);
    if (shader) {
        UsdAttribute attr = shader.GetInput(UsdHydraTokens->textureMemory);
        if (attr) attr.Get(&memoryLimit);
    }
    return memoryLimit;
}

static
GarchImage::ImageOriginLocation
UsdImaging_ComputeTextureOrigin(UsdPrim const& usdPrim)
{
    // XXX : This is transitional code. Currently, only textures read
    //       via UsdUVTexture have the origin at the lower left.
    // Extract the id of the node and if it is a UsdUVTexture
    // then we need to use the new coordinate system with (0,0)
    // in the bottom left.
    GarchImage::ImageOriginLocation origin =
        GarchImage::ImageOriginLocation::OriginUpperLeft;
    TfToken id;
    UsdAttribute attr1 = UsdShadeShader(usdPrim).GetIdAttr();
    attr1.Get(&id);
    if (id == UsdImagingTokens->UsdUVTexture) {
        origin = GarchImage::ImageOriginLocation::OriginLowerLeft;
    }

    return origin;
}

HdTextureResource::ID
UsdImaging_GetTextureResourceID(UsdPrim const& usdPrim,
								SdfPath const& usdPath,
                                UsdTimeCode time,
                                size_t salt)
{
    if (!TF_VERIFY(usdPrim))
        return HdTextureResource::ID(-1);
    if (!TF_VERIFY(usdPath != SdfPath()))
        return HdTextureResource::ID(-1);

    // If the texture name attribute doesn't exist, it might be badly specified
    // in scene data.
    UsdAttribute attr = usdPrim.GetAttribute(usdPath.GetNameToken());
    SdfAssetPath asset;
    if (!attr || !attr.Get(&asset, time)) {
        TF_WARN("Unable to find texture attribute <%s> in scene data",
                usdPath.GetText());
        return HdTextureResource::ID(-1);
    }

    TfToken filePath = TfToken(asset.GetResolvedPath());
    // Fallback to the literal path if it couldn't be resolved.
    if (filePath.IsEmpty()) {
        filePath = TfToken(asset.GetAssetPath());
    }

    const bool isPtex = GarchResourceFactory::GetInstance()->IsSupportedPtexTexture(filePath);

    if (asset.GetResolvedPath().empty()) {
        if (isPtex) {
            TF_WARN("Unable to find Texture '%s' with path '%s'. Fallback " 
                    "textures are not supported for ptex", 
                    filePath.GetText(), usdPath.GetText());
            return HdTextureResource::ID(-1);
        } else {
            TF_WARN("Unable to find Texture '%s' with path '%s'. A black " 
                    "texture will be substituted in its place.", 
                    filePath.GetText(), usdPath.GetText());
            return HdTextureResource::ID(-1);
        }
    }

    GarchImage::ImageOriginLocation origin =
            UsdImaging_ComputeTextureOrigin(usdPrim);

    // Hash on the texture filename.
    size_t hash = asset.GetHash();

    // Hash in wrapping and filtering metadata.
    HdWrap wrapS = _GetWrapS(usdPrim);
    HdWrap wrapT = _GetWrapT(usdPrim);
    HdMinFilter minFilter = _GetMinFilter(usdPrim);
    HdMagFilter magFilter = _GetMagFilter(usdPrim);
    float memoryLimit = _GetMemoryLimit(usdPrim);

    boost::hash_combine(hash, origin);
    boost::hash_combine(hash, wrapS);
    boost::hash_combine(hash, wrapT);
    boost::hash_combine(hash, minFilter);
    boost::hash_combine(hash, magFilter);
    boost::hash_combine(hash, memoryLimit);

    // Salt the result to prevent collisions in non-shared imaging.
    // Note that the salt is ignored for fallback texture hashes above.
    boost::hash_combine(hash, salt);

    return HdTextureResource::ID(hash);
}

HdTextureResourceSharedPtr
UsdImaging_GetTextureResource(UsdPrim const& usdPrim,
                                SdfPath const& usdPath,
                                UsdTimeCode time)
{
    if (!TF_VERIFY(usdPrim))
        return HdTextureResourceSharedPtr();
    if (!TF_VERIFY(usdPath != SdfPath()))
        return HdTextureResourceSharedPtr();

    UsdAttribute attr = usdPrim.GetAttribute(usdPath.GetNameToken());
    SdfAssetPath asset;
    if (!TF_VERIFY(attr) || !TF_VERIFY(attr.Get(&asset, time))) {
        return HdTextureResourceSharedPtr();
    }

    TfToken filePath = TfToken(asset.GetResolvedPath());
    // Fallback to the literal path if it couldn't be resolved.
    if (filePath.IsEmpty()) {
        filePath = TfToken(asset.GetAssetPath());
    }

    GarchImage::ImageOriginLocation origin =
            UsdImaging_ComputeTextureOrigin(usdPrim);

    const bool isPtex = GarchResourceFactory::GetInstance()->IsSupportedPtexTexture(filePath);

    HdWrap wrapS = _GetWrapS(usdPrim);
    HdWrap wrapT = _GetWrapT(usdPrim);
    HdMinFilter minFilter = _GetMinFilter(usdPrim);
    HdMagFilter magFilter = _GetMagFilter(usdPrim);
    float memoryLimit = _GetMemoryLimit(usdPrim);

    TF_DEBUG(USDIMAGING_TEXTURES).Msg(
            "Loading texture: id(%s), isPtex(%s)\n",
            usdPath.GetText(),
            isPtex ? "true" : "false");
 
    if (asset.GetResolvedPath().empty()) {
        TF_DEBUG(USDIMAGING_TEXTURES).Msg(
                "File does not exist, returning nullptr");
        TF_WARN("Unable to find Texture '%s' with path '%s'.", 
            filePath.GetText(), usdPath.GetText());
        return HdTextureResourceSharedPtr();
    }

    HdTextureResourceSharedPtr texResource;
    TfStopwatch timer;
    timer.Start();
    GarchTextureHandleRefPtr texture =
        GarchTextureRegistry::GetInstance().GetTextureHandle(filePath, origin);

    texResource = HdTextureResourceSharedPtr(
        HdStSimpleTextureResource::New(texture, isPtex, wrapS, wrapT,
                                       minFilter, magFilter, memoryLimit));
    timer.Stop();

    TF_DEBUG(USDIMAGING_TEXTURES).Msg("    Load time: %.3f s\n", 
                                     timer.GetSeconds());

    return texResource;
}

PXR_NAMESPACE_CLOSE_SCOPE
