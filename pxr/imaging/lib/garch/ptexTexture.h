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
#ifndef GARCH_PTEXTEXTURE_H
#define GARCH_PTEXTEXTURE_H

/// \file garch/ptexTexture.h

#include "pxr/pxr.h"
#include "pxr/imaging/garch/api.h"

#include <string>

PXR_NAMESPACE_OPEN_SCOPE


/// Returns true if the file given by \p imageFilePath represents a ptex file,
/// and false otherwise.
///
/// This function simply checks the extension of the file name and does not
/// otherwise guarantee that the file is in any way valid for reading.
///
/// If ptex support is disabled, this function will always return false.
///
GARCH_API bool GarchIsSupportedPtexTexture(std::string const & imageFilePath);


PXR_NAMESPACE_CLOSE_SCOPE

#ifdef PXR_PTEX_SUPPORT_ENABLED

#include "pxr/imaging/garch/texture.h"

#include "pxr/base/tf/declarePtrs.h"
#include "pxr/base/tf/token.h"
#include "pxr/base/tf/refPtr.h"
#include "pxr/base/tf/weakPtr.h"

PXR_NAMESPACE_OPEN_SCOPE

TF_DECLARE_WEAK_AND_REF_PTRS(GarchPtexTexture);

/// \class GarchPtexTexture
///
/// Represents a Ptex (per-face texture) object in Glf.
///
/// A MtlfPtexTexture is currently defined by a file path to a valid Ptex file.
/// The current implementation declares _texels as a GL_TEXTURE_2D_ARRAY of n
/// pages of a resolution that matches that of the largest face in the Ptex
/// file.
///
/// Two GL_TEXTURE_BUFFER constructs are used as lookup tables: 
/// * _pages stores the array index in which a given face is located
/// * _layout stores 4 float coordinates : top-left corner and width/height for each face
///
/// GLSL fragments use gl_PrimitiveID and gl_TessCoords to access the _pages and _layout 
/// indirection tables, which provide then texture coordinates for the texels stored in
/// the _texels texture array.
///

class GarchPtexTexture : public GarchTexture {
public:
    GARCH_API
    virtual ~GarchPtexTexture();

    /// Creates a new instance.
    GARCH_API
    static GarchPtexTextureRefPtr New(const TfToken &imageFilePath);

    /// GarchTexture overrides
    GARCH_API
    virtual BindingVector GetBindings(TfToken const & identifier,
                                      GarchSamplerGPUHandle samplerId = GarchSamplerGPUHandle()) override;
    
    GARCH_API
    virtual VtDictionary GetTextureInfo(bool forceLoad) override;

    GARCH_API
    virtual bool IsMinFilterSupported(GLenum filter) override;

    GARCH_API
    virtual bool IsMagFilterSupported(GLenum filter) override;

    // get/set guttering control variables
    static int GetGutterWidth() { return _gutterWidth; }

    static int GetPageMargin() { return _pageMargin; }

    // return Graphics API texture for layout texture buffer
    GARCH_API
    virtual GarchTextureGPUHandle GetLayoutTextureName();

    // return Graphics API texture for texels data texture
    GARCH_API
    virtual GarchTextureGPUHandle GetTexelsTextureName();
	
	GARCH_API
    virtual GarchTextureGPUHandle GetTextureName() override;

protected:
    GARCH_API
    GarchPtexTexture(const TfToken &imageFilePath);

    GARCH_API
    virtual void _FreePtexTextureObject();
    
    GARCH_API
    virtual void _ReadTexture() override;

    GARCH_API
    virtual void _OnMemoryRequestedDirty();

    GARCH_API
    virtual bool _ReadImage() = 0;

    bool _loaded;
    
    GarchTextureGPUHandle _layout;   // per-face lookup table
    GarchTextureGPUHandle _texels;   // texel data

    int _width, _height, _depth;
    int _format;

    static int _gutterWidth, _pageMargin;

    const TfToken	_imageFilePath;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_PTEX_SUPPORT_ENABLED

#endif // GARCH_PTEXTEXTURE_H
