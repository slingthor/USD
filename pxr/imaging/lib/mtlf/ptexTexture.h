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
#ifndef MTLF_PTEXTEXTURE_H
#define MTLF_PTEXTEXTURE_H

/// \file mtlf/ptexTexture.h

#include "pxr/pxr.h"
#include "pxr/imaging/mtlf/api.h"

#include <string>

#ifdef PXR_PTEX_SUPPORT_ENABLED

#include "pxr/imaging/garch/ptexTexture.h"

PXR_NAMESPACE_OPEN_SCOPE

TF_DECLARE_WEAK_AND_REF_PTRS(MtlfPtexTexture);

/// \class MtlfPtexTexture
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

class MtlfPtexTexture : public GarchPtexTexture {
public:
    MTLF_API
    virtual ~MtlfPtexTexture();

protected:
    MTLF_API
    MtlfPtexTexture(const TfToken &imageFilePath);
    
    friend class MtlfResourceFactory;

    MTLF_API
    virtual void _FreePtexTextureObject() override;

    MTLF_API
    virtual bool _ReadImage() override;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_PTEX_SUPPORT_ENABLED

#endif // MTLF_TEXTURE_H
