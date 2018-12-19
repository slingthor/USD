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
#include "pxr/imaging/glf/glew.h"
#include "pxr/imaging/garch/gl.h"

#include "pxr/imaging/hdSt/Metal/metalConversions.h"
#include "pxr/base/tf/iterator.h"
#include "pxr/base/tf/staticTokens.h"

PXR_NAMESPACE_OPEN_SCOPE


struct _FormatDesc {
    GLenum format;
    GLenum type;
    GLenum internalFormat;
};

static const _FormatDesc FORMAT_DESC[] =
{
    // format,  type,          internal format
    {GL_RED,  GL_UNSIGNED_BYTE, GL_R8},      // HdFormatUNorm8,
    {GL_RG,   GL_UNSIGNED_BYTE, GL_RG8},     // HdFormatUNorm8Vec2,
    {GL_RGB,  GL_UNSIGNED_BYTE, GL_RGB8},    // HdFormatUNorm8Vec3,
    {GL_RGBA, GL_UNSIGNED_BYTE, GL_RGBA8},   // HdFormatUNorm8Vec4,

    {GL_RED,  GL_BYTE,          GL_R8_SNORM},      // HdFormatSNorm8,
    {GL_RG,   GL_BYTE,          GL_RG8_SNORM},     // HdFormatSNorm8Vec2,
    {GL_RGB,  GL_BYTE,          GL_RGB8_SNORM},    // HdFormatSNorm8Vec3,
    {GL_RGBA, GL_BYTE,          GL_RGBA8_SNORM},   // HdFormatSNorm8Vec4,
    
    {GL_RED,  GL_HALF_FLOAT,    GL_R16F},    // HdFormatFloat16,
    {GL_RG,   GL_HALF_FLOAT,    GL_RG16F},   // HdFormatFloat16Vec2,
    {GL_RGB,  GL_HALF_FLOAT,    GL_RGB16F},  // HdFormatFloat16Vec3,
    {GL_RGBA, GL_HALF_FLOAT,    GL_RGBA16F}, // HdFormatFloat16Vec4,

    {GL_RED,  GL_FLOAT,         GL_R32F},    // HdFormatFloat32,
    {GL_RG,   GL_FLOAT,         GL_RG32F},   // HdFormatFloat32Vec2,
    {GL_RGB,  GL_FLOAT,         GL_RGB32F},  // HdFormatFloat32Vec3,
    {GL_RGBA, GL_FLOAT,         GL_RGBA32F}, // HdFormatFloat32Vec4,

    {GL_RED,  GL_INT,           GL_R32I},    // HdFormatInt32,
    {GL_RG,   GL_INT,           GL_RG32I},   // HdFormatInt32Vec2,
    {GL_RGB,  GL_INT,           GL_RGB32I},  // HdFormatInt32Vec3,
    {GL_RGBA, GL_INT,           GL_RGBA32I}, // HdFormatInt32Vec4,
};
static_assert(TfArraySize(FORMAT_DESC) ==  HdFormatCount, "FORMAT_DESC to HdFormat enum mismatch");

size_t
HdStMetalConversions::GetComponentSize(int glDataType)
{
    switch (glDataType) {
        case GL_BOOL:
            // Note that we don't use GLboolean here because according to
            // code in vtBufferSource, everything gets rounded up to 
            // size of single value in interleaved struct rounds up to
            // sizeof(GLint) according to GL spec.
            //      _size = std::max(sizeof(T), sizeof(GLint));
            return sizeof(GLint);
        case GL_BYTE:
            return sizeof(GLbyte);
        case GL_UNSIGNED_BYTE:
            return sizeof(GLubyte);
        case GL_SHORT:
            return sizeof(GLshort);
        case GL_UNSIGNED_SHORT:
            return sizeof(GLushort);
        case GL_INT:
            return sizeof(GLint);
        case GL_UNSIGNED_INT:
            return sizeof(GLuint);
        case GL_FLOAT:
            return sizeof(GLfloat);
        case GL_2_BYTES:
            return 2;
        case GL_3_BYTES:
            return 3;
        case GL_4_BYTES:
            return 4;
        case GL_UNSIGNED_INT64_ARB:
            return sizeof(GLuint64EXT);
        case GL_DOUBLE:
            return sizeof(GLdouble);
        case GL_INT_2_10_10_10_REV:
            return sizeof(GLint);
        // following enums are for bindless texture pointers.
        case GL_SAMPLER_2D:
            return sizeof(GLuint64EXT);
        case GL_SAMPLER_2D_ARRAY:
            return sizeof(GLuint64EXT);
        case GL_INT_SAMPLER_BUFFER:
            return sizeof(GLuint64EXT);
    };

    TF_CODING_ERROR("Unexpected GL datatype 0x%x", glDataType);
    return 1;
}


