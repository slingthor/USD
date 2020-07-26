//
// Copyright 2016 Pixar
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
/// \file baseTexture.cpp

#include "pxr/imaging/mtlf/mtlDevice.h"

#include "pxr/imaging/mtlf/baseTexture.h"
#include "pxr/imaging/mtlf/contextCaps.h"
#include "pxr/imaging/mtlf/diagnostic.h"
#include "pxr/imaging/mtlf/utils.h"

#include "pxr/imaging/garch/baseTextureData.h"
#include "pxr/imaging/garch/contextCaps.h"
#include "pxr/imaging/garch/resourceFactory.h"

#include "pxr/base/tf/registryManager.h"
#include "pxr/base/tf/type.h"
#include "pxr/base/trace/trace.h"

PXR_NAMESPACE_OPEN_SCOPE

static bool useAsncTextureUploads = false;

static MTLPixelFormat GetMetalFormat(GLenum inInternalFormat, GLenum inType, size_t *outPixelByteSize, int *numChannels)
{
    MTLPixelFormat mtlFormat = MTLPixelFormatInvalid;
    
    *outPixelByteSize = 0;
    *numChannels = 4;
    
    switch (inInternalFormat)
    {
        case GL_RGB:
            *numChannels = 3;
            // Drop through
        case GL_RGBA:
            mtlFormat = MTLPixelFormatRGBA8Unorm;
            *outPixelByteSize = sizeof(char) * 4;
            break;

        case GL_SRGB:
            *numChannels = 3;
            // Drop through
        case GL_SRGB_ALPHA:
            mtlFormat = MTLPixelFormatRGBA8Unorm_sRGB;
            *outPixelByteSize = sizeof(char) * 4;
            break;
            
        case GL_RED:
            mtlFormat = MTLPixelFormatR8Unorm;
            *outPixelByteSize = sizeof(char);
            *numChannels = 1;
            break;
        
        case GL_RGB16:
            *numChannels = 3;
            // Drop through
        case GL_RGBA16:
            mtlFormat = MTLPixelFormatRGBA16Unorm;
            *outPixelByteSize = sizeof(short) * 4;
            break;
            
        case GL_R16:
            mtlFormat = MTLPixelFormatRGBA16Unorm;
            *outPixelByteSize = sizeof(short);
            *numChannels = 1;
            break;
            
        case GL_RGB16F:
            *numChannels = 3;
            // Drop through
        case GL_RGBA16F:
            mtlFormat = MTLPixelFormatRGBA16Float;
            *outPixelByteSize = sizeof(short) * 4;
            break;
            
        case GL_R16F:
            mtlFormat = MTLPixelFormatR16Float;
            *outPixelByteSize = sizeof(short);
            *numChannels = 1;
            break;
            
        case GL_RGB32F:
            *numChannels = 3;
            // Drop through
        case GL_RGBA32F:
            mtlFormat = MTLPixelFormatRGBA32Float;
            *outPixelByteSize = sizeof(float) * 4;
            break;
            
        case GL_R32F:
            mtlFormat = MTLPixelFormatRGBA32Float;
            *outPixelByteSize = sizeof(float);
            *numChannels = 1;
            break;
    }
    
    return mtlFormat;
}

void *MtlfBaseTexture::PadImage(uint32_t glFormat, void const* rawData, size_t pixelByteSize, int numPixels)
{
    void* texBuffer;
    switch (glFormat)
    {
        case GL_RGB32F:
        {
            texBuffer = new float[pixelByteSize * numPixels];
            
            float *src = (float*)rawData;
            float *dst = (float*)texBuffer;
            while(numPixels-- > 0) {
                *dst++ = *src++;
                *dst++ = *src++;
                *dst++ = *src++;
                *dst++ = 1.0f;
            }
        }
        break;
            
        case GL_RGB16F:
        {
            texBuffer = new uint16_t[pixelByteSize * numPixels];
            
            uint16_t *src = (uint16_t*)rawData;
            uint16_t *dst = (uint16_t*)texBuffer;
            while(numPixels-- > 0) {
                *dst++ = *src++;
                *dst++ = *src++;
                *dst++ = *src++;
                *dst++ = 0x3C00;
            }
        }
        break;
            
        case GL_RGB16:
        {
            texBuffer = new uint16_t[pixelByteSize * numPixels];
            
            uint16_t *src = (uint16_t*)rawData;
            uint16_t *dst = (uint16_t*)texBuffer;
            while(numPixels-- > 0) {
                *dst++ = *src++;
                *dst++ = *src++;
                *dst++ = *src++;
                *dst++ = 0xffff;
            }
        }
        break;
            
        case GL_SRGB:
        case GL_RGB:
        {
            texBuffer = new uint8_t[pixelByteSize * numPixels];
            
            uint8_t *src = (uint8_t*)rawData;
            uint8_t *dst = (uint8_t*)texBuffer;
            while(numPixels-- > 0) {
                *dst++ = *src++;
                *dst++ = *src++;
                *dst++ = *src++;
                *dst++ = 0xff;
            }
        }
        break;
    }
    
    return texBuffer;
}

TF_REGISTRY_FUNCTION(TfType)
{
    TfType::Define<MtlfBaseTexture, TfType::Bases<GarchTexture> >();
}

MtlfBaseTexture::MtlfBaseTexture()
{
}

MtlfBaseTexture::~MtlfBaseTexture()
{
    if (_textureName.IsSet()) {
        [_textureName release];
    }
}

/* virtual */
GarchTexture::BindingVector
MtlfBaseTexture::GetBindings(TfToken const & identifier,
                             GarchSamplerGPUHandle const& samplerName)
{
    _ReadTextureIfNotLoaded();

    return BindingVector(1,
                Binding(identifier, GarchTextureTokens->texels,
                        0, _textureName, samplerName));
}

GarchTextureGPUHandle MtlfBaseTexture::GetAPITextureName()
{
    _ReadTextureIfNotLoaded();
    
    return _textureName;
}

int MtlfBaseTexture::GetWidth()
{
    _ReadTextureIfNotLoaded();

    return _currentWidth;
}

int MtlfBaseTexture::GetHeight()
{
    _ReadTextureIfNotLoaded();

    return _currentHeight;
}

int MtlfBaseTexture::GetFormat()
{
    _ReadTextureIfNotLoaded();
    
    return _format;
}

void 
MtlfBaseTexture::_UpdateTexture(GarchBaseTextureDataConstPtr texData)
{
    // Copy or clear fields required for tracking/reporting.
    if (texData && texData->HasRawBuffer()) {
        _currentWidth  = texData->ResizedWidth();
        _currentHeight = texData->ResizedHeight();
        _currentDepth  = texData->ResizedDepth();
        _format        = texData->GLFormat();
        _hasWrapModeS  = texData->GetWrapInfo().hasWrapModeS;
        _hasWrapModeT  = texData->GetWrapInfo().hasWrapModeT;
        _hasWrapModeR  = texData->GetWrapInfo().hasWrapModeR;
        _wrapModeS     = texData->GetWrapInfo().wrapModeS;
        _wrapModeT     = texData->GetWrapInfo().wrapModeT;
        _wrapModeR     = texData->GetWrapInfo().wrapModeR;

        _SetMemoryUsed(texData->ComputeBytesUsed());

    } else {
        _currentWidth  = _currentHeight = 0;
        _currentDepth  = 1;
        _format        =  GL_RGBA;
        _hasWrapModeS  = _hasWrapModeT  = _hasWrapModeR = false;
        _wrapModeS     = _wrapModeT     = _wrapModeR    = GL_REPEAT;

        _SetMemoryUsed(0);
    }
}

