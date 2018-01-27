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

#include "pxr/imaging/garch/image.h"
#include "pxr/imaging/garch/utils.h"
#include "pxr/imaging/garch/uvTextureData.h"

#include "pxr/base/tf/fileUtils.h"
#include "pxr/base/tracelite/trace.h"

PXR_NAMESPACE_OPEN_SCOPE


GarchUVTextureDataRefPtr
GarchUVTextureData::New(
    std::string const &filePath,
    size_t targetMemory,
    unsigned int cropTop,
    unsigned int cropBottom,
    unsigned int cropLeft,
    unsigned int cropRight)
{
    GarchUVTextureData::Params params;
    params.targetMemory = targetMemory;
    params.cropTop      = cropTop;
    params.cropBottom   = cropBottom;
    params.cropLeft     = cropLeft;
    params.cropRight    = cropRight;
    return New(filePath, params);
}

GarchUVTextureDataRefPtr
GarchUVTextureData::New(std::string const &filePath, Params const &params)
{
    TF_CODING_ERROR("Not Implemented");
    return TfNullPtr;
//    return TfCreateRefPtr(new GarchUVTextureData(filePath, params));
}

GarchUVTextureData::~GarchUVTextureData()
{
}

GarchUVTextureData::GarchUVTextureData(std::string const &filePath,
                                     Params const &params)
  : _filePath(filePath),
    _params(params),
    _targetMemory(0),
    _nativeWidth(0), _nativeHeight(0),
    _resizedWidth(0), _resizedHeight(0),
    _bytesPerPixel(0),
    _glInternalFormat(GL_RGB),
    _glFormat(GL_RGB),
    _glType(GL_UNSIGNED_BYTE),
    _size(0)
{
    /* nothing */
}

// Compute required GPU memory
size_t
GarchUVTextureData_ComputeMemory(GarchImageSharedPtr const &img,
                                  bool generateMipmap)
{
    // Mipmapping on GPU means we need an
    // extra 1/4 + 1/16 + 1/64 + 1/256 + ... of memory
    const double scale = generateMipmap ? 4.0 / 3 : 1.0;

    if (GarchIsCompressedFormat(img->GetFormat())) {
         return scale * GarchGetCompressedTextureSize(img->GetWidth(), 
                            img->GetHeight(), img->GetFormat(), img->GetType());
    }

    const size_t numPixels = img->GetWidth() * img->GetHeight();
    return scale * numPixels * img->GetBytesPerPixel();
}

GarchUVTextureData::_DegradedImageInput
GarchUVTextureData::_GetDegradedImageInputChain(double scaleX, double scaleY, 
                                              int startMip, int lastMip)
{
    _DegradedImageInput chain(scaleX, scaleY);
    for (int level = startMip; level < lastMip; level++) {
        GarchImageSharedPtr image = GarchImage::OpenForReading(_filePath, level);
        chain.images.push_back(image);
    }
    return chain;
}

int
GarchUVTextureData::_GetNumMipLevelsValid(const GarchImageSharedPtr image) const
{
    int potentialMipLevels = image->GetNumMipLevels();

    // Some of our texture loaders will always return an image (even if that
    // mip is not available) so the easiest way to figure out the number of 
    // mip levels is by loading mips and looking at the sizes.
    int previousWidth = image->GetWidth();
    int previousHeight = image->GetHeight();
    
    // Count mips since certain formats will not fail when quering mips,
    // in that case 
    for (int mipCounter = 1; mipCounter < 32; mipCounter++) {
        GarchImageSharedPtr image = GarchImage::OpenForReading(_filePath, mipCounter);
        if (!image) {
            potentialMipLevels = mipCounter;
            break;
        }

        int currentWidth = image->GetWidth();
        int currentHeight = image->GetHeight();
        
        // If previous mip and current mip are equal we have found the end of
        // the chain.
        if (previousWidth == currentWidth && 
            previousHeight == currentHeight) {
            potentialMipLevels = mipCounter;
            break;
        }

        // We need to make sure that the previous mip and the current mip
        // are consecutives powers of two.
        if (previousWidth >> 1 != currentWidth || 
            previousHeight>> 1 != currentHeight) {
            potentialMipLevels = 1;
            break;
        }

        previousWidth = currentWidth;
        previousHeight = currentHeight;
    }

    return potentialMipLevels;
}

GarchUVTextureData::_DegradedImageInput
GarchUVTextureData::_ReadDegradedImageInput(bool generateMipmap,
                                           size_t targetMemory,
                                           size_t degradeLevel)
{
    // Read the header of the image (no subimageIndex given, so at full
    // resolutin when evaluated).
    const GarchImageSharedPtr fullImage = GarchImage::OpenForReading(_filePath);

    // Bail if image file could not be opened.
    if (!fullImage) {
        return _DegradedImageInput(1.0, 1.0);
    }

    // Load full chain if needed
    int numMipLevels = generateMipmap ? _GetNumMipLevelsValid(fullImage) : 1;

    // Return full resolution if the targetMemory and the degradeLevel are not
    // set, i.e., equal to 0.
    if (!(targetMemory > 0 || degradeLevel > 0)) {
        return _GetDegradedImageInputChain(1.0, 1.0, 0, numMipLevels);
    }

    // Compute the estimate required memory at full resolution.
    const size_t fullSize = GarchUVTextureData_ComputeMemory(fullImage,
                                                            generateMipmap);

    // If targetMemory is set and more than required for full resolution,
    // return full resolution.
    if (targetMemory > 0 && fullSize <= targetMemory) {
        return _GetDegradedImageInputChain(1.0, 1.0, 0, numMipLevels);
    }
    
    // If no targetMemory set, use degradeLevel to determine mipLevel
    if (targetMemory == 0) {
        GarchImageSharedPtr image =
		GarchImage::OpenForReading(_filePath, degradeLevel);
        if (!image) {
            return _DegradedImageInput(1.0, 1.0);
        }

        return _GetDegradedImageInputChain(
            double(image->GetWidth()) / fullImage->GetWidth(),
            double(image->GetHeight()) / fullImage->GetHeight(), 
            degradeLevel, degradeLevel + 1);
    }

    // We actually have an image requiring more memory than targetMemory.
    // Iterate through the levels of down-sampled images until either:
    // - The required memory is less or equal to targetMemory
    // - There are no more down-sampled images
    // - An iteration limit has been reached

    // Remember the previous image and size to detect that there are no more
    // down-sampled images
    GarchImageSharedPtr prevImage = fullImage;
    size_t prevSize = fullSize;

    for (int i = 1; i < numMipLevels; i++) {
        // Open the image and is requested to use the i-th
        // down-sampled image (mipLevel).
        GarchImageSharedPtr image = GarchImage::OpenForReading(_filePath, i);

        // If mipLevel could not be opened, return fullImage. We are
        // not supposed to hit this. GarchImage will return the last
        // down-sampled image if the subimageIndex is beyond the range.
        if (!image) {
            return _GetDegradedImageInputChain(1.0, 1.0, 0, 1);
        }

        // Compute the size at the down-sampled resolution.
        size_t size = GarchUVTextureData_ComputeMemory(image,
                                                     generateMipmap);
        if (size <= targetMemory) {
            // We found an image with small enough memory requirement,
            // return it.
            return _GetDegradedImageInputChain(
                double(image->GetWidth()) / fullImage->GetWidth(),
                double(image->GetHeight()) / fullImage->GetHeight(),
                i, numMipLevels);
        }
        
        if (!(size < prevSize)) {
            // GarchImage stopped providing more further-downsampled
            // images, no point to continue, return image from last
            // iteration.
            return _GetDegradedImageInputChain(
                double(prevImage->GetWidth()) / fullImage->GetWidth(),
                double(prevImage->GetHeight()) / fullImage->GetHeight(),
                i-1, numMipLevels);
        }
        
        // Set values to see try to fetch the next downsampled images.
        prevImage = image;
        prevSize = size;
    }

    // Iteration limit reached, return image from last iteration.
    return _GetDegradedImageInputChain(
        double(prevImage->GetWidth()) / fullImage->GetWidth(),
        double(prevImage->GetHeight()) / fullImage->GetHeight(),
        numMipLevels-1, numMipLevels);
}

