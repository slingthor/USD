//
// Copyright 2020 Pixar
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
#ifndef PXR_IMAGING_GARCH_VDB_TEXTURE_CONTAINER_H
#define PXR_IMAGING_GARCH_VDB_TEXTURE_CONTAINER_H

/// \file garch/vdbTextureContainer.h

#include "pxr/pxr.h"
#include "pxr/imaging/garch/api.h"
#include "pxr/imaging/garch/texture.h"
#include "pxr/imaging/garch/textureContainer.h"

#include "pxr/base/tf/declarePtrs.h"

PXR_NAMESPACE_OPEN_SCOPE

TF_DECLARE_WEAK_AND_REF_PTRS(GarchTextureHandle);

TF_DECLARE_WEAK_AND_REF_PTRS(GarchVdbTextureContainer);

/// \class GarchVdbTextureContainer
///
/// A container for 3-dimension textures read from the grids in an OpenVDB file
///
class GarchVdbTextureContainer : public GarchTextureContainer<TfToken> {
public:
    /// Creates a new container for the OpenVDB file \p filePath
    GARCH_API
    static GarchVdbTextureContainerRefPtr New(TfToken const &filePath);

    /// Creates a new container for the OpenVDB file \p filePath
    GARCH_API
    static GarchVdbTextureContainerRefPtr New(std::string const &filePath);

    GARCH_API
    ~GarchVdbTextureContainer() override;

    /// Returns invalid texture name.
    /// 
    /// Clients are supposed to get texture information from the GarchVdbTexture
    /// returned by GarchVdbTextureContainer::GetTextureHandle()
    GARCH_API
    GarchTextureGPUHandle GetTextureName() override;

    
    /// Returns empty vector.
    /// 
    /// Clients are supposed to get texture information from the GarchVdbTexture
    /// returned by GarchVdbTextureContainer::GetTextureHandle()
    GARCH_API
    BindingVector GetBindings(TfToken const & identifier,
                              GarchSamplerGPUHandle const& samplerName) override;

    /// Returns empty dict.
    /// 
    /// Clients are supposed to get texture information from the GarchVdbTexture
    /// returned by GarchVdbTextureContainer::GetTextureHandle()
    GARCH_API
    VtDictionary GetTextureInfo(bool forceLoad) override;

    /// The file path of the OpenVDB file
    ///
    GARCH_API
    TfToken const &GetFilePath() const { return _filePath; }

protected:
    GARCH_API
    void _ReadTexture() override {}
    
private:
    GarchVdbTextureContainer(TfToken const &filePath);

    GarchTextureRefPtr _CreateTexture(TfToken const &identifier) override;

private:
    const TfToken _filePath;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif
