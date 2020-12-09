//
// Copyright 2020 Pixar
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
#include "pxr/imaging/garch/glApi.h"

#include "pxr/imaging/hdSt/textureObject.h"

#include "pxr/imaging/hdSt/glfTextureCpuData.h"
#include "pxr/imaging/hdSt/ptexTextureObject.h"
#include "pxr/imaging/hdSt/resourceRegistry.h"
#include "pxr/imaging/hdSt/textureCpuData.h"
#include "pxr/imaging/hdSt/textureObjectRegistry.h"
#include "pxr/imaging/hdSt/subtextureIdentifier.h"
#include "pxr/imaging/hdSt/fieldSubtextureIdentifier.h"
#include "pxr/imaging/hdSt/textureIdentifier.h"
#include "pxr/imaging/hdSt/tokens.h"
#include "pxr/imaging/hdSt/udimTextureObject.h"

#include "pxr/imaging/garch/uvTextureData.h"
#include "pxr/imaging/garch/fieldTextureData.h"
#ifdef PXR_OPENVDB_SUPPORT_ENABLED
#include "pxr/imaging/garch/vdbTextureData.h"
#endif
#include "pxr/imaging/garch/field3DTextureDataBase.h"

#include "pxr/imaging/hgi/hgi.h"
#include "pxr/imaging/hgi/blitCmds.h"

#include "pxr/usd/ar/resolver.h"

PXR_NAMESPACE_OPEN_SCOPE

///////////////////////////////////////////////////////////////////////////////
// HdStTextureObject

HdStTextureObject::HdStTextureObject(
    const HdStTextureIdentifier &textureId,
    HdSt_TextureObjectRegistry * const textureObjectRegistry)
  : _textureObjectRegistry(textureObjectRegistry)
  , _textureId(textureId)
  , _targetMemory(0)
{
}

void
HdStTextureObject::SetTargetMemory(const size_t targetMemory)
{
    if (_targetMemory == targetMemory) {
        return;
    }
    _targetMemory = targetMemory;
    _textureObjectRegistry->MarkTextureObjectDirty(shared_from_this());
}

HdStResourceRegistry*
HdStTextureObject::_GetResourceRegistry() const
{
    if (!TF_VERIFY(_textureObjectRegistry)) {
        return nullptr;
    }

    HdStResourceRegistry* const registry =
        _textureObjectRegistry->GetResourceRegistry();
    TF_VERIFY(registry);

    return registry;
}

Hgi *
HdStTextureObject::_GetHgi() const
{
    HdStResourceRegistry* const registry = _GetResourceRegistry();
    if (!TF_VERIFY(registry)) {
        return nullptr;
    }

    Hgi * const hgi = registry->GetHgi();
    TF_VERIFY(hgi);

    return hgi;
}

void
HdStTextureObject::_AdjustTotalTextureMemory(
    const int64_t memDiff)
{
    if (TF_VERIFY(_textureObjectRegistry)) {
        _textureObjectRegistry->AdjustTotalTextureMemory(memDiff);
    }
}

void
HdStTextureObject::_AddToTotalTextureMemory(
    const HgiTextureHandle &texture)
{
    if (texture) {
        _AdjustTotalTextureMemory(texture->GetByteSizeOfResource());
    }
}

void
HdStTextureObject::_SubtractFromTotalTextureMemory(
    const HgiTextureHandle &texture)
{
    if (texture) {
        _AdjustTotalTextureMemory(-texture->GetByteSizeOfResource());
    }
}

std::string
HdStTextureObject::_GetDebugName(const HdStTextureIdentifier &textureId)
{
    const std::string &filePath = textureId.GetFilePath().GetString();
    const HdStSubtextureIdentifier * const subId =
        textureId.GetSubtextureIdentifier();

    if (!subId) {
        return filePath;
    }

    if (const HdStOpenVDBAssetSubtextureIdentifier * const vdbSubId =
            dynamic_cast<const HdStOpenVDBAssetSubtextureIdentifier*>(subId)) {
        return
            filePath + " - "
            + vdbSubId->GetFieldName().GetString();
    }

    if (const HdStField3DAssetSubtextureIdentifier * const f3dSubId =
            dynamic_cast<const HdStField3DAssetSubtextureIdentifier*>(subId)) {
        return
            filePath + " - "
            + f3dSubId->GetFieldName().GetString() + " "
            + std::to_string(f3dSubId->GetFieldIndex()) + " "
            + f3dSubId->GetFieldPurpose().GetString();
    }

    if (const HdStAssetUvSubtextureIdentifier * const assetUvSubId =
            dynamic_cast<const HdStAssetUvSubtextureIdentifier*>(subId)) {
        return
            filePath
            + " - flipVertically="
            + std::to_string(int(assetUvSubId->GetFlipVertically()))
            + " - premultiplyAlpha="
            + std::to_string(int(assetUvSubId->GetPremultiplyAlpha()))
            + " - sourceColorSpace="
            + assetUvSubId->GetSourceColorSpace().GetString();
    }

    if (const HdStPtexSubtextureIdentifier * const ptexSubId =
            dynamic_cast<const HdStPtexSubtextureIdentifier*>(subId)) {
        return
            filePath
            + " - premultiplyAlpha="
            + std::to_string(int(ptexSubId->GetPremultiplyAlpha()));
    }

    if (const HdStUdimSubtextureIdentifier * const udimSubId =
            dynamic_cast<const HdStUdimSubtextureIdentifier*>(subId)) {
        return
            filePath +
            + " - premultiplyAlpha="
            + std::to_string(int(udimSubId->GetPremultiplyAlpha()))
            + " - sourceColorSpace="
            + udimSubId->GetSourceColorSpace().GetString();
    }
     
    return filePath + " - unknown subtexture identifier";
}