GLenum
HdStMetalConversions::GetGlDepthFunc(HdCompareFunction func)
{
    static GLenum HD_2_GL_DEPTH_FUNC[] =
    {
        GL_NEVER,    // HdCmpFuncNever
        GL_LESS,     // HdCmpFuncLess
        GL_EQUAL,    // HdCmpFuncEqual
        GL_LEQUAL,   // HdCmpFuncLEqual
        GL_GREATER,  // HdCmpFuncGreater
        GL_NOTEQUAL, // HdCmpFuncNotEqual
        GL_GEQUAL,   // HdCmpFuncGEqual
        GL_ALWAYS,   // HdCmpFuncAlways
    };
    static_assert((sizeof(HD_2_GL_DEPTH_FUNC) / sizeof(HD_2_GL_DEPTH_FUNC[0])) == HdCmpFuncLast, "Mismatch enum sizes in convert function");

    return HD_2_GL_DEPTH_FUNC[func];
}

GLenum
HdStMetalConversions::GetGlStencilFunc(HdCompareFunction func)
{
    static GLenum HD_2_GL_STENCIL_FUNC[] =
    {
        GL_NEVER,    // HdCmpFuncNever
        GL_LESS,     // HdCmpFuncLess
        GL_EQUAL,    // HdCmpFuncEqual
        GL_LEQUAL,   // HdCmpFuncLEqual
        GL_GREATER,  // HdCmpFuncGreater
        GL_NOTEQUAL, // HdCmpFuncNotEqual
        GL_GEQUAL,   // HdCmpFuncGEqual
        GL_ALWAYS,   // HdCmpFuncAlways
    };
    static_assert((sizeof(HD_2_GL_STENCIL_FUNC) / sizeof(HD_2_GL_STENCIL_FUNC[0])) == HdCmpFuncLast, "Mismatch enum sizes in convert function");

    return HD_2_GL_STENCIL_FUNC[func];
}

GLenum
HdStMetalConversions::GetGlStencilOp(HdStencilOp op)
{
    static GLenum HD_2_GL_STENCIL_OP[] =
    {
        GL_KEEP,      // HdStencilOpKeep
        GL_ZERO,      // HdStencilOpZero
        GL_REPLACE,   // HdStencilOpReplace
        GL_INCR,      // HdStencilOpIncrement
        GL_INCR_WRAP, // HdStencilOpIncrementWrap
        GL_DECR,      // HdStencilOpDecrement
        GL_DECR_WRAP, // HdStencilOpDecrementWrap
        GL_INVERT,    // HdStencilOpInvert
    };
    static_assert((sizeof(HD_2_GL_STENCIL_OP) / sizeof(HD_2_GL_STENCIL_OP[0])) == HdStencilOpLast, "Mismatch enum sizes in convert function");

    return HD_2_GL_STENCIL_OP[op];
}

MTLSamplerMinMagFilter
HdStMetalConversions::GetMinFilter(HdMinFilter filter)
{
    switch (filter) {
        case HdMinFilterNearest : return MTLSamplerMinMagFilterNearest;
        case HdMinFilterLinear :  return MTLSamplerMinMagFilterLinear;
        case HdMinFilterNearestMipmapNearest : return MTLSamplerMinMagFilterNearest;
        case HdMinFilterLinearMipmapNearest : return MTLSamplerMinMagFilterLinear;
        case HdMinFilterNearestMipmapLinear : return MTLSamplerMinMagFilterNearest;
        case HdMinFilterLinearMipmapLinear : return MTLSamplerMinMagFilterLinear;
    }

    TF_CODING_ERROR("Unexpected HdMinFilter type %d", filter);
    return MTLSamplerMinMagFilterNearest;
}

MTLSamplerMinMagFilter
HdStMetalConversions::GetMagFilter(HdMagFilter filter)
{
    switch (filter) {
        case HdMagFilterNearest : return MTLSamplerMinMagFilterNearest;
        case HdMagFilterLinear : return MTLSamplerMinMagFilterLinear;
    }

    TF_CODING_ERROR("Unexpected HdMagFilter type %d", filter);
    return MTLSamplerMinMagFilterLinear;
}

MTLSamplerMipFilter
HdStMetalConversions::GetMipFilter(HdMinFilter filter)
{
    switch (filter) {
        case HdMinFilterNearest : return MTLSamplerMipFilterNotMipmapped;
        case HdMinFilterLinear :  return MTLSamplerMipFilterNotMipmapped;
        case HdMinFilterNearestMipmapNearest : return MTLSamplerMipFilterNearest;
        case HdMinFilterLinearMipmapNearest : return MTLSamplerMipFilterNearest;
        case HdMinFilterNearestMipmapLinear : return MTLSamplerMipFilterLinear;
        case HdMinFilterLinearMipmapLinear : return MTLSamplerMipFilterLinear;
    }
    
    TF_CODING_ERROR("Unexpected HdMinFilter type %d", filter);
    return MTLSamplerMipFilterNearest;
}

MTLSamplerAddressMode
HdStMetalConversions::GetWrap(HdWrap wrap)
{
    switch (wrap) {
        case HdWrapClamp : return MTLSamplerAddressModeClampToEdge;
        case HdWrapRepeat : return MTLSamplerAddressModeRepeat;
        case HdWrapMirror : return MTLSamplerAddressModeMirrorRepeat;
#if defined(ARCH_OS_OSX)
        case HdWrapBlack : return MTLSamplerAddressModeClampToBorderColor;
        case HdWrapUseMetadata : return MTLSamplerAddressModeClampToBorderColor;
#else
        case HdWrapBlack : return MTLSamplerAddressModeClampToEdge;
        case HdWrapUseMetadata : return MTLSamplerAddressModeClampToEdge;
#endif
        case HdWrapLegacy : return MTLSamplerAddressModeRepeat;
    }

    TF_CODING_ERROR("Unexpected HdWrap type %d", wrap);
    return MTLSamplerAddressModeClampToEdge;
}

void
HdStMetalConversions::GetGlFormat(HdFormat inFormat, GLenum *outFormat, GLenum *outType, GLenum *outInternalFormat)
{
    if ((inFormat < 0) || (inFormat >= HdFormatCount))
    {
        TF_CODING_ERROR("Unexpected HdFormat %d", inFormat);
        *outFormat         = GL_RGBA;
        *outType           = GL_BYTE;
        *outInternalFormat = GL_RGBA8;
        return;
    }

    const _FormatDesc &desc = FORMAT_DESC[inFormat];

    *outFormat         = desc.format;
    *outType           = desc.type;
    *outInternalFormat = desc.internalFormat;
}

int
HdStMetalConversions::GetGLAttribType(HdType type)
{
    switch (type) {
    case HdTypeInt32:
    case HdTypeInt32Vec2:
    case HdTypeInt32Vec3:
    case HdTypeInt32Vec4:
        return GL_INT;
    case HdTypeUInt32:
    case HdTypeUInt32Vec2:
    case HdTypeUInt32Vec3:
    case HdTypeUInt32Vec4:
        return GL_UNSIGNED_INT;
    case HdTypeFloat:
    case HdTypeFloatVec2:
    case HdTypeFloatVec3:
    case HdTypeFloatVec4:
    case HdTypeFloatMat3:
    case HdTypeFloatMat4:
        return GL_FLOAT;
    case HdTypeDouble:
    case HdTypeDoubleVec2:
    case HdTypeDoubleVec3:
    case HdTypeDoubleVec4:
    case HdTypeDoubleMat3:
    case HdTypeDoubleMat4:
        return GL_DOUBLE;
    case HdTypeInt32_2_10_10_10_REV:
        return GL_INT_2_10_10_10_REV;
    default:
        break;
    };
    return -1;
}

TF_DEFINE_PRIVATE_TOKENS(
    _glTypeNames,
    ((_bool, "bool"))

    ((_float, "float"))
    (vec2)
    (vec3)
    (vec4)
    (mat3)
    (mat4)

    ((_double, "double"))
    (dvec2)
    (dvec3)
    (dvec4)
    (dmat3)
    (dmat4)

    ((_int, "int"))
    (ivec2)
    (ivec3)
    (ivec4)

    ((_uint, "uint"))
    (uvec2)
    (uvec3)
    (uvec4)
                         
    (packed_2_10_10_10)
);

