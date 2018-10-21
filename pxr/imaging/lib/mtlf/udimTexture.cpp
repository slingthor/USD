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
#include "pxr/imaging/mtlf/mtlDevice.h"

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
    size_t pixelByteSize;
    MTLPixelFormat internalFormat = MTLPixelFormatRGBA8Snorm;

    if (type == GL_FLOAT) {
        constexpr MTLPixelFormat internalFormats[] =
        { MTLPixelFormatR32Float, MTLPixelFormatRG32Float, MTLPixelFormatRGBA32Float, MTLPixelFormatRGBA32Float };
        pixelByteSize = 4 * numChannels;
        internalFormat = internalFormats[numChannels - 1];
    } else if (type == GL_UNSIGNED_SHORT) {
        constexpr MTLPixelFormat internalFormats[] =
        { MTLPixelFormatR16Unorm, MTLPixelFormatRG16Unorm, MTLPixelFormatRGBA16Unorm, MTLPixelFormatRGBA16Unorm };
        pixelByteSize = 2 * numChannels;
        internalFormat = internalFormats[numChannels - 1];
    } else if (type == GL_HALF_FLOAT_ARB) {
        constexpr MTLPixelFormat internalFormats[] =
        { MTLPixelFormatR16Float, MTLPixelFormatRG16Float, MTLPixelFormatRGBA16Float, MTLPixelFormatRGBA16Float };
        pixelByteSize = 2 * numChannels;
        internalFormat = internalFormats[numChannels - 1];
    } else if (type == GL_UNSIGNED_BYTE) {
        constexpr MTLPixelFormat internalFormats[] =
        { MTLPixelFormatR8Unorm, MTLPixelFormatRG8Unorm, MTLPixelFormatRGBA8Unorm, MTLPixelFormatRGBA8Unorm };
        pixelByteSize = numChannels;
        internalFormat = internalFormats[numChannels - 1];
    }

    id<MTLDevice> device = MtlfMetalContext::GetMetalContext()->device;
    
    MTLTextureDescriptor* descImage =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormat(internalFormat)
                                                           width:_width
                                                          height:_height
                                                       mipmapped:YES];
    descImage.textureType = MTLTextureType2DArray;
    descImage.arrayLength = _depth;
    descImage.mipmapLevelCount = mipCount;
    descImage.resourceOptions = MTLResourceStorageModeManaged;
    _imageArray = [device newTextureWithDescriptor:descImage];
    
    for (int mip = 0; mip < mipCount; ++mip) {
        _TextureSize const& mipSize = mips[mip];

        for (int slice = 0; slice < _depth; slice++) {
            uint8_t const *sliceBase = static_cast<uint8_t const*>(mipData[mip].data()) +
                pixelByteSize * mipSize.width * mipSize.height * slice;

            [_imageArray replaceRegion:MTLRegionMake2D(0, 0, mipSize.width, mipSize.height)
                           mipmapLevel:mip
                                 slice:slice
                             withBytes:sliceBase
                           bytesPerRow:pixelByteSize * mipSize.width
                         bytesPerImage:0];
        }
    }
    
    MTLTextureDescriptor* descLayout =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR32Float
                                                           width:layoutData.size()
                                                          height:1
                                                       mipmapped:NO];
    descLayout.textureType = MTLTextureType1D;
    descLayout.resourceOptions = MTLResourceStorageModeManaged;
    _layout = [device newTextureWithDescriptor:descLayout];

    [_layout replaceRegion:MTLRegionMake1D(0, layoutData.size())
               mipmapLevel:0
                 withBytes:layoutData.data()
               bytesPerRow:layoutData.size() * sizeof(float)];
}

PXR_NAMESPACE_CLOSE_SCOPE