HdStTextureObject::~HdStTextureObject() = default;

///////////////////////////////////////////////////////////////////////////////
// Helpers

namespace {

// Read from the HdStSubtextureIdentifier whether we need
// to pre-multiply the texture by alpha
//
static
bool
_GetPremultiplyAlpha(const HdStSubtextureIdentifier * const subId, 
                     const HdTextureType textureType)
{    
    switch(textureType) {
    case HdTextureType::Uv:
        if (const HdStAssetUvSubtextureIdentifier* const uvSubId = 
            dynamic_cast<const HdStAssetUvSubtextureIdentifier *>(subId)) {
            return uvSubId->GetPremultiplyAlpha();
        } 
        return false;
    default:
        return false;
    }
}

// Read from the HdStSubtextureIdentifier its source color space
//
static
HioImage::SourceColorSpace
_GetSourceColorSpace(const HdStSubtextureIdentifier * const subId,
                   const HdTextureType textureType)
{
    TfToken sourceColorSpace;
    if (const HdStAssetUvSubtextureIdentifier* const uvSubId =
        dynamic_cast<const HdStAssetUvSubtextureIdentifier *>(subId)) {
        sourceColorSpace = uvSubId->GetSourceColorSpace();
    }

    if (sourceColorSpace == HdStTokens->sRGB) {
        return HioImage::SRGB;
    }
    if (sourceColorSpace == HdStTokens->raw) {
        return HioImage::Raw;
    }
    return HioImage::Auto;
}

} // anonymous namespace

///////////////////////////////////////////////////////////////////////////////
// Uv texture

HdStUvTextureObject::HdStUvTextureObject(
    const HdStTextureIdentifier &textureId,
    HdSt_TextureObjectRegistry * textureObjectRegistry)
  : HdStTextureObject(textureId, textureObjectRegistry)
  , _wrapParameters{HdWrapNoOpinion, HdWrapNoOpinion}
{
}


HdTextureType
HdStUvTextureObject::GetTextureType() const
{
    return HdTextureType::Uv;
}

HdStUvTextureObject::~HdStUvTextureObject()
{
    _DestroyTexture();
}

void
HdStUvTextureObject::_SetWrapParameters(
    const std::pair<HdWrap, HdWrap> &wrapParameters)
{
    _wrapParameters = wrapParameters;
}

void
HdStUvTextureObject::_SetCpuData(
    std::unique_ptr<HdStTextureCpuData> &&cpuData)
{
    _cpuData = std::move(cpuData);
}

HdStTextureCpuData *
HdStUvTextureObject::_GetCpuData() const
{
    return _cpuData.get();
}

void
HdStUvTextureObject::_CreateTexture(const HgiTextureDesc &desc)
{
    Hgi * const hgi = _GetHgi();
    if (!TF_VERIFY(hgi)) {
        return;
    }

    _DestroyTexture();
 
    _gpuTexture = hgi->CreateTexture(desc);
    _AddToTotalTextureMemory(_gpuTexture);
}

void
HdStUvTextureObject::_GenerateMipmaps()
{
    HdStResourceRegistry * const registry = _GetResourceRegistry();
    if (!TF_VERIFY(registry)) {
        return;
    }

    if (!_gpuTexture) {
        return;
    }

    HgiBlitCmds* const blitCmds = registry->GetGlobalBlitCmds();
    blitCmds->GenerateMipMaps(_gpuTexture);
}

void
HdStUvTextureObject::_DestroyTexture()
{
    if (Hgi * hgi = _GetHgi()) {
        _SubtractFromTotalTextureMemory(_gpuTexture);
        hgi->DestroyTexture(&_gpuTexture);
    }
}

///////////////////////////////////////////////////////////////////////////////
// Uv asset texture

