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

#include "pxr/base/gf/math.h"
#include "pxr/base/gf/vec3i.h"

#include "pxr/imaging/hio/image.h"
#include "pxr/imaging/garch/resourceFactory.h"

#include "pxr/base/tf/stringUtils.h"

#include "pxr/base/trace/trace.h"
#include "pxr/base/work/loops.h"

PXR_NAMESPACE_OPEN_SCOPE


GarchUdimTexture::_MipDescArray GarchUdimTexture::_GetMipLevels(const TfToken& filePath,
                                                                HioImage::SourceColorSpace sourceColorSpace)
{
    constexpr int maxMipReads = 32;
    _MipDescArray ret {};
    ret.reserve(maxMipReads);
    unsigned int prevWidth = std::numeric_limits<unsigned int>::max();
    unsigned int prevHeight = std::numeric_limits<unsigned int>::max();
    for (unsigned int mip = 0; mip < maxMipReads; ++mip) {
        HioImageSharedPtr image = HioImage::OpenForReading(filePath, 0, mip, 
                                                           sourceColorSpace);
        if (image == nullptr) {
            break;
        }
        const unsigned int currHeight = std::max(1, image->GetWidth());
        const unsigned int currWidth = std::max(1, image->GetHeight());
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
    HioImage::ImageOriginLocation originLocation,
    std::vector<std::tuple<int, TfToken>>&& tiles,
    bool const premultiplyAlpha,
    HioImage::SourceColorSpace sourceColorSpace) // APPLE METAL: HioImage
    : GarchTexture(originLocation), _tiles(std::move(tiles)), 
      _premultiplyAlpha(premultiplyAlpha), _sourceColorSpace(sourceColorSpace)
{
}

GarchUdimTexture::~GarchUdimTexture()
{
}

GarchTexture::BindingVector
GarchUdimTexture::GetBindings(
    TfToken const& identifier,
    GarchSamplerGPUHandle const & samplerId)
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

// XXX: This code is duplicated in hdSt/textureObject.cpp, but will hopefully
// be removed from this file when Storm begins using Hgi for UDIM textures
namespace {
enum _ColorSpaceTransform
{
     _SRGBToLinear,
     _LinearToSRGB
};

// Convert a [0, 1] value between color spaces
template<_ColorSpaceTransform colorSpaceTransform>
static
float _ConvertColorSpace(const float in)
{
    float out = in;
    if (colorSpaceTransform == _SRGBToLinear) {
        if (in <= 0.04045) {
            out = in / 12.92;
        } else {
            out = pow((in + 0.055) / 1.055, 2.4);
        }
    } else if (colorSpaceTransform == _LinearToSRGB) {
        if (in <= 0.0031308) {
            out = 12.92 * in;
        } else {
            out = 1.055 * pow(in, 1.0 / 2.4) - 0.055;
        }
    }

    return GfClamp(out, 0.f, 1.f);
}

// Pre-multiply alpha function to be used for integral types
template<typename T, bool isSRGB>
static
void
_PremultiplyAlpha(
    T * const data,
    const GfVec3i &dimensions)
{
    TRACE_FUNCTION();

    static_assert(std::numeric_limits<T>::is_integer, "Requires integral type");

    const size_t num = dimensions[0] * dimensions[1] * dimensions[2];

    // Perform all operations using floats.
    const float max = static_cast<float>(std::numeric_limits<T>::max());

    for (size_t i = 0; i < num; i++) {
        const float alpha = static_cast<float>(data[4 * i + 3]) / max;

        for (size_t j = 0; j < 3; j++) {
            float p = static_cast<float>(data[4 * i + j]);

            if (isSRGB) {
                // Convert value from sRGB to linear.
                p = max * _ConvertColorSpace<_SRGBToLinear>(p / max);
            }  
            
            // Pre-multiply RGB values with alpha in linear space.
            p *= alpha;

            if (isSRGB) {
                // Convert value from linear to sRGB.
                p = max * _ConvertColorSpace<_LinearToSRGB>(p / max);
            } 

            // Add 0.5 when converting float to integral type.
            data[4 * i + j] = p + 0.5f;  
        }
    }
}

// Pre-multiply alpha function to be used for floating point types
template<typename T>
static
void _PremultiplyAlphaFloat(
    T * const data,
    const GfVec3i &dimensions)
{
    TRACE_FUNCTION();

    static_assert(GfIsFloatingPoint<T>::value, "Requires floating point type");

    const size_t num = dimensions[0] * dimensions[1] * dimensions[2];

    for (size_t i = 0; i < num; i++) {
        const float alpha = data[4 * i + 3];

        // Pre-multiply RGB values with alpha.
        for (size_t j = 0; j < 3; j++) {
            data[4 * i + j] = data[4 * i + j] * alpha;
        }
    }
}
}

template<typename T, uint32_t alpha>
void
_ConvertRGBToRGBA(
    void * const data,
    const size_t numPixels)
{
    TRACE_FUNCTION();

    T * const typedData = reinterpret_cast<T*>(data);

    size_t i = numPixels;
    while(i--) {
        typedData[4 * i + 0] = typedData[3 * i + 0];
        typedData[4 * i + 1] = typedData[3 * i + 1];
        typedData[4 * i + 2] = typedData[3 * i + 2];
        typedData[4 * i + 3] = T(alpha);
    }
}

void
GarchUdimTexture::_ReadImage()
{
    TRACE_FUNCTION();
    
    if (_loaded) {
        return;
    }
    // APPLE METAL: Deprecated
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
