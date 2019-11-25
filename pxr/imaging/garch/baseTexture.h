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
#ifndef GARCH_BASETEXTURE_H
#define GARCH_BASETEXTURE_H

/// \file garch/uvTexture.h

#include "pxr/pxr.h"
#include "pxr/imaging/garch/api.h"
#include "pxr/imaging/garch/image.h"
#include "pxr/imaging/garch/texture.h"

#include "pxr/base/tf/declarePtrs.h"
#include "pxr/base/tf/refPtr.h"
#include "pxr/base/tf/weakPtr.h"

#include "pxr/imaging/garch/gl.h"

#include <string>

PXR_NAMESPACE_OPEN_SCOPE


TF_DECLARE_WEAK_AND_REF_PTRS(GarchBaseTexture);
TF_DECLARE_WEAK_AND_REF_PTRS(GarchBaseTextureData);

/// \class GarchBaseTexture
///
/// Represents a texture object in Garch
///
class GarchBaseTexture : public GarchTexture {
public:
    GARCH_API
    virtual ~GarchBaseTexture();

    /// Returns the GPU API texture handle for the texture.
    GARCH_API
    GarchTextureGPUHandle GetAPITextureName();
    
    /// Is this a 1-, 2- or 3-dimensional texture.
    GARCH_API
    virtual int GetNumDimensions() const = 0;
    
    GARCH_API
    int GetWidth();
    
    GARCH_API
    int GetHeight();
    
    GARCH_API
    int GetDepth();
    
    GARCH_API
    int GetFormat();

    // GarchTexture overrides
    /// Returns the GPU API texture object for the texture.
    GARCH_API
    virtual GarchTextureGPUHandle GetTextureName() override;

    GARCH_API
    virtual BindingVector GetBindings(TfToken const & identifier,
                                      GarchSamplerGPUHandle samplerName) override = 0;

    GARCH_API
    virtual VtDictionary GetTextureInfo(bool forceLoad) override;

protected:
    
    GARCH_API
    GarchBaseTexture();
    
    GARCH_API
    GarchBaseTexture(GarchImage::ImageOriginLocation originLocation);
    
    GARCH_API
    virtual void _OnMemoryRequestedDirty() override final;
    
    GARCH_API
    virtual void _ReadTexture() override = 0;

    void _ReadTextureIfNotLoaded();

    GARCH_API
    virtual void _UpdateTexture(GarchBaseTextureDataConstPtr texData) = 0;
    GARCH_API
    virtual void _CreateTexture(GarchBaseTextureDataConstPtr texData,
                                bool const useMipmaps,
                                int const unpackCropTop = 0,
                                int const unpackCropBottom = 0,
                                int const unpackCropLeft = 0,
                                int const unpackCropRight = 0,
                                int const unpackCropFront = 0,
                                int const unpackCropBack = 0) = 0;

    GARCH_API
    virtual void _SetLoaded();

    friend class GarchUVTexture;
    friend class GarchUVTextureStorage;
    friend class GarchVdbTexture;

    // texture object
    GarchTextureGPUHandle _textureName;

    // required for stats/tracking
    bool    _loaded;
    int     _currentWidth, _currentHeight, _currentDepth;
    int     _format;
    bool    _hasWrapModeS;
    bool    _hasWrapModeT;
    bool    _hasWrapModeR;
    GLenum	_wrapModeS;
    GLenum	_wrapModeT;
    GLenum  _wrapModeR;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif // GARCH_BASETEXTURE_H