bool
GarchUVTextureData::Read(int degradeLevel, bool generateMipmap)
{   
    TRACE_FUNCTION();

    if (!TfPathExists(_filePath)) {
        TF_WARN("Unable to find Texture '%s'.", _filePath.c_str());
        return false;
    }

    // Read the image from a file, if possible and necessary, a down-sampled
    // version
    const _DegradedImageInput degradedImage = _ReadDegradedImageInput(
                            generateMipmap, _params.targetMemory, degradeLevel);
    if (degradedImage.images.size()<1) {
        TF_WARN("Unable to load Texture '%s'.", _filePath.c_str());
        return false;
    }

    // Load the first mip to extract important data
    GarchImageSharedPtr image = degradedImage.images[0];
    _glFormat = image->GetFormat();
    _glType   = image->GetType();
    _targetMemory = _params.targetMemory;
    _wrapInfo.hasWrapModeS =
        image->GetSamplerMetadata(GL_TEXTURE_WRAP_S, &_wrapInfo.wrapModeS);
    _wrapInfo.hasWrapModeT =
        image->GetSamplerMetadata(GL_TEXTURE_WRAP_T, &_wrapInfo.wrapModeT);
    _size = 0;
    _nativeWidth = _resizedWidth = image->GetWidth();
    _nativeHeight = _resizedHeight = image->GetHeight();

    bool isCompressed = GarchIsCompressedFormat(image->GetFormat());
    bool needsCropping = _params.cropTop || _params.cropBottom ||
                         _params.cropLeft || _params.cropRight;
    bool needsResizeOnLoad = false;
    int cropTop = 0, cropBottom = 0, cropLeft = 0, cropRight = 0;

    if (isCompressed) {
        // When using compressed formats the bytesPerPixel is not 
        // used and the glFormat matches the glInternalFormat.
        _bytesPerPixel = image->GetBytesPerPixel(); 
        _glInternalFormat = _glFormat;
    } else {
        _bytesPerPixel = GarchGetNumElements(_glFormat) *
                         GarchGetElementSize(_glType);
        _glInternalFormat = _GLInternalFormatFromImageData(
                                _glFormat, _glType, image->IsColorSpaceSRGB());

        if (needsCropping) {
            TRACE_SCOPE("GarchUVTextureData::Read(int, bool) (cropping)");

            // The cropping parameters are with respect to the original image,
            // we need to scale them if we have a down-sampled image.
            // Usually, we crop the slates that are black and the boundary might
            // not hit a pixel boundary of the down-sampled image and thus black
            // bleeds into the pixels near the border of the texture. To avoid
            // this, we use ceil here to cut out the pixels with black bleeding.
            cropTop    = ceil(_params.cropTop * degradedImage.scaleY);
            cropBottom = ceil(_params.cropBottom * degradedImage.scaleY);
            cropLeft   = ceil(_params.cropLeft * degradedImage.scaleX);
            cropRight  = ceil(_params.cropRight * degradedImage.scaleX);

            _resizedWidth = std::max(0, _resizedWidth - (cropLeft + cropRight));
            _resizedHeight = std::max(0, _resizedHeight - (cropTop + cropBottom));

            needsResizeOnLoad = true;
        }

        const double scale = generateMipmap ? 4.0 / 3 : 1.0;
        int sizeAprox = _resizedWidth * _resizedHeight * _bytesPerPixel * scale;

        while ((_targetMemory > 0) 
               && (static_cast<size_t>(sizeAprox) > _targetMemory)) {
            _resizedWidth >>= 1;
            _resizedHeight >>= 1;
            sizeAprox = _resizedWidth * _resizedHeight * _bytesPerPixel * scale;
            needsResizeOnLoad = true;
        }

        if (_targetMemory == 0) {
            for (int i = 0; i < degradeLevel; i++) {
                _resizedWidth >>= 1;
                _resizedHeight >>= 1;
                needsResizeOnLoad = true;
            }
        }
    }

    // Check if the image is providing a mip chain and check if it is valid
    // Also, if the user wants cropping/resize then the mip chain 
    // will be discarded.
    bool usePregeneratedMips = !needsResizeOnLoad && generateMipmap;
    int numMipLevels = usePregeneratedMips ? degradedImage.images.size() : 1;

    // If rawbuffer has any memory let's clean it now before we load the 
    // new textures in memory
    _rawBufferMips.clear();
    _rawBufferMips.resize(numMipLevels);

    // Read the metadata for the degraded mips in the structure that keeps
    // track of all the mips
    for(int i = 0 ; i < numMipLevels; i++) {
        GarchImageSharedPtr image = degradedImage.images[i];
        if (!image) {
            TF_RUNTIME_ERROR("Unable to load mip from Texture '%s'.", 
                _filePath.c_str());
            return false;
        }

        // Create the new mipmap
        Mip & mip  = _rawBufferMips[i];
        mip.width  = needsResizeOnLoad ? _resizedWidth : image->GetWidth();
        mip.height = needsResizeOnLoad ? _resizedHeight : image->GetHeight();
        
        const size_t numPixels = mip.width * mip.height;
        mip.size   = isCompressed ? GarchGetCompressedTextureSize( 
                                     mip.width, mip.height, _glFormat, _glType):
                                    numPixels * _bytesPerPixel;
        mip.offset = _size;
        _size += mip.size;
    }

    _rawBuffer.reset(new unsigned char[_size]);
    if (!_rawBuffer) {
        TF_RUNTIME_ERROR("Unable to allocate memory for the mip levels.");
        return false;
    }

    // Read the actual mips from each image and store them in a big buffer of
    // contiguous memory.
    for(int i = 0 ; i < numMipLevels; i++) {
        GarchImageSharedPtr image = degradedImage.images[i];
        if (!image) {
            TF_RUNTIME_ERROR("Unable to load mip from Texture '%s'.", 
                _filePath.c_str());
            return false;
        }

        Mip & mip  = _rawBufferMips[i];
        GarchImage::StorageSpec storage;
        storage.width = mip.width;
        storage.height = mip.height;
        storage.format = _glFormat;
        storage.type = _glType;
        storage.data = _rawBuffer.get() + mip.offset;

        if (!image->ReadCropped(cropTop, cropBottom, cropLeft, cropRight, storage)) {
            TF_WARN("Unable to read Texture '%s'.", _filePath.c_str());
            return false;
        }
    }

    return true;
}

size_t 
GarchUVTextureData::ComputeBytesUsedByMip(int mipLevel) const
{
    // Returns specific mip level sizes
    if (static_cast<size_t>(mipLevel) >= _rawBufferMips.size()) return 0;
    return _rawBufferMips[mipLevel].size;
}

size_t
GarchUVTextureData::ComputeBytesUsed() const
{
    return _size;
}

bool 
GarchUVTextureData::HasRawBuffer(int mipLevel) const
{
    if (static_cast<size_t>(mipLevel) >= _rawBufferMips.size()) return false;
    return (_rawBufferMips[mipLevel].size > 0);
}

unsigned char * 
GarchUVTextureData::GetRawBuffer(int mipLevel) const
{
    if (static_cast<size_t>(mipLevel) >= _rawBufferMips.size() || !_rawBuffer) {
        return 0;
    }
    return _rawBuffer.get() + _rawBufferMips[mipLevel].offset;
}

int 
GarchUVTextureData::ResizedWidth(int mipLevel) const 
{
    if (static_cast<size_t>(mipLevel) >= _rawBufferMips.size()) return 0;
    return _rawBufferMips[mipLevel].width;
}

int 
GarchUVTextureData::ResizedHeight(int mipLevel) const 
{
    if (static_cast<size_t>(mipLevel) >= _rawBufferMips.size()) return 0;
    return _rawBufferMips[mipLevel].height;
}

int 
GarchUVTextureData::GetNumMipLevels() const 
{
    return (int)_rawBufferMips.size();   
}

PXR_NAMESPACE_CLOSE_SCOPE

