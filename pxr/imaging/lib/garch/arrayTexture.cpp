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
/// \file ArrayTexture.cpp
// 

#include "pxr/imaging/garch/arrayTexture.h"
#include "pxr/imaging/garch/resourceFactory.h"
#include "pxr/imaging/garch/uvTextureData.h"
#include "pxr/imaging/garch/utils.h"
#include "pxr/base/tf/registryManager.h"
#include "pxr/base/tf/type.h"
#include "pxr/base/trace/trace.h"

PXR_NAMESPACE_OPEN_SCOPE


TF_REGISTRY_FUNCTION(TfType)
{
    TfType::Define<GarchArrayTexture, TfType::Bases<GarchUVTexture> >();
}

GarchArrayTextureRefPtr GarchArrayTexture::New(TfTokenVector const &imageFilePaths,
                                               unsigned int arraySize,
                                               unsigned int cropTop,
                                               unsigned int cropBottom,
                                               unsigned int cropLeft,
                                               unsigned int cropRight,
                                               GarchImage::ImageOriginLocation originLocation)
{
    if (imageFilePaths.empty()) {
        // Need atleast one valid image file path.
        TF_CODING_ERROR("Attempting to create an array texture with 0 texture file paths.");
        return TfNullPtr;
    }

    return GarchResourceFactory::GetInstance()->NewArrayTexture(imageFilePaths, arraySize,
                                                                cropTop, cropBottom,
                                                                cropLeft, cropRight,
                                                                originLocation);
}


GarchArrayTextureRefPtr
GarchArrayTexture::New(
    std::vector<std::string> const &imageFilePaths,
    unsigned int arraySize,
    unsigned int cropTop,
    unsigned int cropBottom,
    unsigned int cropLeft,
    unsigned int cropRight,
    GarchImage::ImageOriginLocation originLocation)
{
    TfTokenVector imageFilePathTokens(imageFilePaths.begin(), imageFilePaths.end());

    return GarchResourceFactory::GetInstance()->NewArrayTexture(imageFilePathTokens, arraySize,
                                                                cropTop, cropBottom,
                                                                cropLeft, cropRight,
                                                                originLocation);
}

bool 
GarchArrayTexture::IsSupportedImageFile(TfToken const &imageFilePath)
{
    return GarchUVTexture::IsSupportedImageFile(imageFilePath);
}

GarchArrayTexture::GarchArrayTexture(
    TfTokenVector const &imageFilePaths,
    unsigned int arraySize,
    unsigned int cropTop,
    unsigned int cropBottom,
    unsigned int cropLeft,
    unsigned int cropRight,
    GarchImage::ImageOriginLocation originLocation)
    
    : GarchUVTexture(this,
                     imageFilePaths[0],
                     cropTop,
                     cropBottom,
                     cropLeft,
                     cropRight,
                     originLocation),

      _imageFilePaths(imageFilePaths),
    _arraySize(arraySize)
{
    // do nothing
}

void
GarchArrayTexture::_OnSetMemoryRequested(size_t targetMemory)
{
    GarchBaseTextureDataConstRefPtrVector texDataVec(_arraySize, 0);
    for (size_t i = 0; i < _arraySize; ++i) {
        GarchUVTextureDataRefPtr texData =
            GarchUVTextureData::New(_GetImageFilePath(i), targetMemory,
                                  _GetCropTop(), _GetCropBottom(),
                                  _GetCropLeft(), _GetCropRight());
        if (texData) {
            texData->Read(0, _GenerateMipmap());
        }

        _UpdateTexture(texData);

        if (texData && texData->HasRawBuffer()) {
            texDataVec[i] = texData;
        } else {
            TF_WARN("Invalid texture data for texture file: %s",
                    _GetImageFilePath(i).GetString().c_str());
        }
    }

    _CreateTextures(texDataVec, _GenerateMipmap());
}

/* virtual */
const TfToken&
GarchArrayTexture::_GetImageFilePath(size_t index) const
{
    if (TF_VERIFY(index < _imageFilePaths.size())) {
        return _imageFilePaths[index];
    }
    else {
        return _imageFilePaths.front();
    }
}

PXR_NAMESPACE_CLOSE_SCOPE

