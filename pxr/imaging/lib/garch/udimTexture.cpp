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

#include "pxr/imaging/glf/glew.h"

#include "pxr/imaging/garch/udimTexture.h"

#include "pxr/imaging/garch/image.h"
#include "pxr/imaging/garch/resourceFactory.h"

#include "pxr/base/tf/stringUtils.h"

#include "pxr/base/trace/trace.h"
#include "pxr/base/work/loops.h"

PXR_NAMESPACE_OPEN_SCOPE

namespace {

struct _TextureSize {
    _TextureSize(int w, int h) : width(w), height(h) { }
    int width, height;
};

struct _MipDesc {
    _MipDesc(const _TextureSize& s, GarchImageSharedPtr&& i) :
        size(s), image(std::move(i)) { }
    _TextureSize size;
    GarchImageSharedPtr image;
};

using _MipDescArray = std::vector<_MipDesc>;

_MipDescArray _GetMipLevels(const TfToken& filePath)
{
    constexpr int maxMipReads = 32;
    _MipDescArray ret {};
    ret.reserve(maxMipReads);
    int prevWidth = std::numeric_limits<int>::max();
    int prevHeight = std::numeric_limits<int>::max();
    for (int mip = 0; mip < maxMipReads; ++mip) {
        GarchImageSharedPtr image = GarchImage::OpenForReading(filePath, 0, mip);
        if (image == nullptr) {
            break;
        }
        const int currHeight = image->GetWidth();
        const int currWidth = image->GetHeight();
        if (currWidth < prevWidth &&
            currHeight < prevHeight) {
            prevWidth = currWidth;
            prevHeight = currHeight;
            ret.push_back({{currWidth, currHeight}, std::move(image)});
        }
    }
    return ret;
};

}

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
        ret["referenceCount"] = GetRefCount().Get();
    } else {
        ret["memoryUsed"] = size_t{0};
        ret["width"] = 0;
        ret["height"] = 0;
        ret["depth"] = 1;
        ret["format"] = _format;
    }
    ret["referenceCount"] = GetRefCount().Get();
    return ret;
}

void
GarchUdimTexture::_OnMemoryRequestedDirty()
{
    _loaded = false;
}

void
GarchUdimTexture::_ReadTexture()
{
    TF_FATAL_CODING_ERROR("Should not get here!"); //MTL_FIXME
}


PXR_NAMESPACE_CLOSE_SCOPE
