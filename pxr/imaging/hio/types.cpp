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
#include "pxr/imaging/hio/types.h"

#include "pxr/base/tf/diagnostic.h"
#include "pxr/base/tf/iterator.h"

PXR_NAMESPACE_OPEN_SCOPE

// A few random format validations to make sure the HioFormat switch stays
// aligned with the HioFormat table.
constexpr bool _CompileTimeValidateHioFormatSwitch() {
    return (HioFormatCount == 44 &&
            HioFormatUNorm8 == 0 &&
            HioFormatFloat32 == 12 &&
            HioFormatUInt32 == 28 &&
            HioFormatBC6FloatVec3 == 40) ? true : false;
}

static_assert(_CompileTimeValidateHioFormatSwitch(),
              "HioGetNumChannelsFromFormat() and HioGetChannelTypeFromFormat() "
              "switch in HioTypes out of sync with HioFormat enum");

static HioFormat _hioFormats[][4] = {
    { HioFormatUNorm8, HioFormatUNorm8Vec2,
      HioFormatUNorm8Vec4, HioFormatUNorm8Vec4 },
    { HioFormatUNorm8srgb, HioFormatUNorm8Vec2srgb,
      HioFormatUNorm8Vec4srgb, HioFormatUNorm8Vec4srgb },
    { HioFormatFloat16, HioFormatFloat16Vec2,
      HioFormatFloat16Vec4, HioFormatFloat16Vec4 },
    { HioFormatFloat32, HioFormatFloat32Vec2,
      HioFormatFloat32Vec4, HioFormatFloat32Vec4 },
    { HioFormatUInt16, HioFormatUInt16Vec2,
      HioFormatUInt16Vec4, HioFormatUInt16Vec4 },
    { HioFormatInt32, HioFormatInt32Vec2,
      HioFormatInt32Vec4, HioFormatInt32Vec4 },
};

static_assert(
    TfArraySize(_hioFormats) == HioColorChannelTypeCount,
    "_hioFormats array in HioUtils out of sync with "
    "HioColorChannelType enum");

HioFormat
HioGetFormat(uint32_t nchannels,
             HioColorChannelType type,
             bool isSRGB)
{
    if (type >= HioColorChannelTypeCount) {
        TF_CODING_ERROR("Invalid type");
        return HioFormatInvalid;
    }

    if (nchannels == 0 || nchannels > 4) {
        TF_CODING_ERROR("Invalid channel count");
        return HioFormatInvalid;
    }
    
    if (isSRGB && type == HioColorChannelTypeUNorm8) {
        type = HioColorChannelTypeUNorm8srgb;
    }

    return _hioFormats[type][nchannels - 1];
}

HioColorChannelType
HioGetChannelTypeFromFormat(HioFormat format) {
    switch (format) {
    case HioFormatUNorm8:
    case HioFormatUNorm8Vec2:
    case HioFormatUNorm8Vec3:
    case HioFormatUNorm8Vec4:
        return HioColorChannelTypeUNorm8;

    case HioFormatUNorm8srgb:
    case HioFormatUNorm8Vec2srgb:
    case HioFormatUNorm8Vec3srgb:
    case HioFormatUNorm8Vec4srgb:
        return HioColorChannelTypeUNorm8srgb;

    case HioFormatFloat16:
    case HioFormatFloat16Vec2:
    case HioFormatFloat16Vec3:
    case HioFormatFloat16Vec4:
        return HioColorChannelTypeFloat16;

    case HioFormatFloat32:
    case HioFormatFloat32Vec2:
    case HioFormatFloat32Vec3:
    case HioFormatFloat32Vec4:
        return HioColorChannelTypeFloat32;

    case HioFormatUInt16:
    case HioFormatUInt16Vec2:
    case HioFormatUInt16Vec3:
    case HioFormatUInt16Vec4:
        return HioColorChannelTypeUInt16;

    case HioFormatInt32:
    case HioFormatInt32Vec2:
    case HioFormatInt32Vec3:
    case HioFormatInt32Vec4:
        return HioColorChannelTypeInt32;

    default:
        TF_CODING_ERROR("No channel type for format");
        return HioColorChannelTypeUNorm8;
    }
}

/// Returns the bpc (bits per channel) based on the HioColorChannelType stored in storage
uint32_t
HioGetChannelSize(HioColorChannelType type)
{
    switch (type) {
    case HioColorChannelTypeUNorm8:
    case HioColorChannelTypeUNorm8srgb:
        return 1;
    case HioColorChannelTypeUInt16:
    case HioColorChannelTypeFloat16:
        return 2;
    case HioColorChannelTypeFloat32:
    case HioColorChannelTypeInt32:
        return 4;
    default:
        TF_CODING_ERROR("Unsupported channel type");
        return 4;
    }
    static_assert(
        HioColorChannelTypeCount == 6,
        "HioGetBytesPerChannel(...) switch in HioUtils out of "
        "sync with HioColorChannelType enum");
}

uint32_t
HioGetChannelSize(HioFormat format)
{
    return HioGetChannelSize(HioGetChannelTypeFromFormat(format));
}

uint32_t
HioGetNumChannels(HioFormat const & hioFormat)
{
    switch (hioFormat) {
    case HioFormatUNorm8:
    case HioFormatSNorm8:
    case HioFormatFloat16:
    case HioFormatFloat32:
    case HioFormatDouble64:
    case HioFormatUInt16:
    case HioFormatInt16:
    case HioFormatUInt32:
    case HioFormatInt32:
    case HioFormatUNorm8srgb:
        return 1;
    case HioFormatUNorm8Vec2:
    case HioFormatSNorm8Vec2:
    case HioFormatFloat16Vec2:
    case HioFormatFloat32Vec2:
    case HioFormatDouble64Vec2:
    case HioFormatUInt16Vec2:
    case HioFormatInt16Vec2:
    case HioFormatUInt32Vec2:
    case HioFormatInt32Vec2:
    case HioFormatUNorm8Vec2srgb:
        return 2;
    case HioFormatUNorm8Vec3:
    case HioFormatSNorm8Vec3:
    case HioFormatFloat16Vec3:
    case HioFormatFloat32Vec3:
    case HioFormatDouble64Vec3:
    case HioFormatUInt16Vec3:
    case HioFormatInt16Vec3:
    case HioFormatUInt32Vec3:
    case HioFormatInt32Vec3:
    case HioFormatUNorm8Vec3srgb:
    case HioFormatBC6FloatVec3:
    case HioFormatBC6UFloatVec3:
        return 3;
    case HioFormatUNorm8Vec4:
    case HioFormatSNorm8Vec4:
    case HioFormatFloat16Vec4:
    case HioFormatFloat32Vec4:
    case HioFormatDouble64Vec4:
    case HioFormatUInt16Vec4:
    case HioFormatInt16Vec4:
    case HioFormatUInt32Vec4:
    case HioFormatInt32Vec4:
    case HioFormatUNorm8Vec4srgb:
    case HioFormatBC7UNorm8Vec4:
    case HioFormatBC7UNorm8Vec4srgb:
        return 4;

    default:
        TF_CODING_ERROR("Unsupported channel type");
        return 4;
    }
}

bool
HioIsCompressed(HioFormat format)
{
    switch(format) {
        case HioFormatBC6FloatVec3:
        case HioFormatBC6UFloatVec3:
        case HioFormatBC7UNorm8Vec4:
        case HioFormatBC7UNorm8Vec4srgb:
            return true;
        default:
            return false;
    }
}

size_t
HioGetCompressedTextureSize(int width, int height, HioFormat hioFormat)
{
    int blockSize = 0;
    int tileSize = 0;
    int alignSize = 0;
    
    // XXX Only BPTC is supported right now
    if (hioFormat == HioFormatBC6FloatVec3 ||
        hioFormat == HioFormatBC6UFloatVec3 ||
        hioFormat == HioFormatBC7UNorm8Vec4 ||
        hioFormat == HioFormatBC7UNorm8Vec4srgb) {
        blockSize = 16;
        tileSize = 4;
        alignSize = 3;
    }

    size_t numPixels = ((width + alignSize)/tileSize) *
                       ((height + alignSize)/tileSize);
    return numPixels * blockSize;
}

PXR_NAMESPACE_CLOSE_SCOPE
