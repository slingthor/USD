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

static const MTLPixelFormat PIXEL_FORMAT_DESC[] =
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

static const MTLVertexFormat VERTEX_FORMAT_DESC[] =
{
    MTLVertexFormatUCharNormalized,     // HdFormatUNorm8,
    MTLVertexFormatUChar2Normalized,    // HdFormatUNorm8Vec2,
    MTLVertexFormatUChar3Normalized,    // HdFormatUNorm8Vec3,
    MTLVertexFormatUChar4Normalized,    // HdFormatUNorm8Vec4,

    MTLVertexFormatCharNormalized,      // HdFormatSNorm8,
    MTLVertexFormatChar2Normalized,     // HdFormatSNorm8Vec2,
    MTLVertexFormatChar3Normalized,     // HdFormatSNorm8Vec3,
    MTLVertexFormatChar4Normalized,     // HdFormatSNorm8Vec4,

    MTLVertexFormatHalf,                // HdFormatFloat16,
    MTLVertexFormatHalf2,               // HdFormatFloat16Vec2,
    MTLVertexFormatHalf3,               // HdFormatFloat16Vec3,
    MTLVertexFormatHalf4,               // HdFormatFloat16Vec4,

    MTLVertexFormatFloat,               // HdFormatFloat32,
    MTLVertexFormatFloat2,              // HdFormatFloat32Vec2,
    MTLVertexFormatFloat3,              // HdFormatFloat32Vec3,
    MTLVertexFormatFloat4,              // HdFormatFloat32Vec4,

    MTLVertexFormatInt,                 // HdFormatInt32,
    MTLVertexFormatInt2,                // HdFormatInt32Vec2,
    MTLVertexFormatInt3,                // HdFormatInt32Vec3,
    MTLVertexFormatInt4,                // HdFormatInt32Vec4,
};

constexpr bool _CompileTimeValidateHgiPixelFormatTable() {
    return (TfArraySize(PIXEL_FORMAT_DESC) == HgiFormatCount &&
            HgiFormatUNorm8 == 0 &&
            HgiFormatFloat16Vec4 == 11 &&
            HgiFormatFloat32Vec4 == 15 &&
            HgiFormatInt32Vec4 == 19) ? true : false;
}

static_assert(_CompileTimeValidateHgiPixelFormatTable(),
              "_PixelFormatDesc array out of sync with HgiFormat enum");

constexpr bool _CompileTimeValidateHgiVertexFormatTable() {
    return (TfArraySize(VERTEX_FORMAT_DESC) == HgiFormatCount &&
            HgiFormatUNorm8 == 0 &&
            HgiFormatFloat16Vec4 == 11 &&
            HgiFormatFloat32Vec4 == 15 &&
            HgiFormatInt32Vec4 == 19) ? true : false;
}

static_assert(_CompileTimeValidateHgiVertexFormatTable(),
              "_VertexFormatDesc array out of sync with HgiFormat enum");

struct {
    HgiCullMode hgiCullMode;
    MTLCullMode metalCullMode;
} static const _CullModeTable[HgiCullModeCount] =
{
    {HgiCullModeNone,         MTLCullModeNone},
    {HgiCullModeFront,        MTLCullModeFront},
    {HgiCullModeBack,         MTLCullModeBack},
    {HgiCullModeFrontAndBack, MTLCullModeNone} // Unsupported
};

static_assert(TfArraySize(_CullModeTable) == HgiCullModeCount,
              "_CullModeTable array out of sync with HgiFormat enum");

struct {
    HgiPolygonMode hgiFillMode;
    MTLTriangleFillMode metalFillMode;
} static const _PolygonModeTable[HgiCullModeCount] =
{
    {HgiPolygonModeFill,  MTLTriangleFillModeFill},
    {HgiPolygonModeLine,  MTLTriangleFillModeLines},
    {HgiPolygonModePoint, MTLTriangleFillModeFill}, // Unsupported
};

static_assert(TfArraySize(_PolygonModeTable) == HgiCullModeCount,
              "_PolygonModeTable array out of sync with HgiFormat enum");

struct {
    HgiBlendOp hgiBlendOp;
    MTLBlendOperation metalBlendOp;
} static const _blendEquationTable[HgiBlendOpCount] =
{
    {HgiBlendOpAdd,             MTLBlendOperationAdd},
    {HgiBlendOpSubtract,        MTLBlendOperationSubtract},
    {HgiBlendOpReverseSubtract, MTLBlendOperationReverseSubtract},
    {HgiBlendOpMin,             MTLBlendOperationMin},
    {HgiBlendOpMax,             MTLBlendOperationMax},
};

static_assert(TfArraySize(_blendEquationTable) == HgiBlendOpCount,
              "_blendEquationTable array out of sync with HgiFormat enum");

struct {
    HgiBlendFactor hgiBlendFactor;
    MTLBlendFactor metalBlendFactor;
} static const _blendFactorTable[HgiBlendFactorCount] =
{
    {HgiBlendFactorZero,                MTLBlendFactorZero},
    {HgiBlendFactorOne,                 MTLBlendFactorOne},
    {HgiBlendFactorSrcColor,            MTLBlendFactorSourceColor},
    {HgiBlendFactorOneMinusSrcColor,    MTLBlendFactorOneMinusSourceColor},
    {HgiBlendFactorDstColor,            MTLBlendFactorDestinationColor},
    {HgiBlendFactorOneMinusDstColor,    MTLBlendFactorOneMinusDestinationColor},
    {HgiBlendFactorSrcAlpha,            MTLBlendFactorSourceAlpha},
    {HgiBlendFactorOneMinusSrcAlpha,    MTLBlendFactorOneMinusSourceAlpha},
    {HgiBlendFactorDstAlpha,            MTLBlendFactorDestinationAlpha},
    {HgiBlendFactorOneMinusDstAlpha,    MTLBlendFactorOneMinusDestinationAlpha},
    {HgiBlendFactorConstantColor,       MTLBlendFactorZero},  // Unsupported
    {HgiBlendFactorOneMinusConstantColor, MTLBlendFactorZero},  // Unsupported
    {HgiBlendFactorConstantAlpha,       MTLBlendFactorZero},  // Unsupported
    {HgiBlendFactorOneMinusConstantAlpha, MTLBlendFactorZero},  // Unsupported
    {HgiBlendFactorSrcAlphaSaturate,    MTLBlendFactorSourceAlphaSaturated},
    {HgiBlendFactorSrc1Color,           MTLBlendFactorSource1Color},
    {HgiBlendFactorOneMinusSrc1Color,   MTLBlendFactorOneMinusSource1Color},
    {HgiBlendFactorSrc1Alpha,           MTLBlendFactorSourceAlpha},
    {HgiBlendFactorOneMinusSrc1Alpha,   MTLBlendFactorOneMinusSource1Alpha},
};

static_assert(TfArraySize(_blendFactorTable) == HgiBlendFactorCount,
              "_blendFactorTable array out of sync with HgiFormat enum");

struct {
    HgiWinding hgiWinding;
    MTLWinding metalWinding;
} static const _windingTable[HgiWindingCount] =
{
    {HgiWindingClockwise,           MTLWindingClockwise},
    {HgiWindingCounterClockwise,    MTLWindingCounterClockwise},
};

static_assert(TfArraySize(_windingTable) == HgiWindingCount,
              "_windingTable array out of sync with HgiFormat enum");

struct {
    HgiAttachmentLoadOp hgiAttachmentLoadOp;
    MTLLoadAction metalLoadOp;
} static const _attachmentLoadOpTable[HgiAttachmentLoadOpCount] =
{
    {HgiAttachmentLoadOpDontCare,   MTLLoadActionDontCare},
    {HgiAttachmentLoadOpClear,      MTLLoadActionClear},
    {HgiAttachmentLoadOpLoad,       MTLLoadActionLoad},
};

static_assert(TfArraySize(_attachmentLoadOpTable) == HgiAttachmentLoadOpCount,
              "_attachmentLoadOpTable array out of sync with HgiFormat enum");

struct {
    HgiAttachmentStoreOp hgiAttachmentStoreOp;
    MTLStoreAction metalStoreOp;
} static const _attachmentStoreOpTable[HgiAttachmentStoreOpCount] =
{
    {HgiAttachmentStoreOpDontCare,   MTLStoreActionDontCare},
    {HgiAttachmentStoreOpStore,      MTLStoreActionStore},
};

static_assert(TfArraySize(_attachmentStoreOpTable) == HgiAttachmentStoreOpCount,
              "_attachmentStoreOpTable array out of sync with HgiFormat enum");

MTLPixelFormat
HgiMetalConversions::GetPixelFormat(HgiFormat inFormat)
{
    if ((inFormat < 0) || (inFormat >= HgiFormatCount))
    {
        TF_CODING_ERROR("Unexpected HdFormat %d", inFormat);
        return MTLPixelFormatRGBA8Unorm;
    }

    MTLPixelFormat outFormat = PIXEL_FORMAT_DESC[inFormat];
    if (outFormat == MTLPixelFormatInvalid)
    {
        TF_CODING_ERROR("Unsupported HdFormat %d", inFormat);
        return MTLPixelFormatRGBA8Unorm;
    }
    return outFormat;
}

MTLVertexFormat
HgiMetalConversions::GetVertexFormat(HgiFormat inFormat)
{
    if ((inFormat < 0) || (inFormat >= HgiFormatCount))
    {
        TF_CODING_ERROR("Unexpected HdFormat %d", inFormat);
        return MTLVertexFormatFloat4;
    }

    MTLVertexFormat outFormat = VERTEX_FORMAT_DESC[inFormat];
    if (outFormat == MTLVertexFormatInvalid)
    {
        TF_CODING_ERROR("Unsupported HdFormat %d", inFormat);
        return MTLVertexFormatFloat4;
    }
    return outFormat;
}

MTLCullMode
HgiMetalConversions::GetCullMode(HgiCullMode cm)
{
    return _CullModeTable[cm].metalCullMode;
}

MTLTriangleFillMode
HgiMetalConversions::GetPolygonMode(HgiPolygonMode pm)
{
    return _PolygonModeTable[pm].metalFillMode;
}

MTLBlendFactor
HgiMetalConversions::GetBlendFactor(HgiBlendFactor bf)
{
    return _blendFactorTable[bf].metalBlendFactor;
}

MTLBlendOperation
HgiMetalConversions::GetBlendEquation(HgiBlendOp bo)
{
    return _blendEquationTable[bo].metalBlendOp;
}

MTLWinding
HgiMetalConversions::GetWinding(HgiWinding winding)
{
    return _windingTable[winding].metalWinding;
}

MTLLoadAction
HgiMetalConversions::GetAttachmentLoadOp(HgiAttachmentLoadOp loadOp)
{
    return _attachmentLoadOpTable[loadOp].metalLoadOp;
}

MTLStoreAction
HgiMetalConversions::GetAttachmentStoreOp(HgiAttachmentStoreOp storeOp)
{
    return _attachmentStoreOpTable[storeOp].metalStoreOp;
}

PXR_NAMESPACE_CLOSE_SCOPE
