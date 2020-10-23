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
#ifndef PXR_IMAGING_GARCH_VDB_TEXTURE_DATA_H
#define PXR_IMAGING_GARCH_VDB_TEXTURE_DATA_H

/// \file garch/vdbTextureData.h

#include "pxr/pxr.h"
#include "pxr/base/gf/matrix4d.h"
#include "pxr/base/gf/vec3i.h"
#include "pxr/imaging/garch/api.h"
#include "pxr/imaging/hio/image.h"
#include "pxr/imaging/garch/fieldTextureData.h"

#include "pxr/base/gf/bbox3d.h"

#include <memory>

PXR_NAMESPACE_OPEN_SCOPE

TF_DECLARE_WEAK_AND_REF_PTRS(GarchVdbTextureData);

class GarchVdbTextureData_DenseGridHolderBase;

/// \class GarchVdbTextureData
///
/// Implements GlfBaseTextureData to read grid with given name from
/// OpenVDB file at given path.
///
class GarchVdbTextureData final : public GarchFieldTextureData {
public:
    GARCH_API
    static GarchVdbTextureDataRefPtr
    New(std::string const &filePath,
        std::string const &gridName,
        size_t targetMemory);

    GARCH_API
    const GfBBox3d &GetBoundingBox() const override;

    GARCH_API
    int NumDimensions() const override;

    GARCH_API
    int ResizedWidth(int mipLevel = 0) const override;

    GARCH_API
    int ResizedHeight(int mipLevel = 0) const override;

    GARCH_API
    int ResizedDepth(int mipLevel = 0) const override;

    GARCH_API
    HioFormat GetFormat() const override;
    
    GARCH_API
    size_t TargetMemory() const override;

    GARCH_API
    WrapInfo GetWrapInfo() const override;

    GARCH_API
    size_t ComputeBytesUsed() const override;

    GARCH_API
    size_t ComputeBytesUsedByMip(int mipLevel = 0) const override;

    GARCH_API
    bool Read(int degradeLevel, 
              bool generateMipmap,
              HioImage::ImageOriginLocation
                  originLocation = HioImage::OriginUpperLeft) override;
    
    GARCH_API
    bool HasRawBuffer(int mipLevel = 0) const override;

    GARCH_API
    unsigned char * GetRawBuffer(int mipLevel = 0) const override;

    GARCH_API
    int GetNumMipLevels() const override;

private:
    GarchVdbTextureData(std::string const &filePath,
                        std::string const &gridName,
                        size_t targetMemory);
    ~GarchVdbTextureData() override;

    const std::string _filePath;
    const std::string _gridName;

    const size_t _targetMemory;

    int _nativeWidth, _nativeHeight, _nativeDepth;
    int _resizedWidth, _resizedHeight, _resizedDepth;
    int _bytesPerPixel;
    int _numChannels;

    HioFormat _format;

    WrapInfo _wrapInfo;

    size_t _size;

    GfBBox3d _boundingBox;

    std::unique_ptr<GarchVdbTextureData_DenseGridHolderBase> _denseGrid;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif
