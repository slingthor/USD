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
/// \file mtlf/ptexTexture.cpp

// 

#include "pxr/imaging/mtlf/mtlDevice.h"
#include "pxr/imaging/mtlf/baseTexture.h"
#include "pxr/imaging/mtlf/ptexTexture.h"

#include "pxr/base/tf/stringUtils.h"

#ifdef PXR_PTEX_SUPPORT_ENABLED

#include "pxr/imaging/mtlf/diagnostic.h"

#include "pxr/imaging/garch/ptexMipmapTextureLoader.h"

#include "pxr/base/tf/diagnostic.h"
#include "pxr/base/tf/registryManager.h"
#include "pxr/base/tf/type.h"
#include "pxr/base/trace/trace.h"

#include <Ptexture.h>
#include <PtexUtils.h>

#include <string>
#include <vector>
#include <list>
#include <algorithm>

using std::string;
using namespace boost;

PXR_NAMESPACE_OPEN_SCOPE

//------------------------------------------------------------------------------
MtlfPtexTexture::MtlfPtexTexture(const TfToken &imageFilePath)
: GarchPtexTexture(imageFilePath)
{
}

//------------------------------------------------------------------------------
MtlfPtexTexture::~MtlfPtexTexture()
{ 
    _FreePtexTextureObject();
}

//------------------------------------------------------------------------------
bool 
MtlfPtexTexture::_ReadImage()
{
    TRACE_FUNCTION();

    _FreePtexTextureObject( );

    const std::string & filename = _imageFilePath;

    GLint maxNumPages = 2048; // True for all version of Metal and GPUs for macOS

    TRACE_SCOPE("MtlfPtexTexture::_ReadImage() (read ptex)");

    // create a temporary ptex cache
    // (required to build guttering pixels efficiently)
    static const int PTEX_MAX_CACHE_SIZE = 128*1024*1024;
    PtexCache *cache = PtexCache::create(1, PTEX_MAX_CACHE_SIZE);
    if (!cache) {
        TF_WARN("Unable to create PtexCache");
        return false;
    }

    // load
    Ptex::String ptexError;
    PtexTexture *reader = cache->get(filename.c_str(), ptexError);
    //PtexTexture *reader = PtexTexture::open(filename.c_str(), ptexError, true);
    if (!reader) {
        TF_WARN("Unable to open ptex %s : %s",
                filename.c_str(), ptexError.c_str());
        cache->release();
        return false;
    }

    // Read the ptexture data and pack the texels

    TRACE_SCOPE("MtlfPtexTexture::_ReadImage() (generate texture)");
    size_t targetMemory = GetMemoryRequested();

    
    // maxLevels = -1 : load all mip levels
    // maxLevels = 0  : load only the highest resolution
    int maxLevels = -1;
    GarchPtexMipmapTextureLoader loader(reader,
                                        maxNumPages,
                                        maxLevels,
                                        targetMemory);

    {   // create the Metal texture array
        int numChannels = reader->numChannels();
        size_t pixelByteSize;

        GLint glFormat;
        switch (reader->dataType())
        {
            case Ptex::dt_float:
                static GLenum floatFormats[] =
                    { MTLPixelFormatR32Float, MTLPixelFormatRG32Float, MTLPixelFormatRGBA32Float, MTLPixelFormatRGBA32Float };
                _format = floatFormats[numChannels-1];
                pixelByteSize = 4 * numChannels;
                glFormat = GL_RGB32F;
                break;
            case Ptex::dt_uint16:
                static GLenum uint16Formats[] =
                    { MTLPixelFormatR16Unorm, MTLPixelFormatRG16Unorm, MTLPixelFormatRGBA16Unorm, MTLPixelFormatRGBA16Unorm };
                _format = uint16Formats[numChannels-1];
                pixelByteSize = 2 * numChannels;
                glFormat = GL_RGB16;
                break;
            case Ptex::dt_half:
                static GLenum halfFormats[] =
                    { MTLPixelFormatR16Float, MTLPixelFormatRG16Float, MTLPixelFormatRGBA16Float, MTLPixelFormatRGBA16Float };
                _format = halfFormats[numChannels-1];
                pixelByteSize = 2 * numChannels;
                glFormat = GL_RGB16F;
                break;
            default:
                static GLenum uint8Formats[] =
                    { MTLPixelFormatR8Unorm, MTLPixelFormatRG8Unorm, MTLPixelFormatRGBA8Unorm, MTLPixelFormatRGBA8Unorm };
                _format = uint8Formats[numChannels-1];
                pixelByteSize = numChannels;
                glFormat = GL_RGB;
                break;
        }

        _width = loader.GetPageWidth();
        _height = loader.GetPageHeight();
        _depth = loader.GetNumPages();

        void const* texelData = loader.GetTexelBuffer();
        bool needFreeTexelData = false;
        if (numChannels == 3) {
            needFreeTexelData = true;
            pixelByteSize += pixelByteSize / 3;
            numChannels++;

            texelData = MtlfBaseTexture::PadImage(glFormat, texelData, pixelByteSize, _width * _height * _depth);
        }

        int numFaces = loader.GetNumFaces();

        // layout texture buffer

        // ptex layout struct (6 * uint16)
        // struct Layout {
        //     uint16_t page;
        //     uint16_t nMipmap;
        //     uint16_t u;
        //     uint16_t v;
        //     uint16_t adjSizeDiffs; //(4:4:4:4)
        //     uint8_t  width log2;
        //     uint8_t  height log2;
        // };

        id<MTLDevice> device = MtlfMetalContext::GetMetalContext()->device;
        
        // Create the layout texture
        MTLTextureDescriptor* descLayout =
            [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR16Sint
                                                               width:numFaces * 6
                                                              height:1
                                                           mipmapped:NO];
        descLayout.textureType = MTLTextureType1D;
        descLayout.resourceOptions = MTLResourceStorageModeDefault;
        _layout = [device newTextureWithDescriptor:descLayout];

        [_layout replaceRegion:MTLRegionMake1D(0, numFaces * 6)
                   mipmapLevel:0
                     withBytes:loader.GetLayoutBuffer()
                   bytesPerRow:numFaces * 6 * sizeof(uint16_t)];

        // Create the texel texture
        MTLTextureDescriptor* descTexels =
            [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormat(_format)
                                                               width:_width
                                                              height:_height
                                                           mipmapped:NO];
        descTexels.textureType = MTLTextureType2DArray;
        descTexels.arrayLength = _depth;
        descTexels.resourceOptions = MTLResourceStorageModeDefault;
        _texels = [device newTextureWithDescriptor:descTexels];

        size_t pageSize = pixelByteSize * _width * _height;
        for (int i = 0; i < _depth; i++) {
            uint8_t const *pageBase = static_cast<uint8_t const*>(texelData)
                                        + (pageSize * i);
            [_texels replaceRegion:MTLRegionMake2D(0, 0, _width, _height)
                       mipmapLevel:0
                             slice:i
                         withBytes:pageBase
                       bytesPerRow:pixelByteSize * _width
                     bytesPerImage:0];
        }
        
        if (needFreeTexelData) {
            delete[] (uint8_t*)texelData;
            texelData = nullptr;
        }
    }

    reader->release();
    
    _SetMemoryUsed(loader.GetMemoryUsage());
    
    // also releases PtexCache
    cache->release();
    
    _loaded = true;

    return true;
}

//------------------------------------------------------------------------------
void
MtlfPtexTexture::_FreePtexTextureObject()
{
//    // delete layout lookup --------------------------------
    if (_layout.IsSet())
        [_layout release];
    _layout.Clear();

    // delete textures lookup ------------------------------
    if (_texels.IsSet())
       [_texels release];
    _texels.Clear();
}

PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_PTEX_SUPPORT_ENABLED


