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
#include "pxr/imaging/hgiMetal/conversions.h"

#include "pxr/base/tf/iterator.h"
#include "pxr/base/tf/staticTokens.h"
#include "pxr/base/tf/stringUtils.h"

PXR_NAMESPACE_OPEN_SCOPE

static const MTLPixelFormat FORMAT_DESC[] =
{
    MTLPixelFormatR8Unorm,      // HdFormatUNorm8,
    MTLPixelFormatRG8Unorm,     // HdFormatUNorm8Vec2,
    MTLPixelFormatInvalid,      // HdFormatUNorm8Vec3,
    MTLPixelFormatRGBA8Unorm,   // HdFormatUNorm8Vec4,

    MTLPixelFormatR8Snorm,      // HdFormatSNorm8,
    MTLPixelFormatRG8Snorm,     // HdFormatSNorm8Vec2,
    MTLPixelFormatInvalid,      // HdFormatSNorm8Vec3,
    MTLPixelFormatRGBA8Snorm,   // HdFormatSNorm8Vec4,

    MTLPixelFormatR16Float,     // HdFormatFloat16,
    MTLPixelFormatRG16Float,    // HdFormatFloat16Vec2,
    MTLPixelFormatInvalid,      // HdFormatFloat16Vec3,
    MTLPixelFormatRGBA16Float,  // HdFormatFloat16Vec4,

    MTLPixelFormatR32Float,     // HdFormatFloat32,
    MTLPixelFormatRG32Float,    // HdFormatFloat32Vec2,
    MTLPixelFormatInvalid,      // HdFormatFloat32Vec3,
    MTLPixelFormatRGBA32Float,  // HdFormatFloat32Vec4,

    MTLPixelFormatR32Sint,      // HdFormatInt32,
    MTLPixelFormatRG32Sint,     // HdFormatInt32Vec2,
    MTLPixelFormatInvalid,      // HdFormatInt32Vec3,
    MTLPixelFormatRGBA32Sint,   // HdFormatInt32Vec4,
};

constexpr bool _CompileTimeValidateHgiFormatTable() {
    return (TfArraySize(FORMAT_DESC) == HgiFormatCount &&
            HgiFormatUNorm8 == 0 &&
            HgiFormatFloat16Vec4 == 11 &&
            HgiFormatFloat32Vec4 == 15 &&
            HgiFormatInt32Vec4 == 19) ? true : false;
}

static_assert(_CompileTimeValidateHgiFormatTable(), 
              "_FormatDesc array out of sync with HgiFormat enum");

MTLPixelFormat
HgiMetalConversions::GetFormat(HgiFormat inFormat)
{
    if ((inFormat < 0) || (inFormat >= HgiFormatCount))
    {
        TF_CODING_ERROR("Unexpected HdFormat %d", inFormat);
        return MTLPixelFormatRGBA8Unorm;
    }

    MTLPixelFormat outFormat = FORMAT_DESC[inFormat];
    if (outFormat == MTLPixelFormatInvalid)
    {
        TF_CODING_ERROR("Unsupported HdFormat %d", inFormat);
        return MTLPixelFormatRGBA8Unorm;
    }
    return outFormat;
}


PXR_NAMESPACE_CLOSE_SCOPE
