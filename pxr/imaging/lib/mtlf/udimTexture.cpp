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

#include "pxr/imaging/mtlf/udimTexture.h"

#include "pxr/imaging/garch/contextCaps.h"
#include "pxr/imaging/garch/image.h"

#include "pxr/base/tf/stringUtils.h"

#include "pxr/base/trace/trace.h"
#include "pxr/base/work/loops.h"

PXR_NAMESPACE_OPEN_SCOPE

TF_REGISTRY_FUNCTION(TfType)
{
    TfType::Define<MtlfUdimTexture, TfType::Bases<GarchTexture>>();
}

MtlfUdimTexture::MtlfUdimTexture(
    TfToken const& imageFilePath,
    GarchImage::ImageOriginLocation originLocation,
    std::vector<std::tuple<int, TfToken>>&& tiles)
    : GarchUdimTexture(imageFilePath, originLocation, std::move(tiles))
{
}

MtlfUdimTexture::~MtlfUdimTexture()
{
    _FreeTextureObject();
}

void
MtlfUdimTexture::_FreeTextureObject()
{
    if (_imageArray.IsSet()) {
        [_imageArray release];
        _imageArray.Clear();
    }

    if (_layout.IsSet()) {
        [_layout release];
        _layout.Clear();
    }
}

void
MtlfUdimTexture::_CreateGPUResources(unsigned int numChannels,
                                     GLenum const type,
                                     std::vector<_TextureSize> &mips,
                                     std::vector<std::vector<uint8_t>> &mipData,
                                     std::vector<float> &layoutData)
{
    unsigned int mipCount = mips.size();
    MTLPixelFormat internalFormat = MTLPixelFormatRGBA8Snorm;
    if (type == GL_FLOAT) {
        constexpr MTLPixelFormat internalFormats[] =
        { MTLPixelFormatR32Float, MTLPixelFormatRG32Float, MTLPixelFormatRGBA32Float, MTLPixelFormatRGBA32Float };
        internalFormat = internalFormats[numChannels - 1];
    } else if (type == GL_UNSIGNED_SHORT) {
        constexpr MTLPixelFormat internalFormats[] =
        { MTLPixelFormatR16Unorm, MTLPixelFormatRG16Unorm, MTLPixelFormatRGBA16Unorm, MTLPixelFormatRGBA16Unorm };
        internalFormat = internalFormats[numChannels - 1];
    } else if (type == GL_HALF_FLOAT_ARB) {
        constexpr MTLPixelFormat internalFormats[] =
        { MTLPixelFormatR16Float, MTLPixelFormatRG16Float, MTLPixelFormatRGBA16Float, MTLPixelFormatRGBA16Float };
        internalFormat = internalFormats[numChannels - 1];
    } else if (type == GL_UNSIGNED_BYTE) {
        constexpr MTLPixelFormat internalFormats[] =
        { MTLPixelFormatR8Unorm, MTLPixelFormatRG8Unorm, MTLPixelFormatRGBA8Unorm, MTLPixelFormatRGBA8Unorm };
        internalFormat = internalFormats[numChannels - 1];
    }

    TF_FATAL_CODING_ERROR("Not Implemented");
    /*
    glGenTextures(1, &_imageArray);
    glBindTexture(GL_TEXTURE_2D_ARRAY, _imageArray);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY,
        mipCount, internalFormat,
        _width, _height, _depth);

    for (int mip = 0; mip < mipCount; ++mip) {
        _TextureSize const& mipSize = mips[mip];
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, mip, 0, 0, 0,
                        mipSize.width, mipSize.height, _depth, _format, type,
                        mipData[mip].data());
    }

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    glGenTextures(1, &_layout);
    glBindTexture(GL_TEXTURE_1D, _layout);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexImage1D(GL_TEXTURE_1D, 0, GL_R32F, layoutData.size(), 0,
        GL_RED, GL_FLOAT, layoutData.data());
    glBindTexture(GL_TEXTURE_1D, 0);*/
}

PXR_NAMESPACE_CLOSE_SCOPE
