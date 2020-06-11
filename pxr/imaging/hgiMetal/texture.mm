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
#include <Metal/Metal.h>

#include "pxr/imaging/hgiMetal/capabilities.h"
#include "pxr/imaging/hgiMetal/conversions.h"
#include "pxr/imaging/hgiMetal/diagnostic.h"
#include "pxr/imaging/hgiMetal/hgi.h"
#include "pxr/imaging/hgiMetal/texture.h"


PXR_NAMESPACE_OPEN_SCOPE

HgiMetalTexture::HgiMetalTexture(HgiMetal *hgi, HgiTextureDesc const & desc)
    : HgiTexture(desc)
    , _textureId(nil)
{
    MTLResourceOptions resourceOptions = MTLResourceStorageModePrivate;
    MTLTextureUsage usage = MTLTextureUsageUnknown;

    if (desc.initialData && desc.pixelsByteSize > 0) {
        resourceOptions = hgi->GetCapabilities().defaultStorageMode;
    }

    MTLPixelFormat mtlFormat = HgiMetalConversions::GetPixelFormat(desc.format);
    
    
    if (desc.usage & HgiTextureUsageBitsColorTarget) {
        usage = MTLTextureUsageRenderTarget;
    } else if (desc.usage & HgiTextureUsageBitsDepthTarget) {
        TF_VERIFY(desc.format == HgiFormatFloat32);
        mtlFormat = MTLPixelFormatDepth32Float;
        usage = MTLTextureUsageRenderTarget;
    }

//    if (desc.usage & HgiTextureUsageBitsShaderRead) {
        usage |= MTLTextureUsageShaderRead;
//    }
    if (desc.usage & HgiTextureUsageBitsShaderWrite) {
        usage |= MTLTextureUsageShaderWrite;
    }

    const size_t width = desc.dimensions[0];
    const size_t height = desc.dimensions[1];
    const size_t depth = desc.dimensions[2];

    MTLTextureDescriptor* texDesc;

    texDesc =
        [MTLTextureDescriptor
         texture2DDescriptorWithPixelFormat:mtlFormat
                                      width:width
                                     height:height
                                  mipmapped:NO];
    
    texDesc.mipmapLevelCount = desc.mipLevels;

    texDesc.arrayLength = desc.layerCount;
    texDesc.resourceOptions = resourceOptions;
    texDesc.usage = usage;
    
    size_t numChannels = HgiGetComponentCount(desc.format);

    if (usage == MTLTextureUsageShaderRead && numChannels == 1) {
#if (__MAC_OS_X_VERSION_MAX_ALLOWED >= 101500) || (__IPHONE_OS_VERSION_MAX_ALLOWED >= 130000) /* __MAC_10_15 __IOS_13_00 */
        texDesc.usage = MTLTextureUsageShaderRead;
        texDesc.swizzle = MTLTextureSwizzleChannelsMake(MTLTextureSwizzleRed, MTLTextureSwizzleRed, MTLTextureSwizzleRed, MTLTextureSwizzleRed);
#endif
    }

    if (depth > 1) {
        texDesc.depth = depth;
        texDesc.textureType = MTLTextureType3D;
    }

    if (desc.sampleCount > 1) {
        texDesc.sampleCount = desc.sampleCount;
        texDesc.textureType = MTLTextureType2DMultisample;
    }

    _textureId = [hgi->GetPrimaryDevice() newTextureWithDescriptor:texDesc];

    if (desc.initialData && desc.pixelsByteSize > 0) {
        size_t mipWidth = width;
        size_t mipHeight = height;
        size_t mipDepth = (depth > 1) ? depth : 1;
        size_t pixelSize = HgiDataSizeOfFormat(desc.format);
        const uint8_t *byteData = static_cast<const uint8_t*>(desc.initialData);
        
//        for (int i = 0 ; i < desc.mipLevels; i++) {
        for (int i = 0 ; i < 1; i++) {
            size_t byteSize = mipWidth * mipHeight * mipDepth * pixelSize;

            if (depth <= 1) {
                [_textureId replaceRegion:MTLRegionMake2D(0, 0, mipWidth, mipHeight)
                              mipmapLevel:i
                                withBytes:byteData
                              bytesPerRow:mipWidth * pixelSize];
            }
            else {
                [_textureId replaceRegion:MTLRegionMake3D(0, 0, 0, mipWidth, mipHeight, mipDepth)
                              mipmapLevel:i
                                    slice:0
                                withBytes:byteData
                              bytesPerRow:mipWidth * pixelSize
                            bytesPerImage:mipWidth * mipHeight * pixelSize];
            }
            if (mipWidth > 1) {
                mipWidth >>= 1;
            }
            if (mipHeight > 1) {
                mipHeight >>= 1;
            }
            if (mipDepth > 1) {
                mipDepth >>= 1;
            }
            byteData += byteSize;
        }
    }

    HGIMETAL_DEBUG_LABEL(_textureId, _descriptor.debugName.c_str());
}

HgiMetalTexture::~HgiMetalTexture()
{
    if (_textureId != nil) {
        [_textureId release];
        _textureId = nil;
    }
}

id<MTLTexture>
HgiMetalTexture::GetTextureId() const
{
    return _textureId;
}

PXR_NAMESPACE_CLOSE_SCOPE
