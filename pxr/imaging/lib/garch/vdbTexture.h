//
// Copyright 2019 Pixar
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
#ifndef GARCH_VDB_TEXTURE_H
#define GARCH_VDB_TEXTURE_H

/// \file garch/vdbTexture.h

#include "pxr/pxr.h"
#include "pxr/imaging/garch/api.h"
#include "pxr/imaging/garch/baseTexture.h"

#include "pxr/base/gf/bbox3d.h"
#include "pxr/base/tf/declarePtrs.h"

PXR_NAMESPACE_OPEN_SCOPE

TF_DECLARE_WEAK_AND_REF_PTRS(GarchVdbTexture);

/// \class GarchVdbTexture
///
/// Represents a 3-dimensional texture read from an OpenVDB file.
///
/// Current limitations: we always use the first grid in the OpenVDB file.
/// The texture is always loaded at the full resolution of the OpenVDB grid,
/// ignoring the memory request.
///
class GarchVdbTexture : public GarchBaseTexture {
public:
    /// Creates a new texture instance for the OpenVDB file at \p filePath
    GARCH_API
    static GarchVdbTextureRefPtr New(TfToken const &filePath);

    /// Creates a new texture instance for the OpenVDB file at \p filePath
    GARCH_API
    static GarchVdbTextureRefPtr New(std::string const &filePath);

    /// Returns the transform of the grid in the OpenVDB file as well as the
    /// bounding box of the samples in the corresponding OpenVDB tree.
    ///
    /// This pair of information is encoded as GfBBox3d.
    GARCH_API
    const GfBBox3d &GetBoundingBox();

    int GetNumDimensions() const override;
    
    GARCH_API
    VtDictionary GetTextureInfo(bool forceLoad) override;

    GARCH_API
    bool IsMinFilterSupported(GLenum filter) override;
    
    GARCH_API
    virtual BindingVector GetBindings(TfToken const & identifier,
                                      GarchSamplerGPUHandle samplerName) override
    {
        return _baseTexture->GetBindings(identifier, samplerName);
    }

protected:
    GARCH_API
    GarchVdbTexture(GarchBaseTexture *baseTexture, TfToken const &filePath);
    
    friend class GlfResourceFactory;
    friend class MtlfResourceFactory;

    GARCH_API
    void _ReadTexture() override;
    
    GARCH_API
    bool _GenerateMipmap() const;
    
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
                                int const unpackCropRight = 0,
                                int const unpackCropFront = 0,
                                int const unpackCropBack = 0) override
    {
        _baseTexture->_CreateTexture(texData, useMipmaps,
                                     unpackCropTop, unpackCropBottom,
                                     unpackCropLeft, unpackCropRight);
    }
    
    GARCH_API
    virtual void _SetLoaded() override
    {
        _baseTexture->_SetLoaded();
        GarchBaseTexture::_SetLoaded();
    }
    
private:
    GarchBaseTexture *_baseTexture;
    
    const TfToken _filePath;

    GfBBox3d _boundingBox;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif // GARCH_VDBTEXTURE_H