TfToken
HdStMetalConversions::GetGLSLTypename(HdType type)
{
    switch (type) {
    case HdTypeInvalid:
    default:
        return TfToken();
            
    // Packed types (require special handling in codegen)...
    case HdTypeInt32_2_10_10_10_REV:
        return _glTypeNames->packed_2_10_10_10;
        
    case HdTypeBool:
        return _glTypeNames->_bool;

    case HdTypeInt32:
        return _glTypeNames->_int;
    case HdTypeInt32Vec2:
        return _glTypeNames->ivec2;
    case HdTypeInt32Vec3:
        return _glTypeNames->ivec3;
    case HdTypeInt32Vec4:
        return _glTypeNames->ivec4;

    case HdTypeUInt32:
        return _glTypeNames->_uint;
    case HdTypeUInt32Vec2:
        return _glTypeNames->uvec2;
    case HdTypeUInt32Vec3:
        return _glTypeNames->uvec3;
    case HdTypeUInt32Vec4:
        return _glTypeNames->uvec4;

    case HdTypeFloat:
        return _glTypeNames->_float;
    case HdTypeFloatVec2:
        return _glTypeNames->vec2;
    case HdTypeFloatVec3:
        return _glTypeNames->vec3;
    case HdTypeFloatVec4:
        return _glTypeNames->vec4;
    case HdTypeFloatMat3:
        return _glTypeNames->mat3;
    case HdTypeFloatMat4:
        return _glTypeNames->mat4;

    case HdTypeDouble:
        return _glTypeNames->_double;
    case HdTypeDoubleVec2:
        return _glTypeNames->dvec2;
    case HdTypeDoubleVec3:
        return _glTypeNames->dvec3;
    case HdTypeDoubleVec4:
        return _glTypeNames->dvec4;
    case HdTypeDoubleMat3:
        return _glTypeNames->dmat3;
    case HdTypeDoubleMat4:
        return _glTypeNames->dmat4;
    };
}

MTLSamplerAddressMode
HdStMetalConversions::ConvertGLWrap(GLuint wrap)
{
    switch (wrap) {
        case GL_CLAMP_TO_EDGE:
            return MTLSamplerAddressModeClampToEdge;
        case GL_REPEAT:
            return MTLSamplerAddressModeRepeat;
        case GL_CLAMP_TO_BORDER:
#if defined(ARCH_OS_MACOS)
            return MTLSamplerAddressModeClampToBorderColor;
#endif
        case GL_MIRRORED_REPEAT:
            return MTLSamplerAddressModeMirrorRepeat;
    }
    
    TF_CODING_ERROR("Unexpected GL wrap type %d", wrap);
    return MTLSamplerAddressModeRepeat;
}

MTLPixelFormat
HdStMetalConversions::ConvertGLInternalFormat(GLenum inInternalFormat, GLenum inType, size_t *outPixelByteSize)
{
    MTLPixelFormat mtlFormat = MTLPixelFormatInvalid;
    
    *outPixelByteSize = 0;
    
    switch (inInternalFormat)
    {
        case GL_RGB32F:
        case GL_RGB16F:
        case GL_RGB16:
        case GL_SRGB:
        case GL_RGB:
            TF_CODING_ERROR("3 channel textures are unsupported on Metal");
            // Drop through
            
        case GL_RGBA:
            mtlFormat = MTLPixelFormatRGBA8Unorm;
            *outPixelByteSize = sizeof(char) * 4;
            break;
            
        case GL_SRGB_ALPHA:
            mtlFormat = MTLPixelFormatRGBA8Unorm_sRGB;
            *outPixelByteSize = sizeof(char) * 4;
            break;
            
        case GL_RGBA16:
            mtlFormat = MTLPixelFormatRGBA16Unorm;
            *outPixelByteSize = sizeof(short) * 4;
            break;
            
        case GL_R16:
            mtlFormat = MTLPixelFormatRGBA16Unorm;
            *outPixelByteSize = sizeof(short);
            break;
            
        case GL_RGBA16F:
            mtlFormat = MTLPixelFormatRGBA16Float;
            *outPixelByteSize = sizeof(short) * 4;
            break;
            
        case GL_R16F:
            mtlFormat = MTLPixelFormatR16Float;
            *outPixelByteSize = sizeof(short);
            break;
            
        case GL_RGBA32F:
            mtlFormat = MTLPixelFormatRGBA32Float;
            *outPixelByteSize = sizeof(float) * 4;
            break;
            
        case GL_R32F:
            mtlFormat = MTLPixelFormatRGBA32Float;
            *outPixelByteSize = sizeof(float);
            break;
    }
    
    return mtlFormat;
}

PXR_NAMESPACE_CLOSE_SCOPE

