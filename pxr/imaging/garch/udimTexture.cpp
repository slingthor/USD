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
/// \file garch/udimTexture.cpp
#include "pxr/imaging/garch/udimTexture.h"

#include "pxr/imaging/garch/image.h"
#include "pxr/imaging/garch/resourceFactory.h"

#include "pxr/base/tf/stringUtils.h"

#include "pxr/base/trace/trace.h"
#include "pxr/base/work/loops.h"

PXR_NAMESPACE_OPEN_SCOPE

GarchUdimTexture::_MipDescArray GarchUdimTexture::_GetMipLevels(const TfToken& filePath)
{
    constexpr int maxMipReads = 32;
    _MipDescArray ret {};
    ret.reserve(maxMipReads);
    unsigned int prevWidth = std::numeric_limits<unsigned int>::max();
    unsigned int prevHeight = std::numeric_limits<unsigned int>::max();
    for (int mip = 0; mip < maxMipReads; ++mip) {
        GarchImageSharedPtr image = GarchImage::OpenForReading(filePath, 0, mip);
        if (image == nullptr) {
            break;
        }
        const unsigned int currHeight = image->GetWidth();
        const unsigned int currWidth = image->GetHeight();
        if (currWidth < prevWidth &&
            currHeight < prevHeight) {
            prevWidth = currWidth;
            prevHeight = currHeight;
            ret.push_back({{currWidth, currHeight}, std::move(image)});
        }
    }
    return ret;
};

bool GarchIsSupportedUdimTexture(std::string const& imageFilePath)
{
    return TfStringContains(imageFilePath, "<UDIM>");
}

TF_REGISTRY_FUNCTION(TfType)
{
    TfType::Define<GarchUdimTexture, TfType::Bases<GarchTexture>>();
}

GarchUdimTexture::GarchUdimTexture(
    TfToken const& imageFilePath,
    GarchImage::ImageOriginLocation originLocation,
    std::vector<std::tuple<int, TfToken>>&& tiles)
    : GarchTexture(originLocation), _tiles(std::move(tiles))
{
}

GarchUdimTexture::~GarchUdimTexture()
{
}

GarchUdimTextureRefPtr
GarchUdimTexture::New(
    TfToken const& imageFilePath,
    GarchImage::ImageOriginLocation originLocation,
    std::vector<std::tuple<int, TfToken>>&& tiles)
{
    return GarchResourceFactory::GetInstance()->NewUdimTexture(imageFilePath, originLocation, std::move(tiles));
}

GarchTexture::BindingVector
GarchUdimTexture::GetBindings(
    TfToken const& identifier,
    GarchSamplerGPUHandle samplerId)
{
    _ReadImage();
    BindingVector ret;
    ret.push_back(Binding(
        TfToken(identifier.GetString() + "_Images"), GarchTextureTokens->texels,
            GL_TEXTURE_2D_ARRAY, _imageArray, samplerId));
    ret.push_back(Binding(
        TfToken(identifier.GetString() + "_Layout"), GarchTextureTokens->layout,
            GL_TEXTURE_1D, _layout, GarchSamplerGPUHandle()));

    return ret;
}

VtDictionary
GarchUdimTexture::GetTextureInfo(bool forceLoad)
{
    VtDictionary ret;

    if (forceLoad) {
        _ReadImage();
    }

    if (_loaded) {
        ret["memoryUsed"] = GetMemoryUsed();
        ret["width"] = _width;
        ret["height"] = _height;
        ret["depth"] = _depth;
        ret["format"] = _format;
        if (!_tiles.empty()) {
            ret["imageFilePath"] = std::get<1>(_tiles.front());
        }
    } else {
        ret["memoryUsed"] = size_t{0};
        ret["width"] = 0;
        ret["height"] = 0;
        ret["depth"] = 1;
        ret["format"] = _format;
    }
    ret["referenceCount"] = GetCurrentCount();
    return ret;
}

void
GarchUdimTexture::_OnMemoryRequestedDirty()
{
    _loaded = false;
}

