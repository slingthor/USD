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
#ifndef PXR_IMAGING_HD_ST_PTEX_TEXTURE_OBJECT_H
#define PXR_IMAGING_HD_ST_PTEX_TEXTURE_OBJECT_H

#include "pxr/pxr.h"
#include "pxr/imaging/hdSt/api.h"
#include "pxr/imaging/hdSt/textureObject.h"

#include "pxr/imaging/hgi/handle.h"

#include "pxr/base/gf/vec3i.h"

#ifdef PXR_PTEX_SUPPORT_ENABLED
#include "pxr/imaging/hdSt/ptexMipmapTextureLoader.h"
#endif

PXR_NAMESPACE_OPEN_SCOPE

/// Returns true if the file given by \p imageFilePath represents a ptex file,
/// and false otherwise.
///
/// This function simply checks the extension of the file name and does not
/// otherwise guarantee that the file is in any way valid for reading.
///
/// If ptex support is disabled, this function will always return false.
///
HDST_API bool HdStIsSupportedPtexTexture(std::string const & imageFilePath);

enum HgiFormat : int;
using HgiBufferHandle = HgiHandle<class HgiBuffer>;
using HgiTextureHandle = HgiHandle<class HgiTexture>;
using HgiTextureBufferHandle = HgiHandle<class HgiTextureBuffer>;

/// \class HdStPtexTextureObject
///
/// A ptex texture - it is using Garch to both load the texture
/// and allocate the GPU resources (unlike the other texture
/// types).
///
class HdStPtexTextureObject final : public HdStTextureObject
{
public:
    HDST_API
    HdStPtexTextureObject(
        const HdStTextureIdentifier &textureId,
        HdSt_TextureObjectRegistry *textureObjectRegistry);

    HDST_API
    ~HdStPtexTextureObject() override;

    /// Get the GPU texture handle for the texels
    ///
    /// Only valid after commit phase.
    ///
    HgiTextureHandle GetTexelTexture() const { return _texelTexture; }

    /// Get the GPU texture handle for the layout
    ///
    /// Only valid after commit phase.
    ///
    HDST_API
    HgiTextureHandle GetLayoutTexture() const;

    HDST_API
    bool IsValid() const override;

    HDST_API
    HdTextureType GetTextureType() const override;

protected:
    HDST_API
    void _Load() override;

    HDST_API
    void _Commit() override;

private:

    HgiFormat _format;
    size_t _numChannels;
    size_t _numBytesPerPixel;
    GfVec3i _dimensions;
    size_t _numFaces;

    std::unique_ptr<uint8_t[]> _texelData;
    std::unique_ptr<uint16_t[]> _layoutData;

    HgiTextureHandle _texelTexture;
    HgiBufferHandle _layoutBuffer;
    HgiTextureBufferHandle _layoutTexture;

    void _DestroyTextures();
};

template<>
struct HdSt_TypedTextureObjectHelper<HdTextureType::Ptex> {
    using type = HdStPtexTextureObject;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif
