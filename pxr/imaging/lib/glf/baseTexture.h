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
#ifndef GLF_BASETEXTURE_H
#define GLF_BASETEXTURE_H

/// \file glf/uvTexture.h

#include "pxr/pxr.h"
#include "pxr/imaging/glf/api.h"

#include "pxr/imaging/garch/baseTexture.h"

#include "pxr/base/tf/declarePtrs.h"
#include "pxr/base/tf/refPtr.h"
#include "pxr/base/tf/weakPtr.h"

#include "pxr/imaging/garch/gl.h"

#include <string>

PXR_NAMESPACE_OPEN_SCOPE


TF_DECLARE_WEAK_AND_REF_PTRS(GarchBaseTexture);
TF_DECLARE_WEAK_AND_REF_PTRS(GarchBaseTextureData);

/// \class GlfBaseTexture
///
/// Represents a texture object in Glf
///
class GlfBaseTexture : public GarchBaseTexture {
public:
    GLF_API
    virtual ~GlfBaseTexture();

    /// Returns the OpenGl texture name for the texture. 
    GLF_API
    GarchTextureGPUHandle GetAPITextureName();

    GLF_API
    int	GetWidth();

    GLF_API
    int GetHeight();

    GLF_API
    int GetFormat();

    // GarchTexture overrides
    GLF_API
    virtual BindingVector GetBindings(TfToken const & identifier,
                                      GarchSamplerGPUHandle samplerName) override;

protected:
    
    GLF_API
    GlfBaseTexture();

    GLF_API
    GlfBaseTexture(GarchImage::ImageOriginLocation originLocation);

    friend class GlfResourceFactory;
    
    GLF_API
    virtual void _ReadTexture() override { TF_FATAL_CODING_ERROR("Not Implemented!"); } //MTL_FIXME

    GLF_API
    void _UpdateTexture(GarchBaseTextureDataConstPtr texData);
    GLF_API
    void _CreateTexture(GarchBaseTextureDataConstPtr texData,
                        bool const useMipmaps,
                        int const unpackCropTop = 0,
                        int const unpackCropBottom = 0,
                        int const unpackCropLeft = 0,
                        int const unpackCropRight = 0);
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif // GLF_BASETEXTURE_H