void 
MtlfBaseTexture::_CreateTexture(GarchBaseTextureDataConstPtr texData,
                bool const useMipmaps,
                int const unpackCropTop,
                int const unpackCropBottom,
                int const unpackCropLeft,
                int const unpackCropRight,
                int const unpackCropFront,
                int const unpackCropBack)
{
    TRACE_FUNCTION();
    
    if (texData && texData->HasRawBuffer()) {

        const int numDimensions = texData->NumDimensions();
/*
        if (texData->NumDimensions() != numDimensions) {
            TF_CODING_ERROR("Dimension mismatch %d != %d between "
                            "GarchBaseTextureData and MtlfBaseTexture",
                            texData->NumDimensions(), numDimensions);
            return;
        }
*/
        // Check if mip maps have been requested, if so, it will either
        // enable automatic generation or use the ones loaded in cpu memory
        int numMipLevels = 1;
        bool genMips = false;

        if (useMipmaps) {
            numMipLevels = texData->GetNumMipLevels();
            
            // When we are using uncompressed textures and late cropping
            // we won't use cpu loaded mips.
            if (!texData->IsCompressed() &&
                (unpackCropRight || unpackCropLeft ||
                unpackCropTop || unpackCropBottom)) {
                    numMipLevels = 1;
            }
            if (numMipLevels == 1) {
                genMips = true;
            }
        }
        
        if (_textureName.IsSet()) {
            [_textureName release];
            _textureName = nil;
        }

        // Uncompressed textures can have cropping and other special
        // behaviours.

        // If we are not sending full mipchains to the gpu then we can
        // do some extra work in the driver to prepare our textures.
        if (numMipLevels == 1) {
            int texDataWidth = texData->ResizedWidth();
            int texDataHeight = texData->ResizedHeight();
            int texDataDepth = texData->ResizedDepth();
            int unpackRowLength = texDataWidth;
            int unpackSkipPixels = 0;
            int unpackSkipRows = 0;
            
            size_t pixelByteSize;
            int numPixels = texDataWidth * texDataHeight;
            int numChannels;
            MTLPixelFormat mtlFormat = GetMetalFormat(texData->GLInternalFormat(), texData->GLType(), &pixelByteSize, &numChannels);
            int isThreeChannelTexture = numChannels == 3;
            
            void *texBuffer = texData->GetRawBuffer(0);
            if (isThreeChannelTexture) {
                // Pad out 24bit formats to 32bit
                texBuffer = PadImage(texData->GLInternalFormat(), texData->GetRawBuffer(0), pixelByteSize, numPixels);
            }
            
            if (!texData->IsCompressed()) {
                const int width = texData->ResizedWidth();
                const int height = texData->ResizedHeight();
                const int depth = texData->ResizedDepth();
                
                int croppedWidth  = width;
                int croppedHeight = height;
                int croppedDepth  = depth;
                
                if (unpackCropLeft < 0 || unpackCropLeft > croppedWidth) {
                    return;
                }
                
                croppedWidth -= unpackCropLeft;
                
                if (unpackCropRight < 0 || unpackCropRight > croppedWidth) {
                    return;
                }
                
                croppedWidth -= unpackCropRight;
                
                if (unpackCropTop < 0 || unpackCropTop > croppedHeight) {
                    return;
                }
                
                croppedHeight -= unpackCropTop;
                
                if (unpackCropBottom < 0 || unpackCropBottom > croppedHeight) {
                    return;
                }
                
                croppedHeight -= unpackCropBottom;
                
                if (unpackCropFront < 0 || unpackCropFront > croppedDepth) {
                    return;
                }
                
                croppedDepth -= unpackCropFront;
                
                if (unpackCropBack < 0 || unpackCropBack > croppedDepth) {
                    return;
                }
                
                croppedDepth -= unpackCropBack;
            }
            
            if (texDataWidth == 1 || texDataHeight == 1) {
                genMips = false;
            }

            if (mtlFormat == MTLPixelFormatInvalid) {
                TF_FATAL_CODING_ERROR("Unsupported/unimplemented texture format");
            }
            
            id<MTLDevice> device = MtlfMetalContext::GetMetalContext()->currentDevice;
            MTLTextureDescriptor* desc =
                [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:mtlFormat
                                                                   width:texDataWidth
                                                                  height:texDataHeight
                                                               mipmapped:genMips?YES:NO];
            desc.resourceOptions = MTLResourceStorageModeDefault;
            desc.usage = MTLTextureUsageShaderRead;
            
            if (numChannels == 1) {
#if (__MAC_OS_X_VERSION_MAX_ALLOWED >= 101500) || (__IPHONE_OS_VERSION_MAX_ALLOWED >= 130000) /* __MAC_10_15 __IOS_13_00 */
                if (GarchResourceFactory::GetInstance()->GetContextCaps().apiVersion >= MtlfContextCaps::APIVersion_Metal3_0) {
                    desc.swizzle = MTLTextureSwizzleChannelsMake(MTLTextureSwizzleRed, MTLTextureSwizzleRed, MTLTextureSwizzleRed, MTLTextureSwizzleRed);
                }
#endif
            }

            _textureName = [device newTextureWithDescriptor:desc];

            char *rawData = (char*)texBuffer + (unpackSkipRows * unpackRowLength * pixelByteSize)
                + (unpackSkipPixels * pixelByteSize);
            
            if (useAsncTextureUploads) {
                GarchBaseTextureDataConstPtr *asyncOwnedTexData = new GarchBaseTextureDataConstPtr(texData);
                dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
                   ^ {
                       [_textureName replaceRegion:MTLRegionMake2D(0, 0, texDataWidth, texDataHeight)
                                       mipmapLevel:0
                                         withBytes:rawData
                                       bytesPerRow:pixelByteSize * unpackRowLength];
                       
                        if (isThreeChannelTexture) {
                           delete[] (uint8_t*)texBuffer;
                        }
                       
                        if (genMips) {
                            // Blit command encoder to generate mips
                            MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();

                            id<MTLCommandBuffer> commandBuffer = [context->gpus.commandQueue commandBuffer];
                            id<MTLBlitCommandEncoder> blitEncoder = [commandBuffer blitCommandEncoder];

                            [blitEncoder generateMipmapsForTexture:_textureName];
                            [blitEncoder endEncoding];

                            [commandBuffer commit];
                        }
                    delete asyncOwnedTexData;
                });
            }
            else {
                [_textureName replaceRegion:MTLRegionMake2D(0, 0, texDataWidth, texDataHeight)
                                mipmapLevel:0
                                  withBytes:rawData
                                bytesPerRow:pixelByteSize * unpackRowLength];

                if (isThreeChannelTexture) {
                    delete[] (uint8_t*)texBuffer;
                }

                if (genMips) {
                    // Blit command encoder to generate mips
                    MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();
                    
                    id<MTLCommandBuffer> commandBuffer = [context->gpus.commandQueue commandBuffer];
                    id<MTLBlitCommandEncoder> blitEncoder = [commandBuffer blitCommandEncoder];

                    [blitEncoder generateMipmapsForTexture:_textureName];
                    [blitEncoder endEncoding];

                    [commandBuffer commit];
                }
            }
        } else {
            size_t pixelByteSize;
            int numChannels;
            MTLPixelFormat mtlFormat = GetMetalFormat(texData->GLInternalFormat(), texData->GLType(), &pixelByteSize, &numChannels);
            bool isThreeChannelTexture = numChannels == 3;

            if (mtlFormat == MTLPixelFormatInvalid) {
                TF_FATAL_CODING_ERROR("Unsupported/unimplemented texture format");
            }

            id<MTLDevice> device = MtlfMetalContext::GetMetalContext()->currentDevice;
            MTLTextureDescriptor* desc =
                [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:mtlFormat
                                                                   width:texData->ResizedWidth()
                                                                  height:texData->ResizedHeight()
                                                               mipmapped:(numMipLevels > 1)?YES:NO];
            
            desc.resourceOptions = MTLResourceStorageModeDefault;
            desc.usage = MTLTextureUsageShaderRead;

            if (numChannels == 1) {
#if (__MAC_OS_X_VERSION_MAX_ALLOWED >= 101500) || (__IPHONE_OS_VERSION_MAX_ALLOWED >= 130000) /* __MAC_10_15 __IOS_13_00 */
                if (GarchResourceFactory::GetInstance()->GetContextCaps().apiVersion >= MtlfContextCaps::APIVersion_Metal3_0) {
                    desc.swizzle = MTLTextureSwizzleChannelsMake(MTLTextureSwizzleRed, MTLTextureSwizzleRed, MTLTextureSwizzleRed, MTLTextureSwizzleRed);
                }
#endif
            }
            
            _textureName = [device newTextureWithDescriptor:desc];

            if (useAsncTextureUploads) {
                // Retain an active reference to the tex data for the async operation
                GarchBaseTextureDataConstPtr *asyncOwnedTexData = new GarchBaseTextureDataConstPtr(texData);
                dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
                   ^ {
                        for (int i = 0 ; i < numMipLevels; i++) {
                            size_t mipWidth = (*asyncOwnedTexData)->ResizedWidth(i);
                            size_t mipHeight = (*asyncOwnedTexData)->ResizedHeight(i);
                            void *texBuffer = (*asyncOwnedTexData)->GetRawBuffer(i);
                            int numPixels = mipWidth * mipHeight;
                           
                            if (isThreeChannelTexture) {
                                // Pad out 24bit formats to 32bit
                                texBuffer = PadImage((*asyncOwnedTexData)->GLInternalFormat(), (*asyncOwnedTexData)->GetRawBuffer(1), pixelByteSize, numPixels);
                            }

                            [_textureName replaceRegion:MTLRegionMake2D(0, 0, mipWidth, texData->ResizedHeight(i))
                                            mipmapLevel:i
                                              withBytes:texBuffer
                                            bytesPerRow:pixelByteSize * mipWidth];
                           
                            if (isThreeChannelTexture) {
                                delete[] (uint8_t*)texBuffer;
                            }
                        }
                       delete asyncOwnedTexData;
                });
            }
            else {
                for (int i = 0 ; i < numMipLevels; i++) {
                    size_t mipWidth = texData->ResizedWidth(i);
                    size_t mipHeight = texData->ResizedHeight(i);
                    void *texBuffer = texData->GetRawBuffer(i);
                    int numPixels = mipWidth * mipHeight;
                    
                    if (isThreeChannelTexture) {
                        // Pad out 24bit formats to 32bit
                        texBuffer = PadImage(texData->GLInternalFormat(), texData->GetRawBuffer(1), pixelByteSize, numPixels);
                    }
                    
                    [_textureName replaceRegion:MTLRegionMake2D(0, 0, mipWidth, texData->ResizedHeight(i))
                                    mipmapLevel:i
                                      withBytes:texBuffer
                                    bytesPerRow:pixelByteSize * mipWidth];
    
                    if (isThreeChannelTexture) {
                        delete[] (uint8_t*)texBuffer;
                    }
                }
            }
        }

        _SetMemoryUsed(texData->ComputeBytesUsed());
    }
}

PXR_NAMESPACE_CLOSE_SCOPE

