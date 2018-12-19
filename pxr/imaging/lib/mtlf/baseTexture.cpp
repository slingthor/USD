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
#include "pxr/imaging/mtlf/diagnostic.h"
#include "pxr/imaging/mtlf/utils.h"

#include "pxr/imaging/garch/baseTextureData.h"

#include "pxr/base/tf/registryManager.h"
#include "pxr/base/tf/type.h"
#include "pxr/base/trace/trace.h"

PXR_NAMESPACE_OPEN_SCOPE

static MTLPixelFormat GetMetalFormat(GLenum inInternalFormat, GLenum inType, size_t *outPixelByteSize, bool *out24BitFormat)
{
    MTLPixelFormat mtlFormat = MTLPixelFormatInvalid;
    
    *outPixelByteSize = 0;
    *out24BitFormat = false;
    
    switch (inInternalFormat)
    {
        case GL_RGB32F:
        case GL_RGB16F:
        case GL_RGB16:
        case GL_SRGB:
        case GL_RGB:
            *out24BitFormat = true;
            // Drop through
            
        case GL_RGBA:
            mtlFormat = MTLPixelFormatRGBA8Unorm;
            *outPixelByteSize = sizeof(char) * 4;
            break;
            
        case GL_SRGB_ALPHA:
            mtlFormat = MTLPixelFormatRGBA8Unorm_sRGB;
            *outPixelByteSize = sizeof(char) * 4;
            break;
            
        case GL_RED:
            mtlFormat = MTLPixelFormatR8Unorm;
            *outPixelByteSize = sizeof(char);
            break;
            
        case GL_RGBA16:
            mtlFormat = MTLPixelFormatRGBA16Unorm;
            *outPixelByteSize = sizeof(short) * 4;
            break;
            
        case GL_R16:
            mtlFormat = MTLPixelFormatRGBA16Unorm;
            *outPixelByteSize = sizeof(short);
            break;
            
        case GL_RGBA16F:
            mtlFormat = MTLPixelFormatRGBA16Float;
            *outPixelByteSize = sizeof(short) * 4;
            break;
            
        case GL_R16F:
            mtlFormat = MTLPixelFormatR16Float;
            *outPixelByteSize = sizeof(short);
            break;
            
        case GL_RGBA32F:
            mtlFormat = MTLPixelFormatRGBA32Float;
            *outPixelByteSize = sizeof(float) * 4;
            break;
            
        case GL_R32F:
            mtlFormat = MTLPixelFormatRGBA32Float;
            *outPixelByteSize = sizeof(float);
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
    if (_textureName != nil) {
        [_textureName release];
    }
}

/* virtual */
GarchTexture::BindingVector
MtlfBaseTexture::GetBindings(TfToken const & identifier,
                             GarchSamplerGPUHandle samplerName)
{
    if (!_loaded) {
        _ReadTexture();
    }

    return BindingVector(1,
                Binding(identifier, GarchTextureTokens->texels,
                        0, _textureName, samplerName));
}

GarchTextureGPUHandle MtlfBaseTexture::GetAPITextureName()
{
    if (!_loaded) {
        _ReadTexture();
    }
    
    return _textureName;
}

int MtlfBaseTexture::GetWidth()
{
    if (!_loaded) {
        _ReadTexture();
    }

    return _currentWidth;
}

int MtlfBaseTexture::GetHeight()
{
    if (!_loaded) {
        _ReadTexture();
    }

    return _currentHeight;
}

int MtlfBaseTexture::GetFormat()
{
    if (!_loaded) {
        _ReadTexture();
    }
    
    return _format;
}

void 
MtlfBaseTexture::_UpdateTexture(GarchBaseTextureDataConstPtr texData)
{
    // Copy or clear fields required for tracking/reporting.
    if (texData && texData->HasRawBuffer()) {
        _currentWidth  = texData->ResizedWidth();
        _currentHeight = texData->ResizedHeight();
        _format        = texData->GLFormat();
        _hasWrapModeS  = texData->GetWrapInfo().hasWrapModeS;
        _hasWrapModeT  = texData->GetWrapInfo().hasWrapModeT;
        _wrapModeS     = texData->GetWrapInfo().wrapModeS;
        _wrapModeT     = texData->GetWrapInfo().wrapModeT;        

        _SetMemoryUsed(texData->ComputeBytesUsed());

    } else {
        _currentWidth  = _currentHeight = 0;
        _format        =  GL_RGBA;
        _hasWrapModeS  = _hasWrapModeT  = false;
        _wrapModeS     = _wrapModeT     = GL_REPEAT;

        _SetMemoryUsed(0);
    }
}

void 
MtlfBaseTexture::_CreateTexture(GarchBaseTextureDataConstPtr texData,
                bool const useMipmaps,
                int const unpackCropTop,
                int const unpackCropBottom,
                int const unpackCropLeft,
                int const unpackCropRight)
{
    TRACE_FUNCTION();
    
    if (texData && texData->HasRawBuffer()) {

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
        
        if (_textureName != nil) {
            [_textureName release];
        }

        // Uncompressed textures can have cropping and other special
        // behaviours.

        // If we are not sending full mipchains to the gpu then we can
        // do some extra work in the driver to prepare our textures.
        if (numMipLevels == 1) {
            int texDataWidth = texData->ResizedWidth();
            int texDataHeight = texData->ResizedHeight();
            int unpackRowLength = texDataWidth;
            int unpackSkipPixels = 0;
            int unpackSkipRows = 0;
            
            size_t pixelByteSize;
            int numPixels = texDataWidth * texDataHeight;
            bool b24BitFormat = false;
            MTLPixelFormat mtlFormat = GetMetalFormat(texData->GLInternalFormat(), texData->GLType(), &pixelByteSize, &b24BitFormat);
            
            void *texBuffer = texData->GetRawBuffer(0);
            if (b24BitFormat) {
                // Pad out 24bit formats to 32bit
                texBuffer = PadImage(texData->GLInternalFormat(), texData->GetRawBuffer(0), pixelByteSize, numPixels);
            }
            
            if (!texData->IsCompressed()) {
                if (unpackCropTop < 0 || unpackCropTop > texDataHeight) {
                    return;
                } else if (unpackCropTop > 0) {
                    unpackSkipRows = unpackCropTop;
                    texDataHeight -= unpackCropTop;
                }
                if (unpackCropBottom < 0 || unpackCropBottom > texDataHeight) {
                    return;
                } else if (unpackCropBottom) {
                    texDataHeight -= unpackCropBottom;
                }
                if (unpackCropLeft < 0 || unpackCropLeft > texDataWidth) {
                    return;
                } else {
                    unpackSkipPixels = unpackCropLeft;
                    texDataWidth -= unpackCropLeft;
                }
                if (unpackCropRight < 0 || unpackCropRight > texDataWidth) {
                    return;
                } else if (unpackCropRight > 0) {
                    texDataWidth -= unpackCropRight;
                }
            }
            
            if (texDataWidth == 1 || texDataHeight == 1) {
                genMips = false;
            }

            if (mtlFormat == MTLPixelFormatInvalid) {
                TF_FATAL_CODING_ERROR("Unsupported/unimplemented texture format");
            }
            
            id<MTLDevice> device = MtlfMetalContext::GetMetalContext()->device;
            MTLTextureDescriptor* desc =
                [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:mtlFormat
                                                                   width:texDataWidth
                                                                  height:texDataHeight
                                                               mipmapped:genMips?YES:NO];
            desc.resourceOptions = MTLResourceStorageModeDefault;
            _textureName = [device newTextureWithDescriptor:desc];

            char *rawData = (char*)texBuffer + (unpackSkipRows * unpackRowLength * pixelByteSize)
                + (unpackSkipPixels * pixelByteSize);
            [_textureName replaceRegion:MTLRegionMake2D(0, 0, texDataWidth, texDataHeight)
                            mipmapLevel:0
                              withBytes:rawData
                            bytesPerRow:pixelByteSize * unpackRowLength];

            if (b24BitFormat) {
                delete[] (uint8_t*)texBuffer;
            }

            if (genMips) {
                // Blit command encoder to generate mips
                MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();
                id<MTLBlitCommandEncoder> blitEncoder = context->GetBlitEncoder();
             
                [blitEncoder generateMipmapsForTexture:_textureName];
                
                context->ReleaseEncoder(true);
                context->CommitCommandBuffer(false, false);
            }

        } else {
            size_t pixelByteSize;
            bool b24BitFormat;
            MTLPixelFormat mtlFormat = GetMetalFormat(texData->GLInternalFormat(), texData->GLType(), &pixelByteSize, &b24BitFormat);
            
            if (mtlFormat == MTLPixelFormatInvalid) {
                TF_FATAL_CODING_ERROR("Unsupported/unimplemented texture format");
            }

            id<MTLDevice> device = MtlfMetalContext::GetMetalContext()->device;
            MTLTextureDescriptor* desc =
                [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:mtlFormat
                                                                   width:texData->ResizedWidth()
                                                                  height:texData->ResizedHeight()
                                                               mipmapped:genMips?YES:NO];
            
            desc.resourceOptions = MTLResourceStorageModeDefault;
            _textureName = [device newTextureWithDescriptor:desc];

            for (int i = 0 ; i < numMipLevels; i++) {
                size_t mipWidth = texData->ResizedWidth(i);
                size_t mipHeight = texData->ResizedHeight(i);
                void *texBuffer = texData->GetRawBuffer(i);
                int numPixels = mipWidth * mipHeight;
                
                if (b24BitFormat) {
                    // Pad out 24bit formats to 32bit
                    texBuffer = PadImage(texData->GLInternalFormat(), texData->GetRawBuffer(1), pixelByteSize, numPixels);
                }

                [_textureName replaceRegion:MTLRegionMake2D(0, 0, mipWidth, texData->ResizedHeight(i))
                                mipmapLevel:i
                                  withBytes:texBuffer
                                bytesPerRow:pixelByteSize * mipWidth];
                
                if (b24BitFormat) {
                    delete[] (uint8_t*)texBuffer;
                }
            }
        }

        _SetMemoryUsed(texData->ComputeBytesUsed());
    }
}

PXR_NAMESPACE_CLOSE_SCOPE

