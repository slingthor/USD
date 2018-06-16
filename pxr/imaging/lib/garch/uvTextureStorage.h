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
#ifndef GARCH_UVTEXTURESTORAGE_H
#define GARCH_UVTEXTURESTORAGE_H

/// \file garch/uvTextureStorage.h

#include "pxr/pxr.h"
#include "pxr/imaging/garch/api.h"
#include "pxr/imaging/garch/baseTexture.h"

#include "pxr/base/tf/declarePtrs.h"
#include "pxr/base/vt/value.h"

PXR_NAMESPACE_OPEN_SCOPE


TF_DECLARE_WEAK_AND_REF_PTRS(GarchUVTextureStorage);

/// \class GarchUVTextureStorage
///
/// Represents a texture object in Garch initialized from a VtValue.
///
/// A GarchUVTextureStorage is currently initialized from a float/double, GfVec3d, or GfVec4d.
///
class GarchUVTextureStorage : public GarchBaseTexture {
public:
    /// Creates a new texture instance based on input storageData
    /// \p width, and \p height specify the size
    ///
    GARCH_API
    static GarchUVTextureStorageRefPtr New(
        unsigned int width,
        unsigned int height, 
        const VtValue &storageData);
    
    GARCH_API
    virtual BindingVector GetBindings(TfToken const & identifier,
                                      GarchSamplerGPUHandle samplerName) const override
    {
        return _baseTexture->GetBindings(identifier, samplerName);
    }

protected:
    GARCH_API
    GarchUVTextureStorage(
        GarchBaseTexture *baseTexture,
        unsigned int width,
        unsigned int height, 
        const VtValue &storageData);
    
    GARCH_API
    virtual ~GarchUVTextureStorage();

    GARCH_API
    virtual void _OnSetMemoryRequested(size_t targetMemory);
    GARCH_API
    virtual bool _GenerateMipmap() const;
    
    GARCH_API
    virtual void _UpdateTexture(GarchBaseTextureDataConstPtr texData) override
    {
        return _baseTexture->_UpdateTexture(texData);
    }
    GARCH_API
    virtual void _CreateTexture(GarchBaseTextureDataConstPtr texData,
                                bool const useMipmaps,
                                int const unpackCropTop = 0,
                                int const unpackCropBottom = 0,
                                int const unpackCropLeft = 0,
                                int const unpackCropRight = 0) override
    {
        return _baseTexture->_CreateTexture(texData, useMipmaps,
                                            unpackCropTop, unpackCropBottom,
                                            unpackCropLeft, unpackCropRight);
    }
    
private:
    GarchBaseTexture *_baseTexture;
    unsigned int _width, _height;
    VtValue _storageData;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif // GARCH_UVTEXTURESTORAGE_H
