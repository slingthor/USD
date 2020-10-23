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

#include "pxr/imaging/glf/udimTexture.h"

#include "pxr/base/gf/math.h"
#include "pxr/base/gf/vec3i.h"

#include "pxr/imaging/glf/contextCaps.h"
#include "pxr/imaging/glf/diagnostic.h"
#include "pxr/imaging/glf/glContext.h"

#include "pxr/imaging/glf/utils.h"
#include "pxr/base/tf/stringUtils.h"

#include "pxr/base/trace/trace.h"
#include "pxr/base/work/loops.h"

PXR_NAMESPACE_OPEN_SCOPE

TF_REGISTRY_FUNCTION(TfType)
{
    TfType::Define<GlfUdimTexture, TfType::Bases<GarchTexture>>();
}

GlfUdimTexture::GlfUdimTexture(
    TfToken const& imageFilePath,
    HioImage::ImageOriginLocation originLocation,
    std::vector<std::tuple<int, TfToken>>&& tiles,
    bool const premultiplyAlpha,
    HioImage::SourceColorSpace sourceColorSpace) // APPLE METAL: HioImage
    : GarchUdimTexture(imageFilePath, originLocation, std::move(tiles), 
      premultiplyAlpha, sourceColorSpace)
{
}

GlfUdimTexture::~GlfUdimTexture()
{
    _FreeTextureObject();
}

void
GlfUdimTexture::_FreeTextureObject()
{
    GlfSharedGLContextScopeHolder sharedGLContextScopeHolder;

    GLuint h;
    if (glIsTexture(_imageArray)) {
        h = _imageArray;
        glDeleteTextures(1, &h);
        _imageArray.Clear();
    }

    if (glIsTexture(_layout)) {
        h = _layout;
        glDeleteTextures(1, &h);
        _layout.Clear();
    }
}

void
GlfUdimTexture::_CreateGPUResources(unsigned int numChannels,
                                    GLenum const type,
                                    std::vector<_TextureSize> &mips,
                                    std::vector<std::vector<uint8_t>> &mipData,
                                    std::vector<float> &layoutData)
{
    const _MipDescArray firstImageMips = _GetMipLevels(std::get<1>(_tiles[0]),
                                                       _sourceColorSpace);
    GLenum internalFormat = GL_RGBA8;
    unsigned int sizePerElem = 1;
    if (type == GL_FLOAT) {
        constexpr GLenum internalFormats[] =
            { GL_R32F, GL_RG32F, GL_RGB32F, GL_RGBA32F };
        internalFormat = internalFormats[numChannels - 1];
        sizePerElem = 4;
    } else if (type == GL_UNSIGNED_SHORT) {
        constexpr GLenum internalFormats[] =
            { GL_R16, GL_RG16, GL_RGB16, GL_RGBA16 };
        internalFormat = internalFormats[numChannels - 1];
        sizePerElem = 2;
    } else if (type == GL_HALF_FLOAT_ARB) {
        constexpr GLenum internalFormats[] =
            { GL_R16F, GL_RG16F, GL_RGB16F, GL_RGBA16F };
        internalFormat = internalFormats[numChannels - 1];
        sizePerElem = 2;
    } else if (type == GL_UNSIGNED_BYTE) {
        constexpr GLenum internalFormats[] =
            { GL_R8, GL_RG8, GL_RGB8, GL_RGBA8 };
        constexpr GLenum internalFormatsSRGB[] =
            { GL_R8, GL_RG8, GL_SRGB8, GL_SRGB8_ALPHA8 };
        if (firstImageMips[0].image->IsColorSpaceSRGB()) {
            internalFormat = internalFormatsSRGB[numChannels - 1];
        } else {
            internalFormat = internalFormats[numChannels - 1];
        }
        sizePerElem = 1;
    }

    const unsigned int maxTileCount =
        std::get<0>(_tiles.back()) + 1;
    _depth = static_cast<int>(_tiles.size());
    const unsigned int numBytesPerPixel = sizePerElem * numChannels;
    const unsigned int numBytesPerPixelLayer = numBytesPerPixel * _depth;

    unsigned int mipCount = mips.size();
    mipData.resize(mipCount);

    _width = mips[0].width;
    _height = mips[0].height;

    // Texture array queries will use a float as the array specifier.
    layoutData.resize(maxTileCount, 0);
    GLuint h;
    glGenTextures(1, &h);
    _imageArray = h;

    glBindTexture(GL_TEXTURE_2D_ARRAY, _imageArray);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY,
        mipCount, internalFormat,
        _width, _height, _depth);

    size_t totalTextureMemory = 0;
    for (unsigned int mip = 0; mip < mipCount; ++mip) {
        _TextureSize const& mipSize = mips[mip];
        const unsigned int currentMipMemory =
            mipSize.width * mipSize.height * numBytesPerPixelLayer;
        mipData[mip].resize(currentMipMemory, 0);
        totalTextureMemory += currentMipMemory;
    }

    GLenum format = GlfGetGLFormat(_format);
    for (unsigned int mip = 0; mip < mipCount; ++mip) {
        _TextureSize const& mipSize = mips[mip];
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, mip, 0, 0, 0,
                        mipSize.width, mipSize.height, _depth, format, type,
                        mipData[mip].data());
    }

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    h = 0;
    glGenTextures(1, &h);
    _layout = h;

    glBindTexture(GL_TEXTURE_1D, _layout);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexImage1D(GL_TEXTURE_1D, 0, GL_R32F, layoutData.size(), 0,
        GL_RED, GL_FLOAT, layoutData.data());
    glBindTexture(GL_TEXTURE_1D, 0);

    GLF_POST_PENDING_GL_ERRORS();

    _SetMemoryUsed(totalTextureMemory + _tiles.size() * sizeof(float));
}


PXR_NAMESPACE_CLOSE_SCOPE
