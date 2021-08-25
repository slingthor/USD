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
#ifndef PXR_IMAGING_HD_TEXTURE_RESOURCE_H
#define PXR_IMAGING_HD_TEXTURE_RESOURCE_H

#include "pxr/pxr.h"
#include "pxr/imaging/hd/api.h"
#include "pxr/imaging/hd/enums.h"

#include "pxr/imaging/garch/texture.h"
#include "pxr/imaging/garch/textureHandle.h"
#include "pxr/imaging/garch/gl.h"

#include "pxr/base/tf/token.h"

#include <memory>
#include <cstdint>

PXR_NAMESPACE_OPEN_SCOPE


using HdTextureResourceSharedPtr = std::shared_ptr<class HdTextureResource>;

///
/// \class HdTextureResource
///
/// \deprecated It is now the responsibility of the render delegate to load
/// the texture (using the file path authored on the texture node in a
/// material network).
///
class HdTextureResource {
public:
    typedef size_t ID;

    /// Returns the hash value of the texture for \a sourceFile
    HD_API
    static ID ComputeHash(TfToken const & sourceFile);

    HD_API
    virtual ~HdTextureResource();

    virtual HdTextureType GetTextureType() const = 0;

    virtual size_t GetMemoryUsed() = 0;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif //PXR_IMAGING_HD_TEXTURE_RESOURCE_H