static
HdWrap
_GetWrapParameter(const bool hasWrapMode, const GLenum wrapMode)
{
    if (hasWrapMode) {
        switch(wrapMode) {
        case GL_CLAMP_TO_EDGE: return HdWrapClamp;
        case GL_REPEAT: return HdWrapRepeat;
        case GL_CLAMP_TO_BORDER: return HdWrapBlack;
        case GL_MIRRORED_REPEAT: return HdWrapMirror;
        //
        // For HioImage legacy plugins that still use the GL_CLAMP
        // (obsoleted in OpenGL 3.0).
        //
        // Note that some graphics drivers produce results for GL_CLAMP
        // that match neither GL_CLAMP_TO_BORDER not GL_CLAMP_TO_EDGE.
        //
        // We pick GL_CLAMP_TO_EDGE here - breaking backwards compatibility.
        //
        case GL_CLAMP: return HdWrapClamp;
        default:
            TF_CODING_ERROR("Unsupported GL wrap mode 0x%04x", wrapMode);
        }
    }

    return HdWrapNoOpinion;
}

static
std::pair<HdWrap, HdWrap>
_GetWrapParameters(GarchUVTextureDataRefPtr const &uvTexture)
{
    if (!uvTexture) {
        return { HdWrapUseMetadata, HdWrapUseMetadata };
    }

    const GarchBaseTextureData::WrapInfo &wrapInfo = uvTexture->GetWrapInfo();

    return { _GetWrapParameter(wrapInfo.hasWrapModeS, wrapInfo.wrapModeS), 
             _GetWrapParameter(wrapInfo.hasWrapModeT, wrapInfo.wrapModeT) };
}

// Read from the HdStAssetUvSubtextureIdentifier whether we need
// to flip the image.
//
// This is to support the legacy HwUvTexture_1 shader node which has the
// vertical orientation opposite to UsdUvTexture.
//
static
HioImage::ImageOriginLocation
_GetImageOriginLocation(const HdStSubtextureIdentifier * const subId)
{
    using SubId = const HdStAssetUvSubtextureIdentifier;
    
    if (SubId* const uvSubId = dynamic_cast<SubId*>(subId)) {
        if (uvSubId->GetFlipVertically()) {
            return HioImage::OriginUpperLeft;
        }
    }
    return HioImage::OriginLowerLeft;
}

HdStAssetUvTextureObject::HdStAssetUvTextureObject(
    const HdStTextureIdentifier &textureId,
    HdSt_TextureObjectRegistry * const textureObjectRegistry)
  : HdStUvTextureObject(textureId, textureObjectRegistry)
{
}

HdStAssetUvTextureObject::~HdStAssetUvTextureObject() = default;

void
HdStAssetUvTextureObject::_Load()
{
    TRACE_FUNCTION();

    GarchUVTextureDataRefPtr const textureData =
        GarchUVTextureData::New(
            GetTextureIdentifier().GetFilePath(),
            GetTargetMemory(),
            /* borders */ 0, 0, 0, 0,
            _GetSourceColorSpace(
                GetTextureIdentifier().GetSubtextureIdentifier(),
                GetTextureType()));

    textureData->Read(
        /* degradeLevel = */ 0,
        /* generateMipmap = */ false,
        _GetImageOriginLocation(
            GetTextureIdentifier().GetSubtextureIdentifier()));

    _SetWrapParameters(_GetWrapParameters(textureData));

    _SetCpuData(
        std::make_unique<HdStGlfTextureCpuData>(
            textureData,
            _GetDebugName(GetTextureIdentifier()),
            /* useOrGenerateMips = */ true,
            _GetPremultiplyAlpha(
                GetTextureIdentifier().GetSubtextureIdentifier(), 
                GetTextureType())));

    if (_GetCpuData()->IsValid()) {
        if (_GetCpuData()->GetTextureDesc().type != HgiTextureType2D) {
            TF_CODING_ERROR("Wrong texture type for uv");
        }
    }
}

void
HdStAssetUvTextureObject::_Commit()
{
    TRACE_FUNCTION();

    _DestroyTexture();

    if (HdStTextureCpuData * const cpuData = _GetCpuData()) {
        if (cpuData->IsValid()) {
            // Upload to GPU
            _CreateTexture(cpuData->GetTextureDesc());
            if (cpuData->GetGenerateMipmaps()) {
                _GenerateMipmaps();
            }
        }
    }

    // Free CPU memory after transfer to GPU
    _SetCpuData(nullptr);
}

bool
HdStAssetUvTextureObject::IsValid() const
{
    return bool(GetTexture());
}

///////////////////////////////////////////////////////////////////////////////
// Field texture

// Compute transform mapping GfRange3d to unit box [0,1]^3
static
GfMatrix4d
_ComputeSamplingTransform(const GfRange3d &range)
{
    const GfVec3d size(range.GetSize());

    const GfVec3d scale(1.0 / size[0], 1.0 / size[1], 1.0 / size[2]);

    return
        // First map range so that min becomes (0,0,0)
        GfMatrix4d(1.0).SetTranslateOnly(-range.GetMin()) *
        // Then scale to unit box
        GfMatrix4d(1.0).SetScale(scale);
}

// Compute transform mapping bounding box to unit box [0,1]^3
static
GfMatrix4d
_ComputeSamplingTransform(const GfBBox3d &bbox)
{
    return
        // First map so that bounding box goes to its GfRange3d
        bbox.GetInverseMatrix() *
        // Then scale to unit box [0,1]^3
        _ComputeSamplingTransform(bbox.GetRange());
}

static
GarchFieldTextureDataRefPtr
_ComputeFieldTexData(
    const HdStTextureIdentifier &textureId,
    const size_t targetMemory)
{
    const std::string &filePath = textureId.GetFilePath().GetString();
    const HdStSubtextureIdentifier * const subId =
        textureId.GetSubtextureIdentifier();

#ifdef PXR_OPENVDB_SUPPORT_ENABLED
    if (const HdStOpenVDBAssetSubtextureIdentifier * const vdbSubId =
            dynamic_cast<const HdStOpenVDBAssetSubtextureIdentifier*>(subId)) {
        if (vdbSubId->GetFieldIndex() != 0) {
            TF_WARN("Support of field index when reading OpenVDB file not yet "
                    "implemented (file: %s, field name: %s, field index: %d",
                    filePath.c_str(),
                    vdbSubId->GetFieldName().GetText(),
                    vdbSubId->GetFieldIndex());
        }
        return GarchVdbTextureData::New(
            filePath, vdbSubId->GetFieldName(), targetMemory);
    }
#endif

    if (const HdStField3DAssetSubtextureIdentifier * const f3dSubId =
            dynamic_cast<const HdStField3DAssetSubtextureIdentifier*>(subId)) {
        GarchField3DTextureDataBaseRefPtr const texData =
            GarchField3DTextureDataBase::New(
                filePath,
                f3dSubId->GetFieldName(),
                f3dSubId->GetFieldIndex(),
                f3dSubId->GetFieldPurpose(),
                targetMemory);
        if (!texData) {
            TF_WARN("Could not find plugin to load Field3D file.");
        }
        return texData;
    }

    TF_CODING_ERROR("Unsupported field subtexture identifier");

    return TfNullPtr;
}


HdStFieldTextureObject::HdStFieldTextureObject(
    const HdStTextureIdentifier &textureId,
    HdSt_TextureObjectRegistry * const textureObjectRegistry)
  : HdStTextureObject(textureId, textureObjectRegistry)
{
}

HdStFieldTextureObject::~HdStFieldTextureObject()
{
    if (Hgi * hgi = _GetHgi()) {
        _SubtractFromTotalTextureMemory(_gpuTexture);
        hgi->DestroyTexture(&_gpuTexture);
    }
}

void
HdStFieldTextureObject::_Load()
{
    TRACE_FUNCTION();

    GarchFieldTextureDataRefPtr const texData = _ComputeFieldTexData(
        GetTextureIdentifier(),
        GetTargetMemory());

    if (!texData) {
        return;
    }

    texData->Read(
        /* degradeLevel = */ 0,
        /* generateMipmap = */ false);

    _cpuData = std::make_unique<HdStGlfTextureCpuData>(
        texData,
        _GetDebugName(GetTextureIdentifier()));

    if (_cpuData->IsValid()) {
        if (_cpuData->GetTextureDesc().type != HgiTextureType3D) {
            TF_CODING_ERROR("Wrong texture type for field");
        }

        _bbox = texData->GetBoundingBox();
        _samplingTransform = _ComputeSamplingTransform(_bbox);
    } else {
        _bbox = GfBBox3d();
        _samplingTransform = GfMatrix4d(1.0);
    }

}

void
HdStFieldTextureObject::_Commit()
{
    TRACE_FUNCTION();

    Hgi * const hgi = _GetHgi();
    if (!hgi) {
        return;
    }
        
    // Free previously allocated texture
    _SubtractFromTotalTextureMemory(_gpuTexture);
    hgi->DestroyTexture(&_gpuTexture);

    // Upload to GPU only if we have valid CPU data
    if (_cpuData && _cpuData->IsValid()) {
        _gpuTexture = hgi->CreateTexture(_cpuData->GetTextureDesc());
        _AddToTotalTextureMemory(_gpuTexture);
    }

    // Free CPU memory after transfer to GPU
    _cpuData.reset();
}

bool
HdStFieldTextureObject::IsValid() const
{
    return bool(_gpuTexture);
}

HdTextureType
HdStFieldTextureObject::GetTextureType() const
{
    return HdTextureType::Field;
}

PXR_NAMESPACE_CLOSE_SCOPE