void
GarchUdimTexture::_ReadImage()
{
    TRACE_FUNCTION();
    
    if (_loaded) {
        return;
    }
    _loaded = true;
    _FreeTextureObject();
    
    if (_tiles.empty()) {
        return;
    }
    
    const _MipDescArray firstImageMips = _GetMipLevels(std::get<1>(_tiles[0]));
    
    if (firstImageMips.empty()) {
        return;
    }
    
    _format = firstImageMips[0].image->GetFormat();
    const GLenum type = firstImageMips[0].image->GetType();
    unsigned int numChannels;
    if (_format == GL_RED || _format == GL_LUMINANCE) {
        numChannels = 1;
    } else if (_format == GL_RG) {
        numChannels = 2;
    } else if (_format == GL_RGB) {
        numChannels = 3;
    } else if (_format == GL_RGBA) {
        numChannels = 4;
    } else {
        return;
    }
    
    unsigned int sizePerElem = 1;
    if (type == GL_FLOAT) {
        sizePerElem = 4;
    } else if (type == GL_UNSIGNED_SHORT) {
        sizePerElem = 2;
    } else if (type == GL_HALF_FLOAT_ARB) {
        sizePerElem = 2;
    } else if (type == GL_UNSIGNED_BYTE) {
        sizePerElem = 1;
    }
    
    const unsigned int maxTileCount =
    std::get<0>(_tiles.back()) + 1;
    _depth = static_cast<int>(_tiles.size());
    const unsigned int numBytesPerPixel = sizePerElem * numChannels;
    const unsigned int numBytesPerPixelLayer = numBytesPerPixel * _depth;
    
    unsigned int targetPixelCount =
    static_cast<unsigned int>(GetMemoryRequested());
    const bool loadAllTiles = targetPixelCount == 0;
    targetPixelCount /= _depth * numBytesPerPixel;
    
    std::vector<_TextureSize> mips {};
    mips.reserve(firstImageMips.size());
    if (firstImageMips.size() == 1) {
        unsigned int width = firstImageMips[0].size.width;
        unsigned int height = firstImageMips[0].size.height;
        while (true) {
            mips.emplace_back(width, height);
            if (width == 1 && height == 1) {
                break;
            }
            width = std::max(1u, width / 2u);
            height = std::max(1u, height / 2u);
        }
        if (!loadAllTiles) {
            std::reverse(mips.begin(), mips.end());
        }
    } else {
        if (loadAllTiles) {
            for (_MipDesc const& mip: firstImageMips) {
                mips.emplace_back(mip.size);
            }
        } else {
            for (auto it = firstImageMips.crbegin();
                 it != firstImageMips.crend(); ++it) {
                mips.emplace_back(it->size);
            }
        }
    }
    
    unsigned int mipCount = mips.size();
    if (!loadAllTiles) {
        mipCount = 0;
        for (auto const& mip: mips) {
            const unsigned int currentPixelCount = mip.width * mip.height;
            if (targetPixelCount <= currentPixelCount) {
                break;
            }
            ++mipCount;
            targetPixelCount -= currentPixelCount;
        }
        
        if (mipCount == 0) {
            mips.clear();
            mips.emplace_back(1, 1);
            mipCount = 1;
        } else {
            mips.resize(mipCount, {0, 0});
            std::reverse(mips.begin(), mips.end());
        }
    }
    
    std::vector<std::vector<uint8_t>> mipData;
    mipData.resize(mipCount);
    
    _width = mips[0].width;
    _height = mips[0].height;
    
    // Texture array queries will use a float as the array specifier.
    std::vector<float> layoutData;
    layoutData.resize(maxTileCount, 0);

    size_t totalTextureMemory = 0;
    for (unsigned int mip = 0; mip < mipCount; ++mip) {
        _TextureSize const& mipSize = mips[mip];
        const unsigned int currentMipMemory =
        mipSize.width * mipSize.height * numBytesPerPixelLayer;
        mipData[mip].resize(currentMipMemory, 0);
        totalTextureMemory += currentMipMemory;
    }
    
    WorkParallelForN(_tiles.size(), [&](size_t begin, size_t end) {
        for (size_t tileId = begin; tileId < end; ++tileId) {
            std::tuple<int, TfToken> const& tile = _tiles[tileId];
            layoutData[std::get<0>(tile)] = tileId + 1;
            _MipDescArray images = _GetMipLevels(std::get<1>(tile));
            if (images.empty()) { continue; }
            for (unsigned int mip = 0; mip < mipCount; ++mip) {
                _TextureSize const& mipSize = mips[mip];
                const unsigned int numBytesPerLayer =
                mipSize.width * mipSize.height * numBytesPerPixel;
                GarchImage::StorageSpec spec;
                spec.width = mipSize.width;
                spec.height = mipSize.height;
                spec.format = _format;
                spec.type = type;
                spec.flipped = true;
                spec.data = mipData[mip].data()
                + (tileId * numBytesPerLayer);
                const auto it = std::find_if(images.rbegin(), images.rend(),
                                             [&mipSize](_MipDesc const& i)
                                             { return mipSize.width <= i.size.width &&
                                                 mipSize.height <= i.size.height;});
                (it == images.rend() ? images.front() : *it).image->Read(spec);
            }
        }
    }, 1);
    
    _CreateGPUResources(numChannels, type, mips, mipData, layoutData);
    
    _SetMemoryUsed(totalTextureMemory + _tiles.size() * sizeof(float));
}
void
GarchUdimTexture::_ReadTexture()
{
    TF_FATAL_CODING_ERROR("Should not get here!"); //MTL_FIXME
}


PXR_NAMESPACE_CLOSE_SCOPE
