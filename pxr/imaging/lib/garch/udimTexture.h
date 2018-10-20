//
// Copyright 2018 Pixar
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
#ifndef GARCH_UDIMTEXTURE_H
#define GARCH_UDIMTEXTURE_H

/// \file Garch/udimTexture.h

#include "pxr/pxr.h"
#include "pxr/imaging/garch/api.h"

#include "pxr/imaging/garch/texture.h"

#include <string>
#include <vector>
#include <tuple>

PXR_NAMESPACE_OPEN_SCOPE

/// Returns true if the file given by \p imageFilePath represents a udim file,
/// and false otherwise.
///
/// This function simply checks the existence of the <udim> tag in the
/// file name and does not otherwise guarantee that
/// the file is in any way valid for reading.
///
GARCH_API bool GarchIsSupportedUdimTexture(std::string const& imageFilePath);

class GarchUdimTexture;
TF_DECLARE_WEAK_AND_REF_PTRS(GarchUdimTexture);

class GarchUdimTexture : public GarchTexture {
public:
    struct _TextureSize {
        _TextureSize(unsigned int w, unsigned int h) : width(w), height(h) { }
        unsigned int width;
        unsigned int height;
    };
    
    struct _MipDesc {
        _MipDesc(const _TextureSize& s, GarchImageSharedPtr&& i) :
        size(s), image(std::move(i)) { }
        _TextureSize size;
        GarchImageSharedPtr image;
    };
    
    using _MipDescArray = std::vector<_MipDesc>;
    
    _MipDescArray _GetMipLevels(const TfToken& filePath);

    GARCH_API
    virtual ~GarchUdimTexture();

    GARCH_API
    static GarchUdimTextureRefPtr New(
        TfToken const& imageFilePath,
        GarchImage::ImageOriginLocation originLocation,
        std::vector<std::tuple<int, TfToken>>&& tiles);

    GARCH_API
    GarchTexture::BindingVector GetBindings(
        TfToken const& identifier,
        GarchSamplerGPUHandle samplerId) override;

    GARCH_API
    VtDictionary GetTextureInfo(bool forceLoad) override;

    GARCH_API
    virtual GarchTextureGPUHandle GetTextureName() override {
        _ReadImage();
        return _imageArray;
    }

    GARCH_API
    virtual GarchTextureGPUHandle GetLayoutName() {
        _ReadImage();
        return _layout;
    }
    
protected:
    GARCH_API
    GarchUdimTexture(
        TfToken const& imageFilePath,
        GarchImage::ImageOriginLocation originLocation,
        std::vector<std::tuple<int, TfToken>>&& tiles);

    GARCH_API
    virtual void _ReadTexture() override;

    GARCH_API
    virtual void _ReadImage();

    GARCH_API
    virtual void _FreeTextureObject() = 0;

    GARCH_API
    virtual void _CreateGPUResources(unsigned int numChannels,
                                     GLenum const type,
                                     std::vector<_TextureSize> &mips,
                                     std::vector<std::vector<uint8_t>> &mipData,
                                     std::vector<float> &layoutData) = 0;

    GARCH_API
    virtual void _OnMemoryRequestedDirty() override;

    std::vector<std::tuple<int, TfToken>> _tiles;
    unsigned int _width = 0;
    unsigned int _height = 0;
    unsigned int _depth = 0;
    unsigned int _format = 0;
    GarchTextureGPUHandle _imageArray;
    GarchTextureGPUHandle _layout;
    bool _loaded = false;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // GARCH_UDIMTEXTURE_H
