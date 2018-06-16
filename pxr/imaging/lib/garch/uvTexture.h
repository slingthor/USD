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
#ifndef GARCH_UVTEXTURE_H
#define GARCH_UVTEXTURE_H

/// \file garch/uvTexture.h

#include "pxr/pxr.h"
#include "pxr/imaging/garch/api.h"
#include "pxr/imaging/garch/baseTexture.h"

#include "pxr/base/tf/declarePtrs.h"
#include "pxr/base/tf/token.h"

#include <string>

PXR_NAMESPACE_OPEN_SCOPE


TF_DECLARE_WEAK_AND_REF_PTRS(GarchUVTexture);

/// \class GarchUVTexture
///
/// Represents a texture object in Garch.
///
/// An GarchUVTexture is currently defined by an image file path.
/// Currently accepted image formats are png, jpg and bmp.
///
class GarchUVTexture : public GarchBaseTexture {
public:
    /// Creates a new texture instance for the image file at \p imageFilePath.
    /// If given, \p cropTop, \p cropBottom, \p cropLeft, and \p cropRight
    /// specifies the number of pixels to crop from the indicated border of
    /// the source image.
    GARCH_API
    static GarchUVTextureRefPtr New(
        TfToken const &imageFilePath,
        unsigned int cropTop    = 0,
        unsigned int cropBottom = 0,
        unsigned int cropLeft   = 0,
        unsigned int cropRight  = 0);

    GARCH_API
    static GarchUVTextureRefPtr New(
        std::string const &imageFilePath,
        unsigned int cropTop    = 0,
        unsigned int cropBottom = 0,
        unsigned int cropLeft   = 0,
        unsigned int cropRight  = 0);
    
    /// Returns true if the file at \p imageFilePath is an image that
    /// can be used with this texture object.
    GARCH_API
    static bool IsSupportedImageFile(TfToken const &imageFilePath);
    GARCH_API
    static bool IsSupportedImageFile(std::string const &imageFilePath);

    GARCH_API
    virtual VtDictionary GetTextureInfo() const;

    GARCH_API
    virtual bool IsMinFilterSupported(GLenum filter);
    
    GARCH_API
    virtual BindingVector GetBindings(TfToken const & identifier,
                                      GarchSamplerGPUHandle samplerName) const override
    {
        return _baseTexture->GetBindings(identifier, samplerName);
    }

protected:
    GARCH_API
    GarchUVTexture(
        GarchBaseTexture *baseTexture,
        TfToken const &imageFilePath,
        unsigned int cropTop,
        unsigned int cropBottom,
        unsigned int cropLeft,
        unsigned int cropRight);
    
    GARCH_API
    virtual ~GarchUVTexture();

    GARCH_API
    virtual void _OnSetMemoryRequested(size_t targetMemory);
    GARCH_API
    virtual bool _GenerateMipmap() const;
    GARCH_API
    const TfToken& _GetImageFilePath() const;
    unsigned int _GetCropTop() const {return _cropTop;}
    unsigned int _GetCropBottom() const {return _cropBottom;}
    unsigned int _GetCropLeft() const {return _cropLeft;}
    unsigned int _GetCropRight() const {return _cropRight;}
    
    GARCH_API
    virtual void _UpdateTexture(GarchBaseTextureDataConstPtr texData) override
    {
        _baseTexture->_UpdateTexture(texData);
    }
    GARCH_API
    virtual void _CreateTexture(GarchBaseTextureDataConstPtr texData,
                                bool const useMipmaps,
                                int const unpackCropTop = 0,
                                int const unpackCropBottom = 0,
                                int const unpackCropLeft = 0,
                                int const unpackCropRight = 0) override
    {
        _baseTexture->_CreateTexture(texData, useMipmaps,
                                     unpackCropTop, unpackCropBottom,
                                     unpackCropLeft, unpackCropRight);
    }

private:
    GarchBaseTexture *_baseTexture;
    const TfToken _imageFilePath;
    const unsigned int _cropTop;
    const unsigned int _cropBottom;
    const unsigned int _cropLeft;
    const unsigned int _cropRight;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif // GARCH_UVTEXTURE_H
