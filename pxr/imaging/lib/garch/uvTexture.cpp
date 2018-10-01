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
/// \file UVTexture.cpp
// 

#include "pxr/imaging/garch/arrayTexture.h"
#include "pxr/imaging/garch/image.h"
#include "pxr/imaging/garch/resourceFactory.h"
#include "pxr/imaging/garch/uvTexture.h"
#include "pxr/imaging/garch/uvTextureData.h"
#include "pxr/imaging/garch/utils.h"

#include "pxr/base/tf/registryManager.h"
#include "pxr/base/tf/type.h"

PXR_NAMESPACE_OPEN_SCOPE


// Custom factory to handle UVTexture and ArrayTexture for same types.
class Garch_UVTextureFactory : public GarchTextureFactoryBase {
public:
    virtual GarchTextureRefPtr New(const TfToken& texturePath,
                                   GarchImage::ImageOriginLocation originLocation =
                                                GarchImage::OriginUpperLeft) const
    {
        return GarchUVTexture::New(texturePath,
                                   /*cropTop*/ 0,
                                   /*cropBottom*/ 0,
                                   /*cropLeft*/ 0,
                                   /*cropRight*/ 0,
                                   originLocation);
    }

    virtual GarchTextureRefPtr New(const TfTokenVector& texturePaths,
                                   GarchImage::ImageOriginLocation originLocation =
                                                GarchImage::OriginUpperLeft) const
    {
        return GarchArrayTexture::New(texturePaths,
                                      texturePaths.size(),
                                      /*cropTop*/ 0,
                                      /*cropBottom*/ 0,
                                      /*cropLeft*/ 0,
                                      /*cropRight*/ 0,
                                      originLocation);
    }
};

TF_REGISTRY_FUNCTION(TfType)
{
    typedef GarchUVTexture Type;
    TfType t = TfType::Define<Type, TfType::Bases<GarchBaseTexture> >();
    t.SetFactory< Garch_UVTextureFactory >();
}

GarchUVTextureRefPtr
GarchUVTexture::New(
    TfToken const &imageFilePath,
    unsigned int cropTop,
    unsigned int cropBottom,
    unsigned int cropLeft,
    unsigned int cropRight,
    GarchImage::ImageOriginLocation originLocation)
{
    return TfCreateRefPtr(new GarchUVTexture(
            GarchResourceFactory::GetInstance()->NewBaseTexture(),
            imageFilePath, cropTop,
            cropBottom, cropLeft, cropRight,
            originLocation));
}

GarchUVTextureRefPtr 
GarchUVTexture::New(
    std::string const &imageFilePath,
    unsigned int cropTop,
    unsigned int cropBottom,
    unsigned int cropLeft,
    unsigned int cropRight,
    GarchImage::ImageOriginLocation originLocation)
{
    return TfCreateRefPtr(new GarchUVTexture(
            GarchResourceFactory::GetInstance()->NewBaseTexture(),
            TfToken(imageFilePath), cropTop, 
            cropBottom, cropLeft, cropRight,
            originLocation));
}

bool 
GarchUVTexture::IsSupportedImageFile(TfToken const &imageFilePath)
{
    return IsSupportedImageFile(imageFilePath.GetString());
}

bool 
GarchUVTexture::IsSupportedImageFile(std::string const &imageFilePath)
{
    return GarchImage::IsSupportedImageFile(imageFilePath);
}

GarchUVTexture::GarchUVTexture(
    GarchBaseTexture *baseTexture,
    TfToken const &imageFilePath,
    unsigned int cropTop,
    unsigned int cropBottom,
    unsigned int cropLeft,
    unsigned int cropRight,
    GarchImage::ImageOriginLocation originLocation)
    : GarchBaseTexture(originLocation)
    , _baseTexture(baseTexture)
    , _imageFilePath(imageFilePath)
    , _cropTop(cropTop)
    , _cropBottom(cropBottom)
    , _cropLeft(cropLeft)
    , _cropRight(cropRight)
{
    /* nothing */
}

GarchUVTexture::~GarchUVTexture()
{
    if (_baseTexture) {
        delete _baseTexture;
    }
}

VtDictionary
GarchUVTexture::GetTextureInfo(bool forceLoad)
{
    VtDictionary info = GarchBaseTexture::GetTextureInfo(forceLoad);

    info["imageFilePath"] = _imageFilePath;

    return info;
}

bool
GarchUVTexture::IsMinFilterSupported(GLenum filter)
{
    return true;
}

void
GarchUVTexture::_ReadTexture()
{
    GarchUVTextureDataRefPtr texData =
        GarchUVTextureData::New(_GetImageFilePath(), GetMemoryRequested(),
                              _GetCropTop(), _GetCropBottom(),
                              _GetCropLeft(), _GetCropRight());
    if (texData) {
        texData->Read(0, _GenerateMipmap(), GetOriginLocation());
    }
    _UpdateTexture(texData);
    _CreateTexture(texData, _GenerateMipmap());
    _SetLoaded();
}

bool
GarchUVTexture::_GenerateMipmap() const
{
    return true;
}

const TfToken&
GarchUVTexture::_GetImageFilePath() const
{
    return _imageFilePath;
}

PXR_NAMESPACE_CLOSE_SCOPE

