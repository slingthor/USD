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
#ifndef GARCH_ARRAYTEXTURE_H
#define GARCH_ARRAYTEXTURE_H

/// \file garch/arrayTexture.h

#include "pxr/pxr.h"
#include "pxr/imaging/garch/api.h"
#include "pxr/imaging/garch/image.h"
#include "pxr/imaging/garch/uvTexture.h"

#include "pxr/base/tf/declarePtrs.h"
#include "pxr/base/tf/token.h"

#include <string>

PXR_NAMESPACE_OPEN_SCOPE


TF_DECLARE_WEAK_AND_REF_PTRS(GarchArrayTexture);

/// \class GarchArrayTexture
///
/// Represents an array of texture objects in Garch
///
/// An GarchArrayTexture is defined by a set of image file paths.
/// Currently accepted image formats are png, jpg and bmp.
///

class GarchArrayTexture : public GarchUVTexture {
public:

    typedef GarchUVTexture Parent;
    typedef GarchArrayTexture This;
    
    /// Creates a new texture instance for the image file at \p imageFilePath.
    /// If given, \p cropTop, \p cropBottom, \p cropLeft, and \p cropRight
    /// specifies the number of pixels to crop from the indicated border of
    /// the source image.
    GARCH_API
    static GarchArrayTextureRefPtr New(
        TfTokenVector const &imageFilePaths,
        unsigned int arraySize     ,
        unsigned int cropTop    = 0,
        unsigned int cropBottom = 0,
        unsigned int cropLeft   = 0,
        unsigned int cropRight  = 0,
        GarchImage::ImageOriginLocation originLocation =
                            GarchImage::OriginUpperLeft);

    GARCH_API
    static GarchArrayTextureRefPtr New(
        std::vector<std::string> const &imageFilePaths,
        unsigned int arraySize     ,
        unsigned int cropTop    = 0,
        unsigned int cropBottom = 0,
        unsigned int cropLeft   = 0,
        unsigned int cropRight  = 0,
        GarchImage::ImageOriginLocation originLocation =
                            GarchImage::OriginUpperLeft);

    GARCH_API
    static bool IsSupportedImageFile(TfToken const &imageFilePath);

    // GarchBaseTexture overrides
    GARCH_API
    virtual BindingVector GetBindings(TfToken const & identifier,
                                      GarchSamplerGPUHandle samplerName) override = 0;

protected:
    GARCH_API
    virtual void _ReadTexture() override;

    GARCH_API
    virtual void _CreateTextures(GarchBaseTextureDataConstRefPtrVector texDataVec,
                                 bool const generateMipmap) = 0;

    GARCH_API
    GarchArrayTexture(
        TfTokenVector const &imageFilePaths,
        unsigned int arraySize,
        unsigned int cropTop,
        unsigned int cropBottom,
        unsigned int cropLeft,
        unsigned int cropRight,
        GarchImage::ImageOriginLocation originLocation =
                        GarchImage::OriginUpperLeft);

    GARCH_API
    virtual void _OnSetMemoryRequested(size_t targetMemory);
    GARCH_API
    const TfToken& _GetImageFilePath(size_t index) const;
    using GarchUVTexture::_GetImageFilePath;

    TfTokenVector _imageFilePaths;
    const unsigned int _arraySize;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif // GARCH_ARRAYTEXTURE_H
