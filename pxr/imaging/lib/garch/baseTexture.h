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

    /// Returns the OpenGl texture name for the texture. 
    GarchTextureGPUHandle GetGlTextureName() const {
        return _textureName;
    }

    int	GetWidth() const {
        return _currentWidth;
    }

    int GetHeight() const {
        return _currentHeight;
    }

    int GetFormat() const {
        return _format;
    }

    // GarchTexture overrides
    /// Returns the GPU API texture object for the texture.
    GARCH_API
    virtual GarchTextureGPUHandle GetTextureName() const override {
        return _textureName;
    }

    GARCH_API
    virtual BindingVector GetBindings(TfToken const & identifier,
                                      GarchSamplerGPUHandle samplerName) const override = 0;

    GARCH_API
    virtual VtDictionary GetTextureInfo() const override;

protected:
    
    GARCH_API
    GarchBaseTexture();

    GARCH_API
    virtual void _UpdateTexture(GarchBaseTextureDataConstPtr texData) = 0;
    GARCH_API
    virtual void _CreateTexture(GarchBaseTextureDataConstPtr texData,
                                bool const useMipmaps,
                                int const unpackCropTop = 0,
                                int const unpackCropBottom = 0,
                                int const unpackCropLeft = 0,
                                int const unpackCropRight = 0) = 0;

    friend class GarchUVTexture;
    friend class GarchUVTextureStorage;

    // texture object
    GarchTextureGPUHandle _textureName;

    // required for stats/tracking
    int     _currentWidth, _currentHeight;
    int     _format;
    bool    _hasWrapModeS;
    bool    _hasWrapModeT;
    GLenum	_wrapModeS;
    GLenum	_wrapModeT;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif // GARCH_BASETEXTURE_H
