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
#include "pxr/pxr.h"
#include "pxr/imaging/hgi/types.h"
#include "pxr/base/tf/diagnostic.h"

PXR_NAMESPACE_OPEN_SCOPE

size_t
HgiGetComponentCount(const HgiFormat f)
{
    switch (f) {
    case HgiFormatUNorm8:
    case HgiFormatSNorm8:
    case HgiFormatFloat16:
    case HgiFormatFloat32:
    case HgiFormatInt32:
    case HgiFormatFloat32UInt8: // treat as a single component
        return 1;
    case HgiFormatUNorm8Vec2:
    case HgiFormatSNorm8Vec2:
    case HgiFormatFloat16Vec2:
    case HgiFormatFloat32Vec2:
    case HgiFormatInt32Vec2:
        return 2;
    // case HgiFormatUNorm8Vec3: // Unsupported Metal (MTLPixelFormat)
    // case HgiFormatSNorm8Vec3: // Unsupported Metal (MTLPixelFormat)
    case HgiFormatFloat16Vec3:
    case HgiFormatFloat32Vec3:
    case HgiFormatInt32Vec3:
    case HgiFormatBC6FloatVec3:
    case HgiFormatBC6UFloatVec3:
        return 3;
    case HgiFormatUNorm8Vec4:
    case HgiFormatSNorm8Vec4:
    case HgiFormatFloat16Vec4:
    case HgiFormatFloat32Vec4:
    case HgiFormatInt32Vec4:
    case HgiFormatBC7UNorm8Vec4:
    case HgiFormatBC7UNorm8Vec4srgb:
    case HgiFormatUNorm8Vec4srgb:
        return 4;
    case HgiFormatCount:
    case HgiFormatInvalid:
        TF_CODING_ERROR("Invalid Format");
        return 0;
    }
    TF_CODING_ERROR("Missing Format");
    return 0;
}

size_t
HgiDataSizeOfFormat(
    const HgiFormat f,
    size_t * const blockWidth,
    size_t * const blockHeight)
{
    if (blockWidth) {
        *blockWidth = 1;
    }
    if (blockHeight) {
        *blockHeight = 1;
    }

    switch(f) {
    case HgiFormatUNorm8:
    case HgiFormatSNorm8:
        return 1;
    case HgiFormatUNorm8Vec2:
    case HgiFormatSNorm8Vec2:
        return 2;
    // case HgiFormatUNorm8Vec3: // Unsupported Metal (MTLPixelFormat)
    // case HgiFormatSNorm8Vec3: // Unsupported Metal (MTLPixelFormat)
    //     return 3;
    case HgiFormatUNorm8Vec4:
    case HgiFormatSNorm8Vec4:
    case HgiFormatUNorm8Vec4srgb:
        return 4;
    case HgiFormatFloat16:
        return 2;
    case HgiFormatFloat16Vec2:
        return 4;
    case HgiFormatFloat16Vec3:
        return 6;
    case HgiFormatFloat16Vec4:
        return 8;
    case HgiFormatFloat32:
    case HgiFormatInt32:
        return 4;
    case HgiFormatFloat32Vec2:
    case HgiFormatInt32Vec2:
    case HgiFormatFloat32UInt8: // XXX: implementation dependent
        return 8;
    case HgiFormatFloat32Vec3:
    case HgiFormatInt32Vec3:
        return 12;
    case HgiFormatFloat32Vec4:
    case HgiFormatInt32Vec4:
        return 16;
    case HgiFormatBC6FloatVec3:
    case HgiFormatBC6UFloatVec3:
    case HgiFormatBC7UNorm8Vec4:
    case HgiFormatBC7UNorm8Vec4srgb:
        if (blockWidth) {
            *blockWidth = 4;
        }
        if (blockHeight) {
            *blockHeight = 4;
        }
        return 16;
    case HgiFormatCount:
    case HgiFormatInvalid:
        TF_CODING_ERROR("Invalid Format");
        return 0;
    }
    TF_CODING_ERROR("Missing Format");
    return 0;
}

bool
HgiIsCompressed(const HgiFormat f)
{
    switch(f) {
    case HgiFormatBC6FloatVec3:
    case HgiFormatBC6UFloatVec3:
    case HgiFormatBC7UNorm8Vec4:
    case HgiFormatBC7UNorm8Vec4srgb:
        return true;
    default:
        return false;
    }
}

const void*
HgiGetMipInitialData(
    const HgiFormat format,
    const GfVec3i& dimensions,
    const uint16_t mipLevel,
    const size_t initialDataByteSize,
    const void* initialData,
    GfVec3i* mipDimensions,
    size_t* mipByteSize)
{
    // The most common case is loading the first mip. Exit early.
    if (mipLevel == 0) {
        *mipDimensions = dimensions;
        *mipByteSize = initialDataByteSize;
        return initialData;
    }

    size_t blockWidth, blockHeight;
    const size_t bpt = HgiDataSizeOfFormat(format, &blockWidth, &blockHeight);

    GfVec3i& size = *mipDimensions;
    size = dimensions;
    size_t byteOffset = 0;

    // Each mip image is half the dimensions of the previous level.
    for (size_t i=0; i<mipLevel; i++) {
        byteOffset += 
            ((size[0] + blockWidth  - 1) / blockWidth ) *
            ((size[1] + blockHeight - 1) / blockHeight) *
            size[2] * bpt;
        size[0] = std::max(size[0] / 2, 1);
        size[1] = std::max(size[1] / 2, 1);
        size[2] = std::max(size[2] / 2, 1);
    }

    if (byteOffset >= initialDataByteSize) {
        return nullptr;
    }

    // Each mip image is a quarter of the bytes of the previous level.
    *mipByteSize =
        ((size[0] + blockWidth  - 1) / blockWidth ) *
        ((size[1] + blockHeight - 1) / blockHeight) *
        size[2] * bpt;
    return static_cast<char const*>(initialData) + byteOffset;
}

PXR_NAMESPACE_CLOSE_SCOPE
