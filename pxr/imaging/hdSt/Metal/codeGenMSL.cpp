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
#include "pxr/imaging/mtlf/mtlDevice.h"

#include "pxr/imaging/garch/contextCaps.h"
#include "pxr/imaging/garch/gl.h"
#include "pxr/imaging/garch/resourceFactory.h"

#include "pxr/imaging/hio/glslfx.h"

#include "pxr/imaging/hdSt/Metal/codeGenMSL.h"
#include "pxr/imaging/hdSt/Metal/glslProgramMetal.h"

#include "pxr/imaging/hdSt/geometricShader.h"
#include "pxr/imaging/hdSt/package.h"
#include "pxr/imaging/hdSt/resourceBinder.h"
#include "pxr/imaging/hdSt/resourceRegistry.h"
#include "pxr/imaging/hdSt/shaderCode.h"
#include "pxr/imaging/hdSt/surfaceShader.h"
#include "pxr/imaging/hdSt/tokens.h"

#include "pxr/imaging/hd/binding.h"
#include "pxr/imaging/hd/instanceRegistry.h"
#include "pxr/imaging/hd/tokens.h"
#include "pxr/imaging/hd/vtBufferSource.h"

#include "pxr/base/tf/iterator.h"
#include "pxr/base/tf/staticTokens.h"

#include <boost/functional/hash.hpp>

#include <sstream>

#undef tolower  // Python binder defines this, which breaks regex compilation
#include <regex>

#include <opensubdiv/osd/mtlPatchShaderSource.h>

#define MTL_PRIMVAR_PREFIX "__primVar_"

#if defined(GENERATE_METAL_DEBUG_SOURCE_CODE)
template <typename T>
void METAL_DEBUG_COMMENTfn(std::stringstream *str, T t)
{
    *str << t;
}
template<typename T, typename... Args>
void METAL_DEBUG_COMMENTfn(std::stringstream *str, T t, Args... args) // recursive variadic function
{
    *str << t;
    METAL_DEBUG_COMMENTfn(str, args...) ;
}
template<typename... Args>
void METAL_DEBUG_COMMENT(std::stringstream *str, Args... args)
{
    *str << "// ";
    METAL_DEBUG_COMMENTfn(str, args...);
}

#else
#define METAL_DEBUG_COMMENT(str, ...)
#endif

PXR_NAMESPACE_OPEN_SCOPE

std::string replaceStringAll(std::string str, const std::string& old, const std::string& new_s) {
    if(!old.empty()){
        size_t pos = str.find(old);
        while ((pos = str.find(old, pos)) != std::string::npos) {
            str=str.replace(pos, old.length(), new_s);
            pos += new_s.length();
        }
    }
    return str;
}

TF_DEFINE_PRIVATE_TOKENS(
    _tokens,
    ((_double, "double"))
    ((_float, "float"))
    ((_int, "int"))
    ((_bool, "bool"))
    (wrapped_float)
    (wrapped_int)
    (hd_vec2)
    (hd_vec3)
    (hd_vec3_get)
    (hd_vec3_set)
    (hd_ivec2)
    (hd_ivec3)
    (hd_ivec3_get)
    (hd_ivec3_set)
    (hd_dvec2)
    (hd_dvec3)
    (hd_dvec3_get)
    (hd_dvec3_set)
    (hd_mat3)
    (hd_mat3_get)
    (hd_mat3_set)
    (hd_dmat3)
    (hd_dmat3_get)
    (hd_dmat3_set)
    (hd_vec4_2_10_10_10_get)
    (hd_vec4_2_10_10_10_set)
    (inPrimvars)
    (ivec2)
    (ivec3)
    (ivec4)
    (outPrimvars)
    (vec2)
    (vec3)
    (vec4)
    (dvec2)
    (dvec3)
    (dvec4)
    (mat2)
    (mat3)
    (mat4)
    (dmat3)
    (dmat4)
    (packed_2_10_10_10)
    ((ptexTextureSampler, "ptexTextureSampler"))
    ((isamplerBuffer, "texture2d<int>"))
    ((samplerBuffer, "texture2d<float>"))
    (packedSmoothNormals)
    (packedFlatNormals)
);

HdSt_CodeGenMSL::TParam::Usage operator|(HdSt_CodeGenMSL::TParam::Usage const &lhs,
                                         HdSt_CodeGenMSL::TParam::Usage const &rhs) {
    return HdSt_CodeGenMSL::TParam::Usage(int(lhs) | int(rhs));
}

HdSt_CodeGenMSL::TParam::Usage operator|=(HdSt_CodeGenMSL::TParam::Usage &lhs,
                                         HdSt_CodeGenMSL::TParam::Usage const &rhs) {
    return lhs = lhs | rhs;
}

HdSt_CodeGenMSL::HdSt_CodeGenMSL(HdSt_GeometricShaderPtr const &geometricShader,
                             HdStShaderCodeSharedPtrVector const &shaders)
    : _geometricShader(geometricShader), _shaders(shaders)
{
    TF_VERIFY(geometricShader);
}

HdSt_CodeGenMSL::HdSt_CodeGenMSL(HdStShaderCodeSharedPtrVector const &shaders)
    : _geometricShader(), _shaders(shaders)
{
}

HdSt_CodeGenMSL::ID
HdSt_CodeGenMSL::ComputeHash() const
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    ID hash = _geometricShader ? _geometricShader->ComputeHash() : 0;
    boost::hash_combine(hash, _metaData.ComputeHash());
    boost::hash_combine(hash, HdStShaderCode::ComputeHash(_shaders));

    return hash;
}

static
std::string
_GetPtexTextureShaderSource()
{
    static std::string source =
        HioGlslfx(HdStPackagePtexTextureShader(), TfToken("Metal")).GetSource(
            _tokens->ptexTextureSampler);
    return source;
}

static bool InDeviceMemory(const HdBinding binding)
{
    switch (binding.GetType()) {
        case HdBinding::SSBO:
        case HdBinding::UBO:
            return true;
        default:
            return false;
    }
}

static const char *
_GetPackedTypeDefinitions()
{
    return
    "#define hd_ivec2 packed_int2\n"
    "#define hd_ivec3 packed_int3\n"
    "#define hd_vec2 packed_float2\n"
    "#define hd_dvec2 packed_float2\n"
    "#define hd_vec3 packed_float3\n"
    "#define hd_dvec3 packed_float3\n"
    "struct hd_mat3  { float m00, m01, m02,\n"
    "                        m10, m11, m12,\n"
    "                        m20, m21, m22;\n"
    "                    hd_mat3(float _00, float _01, float _02,\n"
    "                            float _10, float _11, float _12,\n"
    "                            float _20, float _21, float _22)\n"
    "                              : m00(_00), m01(_01), m02(_02)\n"
    "                              , m10(_10), m11(_11), m12(_12)\n"
    "                              , m20(_20), m21(_21), m22(_22) {}\n"
    "                };\n"
    "struct hd_dmat3  { float m00, m01, m02,\n"
    "                         m10, m11, m12,\n"
    "                         m20, m21, m22;\n"
    "                    hd_dmat3(float _00, float _01, float _02,\n"
    "                            float _10, float _11, float _12,\n"
    "                            float _20, float _21, float _22)\n"
    "                              : m00(_00), m01(_01), m02(_02)\n"
    "                              , m10(_10), m11(_11), m12(_12)\n"
    "                              , m20(_20), m21(_21), m22(_22) {}\n"
    "                };\n"
    "#define hd_ivec3_get(v) packed_int3(v)\n"
    "#define hd_vec3_get(v)  packed_float3(v)\n"
    "#define hd_dvec3_get(v) packed_float3(v)\n"
    "mat3  hd_mat3_get(hd_mat3 v)   { return mat3(v.m00, v.m01, v.m02,\n"
    "                                             v.m10, v.m11, v.m12,\n"
    "                                             v.m20, v.m21, v.m22); }\n"
    "mat3  hd_mat3_get(mat3 v)      { return v; }\n"
    "dmat3 hd_dmat3_get(hd_dmat3 v) { return dmat3(v.m00, v.m01, v.m02,\n"
    "                                              v.m10, v.m11, v.m12,\n"
    "                                              v.m20, v.m21, v.m22); }\n"
    "dmat3 hd_dmat3_get(dmat3 v)    { return v; }\n"
    "hd_ivec3 hd_ivec3_set(hd_ivec3 v) { return v; }\n"
    "hd_ivec3 hd_ivec3_set(ivec3 v)    { return v; }\n"
    "hd_vec3 hd_vec3_set(hd_vec3 v)    { return v; }\n"
    "hd_vec3 hd_vec3_set(vec3 v)       { return v; }\n"
    "hd_dvec3 hd_dvec3_set(hd_dvec3 v) { return v; }\n"
    "hd_dvec3 hd_dvec3_set(dvec3 v)    { return v; }\n"
    "hd_mat3  hd_mat3_set(hd_mat3 v)   { return v; }\n"
    "hd_mat3  hd_mat3_set(mat3 v)      { return hd_mat3(v[0][0], v[0][1], v[0][2],\n"
    "                                                   v[1][0], v[1][1], v[1][2],\n"
    "                                                   v[2][0], v[2][1], v[2][2]); }\n"
    "hd_dmat3 hd_dmat3_set(hd_dmat3 v) { return v; }\n"
    "hd_dmat3 hd_dmat3_set(dmat3 v)    { return hd_dmat3(v[0][0], v[0][1], v[0][2],\n"
    "                                                    v[1][0], v[1][1], v[1][2],\n"
    "                                                    v[2][0], v[2][1], v[2][2]); }\n"
    "int hd_int_get(int v)          { return v; }\n" //MTL_FIXME: What are these functions really for? Why are the "templatized" for VertFrag shaders?
    "int hd_int_get(ivec2 v)        { return v[0]; }\n"
    "int hd_int_get(ivec3 v)        { return v[0]; }\n"
    "int hd_int_get(ivec4 v)        { return v[0]; }\n"
    
    // udim helper function
    "vec3 hd_sample_udim(vec2 v) {\n"
    "vec2 vf = floor(v);\n"
    "return vec3(v.x - vf.x, v.y - vf.y, clamp(vf.x, 0.0, 10.0) + 10.0 * vf.y);\n"
    "}\n"
    
    // -------------------------------------------------------------------
    // Packed HdType implementation.
    
    "struct packedint1010102 { int x:10, y:10, z:10, w:2; };\n"
    "#define packed_2_10_10_10 int\n"
    "vec4 hd_vec4_2_10_10_10_get(int v) {\n"
    "    packedint1010102 pi = *(thread packedint1010102*)&v;\n"
    "    return vec4(vec3(pi.x, pi.y, pi.z) / 511.0f, pi.w); }\n"
    "int hd_vec4_2_10_10_10_set(vec4 v) {\n"
    "    packedint1010102 pi;\n"
    "    pi.x = v.x * 511.0; pi.y = v.y * 511.0; pi.z = v.z * 511.0; pi.w = 0;\n"
    "    return *(thread int*)&pi;\n"
    "}\n"
    
    "mat4 inverse_fast(float4x4 const a) { return transpose(a); }\n"
    "mat4 inverse(float4x4 const a) {\n"
    "    float b00 = a[0][0] * a[1][1] - a[0][1] * a[1][0];\n"
    "    float b01 = a[0][0] * a[1][2] - a[0][2] * a[1][0];\n"
    "    float b02 = a[0][0] * a[1][3] - a[0][3] * a[1][0];\n"
    "    float b03 = a[0][1] * a[1][2] - a[0][2] * a[1][1];\n"
    "    float b04 = a[0][1] * a[1][3] - a[0][3] * a[1][1];\n"
    "    float b05 = a[0][2] * a[1][3] - a[0][3] * a[1][2];\n"
    "    float b06 = a[2][0] * a[3][1] - a[2][1] * a[3][0];\n"
    "    float b07 = a[2][0] * a[3][2] - a[2][2] * a[3][0];\n"
    "    float b08 = a[2][0] * a[3][3] - a[2][3] * a[3][0];\n"
    "    float b09 = a[2][1] * a[3][2] - a[2][2] * a[3][1];\n"
    "    float b10 = a[2][1] * a[3][3] - a[2][3] * a[3][1];\n"
    "    float b11 = a[2][2] * a[3][3] - a[2][3] * a[3][2];\n"
        
    "    float invdet = 1.0 / (b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06);\n"
        
    "    return mat4(a[1][1] * b11 - a[1][2] * b10 + a[1][3] * b09,\n"
    "                a[0][2] * b10 - a[0][1] * b11 - a[0][3] * b09,\n"
    "                a[3][1] * b05 - a[3][2] * b04 + a[3][3] * b03,\n"
    "                a[2][2] * b04 - a[2][1] * b05 - a[2][3] * b03,\n"
    "                a[1][2] * b08 - a[1][0] * b11 - a[1][3] * b07,\n"
    "                a[0][0] * b11 - a[0][2] * b08 + a[0][3] * b07,\n"
    "                a[3][2] * b02 - a[3][0] * b05 - a[3][3] * b01,\n"
    "                a[2][0] * b05 - a[2][2] * b02 + a[2][3] * b01,\n"
    "                a[1][0] * b10 - a[1][1] * b08 + a[1][3] * b06,\n"
    "                a[0][1] * b08 - a[0][0] * b10 - a[0][3] * b06,\n"
    "                a[3][0] * b04 - a[3][1] * b02 + a[3][3] * b00,\n"
    "                a[2][1] * b02 - a[2][0] * b04 - a[2][3] * b00,\n"
    "                a[1][1] * b07 - a[1][0] * b09 - a[1][2] * b06,\n"
    "                a[0][0] * b09 - a[0][1] * b07 + a[0][2] * b06,\n"
    "                a[3][1] * b01 - a[3][0] * b03 - a[3][2] * b00,\n"
    "                a[2][0] * b03 - a[2][1] * b01 + a[2][2] * b00) * invdet;\n"
    "}\n\n";
}

static TfToken const &
_GetPackedType(TfToken const &token, bool packedAlignment)
{
    if (packedAlignment) {
        if (token == _tokens->ivec2) {
            return _tokens->hd_ivec2;
        } else if (token == _tokens->vec2) {
            return _tokens->hd_vec2;
        } else if (token == _tokens->dvec2) {
            return _tokens->hd_dvec2;
        } if (token == _tokens->ivec3) {
            return _tokens->hd_ivec3;
        } else if (token == _tokens->vec3) {
            return _tokens->hd_vec3;
        } else if (token == _tokens->dvec3) {
            return _tokens->hd_dvec3;
        } else if (token == _tokens->mat3) {
            return _tokens->hd_mat3;
        } else if (token == _tokens->dmat3) {
            return _tokens->hd_dmat3;
        }
    }
    if (token == _tokens->packed_2_10_10_10) {
        return _tokens->_int;
    }
    return token;
}

static TfToken const &
_GetComponentType(TfToken const &token)
{
    if (token == _tokens->ivec2|| token == _tokens->ivec3
        || token == _tokens->ivec4 ) {
        return _tokens->_int;
    } else if (token == _tokens->vec2 || token == _tokens->vec3 ||
               token == _tokens->vec4) {
        return _tokens->_float;
    } else if (token == _tokens->dvec2 || token == _tokens->dvec3 ||
               token == _tokens->dvec4) {
        return _tokens->_double;
    } else if (token == _tokens->packed_2_10_10_10) {
        return _tokens->_int;
    }
    return token;
}

static TfToken const &
_GetUnpackedType(TfToken const &token, bool packedAlignment)
{
    if (token == _tokens->packed_2_10_10_10) {
        return _tokens->vec4;
    }
    else if (token == _tokens->_float) {
        return _tokens->wrapped_float;
    }
    else if (token == _tokens->_int) {
        return _tokens->wrapped_int;
    }
    return token;
}

static TfToken const &
_GetPackedTypeAccessor(TfToken const &token, bool packedAlignment)
{
    if (packedAlignment) {
        if (token == _tokens->ivec3) {
            return _tokens->hd_ivec3_get;
        } else if (token == _tokens->vec3) {
            return _tokens->hd_vec3_get;
        } else if (token == _tokens->dvec3) {
            return _tokens->hd_dvec3_get;
        } else if (token == _tokens->mat3) {
            return _tokens->hd_mat3_get;
        } else if (token == _tokens->dmat3) {
            return _tokens->hd_dmat3_get;
        }
    }
    if (token == _tokens->packed_2_10_10_10) {
        return _tokens->hd_vec4_2_10_10_10_get;
    }
    return token;
}

static TfToken const &
_GetPackedTypeMutator(TfToken const &token, bool packedAlignment)
{
    if (packedAlignment) {
        if (token == _tokens->ivec3) {
            return _tokens->hd_ivec3_set;
        } else if (token == _tokens->vec3) {
            return _tokens->hd_vec3_set;
        } else if (token == _tokens->dvec3) {
            return _tokens->hd_dvec3_set;
        } else if (token == _tokens->mat3) {
            return _tokens->hd_mat3_set;
        } else if (token == _tokens->dmat3) {
            return _tokens->hd_dmat3_set;
        }
    }
    if (token == _tokens->packed_2_10_10_10) {
        return _tokens->hd_vec4_2_10_10_10_set;
    }
    return token;
}

static TfToken const &
_GetFlatType(TfToken const &token)
{
    if (token == _tokens->ivec2) {
        return _tokens->_int;
    } else if (token == _tokens->ivec3) {
        return _tokens->_int;
    } else if (token == _tokens->ivec4) {
        return _tokens->_int;
    } else if (token == _tokens->vec2) {
        return _tokens->_float;
    } else if (token == _tokens->vec3) {
        return _tokens->_float;
    } else if (token == _tokens->vec4) {
        return _tokens->_float;
    } else if (token == _tokens->dvec2) {
        return _tokens->_float;
    } else if (token == _tokens->dvec3) {
        return _tokens->_float;
    } else if (token == _tokens->dvec4) {
        return _tokens->_float;
    } else if (token == _tokens->mat3) {
        return _tokens->_float;
    } else if (token == _tokens->mat4) {
        return _tokens->_float;
    } else if (token == _tokens->dmat3) {
        return _tokens->_float;
    } else if (token == _tokens->dmat4) {
        return _tokens->_float;
    } else if (token == _tokens->_bool) {
        return _tokens->_int;
    }

    return token;
}

static std::string _GetPackedMSLType(const std::string& dataType) {
    if(dataType == "vec2" || dataType == "float2")  return "packed_float2";
    if(dataType == "vec3" || dataType == "float3")  return "packed_float3";
    if(dataType == "vec4" || dataType == "float4")  return "packed_float4";
    if(dataType == "int2")                          return "packed_int2";
    if(dataType == "int3")                          return "packed_int3";
    if(dataType == "int4")                          return "packed_int4";
    if(dataType == "uint2")                         return "packed_uint2";
    if(dataType == "uint3")                         return "packed_uint3";
    if(dataType == "uint4")                         return "packed_uint4";
    return dataType;
}

static HdSt_CodeGenMSL::TParam& _AddInputParam(
        HdSt_CodeGenMSL::InOutParams &inputParams,
        TfToken const &name,
        TfToken const& type,
        TfToken const &attribute,
        HdBinding const &binding =
        HdBinding(HdBinding::UNKNOWN, 0),
        int arraySize = 0, TfToken const &accessor = TfToken())
{
    HdSt_CodeGenMSL::TParam in(name, type, accessor, attribute, HdSt_CodeGenMSL::TParam::Unspecified, binding, arraySize);
    HdBinding::Type bindingType = binding.GetType();
    if(bindingType == HdBinding::VERTEX_ID ||
       bindingType == HdBinding::BASE_VERTEX_ID ||
       bindingType == HdBinding::INSTANCE_ID ||
       bindingType == HdBinding::FRONT_FACING) {
        in.usage |= HdSt_CodeGenMSL::TParam::EntryFuncArgument;
    }
    
    if(bindingType == HdBinding::UNIFORM || bindingType == HdBinding::UNIFORM_ARRAY)
        in.usage |= HdSt_CodeGenMSL::TParam::Uniform;
 
    inputParams.push_back(in);
    return inputParams.back();
}

static HdSt_CodeGenMSL::TParam& _AddInputParam(  HdSt_CodeGenMSL::InOutParams &inputParams,
                                            HdSt_ResourceBinder::MetaData::BindingDeclaration const &bd,
                                            TfToken const &attribute = TfToken(), int arraySize = 0)
{
    return _AddInputParam(inputParams,
                     bd.name, bd.dataType, attribute,
                     bd.binding, arraySize);
}

static HdSt_CodeGenMSL::TParam& _AddInputPtrParam(
        HdSt_CodeGenMSL::InOutParams &inputParams,
        TfToken const &name, TfToken const& type, TfToken const &attribute,
        HdBinding const &binding, int arraySize = 0, bool programScope = false, bool writable = false)
{
    // MTL_FIXME - we need to map vec3 device pointers to the packed variants as that's how HYDRA presents its buffers
    // but we should orobably alter type at source not do a last minute fix up here
    TfToken dataType = type;
    if (type == _tokens->vec3) {
        dataType = _tokens->hd_vec3;
    }
    TfToken ptrName(std::string("*") + name.GetString());
    HdSt_CodeGenMSL::TParam& result(_AddInputParam(inputParams, ptrName, dataType, attribute, binding, arraySize));
    result.usage |= HdSt_CodeGenMSL::TParam::Usage::EntryFuncArgument;
    
    if (programScope)
        result.usage |= HdSt_CodeGenMSL::TParam::Usage::ProgramScope;
    
    if (writable)
        result.usage |= HdSt_CodeGenMSL::TParam::Usage::Writable;
    
    return result;
}

static HdSt_CodeGenMSL::TParam& _AddInputPtrParam(
        HdSt_CodeGenMSL::InOutParams &inputParams,
        HdSt_ResourceBinder::MetaData::BindingDeclaration const &bd,
        TfToken const &attribute = TfToken(), int arraySize = 0, bool programScope = false)
{
    return _AddInputPtrParam(inputParams,
                        bd.name, bd.dataType, attribute,
                        bd.binding, arraySize, programScope, bd.writable);
}

static void _EmitDeclaration(std::stringstream &str,
                             TfToken const &name,
                             TfToken const &type,
                             TfToken const &attribute,
                             HdBinding const &binding,
                             int arraySize = 0)
{
    if (!arraySize)
        str << _GetPackedType(type, true) << " " << name << ";\n";
    else
        str << "device const " << _GetPackedType(type, true) << " *" << name /* << "[" << arraySize << "]"*/  << ";\n";
}

static void _EmitDeclaration(std::stringstream &str,
                             HdSt_ResourceBinder::MetaData::BindingDeclaration const &bd,
                             TfToken const &attribute = TfToken(), int arraySize = 0)
{
    _EmitDeclaration(str,
                     bd.name, bd.dataType, attribute,
                     bd.binding, arraySize);
}

static void _EmitDeclarationPtr(std::stringstream &str,
                                TfToken const &name, TfToken const &type, TfToken const &attribute,
                                HdBinding const &binding, int arraySize = 0, bool programScope = false)
{
    TfToken ptrName(std::string("*") + name.GetString());
    str << "device const ";
    if (programScope) {
        str << "ProgramScope<st>::";
    }
    
    _EmitDeclaration(str, ptrName, type, attribute, binding, arraySize);
}

static void _EmitDeclarationMutablePtr(std::stringstream &str,
                                TfToken const &name, TfToken const &type, TfToken const &attribute,
                                HdBinding const &binding, int arraySize = 0, bool programScope = false)

{
    TfToken ptrName(std::string("*") + name.GetString());
    str << "device VTXCONST ";
    if (programScope) {
        str << "ProgramScope<st>::";
    }
    
    _EmitDeclaration(str, ptrName, type, attribute, binding, arraySize);
}
static void _EmitDeclarationPtr(std::stringstream &str,
                                HdSt_ResourceBinder::MetaData::BindingDeclaration const &bd,
                                TfToken const &attribute = TfToken(), int arraySize = 0, bool programScope = false)
{
    _EmitDeclarationPtr(str,
                        bd.name, bd.dataType, attribute,
                        bd.binding, arraySize, programScope);
}

static void _EmitDeclarationMutablePtr(std::stringstream &str,
                                HdSt_ResourceBinder::MetaData::BindingDeclaration const &bd,
                                TfToken const &attribute = TfToken(), int arraySize = 0, bool programScope = false)
{
    _EmitDeclarationMutablePtr(str,
                        bd.name, bd.dataType, attribute,
                        bd.binding, arraySize, programScope);
}


// TODO: Shuffle code to remove these declarations.
static void _EmitStructAccessor(std::stringstream &str,
                                TfToken const &structMemberName,
                                TfToken const &name,
                                TfToken const &type,
                                int arraySize,
                                bool pointerDereference,
                                const char *index);

static void _EmitComputeAccessor(std::stringstream &str,
                                 TfToken const &structMemberName,
                                 TfToken const &name,
                                 TfToken const &type,
                                 HdBinding const &binding,
                                 const char *index);

static void _EmitComputeMutator(std::stringstream &str,
                                TfToken const &structMemberName,
                                TfToken const &name,
                                TfToken const &type,
                                HdBinding const &binding,
                                const char *index);

static void _EmitAccessor(std::stringstream &str,
                          TfToken const &name,
                          TfToken const &type,
                          HdBinding const &binding,
                          const char *index=NULL);

static void _EmitOutput(std::stringstream &str,
                        TfToken const &name, TfToken const &type, TfToken const &attribute = TfToken(),
                        HdSt_CodeGenMSL::TParam::Usage usage = HdSt_CodeGenMSL::TParam::Unspecified)
{
    METAL_DEBUG_COMMENT(&str, "_EmitOutput\n"); //MTL_FIXME
    str << type << " " << name << ";\n";
}

static HdSt_CodeGenMSL::TParam& _AddOutputParam(HdSt_CodeGenMSL::InOutParams &outputParams,
                                                TfToken const &name, TfToken const &type)
{
    HdSt_CodeGenMSL::TParam out(name, type, TfToken(), TfToken(),
        HdSt_CodeGenMSL::TParam::Unspecified, HdBinding(HdBinding::UNKNOWN, 0),
        0);
    outputParams.push_back(out);
    return outputParams.back();
}

static HdSt_CodeGenMSL::TParam& _EmitStructMemberOutput(HdSt_CodeGenMSL::InOutParams &outputParams,
                                                        TfToken const &name,
                                                        TfToken const &accessor,
                                                        TfToken const &type,
                                                        TfToken const &attribute = TfToken(),
                                                        HdSt_CodeGenMSL::TParam::Usage usage = HdSt_CodeGenMSL::TParam::Unspecified)
{
    HdSt_CodeGenMSL::TParam out(name, type, accessor, attribute, usage);
    outputParams.push_back(out);
    return outputParams.back();
}

/*
  1. If the member is a scalar consuming N basic machine units,
  the base alignment is N.
  2. If the member is a two- or four-component vector with components
  consuming N basic machine units, the base alignment is 2N or 4N,
  respectively.
  3. If the member is a three-component vector with components
  consuming N basic machine units, the base alignment is 4N.
  4. If the member is an array of scalars or vectors, the base
  alignment and array stride are set to match the base alignment of
  a single array element, according to rules (1), (2), and (3), and
  rounded up to the base alignment of a vec4. The array may have
  padding at the end; the base offset of the member following the
  array is rounded up to the next multiple of the base alignment.

  9. If the member is a structure, the base alignment of the structure
  is <N>, where <N> is the largest base alignment value of any of its
  members, and rounded up to the base alignment of a vec4. The
  individual members of this sub-structure are then assigned offsets
  by applying this set of rules recursively, where the base offset of
  the first member of the sub-structure is equal to the aligned offset
  of the structure. The structure may have padding at the end; the
  base offset of the member following the sub-structure is rounded up
  to the next multiple of the base alignment of the structure.

  When using the std430 storage layout, shader storage blocks will be
  laid out in buffer storage identically to uniform and shader storage
  blocks using the std140 layout, except that the base alignment and
  stride of arrays of scalars and vectors in rule 4 and of structures
  in rule 9 are not rounded up a multiple of the base alignment of a
  vec4.

  i.e. rule 3 is still applied in std430. we use an array of 3-element
  struct instead of vec3/dvec3 to avoid this undesirable padding.

  struct instanceData0 {
    float x, y, z;
  }
  buffer buffer0 {
    instanceData0 data[];
  };
*/

static TfToken const &
_GetSamplerBufferType(TfToken const &token)
{
    if (token == _tokens->_int  ||
        token == _tokens->ivec2 ||
        token == _tokens->ivec3 ||
        token == _tokens->ivec4 ||
        token == _tokens->packed_2_10_10_10) {
        return _tokens->isamplerBuffer;
    } else {
        return _tokens->samplerBuffer;
    }
}

namespace {
    struct AddressSpace {
        AddressSpace(HdBinding const &binding) :
            binding(binding) {
        }
        friend std::ostream & operator << (std::ostream & out,
                                           const AddressSpace &lq);
        HdBinding binding;
    };
    std::ostream & operator << (std::ostream & out, const AddressSpace &lq)
    {
        GarchContextCaps const &caps = GarchResourceFactory::GetInstance()->GetContextCaps();
        int location = lq.binding.GetLocation();

        switch (lq.binding.GetType()) {
        case HdBinding::DRAW_INDEX:
        case HdBinding::DRAW_INDEX_INSTANCE:
        case HdBinding::DRAW_INDEX_INSTANCE_ARRAY:
        case HdBinding::UBO:
            out << "constant ";
            break;
        case HdBinding::UNIFORM:
        case HdBinding::UNIFORM_ARRAY:
        case HdBinding::SSBO:
        case HdBinding::BINDLESS_UNIFORM:
        case HdBinding::TEXTURE_2D:
        case HdBinding::BINDLESS_TEXTURE_2D:
        case HdBinding::TEXTURE_FIELD:
        case HdBinding::BINDLESS_TEXTURE_FIELD:
        case HdBinding::TEXTURE_UDIM_ARRAY:
        case HdBinding::BINDLESS_TEXTURE_UDIM_ARRAY:
        case HdBinding::TEXTURE_UDIM_LAYOUT:
        case HdBinding::BINDLESS_TEXTURE_UDIM_LAYOUT:
        case HdBinding::TEXTURE_PTEX_TEXEL:
        case HdBinding::TEXTURE_PTEX_LAYOUT:
            out << "device ";
            break;
        default:
            break;
        }
        return out;
    }
}

void
HdSt_CodeGenMSL::_ParseHints(std::stringstream &source) {
    std::string result = source.str();

    //Scan for MTL_HINTs
    std::string::size_type cursor = 0;
    bool hintError = false;
    const static std::string    strHint("// MTL_HINT"),
                                strPassthrough("PASSTHROUGH"),
                                strUses("USES:"),
                                strAffects("AFFECTS:"),
                                strExports("EXPORTS:"),
                                strWhitespace(" \t\n\r"),
                                strNewline("\n\r"),
                                strSeparator(", \t\n\r");
    struct MSL_CodeGen_Hint {
        std::set<std::string> uses;
        std::set<std::string> affects;
        std::string           _export;
        bool                  isPassthrough;
        MSL_CodeGen_Hint() : isPassthrough(false) {}
    };
    std::vector<MSL_CodeGen_Hint> hints;
    while((cursor = result.find(strHint, cursor)) != std::string::npos) {
        MSL_CodeGen_Hint hint;
        cursor += strHint.length();
        std::string::size_type endOfHint = result.find_first_of(strNewline, cursor);
        std::string::size_type nextWhitespace = result.find_first_of(strWhitespace, cursor);
        while((cursor = result.find_first_not_of(strWhitespace, nextWhitespace)) < endOfHint) {
            nextWhitespace = result.find_first_of(strWhitespace, cursor);
            if(result.compare(cursor, strUses.length(), strUses) == 0) {
                cursor += strUses.length();
                std::string::size_type labelEnd;
                while((labelEnd = result.find_first_of(strSeparator, cursor)) <= nextWhitespace) {
                    hint.uses.insert(result.substr(cursor, labelEnd - cursor));
                    cursor = labelEnd + 1;
                }
            } else if(result.compare(cursor, strAffects.length(), strAffects) == 0) {
                cursor += strAffects.length();
                std::string::size_type labelEnd;
                while((labelEnd = result.find_first_of(strSeparator, cursor)) <= nextWhitespace) {
                    hint.affects.insert(result.substr(cursor, labelEnd - cursor));
                    cursor = labelEnd + 1;
                }
            } else if(result.compare(cursor, strExports.length(), strExports) == 0) {
                cursor += strExports.length();
                std::string::size_type labelEnd;
                while((labelEnd = result.find_first_of(strSeparator, cursor)) <= nextWhitespace) {
                    if(!hint._export.empty()) { hintError = true; break; }
                    hint._export = result.substr(cursor, labelEnd - cursor);
                    cursor = labelEnd + 1;
                }
            } else if(result.compare(cursor, strPassthrough.length(), strPassthrough) == 0) {
                cursor += strPassthrough.length();
                hint.isPassthrough = true;
            }
            else { hintError = true; break; }
        }
        hints.push_back(hint);
    }

    if(hintError) {
        TF_FATAL_CODING_ERROR("Malformed MTL_HINT!");
    } else {
        //Propagate passthrough status
        bool changed = true;
        while(changed) {
            changed = false;
            for(auto& hint : hints) {
                if(hint.isPassthrough || hint.uses.empty())
                    continue;
                
                bool canPassthrough = true;
                //For every dependency check if it prohibits passthrough
                for(const auto& usesLabel : hint.uses) {
                    for(const auto& _hint : hints) {
                        if(_hint.affects.count(usesLabel) == 0 || _hint.isPassthrough)
                            continue;
                        canPassthrough = false;
                        break;
                    }
                }
                
                if(canPassthrough) {
                    hint.isPassthrough = true;
                    changed = true;
                }
            }
        }
    
        //Set ignored GS exports
        for(const auto& hint : hints) {
            if(hint._export.empty() || !hint.isPassthrough)
                continue;
            _gsIgnoredExports.insert(hint._export);
        }
    }
}

void
HdSt_CodeGenMSL::_ParseGLSL(std::stringstream &source, InOutParams& inParams, InOutParams& outParams, bool asComputeGS)
{
    static std::regex regex_word("(\\S+)");

    std::string result = source.str();
    std::stringstream dummy;
    
    if(asComputeGS) {
        //For now these are the only types we understand. Should get a proper treatment that accepts any types/number.
        std::string::size_type inLayoutPos = result.find("layout(triangles) in;");
        if(inLayoutPos == std::string::npos)
            inLayoutPos = result.find("layout(lines_adjacency) in;");
        if(inLayoutPos != std::string::npos)
            result.insert(inLayoutPos, "//");
        
        std::string::size_type outLayoutPos = result.find("layout(triangle_strip, max_vertices = 3) out;");
        if(outLayoutPos == std::string::npos)
            outLayoutPos = result.find("layout(triangle_strip, max_vertices = 6) out;");
        if(outLayoutPos != std::string::npos)
            result.insert(outLayoutPos, "//");
    }
    
    struct TagSpec {
        TagSpec(char const* const _tag, InOutParams& _params, bool _isInput)
        : glslTag(_tag),
          params(_params),
          isInput(_isInput)
        {}

        std::string  glslTag;
        InOutParams& params;
        bool         isInput; //If not -> output
    };
    
    std::vector<TagSpec> tags;
    
    tags.push_back(TagSpec("\nout ", outParams, false));
    tags.push_back(TagSpec("\nin ", inParams, true));
    int uniformIndex = tags.size();
    tags.push_back(TagSpec("\nuniform ", inParams, true));
    tags.push_back(TagSpec("\nlayout(std140) uniform ", inParams, true));
    
    TfTokenVector mslAttribute;
    
    int firstPerspectiveIndex = tags.size();
    tags.push_back(TagSpec("\nflat out ", outParams, false));
    mslAttribute.push_back(TfToken("[[flat]]"));
    tags.push_back(TagSpec("\nflat in ", inParams, true));
    mslAttribute.push_back(TfToken("[[flat]]"));
    
    tags.push_back(TagSpec("\ncentroid out ", outParams, false));
    mslAttribute.push_back(TfToken("[[centroid_perspective]]"));
    tags.push_back(TagSpec("\ncentroid in ", inParams, true));
    mslAttribute.push_back(TfToken("[[centroid_perspective]]"));
    
    tags.push_back(TagSpec("\nnoperspective out ", outParams, false));
    mslAttribute.push_back(TfToken("[[center_no_perspective]]"));
    tags.push_back(TagSpec("\nnoperspective in ", inParams, true));
    mslAttribute.push_back(TfToken("[[center_no_perspective]]"));

    int pass = 0;
    for (auto tag : tags) {

        std::string::size_type pos = 0;
        int tagSize = tag.glslTag.length() - 1;

        bool isUniform = tag.glslTag == "\nuniform" || tag.glslTag == "\nlayout(std140) uniform ";

        while ((pos = result.find(tag.glslTag, pos)) != std::string::npos) {
            
            // check for a ';' before the next '\n'
            std::string::size_type newLine = result.find_first_of('\n', pos + tagSize);
            std::string::size_type semiColon = result.find_first_of(';', pos + tagSize);
            
            if (newLine < semiColon) {
                std::string::size_type endOfName = result.find_first_of(" {\n", pos + tag.glslTag.length());
                std::string structName = asComputeGS ? (tag.isInput ? "__in_" : "__out_") : "";
                structName += result.substr(pos + tagSize + 1, endOfName - (pos + tagSize + 1));
                TfToken structNameToken(structName.c_str());
                TfToken bufferNameToken;
                TfToken bufferNameTokenPtr;
                {
                    std::stringstream bufferVarName;
                    bufferVarName << "___" << structName;
                    bufferNameToken = TfToken(bufferVarName.str().c_str());
                    std::stringstream bufferVarNamePtr;
                    bufferVarNamePtr << "*" << bufferVarName.str();
                    bufferNameTokenPtr = TfToken(bufferVarNamePtr.str().c_str());
                }
                
                //Prefix in/out to prevent duplicate struct names in Geometry Shaders.
                if(asComputeGS)
                    result.replace(pos + tagSize + 1, endOfName - (pos + tagSize + 1), structName);
                
                // output structure. Replace the 'out' tag with 'struct'. Search between the {} for lines, and extract a type and name from each one.
                result.replace(pos, tagSize, std::string("\nstruct"));
                
                std::string::size_type openParenthesis = result.find_first_of('{', pos);
                std::string::size_type closeParenthesis = result.find_first_of('}', pos);
                std::string::size_type lineStart = openParenthesis + 1;
                
                // Grab the variable instance name
                std::string::size_type endLine = result.find_first_of(';', closeParenthesis + 1);
                std::string line = result.substr(closeParenthesis + 1, endLine - closeParenthesis - 1);
                
                std::smatch match;
                std::string parent;
                std::string parentAccessor;
                std::string::size_type structNamePos, structNameLength;
                if (std::regex_search(line, match, regex_word)) {
                    structNamePos = match[0].first  - line.begin();
                    structNameLength = match[0].second - match[0].first;
                    parent = line.substr(structNamePos, structNameLength);
                    parentAccessor = parent + ".";
                }

                bool instantiatedStruct = !parentAccessor.empty();

                pos = lineStart;
                
                std::stringstream structAccessors;
                while ((pos = result.find("\n", pos)) != std::string::npos &&
                    pos < closeParenthesis)
                {
                    endLine = result.find_first_of(';', lineStart + 1);
                    line = result.substr(lineStart, endLine - lineStart);
                    
                    auto words_begin = std::sregex_iterator(result.begin() + lineStart, result.begin() + endLine, regex_word);
                    auto words_end = std::sregex_iterator();
                    int numWords = std::distance(words_begin, words_end);
                    
                    if (numWords == 2 || numWords == 3) // qualifier, type, name
                    {
                        std::sregex_iterator i = words_begin;
                        TfToken qualifier;
                        if(numWords == 3) {
                            NSLog(@"HdSt_CodeGenMSL::_ParseGLSL - Ignoring qualifier (for now)"); //MTL_FIXME - Add support for interpolation type (qualifier) here
                            qualifier = TfToken((*i).str().c_str());
                            ++i;
                        }
                        TfToken type((*i).str().c_str());
                        ++i;
                        TfToken name((*i).str().c_str());
                        TfToken accessor((parentAccessor + (*i).str()).c_str());
                        
                        //Only output these as individuals if:
                        // - the uniform block is unnamed
                        // - the block is marked "in" or "out"
                        if(instantiatedStruct)
                        {
                            if(!isUniform) {
                                std::string accessor_str = accessor.GetString();
                                size_t posOpen = accessor_str.find_first_of("[");
                                size_t posClose = accessor_str.find_first_of("]");
                                if(posOpen != std::string::npos && posClose != std::string::npos)
                                    accessor_str.replace(posOpen + 1, posClose - (posOpen+1), "i");
                                _EmitStructMemberOutput(tag.params, name, TfToken(accessor_str), type).usage |= TParam::Usage::VertexData;
                            }
                        }
                        else
                        {
                            std::string nameStr = name.GetString();
                            std::string::size_type openingBracket = nameStr.find_first_of("[");
                            if(openingBracket != std::string::npos) {
                                nameStr = nameStr.substr(0, openingBracket);
                                structAccessors << ";\n device const " << type.GetString() << "* " << nameStr;
                            }
                            else
                                structAccessors << ";\n" << type.GetString() << " " << name.GetString();
                            HdSt_CodeGenMSL::TParam outParam(name, type, bufferNameToken, TfToken(), HdSt_CodeGenMSL::TParam::Usage::UniformBlockMember);
                            tag.params.push_back(outParam);
                        }
                    }
                    else if (numWords) { // Allow blank lines
                        TF_CODING_WARNING("Unparsable glslfx line in '%s<type> <name>;' definition. Expecting '%s<type> <name>;'. Got %s",
                                          tag.glslTag.substr(1).c_str(),
                                          tag.glslTag.substr(1).c_str(),
                                          result.substr(pos + 1, endLine - pos - 1).c_str());
                    }

                    lineStart = result.find("\n", endLine) + 1;
                    pos = lineStart;
                }
                
                if(!instantiatedStruct)
                {
                    result.replace(closeParenthesis + 1, 0, structAccessors.str());
                    HdSt_CodeGenMSL::TParam outParam(bufferNameTokenPtr, structNameToken, TfToken(), TfToken(), HdSt_CodeGenMSL::TParam::Usage::ProgramScope | HdSt_CodeGenMSL::TParam::Usage::EntryFuncArgument | HdSt_CodeGenMSL::TParam::Usage::UniformBlock);
                    tag.params.push_back(outParam);
                }
                else if(isUniform)
                {
                    HdSt_CodeGenMSL::TParam outParam(TfToken(parent), structNameToken, TfToken(), TfToken(), HdSt_CodeGenMSL::TParam::Usage::ProgramScope | HdSt_CodeGenMSL::TParam::Usage::EntryFuncArgument | HdSt_CodeGenMSL::TParam::Usage::UniformBlock);
                    tag.params.push_back(outParam);
                }
                
                pos = closeParenthesis + 1;
            }
            else {
                // Single line - remove the tag from the GLSL. Extract the type and variable name from the string.
                result.replace(pos, tagSize, std::string("\n"));
                std::string::size_type endLine = result.find_first_of(';', pos + 1);

                std::string line = result.substr(pos + 1, endLine - pos - 1);

                auto words_begin = std::sregex_iterator(line.begin(), line.end(), regex_word);
                auto words_end = std::sregex_iterator();
                
                if (std::distance(words_begin, words_end) == 2)
                {
                    std::sregex_iterator i = words_begin;
                    std::string t = (*i).str();
                    std::string n = (*++i).str();
                    char const* const typeStr = t.c_str();
                    char const* const nameStr = n.c_str();
                    
                    TfToken type(typeStr);
                    TfToken name(nameStr);
                    
                    // detect if this is a texture or a sampler, and mark accordingly
                    HdSt_CodeGenMSL::TParam::Usage usage = HdSt_CodeGenMSL::TParam::Unspecified;
                    if (!strncmp(typeStr, "texture", 7) ||
                        !strncmp(typeStr, "depth", 5)) {
                        usage = HdSt_CodeGenMSL::TParam::Texture;
                    }
                    else if (!strncmp(typeStr, "sampler", 7)) {
                        usage = HdSt_CodeGenMSL::TParam::Sampler;
                    }
                    else if (pass == uniformIndex) {//if (!strncmp(typeStr, "mat4", 7)) {
                        usage = HdSt_CodeGenMSL::TParam::Uniform;
                    }

                    if (nameStr[0] == '*') {
                        result.replace(pos, 0, std::string("\ndevice "));
                        usage |= HdSt_CodeGenMSL::TParam::EntryFuncArgument;
                        
                        // If this is a built-in type, we want to use global scope to access
                        // If it's a custom struct, we want to use ProgramScope to access
                        // We crudely detect this by searching for 'struct TypeName' in the source.
                        // XXX This needs improving, as it's very easy to break it!
                        std::stringstream search;
                        search << "struct " << type.GetString();
                        if (result.find(search.str())) {
                            usage |= HdSt_CodeGenMSL::TParam::ProgramScope;
                        }
                    }
                    
                    TParam &param = _AddOutputParam(tag.params, name, type);
                    if (pass >= firstPerspectiveIndex) {
                        param.attribute = mslAttribute[pass - firstPerspectiveIndex];
                    }
                    param.usage = usage;
                }
                else {
                    TF_CODING_WARNING("Unparsable glslfx line in '%s<type> <name>;' definition. Expecting '%s<type> <name>;'. Got %s",
                                      tag.glslTag.substr(1).c_str(),
                                      tag.glslTag.substr(1).c_str(),
                                      result.substr(pos + 1, endLine - pos - 1).c_str());
                }
            }
        }
        pass++;
    }
    source.clear();
    source.str(result);
    source.seekp(0, std::stringstream::end);
}

static bool IsIgnoredVSAttribute(const TfToken& name) {
    const static TfToken ignoreList[] = {
        TfToken("tesPatchCoord"),
        TfToken("tesTessCoord"),
        TfToken("gsPatchCoord"),
        TfToken("gsTessCoord") };

    for(uint i = 0; i < (sizeof(ignoreList) / sizeof(ignoreList[0])); i++) {
        if(ignoreList[i] != name)
            continue;
        return true;
    }
    return false;
}

void HdSt_CodeGenMSL::_GenerateGlue(std::stringstream& glueVS,
                                    std::stringstream& glueGS,
                                    std::stringstream& gluePS,
                                    std::stringstream& glueCS,
                                    HdStGLSLProgramMSLSharedPtr mslProgram)
{
    std::stringstream   glueCommon, copyInputsVtx, copyOutputsVtx, copyInputsVtxStruct_Compute,
                        copyInputsVtx_Compute, copyGSOutputsIntoVSOutputs, fragExtrasStruct;
    std::stringstream   copyInputsFrag, copyOutputsFrag;

    std::stringstream   vsInputStruct, vsOutputStruct, vsAttributeDefineEnabled, vsAttributeDefineDisabled, vsAttributeDefineUndef,
                        vsFuncDef, vsMI_FuncDef, csFuncDef,
                        vsMI_EP_FuncDef, vsMI_EP_FuncDefParams, vsMI_EP_CallCode, vsMI_EP_InputCode,
                        vsCode, vsEntryPointCode, vsInputCode, vsOutputCode, vsGsOutputMergeCode,
                        vsUniformStruct, drawArgsStruct, csCode,
                        gsVertIntermediateStruct, gsVertIntermediateFlatStruct, gs_IntermediateVSOutput, gs_IntermediateVSOutput_Flat;

    METAL_DEBUG_COMMENT(&glueCommon, "_GenerateGlue(glueCommon)\n"); //MTL_FIXME
    METAL_DEBUG_COMMENT(&glueVS, "_GenerateGlue(glueVS)\n"); //MTL_FIXME
    METAL_DEBUG_COMMENT(&gluePS, "_GenerateGlue(gluePS)\n"); //MTL_FIXME
    METAL_DEBUG_COMMENT(&glueCS, "_GenerateGlue(glueCS)\n"); //MTL_FIXME
    
    vsAttributeDefineEnabled    << "/****** Vertex Attributes Specifiers are ENABLED ******/\n"
                                << "#define HD_MTL_VS_ATTRIBUTE(t,n,a,s) t n a\n"
                                << "#define HD_MTL_VS_ATTRIBUTE_ARRAY(t,n,a,s) t n a s\n\n";
    vsAttributeDefineDisabled   << "/****** Vertex Attributes Specifiers are DISABLED ******/\n"
                                << "#define HD_MTL_VS_ATTRIBUTE(t,n,a,s) t n\n"
                                << "#define HD_MTL_VS_ATTRIBUTE_ARRAY(t,n,a,s) t n s\n\n";
    vsAttributeDefineUndef      << "#undef HD_MTL_VS_ATTRIBUTE\n"
                                << "#undef HD_MTL_VS_ATTRIBUTE_ARRAY\n\n";

    drawArgsStruct              << "////////////////////////////////////////////////////////////////////////////////////////////////////////////////////\n"
                                << "// MSL Draw Args Struct ////////////////////////////////////////////////////////////////////////////////////////////\n\n"
                                << "struct MSLDrawArgs { uint indexCount, startIndex, baseVertex,\n"
                                   "    instanceCount, batchIndexOffset, primitiveCount, batchPrimitiveOffset; };\n";

    //Do an initial pass over _mslVSInputParams and _mslFSInputParams to count the number of vertexAttributes that will needs
    // slots. This allows us to do the rest of the VS/PS generation in a single pass.
    
    int vsNumVertexAttributes = 0;
    bool hasVSUniformBuffer(false), hasFSUniformBuffer(false);
    
    TF_FOR_ALL(it, _mslVSInputParams) {
        HdSt_CodeGenMSL::TParam const &input = *it;
        if(input.usage & HdSt_CodeGenMSL::TParam::EntryFuncArgument)
            continue;
        else if(input.usage & HdSt_CodeGenMSL::TParam::Uniform) {
            hasVSUniformBuffer = true;
            continue;
        }
        
        vsNumVertexAttributes++;
    }

    TF_FOR_ALL(it, _mslPSInputParams) {
        HdSt_CodeGenMSL::TParam const &input = *it;
        if((input.usage & HdSt_CodeGenMSL::TParam::EntryFuncArgument) || (input.usage & HdSt_CodeGenMSL::TParam::VertexShaderOnly))
            continue;
        else if(input.usage & HdSt_CodeGenMSL::TParam::Uniform) {
            hasFSUniformBuffer = true;
            break;
        }
    }
    
    //////////////////////////// Additional Buffer Binding /////////////////////////////////
    
    int vsUniformsBufferSlot(-1), fsUniformsBufferSlot(-1), drawArgsSlot(-1);
    int fragExtrasSlot(-1), currentUniformBufferSlot(-1), indexBufferSlot(-1);

    mslProgram->AddBinding("indices", -1, HdBinding(),
        kMSL_BindingType_IndexBuffer, kMSL_ProgramStage_Vertex);
    
    //Add an index buffer for CSGS/vsMI. Increment vertexAttribsLocation (this means that in the VS
    //the buffers are offset by 1, with 1 slot skipped, the indices). You must bind it if you're
    //running vsMI or CSGS, not required otherwise.
    if(_buildTarget != kMSL_BuildTarget_Regular) {
        indexBufferSlot = vsNumVertexAttributes;
        //Instead of using BindingType IndexBuffer we use UniformBuffer as that is how we use the indexBuffer in this case.
        mslProgram->AddBinding("indices", indexBufferSlot, HdBinding(),
            kMSL_BindingType_UniformBuffer, kMSL_ProgramStage_Vertex);
        
        if(_buildTarget == kMSL_BuildTarget_MVA_ComputeGS)
            mslProgram->AddBinding("indices", indexBufferSlot, HdBinding(),
                kMSL_BindingType_UniformBuffer, kMSL_ProgramStage_Compute);
    }
    vsNumVertexAttributes++;

    //The uniform buffers are placed right after the vertex attribute slots.
    currentUniformBufferSlot = vsNumVertexAttributes;

    //Add the DrawArgs buffer for MI calls. The index increase it causes is visible for all versions, this is intended
    //in an effort to keep the slot mapping the same between MI/GS and regular calls. The drawArgs buffer is only
    //present for MI calls. The drawArgs buffer is not known to Hydra, it is kept internal to the Metal code.
    drawArgsSlot = currentUniformBufferSlot;
    if(_buildTarget != kMSL_BuildTarget_Regular) {
        mslProgram->AddBinding("drawArgs", drawArgsSlot, HdBinding(),
            kMSL_BindingType_DrawArgs, kMSL_ProgramStage_Vertex);
        mslProgram->AddBinding("drawArgs", drawArgsSlot, HdBinding(),
            kMSL_BindingType_DrawArgs, kMSL_ProgramStage_Compute);
    }
    currentUniformBufferSlot++;
    
    //Add a binding for the Vertex output generated in compute, and a binding for compute argument buffer
    const UInt32 gsVertOutputSlot = currentUniformBufferSlot++;
    const UInt32 gsPrimOutputSlot = currentUniformBufferSlot++;
    if(_buildTarget == kMSL_BuildTarget_MVA_ComputeGS) {
        mslProgram->AddBinding("gsVertOutput", gsVertOutputSlot, HdBinding(),
            kMSL_BindingType_GSVertOutput, kMSL_ProgramStage_Compute);
        mslProgram->AddBinding("gsVertOutput", gsVertOutputSlot, HdBinding(),
            kMSL_BindingType_GSVertOutput, kMSL_ProgramStage_Vertex);      //As input
        mslProgram->AddBinding("gsVertOutput", gsVertOutputSlot, HdBinding(),
            kMSL_BindingType_GSVertOutput, kMSL_ProgramStage_Fragment);    //As input
        mslProgram->AddBinding("gsPrimOutput", gsPrimOutputSlot, HdBinding(),
            kMSL_BindingType_GSPrimOutput, kMSL_ProgramStage_Compute);
        mslProgram->AddBinding("gsPrimOutput", gsPrimOutputSlot, HdBinding(),
            kMSL_BindingType_GSPrimOutput, kMSL_ProgramStage_Vertex);      //As input
        mslProgram->AddBinding("gsPrimOutput", gsPrimOutputSlot, HdBinding(),
            kMSL_BindingType_GSPrimOutput, kMSL_ProgramStage_Fragment);    //As input
    }

    //Add our (to be) generated uniform buffer as input param for VS.
    if(hasVSUniformBuffer) {
        _AddInputParam(_mslVSInputParams, TfToken("*vsUniforms"), TfToken("MSLVsUniforms"), TfToken()).usage
            |= HdSt_CodeGenMSL::TParam::EntryFuncArgument;
    }
    
    //Add our (to be) generated uniform buffer as input param for FS.
    if(hasFSUniformBuffer) {
        _AddInputParam(_mslPSInputParams, TfToken("*fsUniforms"), TfToken("MSLFsUniforms"), TfToken()).usage
            |= HdSt_CodeGenMSL::TParam::EntryFuncArgument;
    }
    _AddInputParam(_mslPSInputParams, TfToken("*fragExtras"), TfToken("MSLFragExtras"), TfToken()).usage
        |= HdSt_CodeGenMSL::TParam::EntryFuncArgument;

    ///////////////////////// Vertex Input ////////////////////////////
    
    std::stringstream computeBufferArguments;
    
    int vsCurrentVertexAttributeSlot(0), vsUniformStructSize(0);
    
    vsFuncDef           << "vertex MSLVsOutputs vertexEntryPoint(MSLVsInputs input[[stage_in]]";
    vsMI_FuncDef        << "/****** Manually Indexed Wrapper Function (MI) ******/\n"
                        << "MSLVsOutputs vertexShader_MI("
                        << "\n    MSLVsInputs input //[[stage_in]]";
    vsMI_EP_FuncDef     << "vertex MSLVsOutputs vertexEntryPoint("
                        << "\n      uint _vertexID[[vertex_id]]";
    csFuncDef           << "kernel void computeEntryPoint(\n"
                        << "    uint _threadPositionInGrid[[thread_position_in_grid]]\n";

    if (_buildTarget != kMSL_BuildTarget_MVA_ComputeGS)
        vsMI_EP_FuncDef << "\n    , uint _instanceID[[instance_id]]";
    
    if(_buildTarget != kMSL_BuildTarget_Regular) {
        vsMI_EP_FuncDefParams   << "\n    , device const uint *indices[[buffer(" << indexBufferSlot << ")]]"
                                << "\n    , device const MSLDrawArgs *drawArgs[[buffer(" << drawArgsSlot << ")]]";
    }
    if(_buildTarget == kMSL_BuildTarget_MVA_ComputeGS) {
        vsMI_EP_FuncDef         << "\n    , const device MSLGsVertOutStruct* gsVertOutBuffer[[buffer(" << gsVertOutputSlot << ")]]"
                                << "\n    , const device MSLGsPrimOutStruct* gsPrimOutBuffer[[buffer(" << gsPrimOutputSlot << ")]]";
    }
    
    vsMI_EP_InputCode   << "    MSLVsInputs vsInput = {//";
    vsMI_EP_CallCode    << "    vsOutput = vertexShader_MI("
                        << "\n            vsInput";
    
    vsInputStruct   << "struct MSLVsInputs {\n";
    vsUniformStruct << "////////////////////////////////////////////////////////////////////////////////////////////////////////////////////\n"
                    << "// MSL VS Uniforms Struct //////////////////////////////////////////////////////////////////////////////////////////\n\n"
                    << "struct MSLVsUniforms {\n";
    
    {
        TF_FOR_ALL(it, _mslVSInputParams) {
            HdSt_CodeGenMSL::TParam const &input = *it;
            
            std::string name(input.name), dataType(input.dataType);
            bool isVertexAttribute = true;
            bool usesPackedNormals = (input.name == _tokens->packedSmoothNormals) || (input.name == _tokens->packedFlatNormals);
            
            bool inputIsAtomic = (input.dataType.GetString().find("atomic") != std::string::npos);
            bool isShaderWritable = (input.usage & (HdSt_CodeGenMSL::TParam::Usage::Mutable | HdSt_CodeGenMSL::TParam::Usage::Writable));
            
            if (input.usage & HdSt_CodeGenMSL::TParam::Uniform) {
                //This input param is a uniform
                
                vsUniformStruct << dataType << " " << name;
                if (input.arraySize) vsUniformStruct << "[" << input.arraySize << "]";
                vsUniformStruct << ";\n";

                vsInputCode << "    scope." << name << " = vsUniforms->" << name << ";\n";
                
                //Update the vsUniformStructSize, taking into account alignment and member sizes and create a binding for the uniform.
                uint32_t size = 4;
                if(input.dataType.GetString().find("vec2") != std::string::npos) size = 8;
                else if(input.dataType.GetString().find("vec3") != std::string::npos) size = 12;
                else if(input.dataType.GetString().find("vec4") != std::string::npos) size = 16;
                uint32_t regStart = vsUniformStructSize / 16;
                uint32_t regEnd = (vsUniformStructSize + size - 1) / 16;
                if(regStart != regEnd && vsUniformStructSize % 16 != 0)
                    vsUniformStructSize += 16 - (vsUniformStructSize % 16);
                //Add a binding for each uniform. They are currently all bound to slot -1 which is "patched" a little further down, once
                //the actual slot is known (depends on other elements of _mslVSInputParams which may not have been processed yet)
                mslProgram->AddBinding(input.name, -1, input.binding,
                    kMSL_BindingType_Uniform, kMSL_ProgramStage_Vertex,
                    vsUniformStructSize);
                vsUniformStructSize += size;
            }
            else if(input.usage & HdSt_CodeGenMSL::TParam::UniformBlockMember) {
                //This parameter is a uniform block member
                
                TF_FATAL_CODING_ERROR("Not implemented!");
            }
            else if (input.usage & HdSt_CodeGenMSL::TParam::EntryFuncArgument) {
                //This input param is either a built-in variable or a uniform buffer.
                
                bool availableInMI_EP = true;
                bool isPtrParam = false;
                bool inProgramScope = (input.usage & HdSt_CodeGenMSL::TParam::ProgramScope);
                std::string attrib = input.attribute.GetString();
                if (input.attribute.IsEmpty()) {
                    //This input param is a uniform buffer
                    
                    if (input.name.GetText()[0] == '*') {
                        int prefixSize = (input.usage & HdSt_CodeGenMSL::TParam::UniformBlock) ? strlen("*___") : strlen("*");
                        name = input.name.GetText() + prefixSize;
                        isPtrParam = true;
                    }
                    attrib = TfStringPrintf("[[buffer(%d)]]", currentUniformBufferSlot);
                    
                    //Check whether it is the uniform buffer we made
                    if(name == "vsUniforms")
                        vsUniformsBufferSlot = currentUniformBufferSlot;
                    else
                        mslProgram->AddBinding(name, currentUniformBufferSlot,
                            input.binding, kMSL_BindingType_UniformBuffer,
                            kMSL_ProgramStage_Vertex, 0, 0);
                    
                    currentUniformBufferSlot++;
                }
                else {
                    //This input param is a built-in variable of some kind
                    
                    // Built-in variables like gl_VertexID are _always_ supplied
                    // to the MI wrapper but are passed in via a different
                    // mechanism because of required special behavior
                    availableInMI_EP = false;
                }
                
                // Don't treat our own uniform buffer as a regular input
                // parameter. It's members will be copied over individually
                // instead.
                if(name != "vsUniforms")
                    vsInputCode << "    scope." << name << " = "
                                << name << ";\n";
                    
                vsFuncDef   << "\n    , device const "
                            << (inProgramScope ? "ProgramScope_Vert::" : "")
                            << dataType << (isPtrParam ? "* " : " ")
                            << name << attrib;
                
                csFuncDef   << "\n    , " << (isPtrParam ? "device " : "")
                            << (isShaderWritable ? "" : "const ")
                            << (inProgramScope ? "ProgramScope_Compute::" : "")
                            << dataType << (isPtrParam ? "* " : " ")
                            << name << attrib;
                
                if(availableInMI_EP) {
                    //The MI entry point needs these too in identical form.
                    vsMI_EP_FuncDefParams << "\n    , "
                            << (isPtrParam ? "device const " : "")
                            << (inProgramScope ? "ProgramScope_Vert::" : "")
                            << dataType << (isPtrParam ? "* " : " ")
                            << name << attrib;
                }
                
                //MI wrapper code can't use "attrib" attribute specifier.
                vsMI_FuncDef << "\n    , " << (isPtrParam ? "device const " : "")
                             << (inProgramScope ? "ProgramScope_Vert::" : "")
                             << dataType << (isPtrParam ? "* " : " ")
                             << name;
                
                //Add as a parameter to the call to the vertex wrapper function
                vsMI_EP_CallCode    << ",\n            " << name;
            }
            else {
                //This input param is a vertex attribute
                
                std::stringstream arrayDecl;

                arrayDecl << "[" << input.arraySizeStr << "]";
                
                if (input.arraySize) {
                    vsInputStruct << "HD_MTL_VS_ATTRIBUTE_ARRAY(";
                }
                else {
                    vsInputStruct << "HD_MTL_VS_ATTRIBUTE(";
                }
                
                vsInputStruct << dataType << ", "
                              << name << ", [[attribute("
                              << vsCurrentVertexAttributeSlot << ")]], "
                              << arrayDecl.str() << ");\n";

                vsInputCode << "    scope." << name
                            << " = input." << name << ";\n";
                
                // MI Entry Point needs these as buffers instead of vertex
                // attributes. And to full the MSLVsInput struct we add them to
                // the InputCode as well.
                
                // We need to replace some of the dataTypes here to account
                // for changes in packing/alignment/unpacking
                if(usesPackedNormals)
                    dataType = _tokens->_int;
                else
                    dataType = _GetPackedMSLType(dataType);
                
                vsMI_EP_FuncDefParams << "\n    , device const " << dataType
                                      << " *" << name << "[[buffer("
                                      << vsCurrentVertexAttributeSlot << ")]]";
                vsMI_EP_InputCode << ",\n            "
                                  << (usesPackedNormals ?
                                      "hd_vec4_2_10_10_10_get(" : "")
                                  << name << "[gl_VertexID]"
                                  << (usesPackedNormals ? ")" : "");
                
                mslProgram->AddBinding(name, vsCurrentVertexAttributeSlot++,
                    input.binding, kMSL_BindingType_VertexAttribute,
                    kMSL_ProgramStage_Vertex);
            }
        }
    }
    vsInputStruct << "};\n\n";
    vsUniformStruct << "};\n\n";
    
    //Close function definition
    vsFuncDef       << ")\n{\n";
    vsMI_FuncDef    << ")\n{\n";
    vsMI_EP_FuncDef << vsMI_EP_FuncDefParams.str() << ")\n{\n";
    csFuncDef       << ")\n{\n";
    
    vsMI_EP_CallCode    << ");\n";
    vsMI_EP_InputCode   << "\n        };\n";
    
    //Round up size of uniform buffer to next 16 byte boundary.
    vsUniformStructSize = ((vsUniformStructSize + 15) / 16) * 16;
    if(hasVSUniformBuffer)
        mslProgram->AddBinding("vsUniforms", vsUniformsBufferSlot, HdBinding(),
            kMSL_BindingType_UniformBuffer, kMSL_ProgramStage_Vertex, 0,
            vsUniformStructSize);

    ///////////////////////// Setup Geometry Shader Attributes ////////////////////////////

    int numVerticesInPerPrimitive = -1;
    int numVerticesOutPerPrimitive = 3;
    int numPrimitivesOutPerPrimitive = 1;
    bool quadIndexRemap = false;

    if(_buildTarget == kMSL_BuildTarget_MVA || _buildTarget == kMSL_BuildTarget_MVA_ComputeGS) {
        numVerticesInPerPrimitive = 3;    //MTL_FIXME: Code belows isn't robust enough, need a better way to determine verts per primitive
        
        std::string result = _genGS.str();
        std::string::size_type inLayoutPos = result.find("layout(lines_adjacency) in;");
        if(inLayoutPos != std::string::npos) {
            numVerticesInPerPrimitive = 4;
            quadIndexRemap = true;
        }

        std::string::size_type outLayoutPos = result.find("layout(triangle_strip, max_vertices = 6) out;");
        if(outLayoutPos != std::string::npos)
        {
            numVerticesOutPerPrimitive = 6;
            numPrimitivesOutPerPrimitive = numVerticesOutPerPrimitive / 3;
        }
        
//        //Determine geometry type
//        for(auto key : _geometricShader->GetSourceKeys(HdShaderTokens->geometryShader)) {
//            if(key == "Mesh.Geometry.Triangle") { numVerticesPerPrimitive = 3; break; }
//        }
//        if(numVerticesPerPrimitive == -1)
//            TF_FATAL_ERROR("Unsupported Primitive Type encountered during Geometry Shader generation!");
    }

    ///////////////////////// Vertex Output ////////////////////////////
    
    if(_buildTarget == kMSL_BuildTarget_MVA_ComputeGS) {
        gsVertIntermediateStruct << "struct MSLGsVertIntermediateStruct {\n";
        gsVertIntermediateFlatStruct << "struct MSLGsVertIntermediateStruct_Flat {\n";
    }

    vsOutputStruct  << "struct MSLVsOutputs {\n";
    {
        if(_buildTarget != kMSL_BuildTarget_Regular) {
            vsOutputStruct  << "    uint gl_PrimitiveID[[flat]];\n"
                            << "    uint _gsPrimitiveID[[flat]];\n"
                            << "    vec2 _barycentricCoords[[center_perspective]];\n";
        }
        std::stringstream indexStr;
        indexStr << "_threadIndexInThreadgroup * " << numVerticesInPerPrimitive << " + i";

        TF_FOR_ALL(it, _mslVSOutputParams) {
            HdSt_CodeGenMSL::TParam const &output = *it;
            
            //Ignore these because they serve no purpose as output of the VS. Just a symptom of how Hydra was setup.
            if(IsIgnoredVSAttribute(output.name))
                continue;
            
            std::stringstream arrayDecl;

            arrayDecl << "[" << output.arraySizeStr << "]";

            bool hasDefineWrapper = !output.defineWrapperStr.empty();
            if (hasDefineWrapper) {
                vsOutputStruct << "#if defined("
                               << output.defineWrapperStr
                               << ")\n";
                vsOutputCode << "#if defined("
                             << output.defineWrapperStr
                             << ")\n";
            }
            if (output.arraySize) {
                vsOutputStruct << "#if !defined(HD_FRAGMENT_SHADER)\n"
                               << "    HD_MTL_VS_ATTRIBUTE_ARRAY(";
            }
            else {
                vsOutputStruct << "    HD_MTL_VS_ATTRIBUTE(";
            }

            vsOutputStruct  << output.dataType
                            << ", " << output.name
                            << ", " << (output.attribute.IsEmpty() ? "[[center_perspective]]" : output.attribute.GetString())
                            << ", " << arrayDecl.str()
                            << ");\n";
            
            if (output.arraySize) {
                vsOutputStruct << "#endif\n";
                vsOutputCode << "    for (int i = 0; i < " << output.arraySizeStr << "; i++)\n";
                vsOutputCode << "        vsOut." << output.name << "[i] = scope." << (output.accessorStr.IsEmpty() ? output.name : output.accessorStr) << "[i];\n";
            }
            else {
                vsOutputCode << "    vsOut." << output.name << " = scope." << (output.accessorStr.IsEmpty() ? output.name : output.accessorStr) << ";\n";
            }

            //Build additional intermediate struct for the GS later as an optimization.
            if(_buildTarget == kMSL_BuildTarget_MVA_ComputeGS) {
                bool isFlat = (output.attribute == "[[flat]]");
                std::stringstream& intermStructStream = (isFlat ? gsVertIntermediateFlatStruct : gsVertIntermediateStruct);
                std::stringstream& intermediateVSOutput = (isFlat ? gs_IntermediateVSOutput_Flat : gs_IntermediateVSOutput);
                std::string dataType = _GetPackedMSLType(output.dataType.GetString());
                
                if (hasDefineWrapper) {
                    intermStructStream << "#if defined("
                                       << output.defineWrapperStr
                                       << ")\n";
                    intermediateVSOutput << "#if defined("
                                         << output.defineWrapperStr
                                         << ")\n";
                }

                intermStructStream << "    " << dataType << " " << output.name;
                if (output.arraySize) {
                    intermStructStream << "[" << output.arraySize << "]";
                }
                intermStructStream << ";\n";

                if(isFlat)
                    intermediateVSOutput << "            vsData_Flat." << output.name << " = vsOutput." << output.name << ";\n";
                else {
                    if (output.arraySize) {
                        intermediateVSOutput << "        for (int j = 0; j < " << output.arraySizeStr << "; j++)\n";
                        intermediateVSOutput << "            vsData[" << indexStr.str() << "]." << output.name << "[j] = vsOutput." << output.name << "[j];\n";
                    }
                    else {
                        intermediateVSOutput << "        vsData[" << indexStr.str() << "]." << output.name << " = vsOutput." << output.name << ";\n";
                    }
                }
                
                if (hasDefineWrapper) {
                    intermStructStream << "#endif\n";
                    intermediateVSOutput << "#endif\n";
                }
            }
            
            if (hasDefineWrapper) {
                vsOutputStruct << "#endif\n";
                vsOutputCode << "#endif\n";
            }
        }
    }
    vsOutputStruct << "};\n\n";
    
    if(_buildTarget == kMSL_BuildTarget_MVA_ComputeGS) {
        gsVertIntermediateStruct << "};\n\n";
        gsVertIntermediateFlatStruct << "};\n\n";
    }
    
    //Update indiviual uniforms with the assigned uniform buffer slot.
    TF_FOR_ALL(it, _mslVSInputParams) {
        HdSt_CodeGenMSL::TParam const &input = *it;
        TfToken attrib;
        if (!(input.usage & HdSt_CodeGenMSL::TParam::Uniform))
            continue;
        std::string name = (input.name.GetText()[0] == '*') ? input.name.GetText() + 1 : input.name.GetText();
        mslProgram->UpdateUniformBinding(name, vsUniformsBufferSlot);
    }
    
    ///////////////////////////////// Compute Geometry Shader ///////////////////////
    
    std::stringstream   gsCode, gsFuncDef, cs_EP_FuncDef, gs_VSInputCode, gs_GSInputCode,
                        gs_GSCallCode, gs_VSCallCode, gs_GSVertEmitCode, gs_GSPrimEmitCode,
                        gsVertOutStruct, gsPrimOutStruct, gsEmitCode;

    if(_buildTarget == kMSL_BuildTarget_MVA_ComputeGS) {
        int                 gsVertOutStructSize(0), gsPrimOutStructSize(0);
        
        ////////////////////////////////// Geometry Input ////////////////////////////////

        cs_EP_FuncDef   << "kernel void computeEntryPoint(\n"
                        << "    uint _threadPositionInGrid[[thread_position_in_grid]]\n"
                        << "    , uint _threadIndexInThreadgroup[[thread_position_in_threadgroup]]\n"
                        << "    , uint _threadsInThreadgroup[[threads_per_threadgroup]]\n"
                        << "    , device ProgramScope_Geometry::MSLGsVertOutStruct* gsVertOutBuffer[[buffer(" << gsVertOutputSlot << ")]]\n"
                        << "    , device ProgramScope_Geometry::MSLGsPrimOutStruct* gsPrimOutBuffer[[buffer(" << gsPrimOutputSlot << ")]]";
        
        //Since we are calling the vertex function too we'll need all of these.
        cs_EP_FuncDef   << vsMI_EP_FuncDefParams.str();
        
        TF_FOR_ALL(it, _mslGSInputParams) {
            std::string name(it->name.GetString()), accessor(it->accessorStr.GetString()),
                        dataType(it->dataType.GetString()), attribute(it->attribute.GetString());
            
            bool isVPrimVar = (it->usage & TParam::Usage::VPrimVar);
            bool isFPrimVar = (it->usage & TParam::Usage::FPrimVar);
            bool isDrawingCoord = (it->usage & TParam::Usage::DrawingCoord);
            bool isVertexData = (it->usage & TParam::Usage::VertexData);
            
            bool prefixScope = (it->usage & TParam::Usage::ProgramScope);
            
            bool isPtr = false;
            if(name.at(0) == '*') { name = name.substr(1, name.length() - 1); isPtr = true; }
            bool isWritable = (it->usage & TParam::Usage::Writable);
            
            if(isVPrimVar || isDrawingCoord || isVertexData) {
                bool isFlat = attribute == "[[flat]]";
                gs_VSInputCode  << "            scope." << (accessor.empty() ? name : accessor);
                if(isFlat)
                    gs_VSInputCode << " = vsData_Flat." << name << ";\n";
                else
                    gs_VSInputCode << " = vsData[_threadIndexInThreadgroup * " << numVerticesInPerPrimitive << " + i]." << name << ";\n";
            } else {
                gs_GSInputCode << "        scope." << (accessor.empty() ? name : accessor) << " = ";
                
                if(prefixScope && isPtr)
                    gs_GSInputCode << "(const device ProgramScope_Geometry::" << dataType << "*)";
                else if (it->usage & HdSt_CodeGenMSL::TParam::Uniform)
                    gs_GSInputCode << "vsUniforms->";
                gs_GSInputCode << name << ";\n";
                
                //If this parameter is already present in the VS we shouldn't include it in our function definition as it will be a duplicate.
                bool isPresentInVS = false;
                TF_FOR_ALL(it_vs, _mslVSInputParams) {
                    std::string vs_name = it_vs->name.GetString();
                    if(vs_name.at(0) == '*') vs_name = vs_name.substr(1,vs_name.length()-1);
                    // If the name matches but for example type is different we
                    // have a problem. We're assuming this doesn't happen.
                    // Cumbersome to design around.
                    if(vs_name != name)
                        continue;
                    isPresentInVS = true;
                }
                if(!isPresentInVS) {
                    cs_EP_FuncDef   << "\n    , " << (isPtr ? (isWritable ? "device " : "device const ") : "")
                                    << (prefixScope ? "ProgramScope_Geometry::" : "")
                                    << _GetPackedMSLType(_GetPackedType(it->dataType, true))
                                    << (isPtr ? "* " : " ")
                                    << name << "[[buffer(" << currentUniformBufferSlot << ")]]";
                    mslProgram->AddBinding(name, currentUniformBufferSlot++,
                        it->binding, kMSL_BindingType_UniformBuffer,
                        kMSL_ProgramStage_Compute);
                }
            }
        }
        cs_EP_FuncDef << ")\n{\n";
        
        ////////////////////////////////// Geometry Output ///////////////////////////////
        
        gsVertOutStruct << "struct alignas(4) MSLGsVertOutStruct {\n";
        gsPrimOutStruct << "struct alignas(4) MSLGsPrimOutStruct {\n";
    
        std::stringstream vertBufferAccessor, primBufferAccessor, vsVertBufferAccessor, vsPrimBufferAccessor;
        vertBufferAccessor << "gsVertOutBuffer[gsOutputOffset * " << numVerticesOutPerPrimitive << " + gsVertexCounter].";
        primBufferAccessor << "gsPrimOutBuffer[gsOutputOffset * " <<
            numPrimitivesOutPerPrimitive << " + gsPrimCounter].";
        vsVertBufferAccessor << "gsVertOutBuffer[_gsVertexID].";
        vsPrimBufferAccessor << "gsPrimOutBuffer[_gsPrimitiveID].";
        TF_FOR_ALL(it, _mslGSOutputParams) {
            std::string name(it->name.GetString()), accessor(it->accessorStr.GetString()),
                        dataType(_GetPackedMSLType(it->dataType.GetString())), attribute(it->attribute.GetString());
        
            //Check whether the hints say this shouldn't be exported.
            bool requiresExport = (_gsIgnoredExports.count(name) + _gsIgnoredExports.count(accessor)) == 0;
            if(!requiresExport)
                continue;
        
            bool isPerPrim = (attribute == "[[flat]]");
            std::stringstream& structStream = (isPerPrim ? gsPrimOutStruct : gsVertOutStruct);
            std::stringstream& emitStream = (isPerPrim ? gs_GSPrimEmitCode : gs_GSVertEmitCode);
            
            structStream << "    " << dataType << " " << name << ";\n";
            
            emitStream  << "    " << (isPerPrim ? primBufferAccessor.str() : vertBufferAccessor.str())
                        << name << " = " << (accessor.empty() ? name : accessor) << ";\n";
            
            //Generate code for merging GS results into pass-through VS, only export those that have a matching VSOut member.
            if(!IsIgnoredVSAttribute(it->name)) {
                TF_FOR_ALL(itVS, _mslVSOutputParams) {
                    if(itVS->name != it->name)
                        continue;
                    //MTL_TODO: Make this optional per member, we want to read/export as little GS data as possible. If it's
                    //          not being touched in the GS, we shouldn't read it here as the VS will already have the re-
                    //          calculated results.
                    //NOTE: Accessing the GS output buffer needs to happen on the bare vertexID, not offset with anything or indexed.
                    vsGsOutputMergeCode << "    vsOutput." << name << " = " << (isPerPrim ? vsPrimBufferAccessor.str() : vsVertBufferAccessor.str()) << name << ";\n";
                    break;
                }
            }
            
            //MTL_FIXME: Find size of dataTypes by using existing Hd functionality.
            int& structSize = (isPerPrim ? gsPrimOutStructSize : gsVertOutStructSize);
            uint32_t memberSize(4), memberAlignment(4); //Alignment would be 16 if not using packed_vecs
            if(dataType.find("mat") != std::string::npos) TF_FATAL_CODING_ERROR("Not implemented!");
            else if(dataType.find("2") != std::string::npos) memberSize = 8;
            else if(dataType.find("3") != std::string::npos) memberSize = 12;
            else if(dataType.find("4") != std::string::npos) memberSize = 16;
            uint32_t regStart = structSize / memberAlignment;
            uint32_t regEnd = (structSize + memberSize - 1) / memberAlignment;
            if(regStart != regEnd && structSize % memberAlignment != 0)
                structSize += memberAlignment - (structSize % memberAlignment);
            structSize += memberSize;
        }
        
        gsVertOutStruct << "};\n\n";
        gsPrimOutStruct << "};\n\n";
        
        _mslGSPrimOutStructSize = gsPrimOutStructSize;
        _mslGSVertOutStructSize = gsVertOutStructSize;

        gsEmitCode  << "////////////////////////////////////////////////////////////////////////////////////////////////////////////////////\n"
                    << "// MSL GS Emit Code ////////////////////////////////////////////////////////////////////////////////////////////////\n\n"
                    << "device MSLGsVertOutStruct* gsVertOutBuffer;\n"
                    << "device MSLGsPrimOutStruct* gsPrimOutBuffer;\n"
                    << "\n"
                    << "uint gsVertexCounter = 0;\n"
                    << "uint gsOutputOffset = 0;\n"
                    << "void EmitVertex() {\n"
                    << gs_GSVertEmitCode.str()
                    << "    gsVertexCounter++;\n"
                    << "}\n"
                    << "\n"
                    << "uint gsPrimCounter = 0;\n"
                    << "void EndPrimitive() {\n"
                    << gs_GSPrimEmitCode.str()
                    << "    gsPrimCounter++;\n"
                    << "}\n"
                    << "\n"
                    << "}; //Close ProgramScope_Geometry\n\n";
    }
    
    ///////////////////////////////// VS Code Concatenation ////////////////////////////////////////
    
    //Start concatenating the generated code snippets into glueVS
    {
        bool useMI = (_buildTarget != kMSL_BuildTarget_Regular);
        
        vsCode  << drawArgsStruct.str()
                << vsUniformStruct.str();
        
        vsCode  << "////////////////////////////////////////////////////////////////////////////////////////////////////////////////////\n"
                << "// MSL Vertex Input Struct /////////////////////////////////////////////////////////////////////////////////////////\n\n"
                << (_buildTarget == kMSL_BuildTarget_Regular ? vsAttributeDefineEnabled.str() : vsAttributeDefineDisabled.str())
                << vsInputStruct.str() << vsAttributeDefineUndef.str();
        
        vsCode  << "////////////////////////////////////////////////////////////////////////////////////////////////////////////////////\n"
                << "// MSL Vertex Output Struct ////////////////////////////////////////////////////////////////////////////////////////\n\n"
                << vsAttributeDefineEnabled.str() << vsOutputStruct.str() << vsAttributeDefineUndef.str();
        
        vsCode      << "////////////////////////////////////////////////////////////////////////////////////////////////////////////////////\n";
        if(useMI)
            vsCode  << "// MSL Vertex Wrapper Function /////////////////////////////////////////////////////////////////////////////////////\n\n";
        else
            vsCode  << "// MSL Vertex Entry Point //////////////////////////////////////////////////////////////////////////////////////////\n\n";
        vsCode  << (useMI ? vsMI_FuncDef.str() : vsFuncDef.str())
                << "    ProgramScope_Vert scope;\n"
                << vsInputCode.str()
                << "\n"
                << "    scope.main();\n"
                << "\n"
                << "    MSLVsOutputs vsOut;\n"
                << vsOutputCode.str()
                << "    return vsOut;\n"
                << "}\n\n";
        
        //If we are building for anything else than the regular setup we create a separate entry point.
        if(useMI) {
            vsEntryPointCode    << "////////////////////////////////////////////////////////////////////////////////////////////////////////////////////\n"
                                << "// MSL Geometry Output Structs /////////////////////////////////////////////////////////////////////////////////////\n\n"
                                << gsVertOutStruct.str()
                                << gsPrimOutStruct.str()
                                << "////////////////////////////////////////////////////////////////////////////////////////////////////////////////////\n"
                                << "// MSL Vertex Entry Point //////////////////////////////////////////////////////////////////////////////////////////\n\n"
                                << vsMI_EP_FuncDef.str()
                                << "    uint _gsVertexID = _vertexID;\n";
            if (quadIndexRemap) {

                vsEntryPointCode << "    uint quadRemap[] = { 3, 0, 2, 2, 0, 1 };\n"
                                 << "    uint _index = drawArgs->batchIndexOffset + (_vertexID / 6) * 4 + quadRemap[_vertexID % 6];\n"
                                 << "    uint _primitiveID = (drawArgs->batchIndexOffset + (_vertexID / 6)) % drawArgs->primitiveCount;\n";
            }
            else {
                vsEntryPointCode << "    uint _index = drawArgs->batchIndexOffset + _gsVertexID;\n"
                                 << "    uint _primitiveID = (drawArgs->batchIndexOffset + (_vertexID / 3)) % drawArgs->primitiveCount;\n";
            }
            if (_buildTarget == kMSL_BuildTarget_MVA_ComputeGS) { //_instanceID is the real Metal instance ID if not using ComputeGS
                vsEntryPointCode << "    uint _instanceID = _index / drawArgs->indexCount;\n";
            }
            vsEntryPointCode    << "    uint _gsPrimitiveID = _gsVertexID / "
                                << (numVerticesOutPerPrimitive / numPrimitivesOutPerPrimitive) << ";\n"
                                << "    _index = _index % drawArgs->indexCount;\n"
                                << "    uint gl_InstanceID = _instanceID;\n"
                                << "    uint gl_BaseVertex = drawArgs->baseVertex;\n"
                                << "    uint gl_VertexID = indices[drawArgs->startIndex + _index] + gl_BaseVertex;\n"
                                << "    uint gl_PrimitiveIDIn = _primitiveID;\n"
                                << "\n"
                                << vsMI_EP_InputCode.str()
                                << "\n"
                                << "    //Full-screen passes need _vertexID for proper FS triangle to be generated.\n"
                                << "    //AFAIK gl_VertexID is not used further for other passes at the moment\n"
                                << "    gl_VertexID = _gsVertexID;\n"
                                << "\n"
                                << "    MSLVsOutputs vsOutput;\n"
                                << vsMI_EP_CallCode.str()
                                << "\n    vsOutput.gl_PrimitiveID = _primitiveID;\n"
                                << "    vsOutput._gsPrimitiveID = _gsPrimitiveID;\n"
                                << "    vsOutput._barycentricCoords = vec2((_vertexID % 3 == 1)? 1.0 : 0.0, (_vertexID % 3 == 2) ? 1.0 : 0.0);\n\n"
                                << vsGsOutputMergeCode.str()
                                << "\n"
                                << "    return vsOutput;\n"
                                << "}\n";
        }
    }
    
    ////////////////////////////////// GS Code Concatenation ////////////////////////////
    
    if(_buildTarget == kMSL_BuildTarget_MVA_ComputeGS) {
        //Placing these struct here means they reside *inside* of the ProgramScope_Geometry class. This is required because of usage in the Emit functions.
        gsCode  << "////////////////////////////////////////////////////////////////////////////////////////////////////////////////////\n"
                << "// MSL GS Output Structs ///////////////////////////////////////////////////////////////////////////////////////////\n\n"
                << gsVertOutStruct.str()
                << gsPrimOutStruct.str();
        
        gsCode  << gsEmitCode.str()     //This is where the ProgramScope_Geometry ends.
                << vsCode.str();        //Include vertex shader code into our gsCode, note that this does not include the VS Entry Point.
    
        gsCode  << "////////////////////////////////////////////////////////////////////////////////////////////////////////////////////\n"
                << "// MSL VS Intermediate Output Structs //////////////////////////////////////////////////////////////////////////////\n\n"
                << gsVertIntermediateStruct.str()
                << gsVertIntermediateFlatStruct.str();
    
        gsCode  << "////////////////////////////////////////////////////////////////////////////////////////////////////////////////////\n"
                << "// MSL Compute Entry Point /////////////////////////////////////////////////////////////////////////////////////////\n\n"
                << cs_EP_FuncDef.str()
                << "    uint _vertexID = drawArgs->batchIndexOffset + _threadPositionInGrid * " << numVerticesInPerPrimitive << ";\n"
                << "    uint _primitiveID = (drawArgs->batchPrimitiveOffset + _threadPositionInGrid) % drawArgs->primitiveCount;\n"
                << "    uint _instanceID = _vertexID / drawArgs->indexCount;\n"
                << "    _vertexID = _vertexID % drawArgs->indexCount;\n"
                << "    uint gl_BaseVertex = drawArgs->baseVertex;\n"
                << "    uint gl_InstanceID = _instanceID;\n"
                << "    uint gl_PrimitiveIDIn = _primitiveID;\n"
                << "\n"
                << "    if(gl_InstanceID >= drawArgs->instanceCount) return;\n"
                << "    \n"
                << "    //Vertex Shader\n"
                << "    threadgroup MSLGsVertIntermediateStruct vsData[" << numVerticesInPerPrimitive << " * " << METAL_GS_THREADGROUP_SIZE << "];\n"
                << "    MSLGsVertIntermediateStruct_Flat vsData_Flat;\n"
                << "    for(uint i = 0; i < " << numVerticesInPerPrimitive << "; i++) {\n"
                << "        uint gl_VertexID = gl_BaseVertex + indices[drawArgs->startIndex + _vertexID + i];\n"
                << "\n"
                << "    " << vsMI_EP_InputCode.str()
                << "\n"
                << "        MSLVsOutputs vsOutput;\n"
                << "    " << vsMI_EP_CallCode.str()
                << "\n"
                << gs_IntermediateVSOutput.str()
                << "\n\n"
                << "        if(i == 0) {\n"
                << gs_IntermediateVSOutput_Flat.str()
                << "        }\n"
                << "    }\n"
                << "\n"
                << "    //Geometry Shader\n"
                << "    {\n"
                << "        ProgramScope_Geometry scope;\n\n"
                << "        for(uint i = 0; i < " << numVerticesInPerPrimitive << "; i++){\n"
                << gs_VSInputCode.str()
                << "        }\n\n"
                << "        scope.gl_PrimitiveIDIn = gl_PrimitiveIDIn;\n"
                << "        scope.gl_InstanceID = gl_InstanceID;\n"
                << "        scope.gsVertOutBuffer = gsVertOutBuffer;\n"
                << "        scope.gsPrimOutBuffer = gsPrimOutBuffer;\n"
                << "        scope.gsOutputOffset = _threadPositionInGrid;\n"
                << "\n"
                << gs_GSInputCode.str()
                << "\n"
                << "        scope.CacheDrawingCoord();\n"
                << "        scope.main();\n"
                << "    }\n"
                << "}\n";
    }
    
    //Start concatenating the generated code snippets into glueCS
    {
        csCode  << "////////////////////////////////////////////////////////////////////////////////////////////////////////////////////\n";
        csCode  << "// MSL Compute Entry Point //////////////////////////////////////////////////////////////////////////////////////////\n\n";
        csCode  << csFuncDef.str()
                << "    ProgramScope_Compute scope;\n"
                << vsInputCode.str()
                << "\n"
                << "    scope.compute(_threadPositionInGrid);\n"
                << "\n"
                << "}\n\n";
    }
    
    ////////////////////////////////// Fragment Shader //////////////////////////////
    std::stringstream   fsCode, fsFuncDef, fsInputCode, fsOutputCode, fsOutputStruct, fsTexturingStruct, fsUniformStruct, fsInterpolationCode;
    int                 fsUniformStructSize(0);
    
    fsInterpolationCode << "////////////////////////////////////////////////////////////////////////////////////////////////////////////////////\n"
                        << "// MSL FS Interpolation Code ///////////////////////////////////////////////////////////////////////////////////////\n\n"
                        << "float Interpolate_CenterPerspective(float in1, float in2, float in3, vec2 bary) { return in1 * (1 - (bary.x + bary.y)) + in2 * bary.x + in3 * bary.y; }\n"
                        << "vec2 Interpolate_CenterPerspective(vec2 in1, vec2 in2, vec2 in3, vec2 bary) { return in1 * (1 - (bary.x + bary.y)) + in2 * bary.x + in3 * bary.y; }\n"
                        << "vec3 Interpolate_CenterPerspective(vec3 in1, vec3 in2, vec3 in3, vec2 bary) { return in1 * (1 - (bary.x + bary.y)) + in2 * bary.x + in3 * bary.y; }\n"
                        << "vec4 Interpolate_CenterPerspective(vec4 in1, vec4 in2, vec4 in3, vec2 bary) { return in1 * (1 - (bary.x + bary.y)) + in2 * bary.x + in3 * bary.y; }\n\n";
    
    if(_buildTarget == kMSL_BuildTarget_MVA_ComputeGS) {
        fsFuncDef   << "\n    , const device MSLGsVertOutStruct* gsVertOutBuffer[[buffer(" << gsVertOutputSlot << ")]]"
                    << "\n    , const device MSLGsPrimOutStruct* gsPrimOutBuffer[[buffer(" << gsPrimOutputSlot << ")]]";
    }
    
    ////////////////////////////////// Fragment Inputs //////////////////////////////
    
    fsTexturingStruct   << "////////////////////////////////////////////////////////////////////////////////////////////////////////////////////\n"
                        << "// MSL FS Texturing Struct /////////////////////////////////////////////////////////////////////////////////////////\n\n"
                        << "struct MSLFsTexturing {\n";
    
    fsUniformStruct << "////////////////////////////////////////////////////////////////////////////////////////////////////////////////////\n"
                    << "// MSL FS Uniform Struct ///////////////////////////////////////////////////////////////////////////////////////////\n\n"
                    << "struct MSLFsUniforms {\n";
    
    int fsCurrentSamplerSlot(0), fsCurrentTextureSlot(0);
    TF_FOR_ALL(it, _mslPSInputParams) {
        std::string name(it->name.GetString()), accessor(it->accessorStr.GetString()),
                    dataType(it->dataType.GetString());
        std::stringstream attribute;
        
        if (it->usage & HdSt_CodeGenMSL::TParam::VertexShaderOnly)
            continue;
        
        bool                isPtr(false), isScopeMember(true);
        std::stringstream   sourcePrefix, destPrefix, sourcePostfix;
        if((it->usage & HdSt_CodeGenMSL::TParam::maskShaderUsage) == HdSt_CodeGenMSL::TParam::Sampler) {
            //This parameter is a sampler
            
            fsTexturingStruct << "    " << dataType << " " << name << "[[sampler(" << fsCurrentSamplerSlot << ")]]" << ";\n";
            mslProgram->AddBinding(name, fsCurrentSamplerSlot, it->binding,
                kMSL_BindingType_Sampler, kMSL_ProgramStage_Fragment);
            sourcePrefix << "fsTexturing.";
            fsCurrentSamplerSlot++;
        }
        else if((it->usage & HdSt_CodeGenMSL::TParam::maskShaderUsage) == HdSt_CodeGenMSL::TParam::Texture) {
            //This parameter is a texture
            
            fsTexturingStruct << "    " << dataType << " " << name << "[[texture(" << fsCurrentTextureSlot << ")]]" << ";\n";
            mslProgram->AddBinding(name, fsCurrentTextureSlot, it->binding,
                kMSL_BindingType_Texture, kMSL_ProgramStage_Fragment);
            sourcePrefix << "fsTexturing.";
            fsCurrentTextureSlot++;
        }
        else if (it->usage & HdSt_CodeGenMSL::TParam::EntryFuncArgument) {
            //This parameter is either a uniform buffer or a built-in variable
            
            if (it->attribute.IsEmpty()) {
                //This parameter is a uniform buffer
                
                if(name.at(0) == '*') {
                    name = name.substr(1, name.length() - 1);
                    isPtr = true;
                }
                else
                    sourcePrefix << "*";
                
                //Uniform Blocks need a different name as a binding to stay matching with Hydra
                std::string bindingName = name;
                if(it->usage & HdSt_CodeGenMSL::TParam::UniformBlock) {
                    bindingName = dataType;
                    //MTL_FIXME: Should centralize this prefix and make it more descriptive e.g.: "_flatUB_" for a flat uniform block.
                    //           Should also deal with these blocks in a cleaner manner instead of checking for the prefix.
                    if(name.find("___") == 0) {
                        isScopeMember = false;
                    }
                }
                
                //We can't add our own uniform buffer as a binding yet because we don't know it's size
                int assignedSlot = currentUniformBufferSlot;
                if(bindingName == "fsUniforms") {
                    fsUniformsBufferSlot = assignedSlot;
                    isScopeMember = false;
                    currentUniformBufferSlot++;
                }
                else if(bindingName == "fragExtras") {
                    fragExtrasSlot = assignedSlot;
                    isScopeMember = false;
                    currentUniformBufferSlot++;
                    
                    mslProgram->AddBinding(bindingName, assignedSlot,
                        it->binding, kMSL_BindingType_FragExtras,
                        kMSL_ProgramStage_Fragment);
                }
                else {
                    //Attempt to find the same buffer in the VS inputs. If found, assign this buffer to the same slot.
                    TfToken bindingNameToken(bindingName);
                    MSL_ShaderBinding const * const binding =
                        MSL_FindBinding(mslProgram->GetBindingMap(),
                            bindingNameToken, kMSL_BindingType_UniformBuffer,
                            kMSL_ProgramStage_Vertex);
                    
                    if(binding)
                        assignedSlot = binding->_index;
                    else
                        currentUniformBufferSlot++;
                    mslProgram->AddBinding(bindingName, assignedSlot,
                        it->binding, kMSL_BindingType_UniformBuffer,
                        kMSL_ProgramStage_Fragment);
                }
                
                bool isAtomicType = it->dataType.GetString().find("atomic") != std::string::npos;
                bool isShaderWritable = (it->usage & HdSt_CodeGenMSL::TParam::Usage::Writable);
                
                fsFuncDef << "\n    , " << ((isAtomicType || isShaderWritable) ? "" : "const ") << "device "
                          << ((it->usage & TParam::Usage::ProgramScope) ?
                            "ProgramScope_Frag::" : "")
                          << _GetPackedType(it->dataType, true)
                          << "* " << name << "[[buffer("
                          << assignedSlot << ")]]";
            }
            //else
                // This parameter is a built-in variable and we won't add it to
                // the function definition as built-ins get added regardless in
                // a different way.
        }
        else
        {
            if(it->usage & HdSt_CodeGenMSL::TParam::UniformBlockMember) {
                //This parameter is a uniform block member
                
                //Strip the array brackets from the name if they exist
                std::string::size_type bracketPos = name.find_first_of("[");
                if(bracketPos != std::string::npos)
                    name = name.substr(0, bracketPos);
                
                //"name" contains the variable name while "accessor" contains the structure name in this case.
                sourcePrefix << accessor << "->";
                accessor = name;
            }
            else if(it->usage & HdSt_CodeGenMSL::TParam::Uniform) {
                //This parameter is a uniform
                
                sourcePrefix << "fsUniforms->";
                
                fsUniformStruct << "    " << dataType << " " << name << ";\n";

                //Alignment would be 16 if not using packed_vecs
                uint32_t memberSize(4), memberAlignment(4);
                if(dataType.find("mat") != std::string::npos) {
                    TF_FATAL_CODING_ERROR("Not implemented!");
                }
                else if(dataType.find("2") != std::string::npos) {
                    memberSize = 8;
                }
                else if(dataType.find("3") != std::string::npos) {
                    memberSize = 12;
                }
                else if(dataType.find("4") != std::string::npos) {
                    memberSize = 16;
                }
                uint32_t regStart = fsUniformStructSize / memberAlignment;
                uint32_t regEnd =
                    (fsUniformStructSize + memberSize - 1) / memberAlignment;
                if(regStart != regEnd &&
                   fsUniformStructSize % memberAlignment != 0) {
                    fsUniformStructSize += memberAlignment -
                    (fsUniformStructSize % memberAlignment);
                }
                // Add a binding for each uniform. They are currently all bound
                // to slot -1 which is "patched" a little further down, once
                // the actual slot is known (depends on other elements of
                // _mslVSInputParams which may not have been processed yet)
                mslProgram->AddBinding(name, -1, it->binding,
                    kMSL_BindingType_Uniform, kMSL_ProgramStage_Fragment,
                    fsUniformStructSize);
                fsUniformStructSize += memberSize;
            }
            else {
                //This parameter is a Vertex output member
                
                if(it->usage & HdSt_CodeGenMSL::TParam::VPrimVar) {
                    std::string cpy = name;
                    name = accessor;
                    accessor = cpy;
                    destPrefix << "inPrimvars.";
                }
                
                //Check the Geometry outputs for this parameter so that we may get those results instead of the VS output
                bool takenFromGS = false;
                if(_buildTarget == kMSL_BuildTarget_MVA_ComputeGS) {
                    for (auto gsOutput : _mslGSOutputParams) {
                        std::string gs_name = gsOutput.name.GetString();
                        std::string gs_accessor = gsOutput.accessorStr.GetString();
                        
                        if(gsOutput.usage & TParam::Usage::FPrimVar) gs_name = gs_name.substr(strlen(MTL_PRIMVAR_PREFIX), gs_name.length() - strlen(MTL_PRIMVAR_PREFIX));
                        else if(gs_name.at(0) == '*') gs_name = gs_name.substr(1, gs_name.length()-1);
                        
                        if(gs_name != name)
                            continue;
                        
                        //Check whether this exports was ignored
                        bool requiresExport = (_gsIgnoredExports.count(gs_name) + _gsIgnoredExports.count(gs_accessor)) == 0;
                        if(!requiresExport)
                            continue;
                        
                        if(gsOutput.attribute.GetString() == "[[flat]]")
                            sourcePrefix << "gsPrimOutBuffer[vsOutput._gsPrimitiveID].";
                        else {
                            //MTL_TODO: Investigate interpolating gsOutput manually in this cases, would remove this var from the vertexstruct which tends to be large for hydra.
                            std::string interpolation = "CenterPerspective";
                            sourcePrefix << "Interpolate_" << interpolation << "(gsVertOutBuffer[_provokingVertex + 0]." << gsOutput.name << ", "
                                                                            <<  "gsVertOutBuffer[_provokingVertex + 1]." << gsOutput.name << ", "
                                                                            <<  "gsVertOutBuffer[_provokingVertex + 2]." << gsOutput.name << ", _barycentricCoords)";
                            if(accessor.empty())
                                accessor = name;
                            name = "";
                        }
                        takenFromGS = true;
                        break;
                    }
                }
                if (!takenFromGS) {
                    bool takenFromVS = false;
                    for (auto vsOutput : _mslVSOutputParams) {
                        std::string vs_name = vsOutput.name.GetString();
                        
                        if(vs_name != name)
                            continue;
                        
                        takenFromVS = true;
                        break;
                    }
                    
                    if (takenFromVS) {
                        if(IsIgnoredVSAttribute(it->name))
                            sourcePrefix << "0; // ";
                        else
                            sourcePrefix << "vsOutput.";
                    }
                    else {
                        // Special case some stuff!
                        if (name == "u") {
                            fsInputCode << "    scope." << destPrefix.str() << (accessor.empty() ? name : accessor) << " = "
                                        << "_barycentricCoords.y;\n";
                            continue;
                        }
                        else if (name == "v") {
                            fsInputCode << "    scope." << destPrefix.str() << (accessor.empty() ? name : accessor) << " = "
                                        << "_barycentricCoords.x;\n";
                            continue;
                        }
                    }
                }
            }
        }
        
        if(isScopeMember) {
            fsInputCode << "    scope." << destPrefix.str() << (accessor.empty() ? name : accessor) << " = " << sourcePrefix.str() << name << ";\n";
        }
    }
    // How we emulate gl_FragCoord is different based on whether we're
    // using a pass-through vertex shader due to using compute for GS.
    // [[position]] is very different, and we we need to standardise it here
    // by manually doing the division by w to normalise
    if (_buildTarget != kMSL_BuildTarget_MVA_ComputeGS) {
        fsInputCode << "    scope.gl_FragCoord = scope.gl_Position;\n";
    } else {
        fsInputCode << "    scope.gl_FragCoord.zw = scope.gl_Position.zw;\n";
        fsInputCode << "    vec2 xy = scope.gl_Position.xy / scope.gl_Position.w;\n";
        //fsInputCode << "    xy.y *= -1.0;\n";
        fsInputCode << "    xy += 1.0;\n";
        fsInputCode << "    scope.gl_FragCoord.xy = xy * 0.5 * vec2(fragExtras->renderTargetWidth, fragExtras->renderTargetHeight);\n";
    }
    fsTexturingStruct << "};\n\n";
    fsUniformStruct << "};\n\n";
    fsFuncDef << ")\n{\n";
    
    //Round up size of uniform buffer to next 16 byte boundary.
    fsUniformStructSize = ((fsUniformStructSize + 15) / 16) * 16;
    if(hasFSUniformBuffer) {
        mslProgram->AddBinding("fsUniforms", fsUniformsBufferSlot, HdBinding(),
            kMSL_BindingType_UniformBuffer, kMSL_ProgramStage_Fragment, 0,
            fsUniformStructSize);
    }
    
    bool usesTexturingStruct = (fsCurrentSamplerSlot != 0 || fsCurrentTextureSlot != 0);
    
    fragExtrasStruct << "//////////////////////////////////////////////////////////////////////////\n"
                     << "// MSL Frag Extras Struct ////////////////////////////////////////////////\n\n"
                     << "struct MSLFragExtras { float renderTargetWidth, renderTargetHeight; };\n\n";

    ////////////////////////////////// Fragment Outputs ////////////////////////
    
    int fsCurrentOutputSlot = 0;
    fsOutputStruct  << "////////////////////////////////////////////////////////////////////////////////////////////////////////////////////\n"
                    << "// MSL FS Output Struct ////////////////////////////////////////////////////////////////////////////////////////////\n\n"
                    << "struct MSLFsOutputs {\n";
    TF_FOR_ALL(it, _mslPSOutputParams) {
        std::string name(it->name.GetString()), accessor(it->accessorStr.GetString()),
                    dataType(it->dataType.GetString()), attribute(it->attribute.GetString());
        fsOutputStruct << "    " << dataType << " " << name << "[[color(" << fsCurrentOutputSlot << ")]];\n";
        fsOutputCode << "    fsOutput." << name << " = scope." << (accessor.empty() ? name : accessor) << ";\n";
        fsCurrentOutputSlot++;
    }
    fsOutputStruct  << "};\n\n";
    
    ////////////////////////////////// FS Code Concatenation ///////////////////
    
    if(hasFSUniformBuffer)
        fsCode << fsUniformStruct.str();
    
    fsCode  << fragExtrasStruct.str()
            << (usesTexturingStruct ? fsTexturingStruct.str() : "");
    
    fsCode  << "////////////////////////////////////////////////////////////////////////////////////////////////////////////////////\n"
            << "// MSL Vertex Output Struct ////////////////////////////////////////////////////////////////////////////////////////\n\n"
            << vsAttributeDefineEnabled.str() << vsOutputStruct.str() << vsAttributeDefineUndef.str();
    
    if(_buildTarget == kMSL_BuildTarget_MVA_ComputeGS) {
        fsCode  << fsInterpolationCode.str()
                << "////////////////////////////////////////////////////////////////////////////////////////////////////////////////////\n"
                << "// MSL Geometry Output Structs /////////////////////////////////////////////////////////////////////////////////////\n\n"
                << gsVertOutStruct.str()
                << gsPrimOutStruct.str();
    }

    fsCode  << fsOutputStruct.str();
    
    fsCode  << "////////////////////////////////////////////////////////////////////////////////////////////////////////////////////\n"
            << "// MSL Fragment Entry Point ////////////////////////////////////////////////////////////////////////////////////////\n\n"
            << "fragment MSLFsOutputs fragmentEntryPoint("
            << "\n      bool gl_FrontFacing[[front_facing]]"
            << "\n    , MSLVsOutputs vsOutput[[stage_in]]"
            << (usesTexturingStruct ? "\n    , MSLFsTexturing fsTexturing" : "")
            << fsFuncDef.str();
    
    if(_buildTarget != kMSL_BuildTarget_Regular) {
        fsCode  << "    uint gl_PrimitiveID = vsOutput.gl_PrimitiveID;\n"
                << "    uint _provokingVertex = vsOutput._gsPrimitiveID * " << (numVerticesOutPerPrimitive / numPrimitivesOutPerPrimitive) << ";\n"
                << "    vec2 _barycentricCoords = vsOutput._barycentricCoords;\n";
    }
    
    fsCode  << "\n"
            << "    ProgramScope_Frag scope;\n"
            << "\n"
            << (_buildTarget != kMSL_BuildTarget_Regular ? "    scope.gl_PrimitiveID = gl_PrimitiveID;\n" : "")
            << fsInputCode.str()
            << "\n"
            << "    scope.CacheDrawingCoord();\n"
            << "    scope.main();\n"
            << "\n"
            << "    MSLFsOutputs fsOutput;\n"
            << "\n"
            << fsOutputCode.str()
            << "\n"
            << "    return fsOutput;\n"
            << "}\n";
    
    ////////////////////////////////// Write Out Shaders ////////////////////////////
    
    glueVS << vsCode.str() << (_buildTarget != kMSL_BuildTarget_Regular ? vsEntryPointCode.str() : "");
    glueGS << gsCode.str();
    gluePS << fsCode.str();
    glueCS << csCode.str();
    
    METAL_DEBUG_COMMENT(&glueVS, "End of _GenerateGlue(glueVS)\n\n"); //MTL_FIXME
    METAL_DEBUG_COMMENT(&gluePS, "End of _GenerateGlue(gluePS)\n\n"); //MTL_FIXME
    METAL_DEBUG_COMMENT(&glueCS, "End of _GenerateGlue(glueCS)\n\n"); //MTL_FIXME
}

HdStGLSLProgramSharedPtr
HdSt_CodeGenMSL::Compile(HdStResourceRegistry* const registry)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();
    
    // shader sources
    // geometric shader owns main()
    std::string vertexShader =
    _geometricShader->GetSource(HdShaderTokens->vertexShader);
//    std::string tessControlShader =
//    _geometricShader->GetSource(HdShaderTokens->tessControlShader);
//    std::string tessEvalShader =
//    _geometricShader->GetSource(HdShaderTokens->tessEvalShader);
    std::string geometryShader =
    _geometricShader->GetSource(HdShaderTokens->geometryShader);
    std::string fragmentShader =
    _geometricShader->GetSource(HdShaderTokens->fragmentShader);
    
//    bool hasTCS = (!tessControlShader.empty());
//    bool hasTES = (!tessEvalShader.empty());
    
    _hasVS  = (!vertexShader.empty());
    _hasGS  = (!geometryShader.empty());
    _hasFS  = (!fragmentShader.empty());

    // decide to build shaders that use a compute GS or not
    // MTL_TODO: We are using MVA (Manual Vertex Assembly) in all cases currently. This may not be what we want due to performance concerns.
    _buildTarget = (_hasGS ? kMSL_BuildTarget_MVA_ComputeGS : kMSL_BuildTarget_MVA);
    
    // create MSL program.
    HdStGLSLProgramMSLSharedPtr mslProgram(
        new HdStGLSLProgramMSL(HdTokens->drawingShader, registry));
    
    // initialize autogen source buckets
    _genDefinitions.str(""); _genOSDDefinitions.str(""); _genCommon.str("");
    _genVS.str(""); _genTCS.str(""); _genTES.str(""); _genGS.str("");
    _genFS.str(""); _genCS.str(""); _procVS.str(""); _procTCS.str("");
    _procTES.str(""); _procGS.str("");

    // Metal conversion defines

    GarchContextCaps const &caps = GarchResourceFactory::GetInstance()->GetContextCaps();
    
    METAL_DEBUG_COMMENT(&_genDefinitions, "Compile()\n"); //MTL_FIXME
    
    _GenerateCommonDefinitions();

    // Start of Program Scope

    _GenerateCommonCode();
    
    _GenerateBindingsCode();
    
    // include Mtlf ptex utility (if needed)
    TF_FOR_ALL (it, _metaData.shaderParameterBinding) {
        HdBinding::Type bindingType = it->first.GetType();
        if (bindingType == HdBinding::TEXTURE_PTEX_TEXEL ||
            bindingType == HdBinding::BINDLESS_TEXTURE_PTEX_TEXEL) {
            _genCommon << _GetPtexTextureShaderSource();
            break;
        }
    }
    
    TF_FOR_ALL (it, _metaData.topologyVisibilityData) {
        TF_FOR_ALL (pIt, it->second.entries) {
            _genCommon << "#define HD_HAS_" << pIt->name  << " 1\n";
        }
    }
    
    // prep interstage plumbing function
    _procVS  << "void ProcessPrimvars() {\n";
//    _procTCS << "void ProcessPrimvars() {\n";
//    _procTES << "void ProcessPrimvars(vec4 basis, int i0, int i1, int i2, int i3) {\n";
    
    // geometry shader plumbing
    switch(_geometricShader->GetPrimitiveType())
    {
        case HdSt_GeometricShader::PrimitiveType::PRIM_MESH_REFINED_QUADS:
        case HdSt_GeometricShader::PrimitiveType::PRIM_MESH_REFINED_TRIANGLES:
        case HdSt_GeometricShader::PrimitiveType::PRIM_MESH_BSPLINE:
        case HdSt_GeometricShader::PrimitiveType::PRIM_MESH_BOXSPLINETRIANGLE:
        {
            // patch interpolation
            _procGS //<< "vec4 GetPatchCoord(int index);\n"
            << "void ProcessPrimvars(int index) {\n"
            << "   vec2 localST = GetPatchCoord(index).xy;\n";
            break;
        }

        case HdSt_GeometricShader::PrimitiveType::PRIM_MESH_COARSE_QUADS:
        {
            // quad interpolation
            _procGS  << "void ProcessPrimvars(int index) {\n"
                     << "   vec2 lut[] = { vec2(0,0), vec2(1,0), vec2(1,1), vec2(0,1) };\n"
                     << "   vec2 localST = lut[index];\n";
            break;
        }

        case HdSt_GeometricShader::PrimitiveType::PRIM_MESH_COARSE_TRIANGLES:
        {
            // barycentric interpolation
            _procGS  << "void ProcessPrimvars(int index) {\n"
                     << "   vec2 lut[] = { vec2(0,0), vec2(1,0), vec2(0,1) };\n"
                     << "   vec2 localST = lut[index];\n";
            break;
        }

        default: // points, basis curves
            // do nothing. no additional code needs to be generated.
            ;
    }
    
    // generate drawing coord and accessors
    _GenerateDrawingCoord();

    // mixin shaders
    _genCommon << _geometricShader->GetSource(HdShaderTokens->commonShaderSource);
    TF_FOR_ALL(it, _shaders) {
        _genCommon << (*it)->GetSource(HdShaderTokens->commonShaderSource);
    }
    
    // generate primvars
    _GenerateConstantPrimvar();
    _GenerateInstancePrimvar();
    _GenerateElementPrimvar();
    _GenerateVertexAndFaceVaryingPrimvar(_hasGS);
    
    _GenerateTopologyVisibilityParameters();
    
    //generate shader parameters (is going last since it has primvar redirects)
    _GenerateShaderParameters();
        
    // finalize buckets
    _procVS  << "}\n";
    _procGS  << "}\n";
//    _procTCS << "}\n";
//    _procTES << "}\n";
    
    // insert interstage primvar plumbing procs into genVS/TCS/TES/GS
    _genVS  << _procVS.str();
//    _genTCS << _procTCS.str();
//    _genTES << _procTES.str();
    _genGS  << _procGS.str();
    
    // insert fixes for unsupported functionality
    
//    if(enableFragmentNormalReconstruction) {
//        _genFS << "#define __RECONSTRUCT_FLAT_NORMAL 1\n";  //See meshNormal.glslfx
//    }
    
    // other shaders (renderpass, lighting, surface) first
    TF_FOR_ALL(it, _shaders) {
        HdStShaderCodeSharedPtr const &shader = *it;
        if (_hasVS)
            _genVS  << shader->GetSource(HdShaderTokens->vertexShader);
        if (_hasGS)
            _genGS  << shader->GetSource(HdShaderTokens->geometryShader);
        if (_hasFS)
            _genFS  << shader->GetSource(HdShaderTokens->fragmentShader);
//        if (hasTCS)
//            _genTCS << shader->GetSource(HdShaderTokens->tessControlShader);
//        if (hasTES)
//            _genTES << shader->GetSource(HdShaderTokens->tessEvalShader);
    }
    
    // OpenSubdiv tessellation shader (if required)
    const bool allowOsd = true;
    if(allowOsd) {
        if (geometryShader.find("OsdInterpolatePatchCoord") != std::string::npos) {
            std::string osdCode = OpenSubdiv::Osd::MTLPatchShaderSource::GetCommonShaderSource();
            _genOSDDefinitions  << "#define CONTROL_INDICES_BUFFER_INDEX <cibi>\n"
                                << "#define OSD_PATCHPARAM_BUFFER_INDEX <osd_ppbi>\n"
                                << "#define OSD_PERPATCHVERTEX_BUFFER_INDEX <osd_ppvbbi>\n"
                                << "#define OSD_PERPATCHTESSFACTORS_BUFFER_INDEX <osd_pptfbi>\n"
                                << "#define OSD_KERNELLIMIT_BUFFER_INDEX <osd_klbi>\n"
                                << "#define OSD_PATCHPARAM_BUFFER_INDEX <osd_ppbi>\n"
                                << "#define VERTEX_BUFFER_INDEX <vbi>\n"
                                << "#define OSD_MAX_VALENCE 4\n"       //There is a mistake in Osd where the ifndef for this is AFTER first usage!
                                << "\n"
                                << "struct OsdInputVertexType {\n"
                                << "    vec3 position;\n"
                                << "};\n"
                                << "\n"
                                << osdCode;
        }
        if (fragmentShader.find("vec4 GetPatchCoord(int ") == std::string::npos) {
            _genFS << "vec4 GetPatchCoord(int localIndex) { return vec4(1); }\n";
        }
    }
    
    // geometric shader
    _genVS  << vertexShader;
//    _genTCS << tessControlShader;
//    _genTES << tessEvalShader;
    _genGS  << geometryShader;
    _genFS  << fragmentShader;
    
//    // Sanity check that if you provide a control shader, you have also provided
//    // an evaluation shader (and vice versa)
//    if (hasTCS ^ hasTES) {
//        TF_CODING_ERROR(
//                        "tessControlShader and tessEvalShader must be provided together.");
//        hasTCS = hasTES = false;
//    };
    
    std::stringstream termination;
    termination << "}; // ProgramScope<st>\n";
    
    // Externally sourced glslfx translation to MSL
    bool _mslBuildComputeGS = _buildTarget == kMSL_BuildTarget_MVA_ComputeGS;
    if(_mslBuildComputeGS) {
        _ParseGLSL(_genOSDDefinitions, _mslGSInputParams, _mslGSOutputParams, true);
        _ParseGLSL(_genGS, _mslGSInputParams, _mslGSOutputParams, true);
        
        std::stringstream temp;
        temp << _genCommon.str();
        _ParseGLSL(temp, _mslGSInputParams, _mslGSOutputParams, true);
        temp << _genGS.str();
        _ParseHints(temp);
    }

    _ParseGLSL(_genVS, _mslVSInputParams, _mslVSOutputParams);
    _ParseGLSL(_genFS, _mslPSInputParams, _mslPSOutputParams);
    _ParseGLSL(_genCommon, _mslVSInputParams, _mslVSOutputParams);
    
    {
        // TEMP: Metal compiler doesn't like missing function definitions even in
        // code that isn't called, so we hack a fix in here until the Storm shader
        // source/logic is correctly fixed at some future point
        std::string result = _genFS.str();
        if (result.find("\nintegrateLights(", 0) == std::string::npos &&
            result.find("//integrateLights(", 0) != std::string::npos) {
                _genFS << "LightingContribution\n"
                          "integrateLights(vec4 Peye, vec3 Neye, LightingInterfaceProperties props){\n"
                          "  return integrateLightsDefault(Peye, Neye, props);\n"
                          "}\n";
        }
    }

    // MSL<->Metal API plumbing
    std::stringstream glueVS, gluePS, glueGS, glueCS;
    glueVS.str(""); gluePS.str(""); glueGS.str(""); glueCS.str("");
    
    _GenerateGlue(glueVS, glueGS, gluePS, glueCS, mslProgram);

    std::stringstream vsConfigString, fsConfigString, gsConfigString;
    _GenerateConfigComments(vsConfigString, fsConfigString, gsConfigString);

    bool shaderCompiled = true;
    // compile shaders
    // note: _vsSource, _fsSource etc are used for diagnostics (see header)
    
    mslProgram->SetBuildTarget(_buildTarget);
    
    if (_hasVS) {
        _vsSource = vsConfigString.str() + _genDefinitions.str() +
                "#define HD_VERTEX_SHADER\n" + _genCommon.str() +
                _genVS.str() + termination.str() + glueVS.str();
        _vsSource = replaceStringAll(_vsSource, "<st>", "_Vert");
        
        if (!mslProgram->CompileShader(HgiShaderStageVertex, _vsSource)) {
            shaderCompiled = false;
        }
    }
    if (_buildTarget == kMSL_BuildTarget_MVA_ComputeGS) {
        _gsSource = vsConfigString.str() + gsConfigString.str() +
                _genDefinitions.str() + "#define HD_GEOMETRY_SHADER\n" +
                _genOSDDefinitions.str() + _genCommon.str() + _genVS.str() +
                termination.str();
        _gsSource = replaceStringAll(_gsSource, "<st>", "_Vert");
        _gsSource += _genCommon.str() + _genGS.str() + glueGS.str();    //Termination of Geometry ProgramScope is done in glueCS due to addition of EmitVertex/Primitive
        _gsSource = replaceStringAll(_gsSource, "<st>", "_Geometry");
        
        //MTL_FIXME: These need to point to actual buffers if Osd is actively used.
        _gsSource = replaceStringAll(_gsSource, "<cibi>", "0");
        _gsSource = replaceStringAll(_gsSource, "<osd_ppbi>", "0");
        _gsSource = replaceStringAll(_gsSource, "<osd_ppvbbi>", "0");
        _gsSource = replaceStringAll(_gsSource, "<osd_pptfbi>", "0");
        _gsSource = replaceStringAll(_gsSource, "<osd_klbi>", "0");
        _gsSource = replaceStringAll(_gsSource, "<osd_ppbi>", "0");
        _gsSource = replaceStringAll(_gsSource, "<vbi>", "0");
        
        if (!mslProgram->CompileShader(HgiShaderStageGeometry, _gsSource))
            shaderCompiled = false;
        
        mslProgram->SetGSOutStructsSize(_mslGSVertOutStructSize, _mslGSPrimOutStructSize);
    }
    if (_hasFS) {
        _fsSource = fsConfigString.str() + _genDefinitions.str() +
                "#define HD_FRAGMENT_SHADER\n" + _genCommon.str() +
                _genFS.str() + termination.str() + gluePS.str();
        _fsSource = replaceStringAll(_fsSource, "<st>", "_Frag");

        if (!mslProgram->CompileShader(HgiShaderStageFragment, _fsSource)) {
            shaderCompiled = false;
        }
    }
//    if (hasTCS) {
//        _tcsSource = _genCommon.str() + _genTCS.str() + termination.str();
//        if (!mslProgram->CompileShader(HgiShaderStageTesselationControl, _tcsSource)) {
//            shaderCompiled = false;
//        }
//    }
//    if (hasTES) {
//        _tesSource = _genCommon.str() + _genTES.str() + termination.str();
//        if (!mslProgram->CompileShader(HgiShaderStageTesselationEval, _tesSource)) {
//            shaderCompiled = false;
//        }
//    }
//    if (hasGS) {
//        _gsSource = gsConfigString.str() + _genCommon.str() + _genGS.str() + termination.str();
//        if (!mslProgram->CompileShader(HgiShaderStageGeometry, _gsSource)) { //REMOVE ME
//            shaderCompiled = false;
//        }
//    }
    
    if (!shaderCompiled) {
        return HdStGLSLProgramSharedPtr();
    }
    
    return mslProgram;
}

std::string
HdSt_CodeGenMSL::GetComputeHeader()
{
    std::stringstream header;
    header.str("");

    GarchContextCaps const &caps = GarchResourceFactory::GetInstance()->GetContextCaps();
        header  << "#define HD_SHADER_API " << HD_SHADER_API << "\n"
                << "#define ARCH_GFX_METAL\n"
                << "#define METAL_API_VERSION " << caps.apiVersion + 1 << "\n";
    
    // Metal feature set defines
    id<MTLDevice> device = MtlfMetalContext::GetMetalContext()->currentDevice;
#if defined(ARCH_OS_MACOS)
    header  << "#define ARCH_OS_MACOS\n";
    // Define all macOS 10.13 feature set enums onwards
    if ([device supportsFeatureSet:MTLFeatureSet_macOS_GPUFamily1_v3])
        header << "#define METAL_FEATURESET_MACOS_GPUFAMILY1_v3\n";
    if ([device supportsFeatureSet:MTLFeatureSet_macOS_GPUFamily1_v4])
        header << "#define METAL_FEATURESET_MACOS_GPUFAMILY1_v4\n";
    if ([device supportsFeatureSet:MTLFeatureSet_macOS_GPUFamily2_v1])
        header << "#define METAL_FEATURESET_MACOS_GPUFAMILY2_v1\n";
    
#else // ARCH_OS_MACOS
    header  << "#define ARCH_OS_IOS\n";
    // Define all iOS 12 feature set enums onwards
    if ([device supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily1_v5])
        header << "#define METAL_FEATURESET_IOS_GPUFAMILY1_v5\n";
    if ([device supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily2_v5])
        header << "#define METAL_FEATURESET_IOS_GPUFAMILY2_v5\n";
    if ([device supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily3_v4])
        header << "#define METAL_FEATURESET_IOS_GPUFAMILY3_v4\n";
    if ([device supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily4_v2])
        header << "#define METAL_FEATURESET_IOS_GPUFAMILY4_v2\n";
#endif // ARCH_OS_MACOS
    
    header  << "#include <metal_stdlib>\n"
            << "#include <simd/simd.h>\n"
            << "#include <metal_pack>\n"
            << "using namespace metal;\n";
    
    header  << "#define double float\n"
            << "#define vec2 float2\n"
            << "#define vec3 float3\n"
            << "#define vec4 float4\n"
            << "#define mat2 float2x2\n"
            << "#define mat3 float3x3\n"
            << "#define mat4 float4x4\n"
            << "#define ivec2 int2\n"
            << "#define ivec3 int3\n"
            << "#define ivec4 int4\n"
            << "#define bvec2 bool2\n"
            << "#define bvec3 bool3\n"
            << "#define bvec4 bool4\n"
            << "#define dvec2 float2\n"
            << "#define dvec3 float3\n"
            << "#define dvec4 float4\n"
            << "#define dmat2 float2x2\n"
            << "#define dmat3 float3x3\n"
            << "#define dmat4 float4x4\n";
    
    // XXX: this macro is still used in GlobalUniform.
    header  << "#define MAT4 mat4\n";

    // a trick to tightly pack vec3 into SSBO/UBO.
    header  << _GetPackedTypeDefinitions();
    
    header  << "#define in /*in*/\n"
            //<< "#define out /*out*/\n"  //This define is going to cause issues, do not enable.
            << "#define discard discard_fragment();\n"
            << "#define radians(d) (d * 0.01745329252)\n"
            << "#define noperspective /*center_no_perspective MTL_FIXME*/\n"
            << "#define dFdx    dfdx\n"
            << "#define dFdy    dfdy\n"
    
            << "#define lessThan(a, b) ((a) < (b))\n"
            << "#define lessThanEqual(a, b) ((a) <= (b))\n"
            << "#define greaterThan(a, b) ((a) > (b))\n"
            << "#define greaterThanEqual(a, b) ((a) >= (b))\n"
            << "#define equal(a, b) ((a) == (b))\n"
            << "#define notEqual(a, b) ((a) != (b))\n"

            << "template <typename T>\n"
            << "T mod(T y, T x) { return fmod(y, x); }\n\n"
            << "template <typename T>\n"
            << "T atan(T y, T x) { return atan2(y, x); }\n\n"
            << "template <typename T>\n"
            << "T bitfieldReverse(T x) { return reverse_bits(x); }\n\n"
            << "template <typename T>\n"
            << "ivec2 imageSize(T texture) {\n"
            << "    return ivec2(texture.get_width(), texture.get_height());\n"
            << "}\n\n"

            << "template <typename T>\n"
            << "ivec2 textureSize(T texture, int lod) {\n"
            << "    return ivec2(texture.get_width(lod), texture.get_height(lod));\n"
            << "}\n\n"
    
            << "#define texelFetch(sampler, coords, lod) sampler.read(uint2(coords.x, coords.y))\n"

            << "constexpr sampler texelSampler(address::clamp_to_edge,\n"
            << "                               filter::linear);\n";
        
    
    // wrapper for type float and int to deal with .x accessors and the like that are valid in GLSL
    header  << "struct wrapped_float {\n"
            << "    union {\n"
            << "        float x, xx, xxx, xxxx, y, z, w;\n"
            << "        float r, rr, rrr, rrrr, g, b, a;\n"
            << "    };\n"
            << "    wrapped_float(float _x) { x = _x;}\n"
            << "    wrapped_float(const thread wrapped_float &_x) { x = _x.x;}\n"
            << "    wrapped_float(const device wrapped_float &_x) { x = _x.x;}\n"
            << "    operator float () const {\n"
            << "        return x;\n"
            << "    }\n"
            << "};\n";
    
    header  << "struct wrapped_int {\n"
            << "    union {\n"
            << "        int x, xx, xxx, xxxx, y, z, w;\n"
            << "        int r, rr, rrr, rrrr, g, b, a;\n"
            << "    };\n"
            << "    wrapped_int(int _x) { x = _x;}\n"
            << "    wrapped_int(const thread wrapped_int &_x) { x = _x.x;}\n"
            << "    wrapped_int(const device wrapped_int &_x) { x = _x.x;}\n"
            << "    operator int () const {\n"
            << "        return x;\n"
            << "    }\n"
            << "};\n";

    return header.str();
}
                
HdStGLSLProgramSharedPtr
HdSt_CodeGenMSL::CompileComputeProgram(HdStResourceRegistry* const registry)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    // initialize autogen source buckets
    _genCommon.str(""); _genVS.str(""); _genTCS.str(""); _genTES.str("");
    _genGS.str(""); _genFS.str(""); _genCS.str("");
    _procVS.str(""); _procTCS.str(""), _procTES.str(""), _procGS.str("");
    
    _genCommon  << GetComputeHeader();
    
    _buildTarget = kMSL_BuildTarget_Regular;

    std::stringstream uniforms;
    std::stringstream declarations;
    std::stringstream accessors;
    
    _genCommon  << "#define VTXCONST\n"
                << "class ProgramScope<st> {\n"
                << "public:\n";

    uniforms << "// Uniform block\n";

    HdBinding uboBinding(HdBinding::UBO, 0);
    TfToken varName(TfStringPrintf("ubo_%d", uboBinding.GetLocation()));
    TfToken typeName(TfStringPrintf("ubo_%d_t", uboBinding.GetLocation()));
    uniforms << "struct " << typeName << " {\n";
    
    _EmitDeclarationPtr(
        declarations, varName, typeName, TfToken(), HdBinding(), 0, true);
    _AddInputPtrParam(
        _mslVSInputParams, varName, typeName, TfToken(), HdBinding(), 0, true);
    
//    declarations << "device const " << typeName << " *" << varName << ";\n";

    accessors << "// Read-Write Accessors & Mutators\n";
    uniforms << "    int vertexOffset;       // offset in aggregated buffer\n";
    TF_FOR_ALL(it, _metaData.computeReadWriteData) {
        TfToken const &name = it->second.name;
        HdBinding const &binding = it->first;
        TfToken const &dataType = it->second.dataType;
        
        uniforms << "    int " << name << "Offset;\n";
        uniforms << "    int " << name << "Stride;\n";
        
        _EmitDeclarationMutablePtr(declarations,
                            name, _GetFlatType(dataType), TfToken(), //compute shaders need vector types to be flat arrays
                            binding);
        _AddInputPtrParam(_mslVSInputParams,
                          name, _GetFlatType(dataType), TfToken(),
                          binding).usage |= TParam::EntryFuncArgument | TParam::Mutable;
        
        // getter & setter
        {
            std::stringstream indexing;
            indexing << "(localIndex + " << varName << "->" << "vertexOffset)"
                     << " * " << varName << "->"  << name << "Stride"
                     << " + " << varName << "->"  << name << "Offset";
            _EmitComputeAccessor(accessors, varName, name, dataType, binding,
                    indexing.str().c_str());
            _EmitComputeMutator(accessors, varName, name, dataType, binding,
                    indexing.str().c_str());
        }
    }
    accessors << "// Read-Only Accessors\n";
    // no vertex offset for constant data
    TF_FOR_ALL(it, _metaData.computeReadOnlyData) {
        TfToken const &name = it->second.name;
        HdBinding const &binding = it->first;
        TfToken const &dataType = it->second.dataType;
        
        uniforms << "    int " << name << "Offset;\n";
        uniforms << "    int " << name << "Stride;\n";
        _EmitDeclarationPtr(declarations,
                            name, _GetFlatType(dataType), TfToken(),
                            binding);
        _AddInputPtrParam(_mslVSInputParams,
                          name, _GetFlatType(dataType), TfToken(),
                          binding).usage |= TParam::EntryFuncArgument;
        // getter
        {
            std::stringstream indexing;
            // no vertex offset for constant data
            indexing << "(localIndex)"
                     << " * " << varName << "->"  << name << "Stride"
                     << " + " << varName << "->"  << name << "Offset";
            _EmitComputeAccessor(accessors, varName, name, dataType, binding,
                    indexing.str().c_str());
        }
    }
    uniforms << "};\n";
    
    _genCommon << uniforms.str()
               << declarations.str()
               << accessors.str();
    
    // other shaders (renderpass, lighting, surface) first
    TF_FOR_ALL(it, _shaders) {
        HdStShaderCodeSharedPtr const &shader = *it;
        _genCS  << shader->GetSource(HdShaderTokens->computeShader);
    }

    _genCS << "};\n\n";
    
    // MSL<->Metal API plumbing
    std::stringstream glueVS, gluePS, glueGS, glueCS;
    glueVS.str(""); gluePS.str(""); glueGS.str(""); glueCS.str("");
    
    // create Metal function.
    HdStGLSLProgramMSLSharedPtr program(
        new HdStGLSLProgramMSL(HdTokens->drawingShader, registry));

    _GenerateGlue(glueVS, glueGS, gluePS, glueCS, program);
    
    // compile shaders
    {
        _csSource = _genCommon.str() + _genCS.str() + glueCS.str();
        _csSource = replaceStringAll(_csSource, "<st>", "_Compute");

        if (!program->CompileShader(HgiShaderStageCompute, _csSource)) {
            return HdStGLSLProgramSharedPtr();
        }
    }

    return program;
}

static std::string _GetSwizzleString(TfToken const& type,
                                     std::string const& swizzle=std::string())
{
    if (!swizzle.empty()) {
        return "." + swizzle;
    }
    if (type == _tokens->vec4 || type == _tokens->ivec4) {
        return "";
    }
    if (type == _tokens->vec3 || type == _tokens->ivec3) {
        return ".xyz";
    }
    if (type == _tokens->vec2 || type == _tokens->ivec2) {
        return ".xy";
    }
    if (type == _tokens->_float || type == _tokens->_int) {
        return ".x";
    }
    if (type == _tokens->packed_2_10_10_10) {
        return ".x";
    }

    return "";
}

static void _EmitStructAccessor(std::stringstream &str,
                                TfToken const &structMemberName,
                                TfToken const &name,
                                TfToken const &type,
                                int arraySize,
                                bool pointerDereference,
                                const char *index = NULL)
{
    METAL_DEBUG_COMMENT(&str, "_EmitStructAccessor\n"); //MTL_FIXME
    // index != NULL  if the struct is an array
    // arraySize > 1  if the struct entry is an array.
    char const* ptrAccessor;
    if (pointerDereference) {
        ptrAccessor = "->";
    }
    else {
        ptrAccessor = ".";
    }

    if (index) {
        if (arraySize > 1) {
            str << _GetUnpackedType(type, false) << " HdGet_" << name
                << "(int arrayIndex, int localIndex) {\n"
                << "  return "
                << _GetUnpackedType(_GetPackedTypeAccessor(type, false), false) << "("
                << structMemberName << "[" << index << "]." << name << "[arrayIndex]);\n}\n";
        } else {
            str << _GetUnpackedType(type, false) << " HdGet_" << name
                << "(int localIndex) {\n"
                << "  return "
                << _GetUnpackedType(_GetPackedTypeAccessor(type, false), false) << "("
                << structMemberName << "[" << index << "]." << name << ");\n}\n";
        }
    } else {
        if (arraySize > 1) {
            str << _GetUnpackedType(type, false) << " HdGet_" << name
                << "(int arrayIndex, int localIndex) { return "
                << _GetUnpackedType(_GetPackedTypeAccessor(type, false), false) << "("
                << structMemberName << ptrAccessor << name << "[arrayIndex]);}\n";
        } else {
            str << _GetUnpackedType(type, false) << " HdGet_" << name
                << "(int localIndex) { return "
                << _GetUnpackedType(_GetPackedTypeAccessor(type, false), false) << "("
                << structMemberName << ptrAccessor << name << ");}\n";
        }
    }
    // GLSL spec doesn't allow default parameter. use function overload instead.
    // default to localIndex=0
    if (arraySize > 1) {
        str << _GetUnpackedType(type, false) << " HdGet_" << name
            << "(" << "int arrayIndex)"
            << " { return HdGet_" << name << "(arrayIndex, 0); }\n";
    } else {
        str << _GetUnpackedType(type, false) << " HdGet_" << name
            << "()" << " { return HdGet_" << name << "(0); }\n";
    }
}

static int _GetNumComponents(TfToken const& type)
{
    int numComponents = 1;
    if (type == _tokens->vec2 || type == _tokens->ivec2) {
        numComponents = 2;
    } else if (type == _tokens->vec3 || type == _tokens->ivec3) {
        numComponents = 3;
    } else if (type == _tokens->vec4 || type == _tokens->ivec4) {
        numComponents = 4;
    } else if (type == _tokens->mat3 || type == _tokens->dmat3) {
        numComponents = 9;
    } else if (type == _tokens->mat4 || type == _tokens->dmat4) {
        numComponents = 16;
    }
    
    return numComponents;
}

static void _EmitComputeAccessor(
                    std::stringstream &str,
                    TfToken const &structMemberName,
                    TfToken const &name,
                    TfToken const &type,
                    HdBinding const &binding,
                    const char *index)
{
    METAL_DEBUG_COMMENT(&str,"_EmitComputeAccessor\n"); //MTL_FIXME
    if (index) {
        str << _GetUnpackedType(type, false)
            << " HdGet_" << name << "(int localIndex) {\n"
            << "  int index = " << index << ";\n";
        if (binding.GetType() == HdBinding::SSBO) {
            str << "  return " << _GetPackedTypeAccessor(type, false) << "(";
            int numComponents = _GetNumComponents(type);
            for (int c = 0; c < numComponents; ++c) {
                if (c > 0) {
                    str << ",\n              ";
                }
                str << name << "[index + " << c << "]";
            }
            str << ");\n}\n";
        } else {
            str << "  return " << _GetPackedTypeAccessor(type, false) << "("
                << name << "[index]);\n}\n";
        }
    } else {
        // non-indexed, only makes sense for uniform or vertex.
        if (binding.GetType() == HdBinding::UNIFORM || 
            binding.GetType() == HdBinding::VERTEX_ATTR) {
            str << _GetUnpackedType(type, false)
                << " HdGet_" << name << "(int localIndex) { return ";
            str << _GetPackedTypeAccessor(type, true)
                << "(" << name << ");}\n";
        }
    }
    // GLSL spec doesn't allow default parameter. use function overload instead.
    // default to locaIndex=0
    str << _GetUnpackedType(type, false) << " HdGet_" << name << "()"
        << " { return HdGet_" << name << "(0); }\n";
    
}

static void _EmitComputeMutator(
                    std::stringstream &str,
                    TfToken const &structMemberName,
                    TfToken const &name,
                    TfToken const &type,
                    HdBinding const &binding,
                    const char *index)
{
    METAL_DEBUG_COMMENT(&str, "_EmitComputeMutator\n"); //MTL_FIXME
    if (index) {
        str << "void"
            << " HdSet_" << name << "(int localIndex, "
            << _GetUnpackedType(type, false) << " value) {\n"
            << "  int index = " << index << ";\n";
        if (binding.GetType() == HdBinding::SSBO) {
            str << "  " << _GetPackedType(type, false) << " packedValue = "
                << _GetPackedTypeMutator(type, false) << "(value);\n";
            int numComponents = _GetNumComponents(_GetPackedType(type, false));
            if (numComponents == 1) {
                str << "  "
                    << name << "[index] = packedValue;\n";
            } else {
                for (int c = 0; c < numComponents; ++c) {
                    str << "  "
                        << name << "[index + " << c << "] = "
                        << "packedValue[" << c << "];\n";
                }
            }
            str << "}\n";
        } else {
            TF_WARN("mutating non-SSBO not supported");
        }
    } else {
        TF_WARN("mutating non-indexed data not supported");
    }
    // XXX Don't output a default mutator as we don't want accidental overwrites
    // of compute read-write data.
    // GLSL spec doesn't allow default parameter. use function overload instead.
    // default to locaIndex=0
    //str << "void HdSet_" << name << "(" << type << " value)"
    //    << " { HdSet_" << name << "(0, value); }\n";
    
}

static void _EmitAccessor(std::stringstream &str,
                          TfToken const &name,
                          TfToken const &type,
                          HdBinding const &binding,
                          const char *index)
{
    METAL_DEBUG_COMMENT(&str, "_EmitAccessor ", (index == NULL ? "noindex" : index), (std::to_string(binding.GetType()).c_str()), "\n"); // MTL_FIXME
    bool emitIndexlessVariant = false;
    if (index) {
        emitIndexlessVariant = true;
        str << _GetUnpackedType(type, false)
            << " HdGet_" << name << "(int localIndex) {\n"
            << "  int index = " << index << ";\n";
        str << "  return " << _GetUnpackedType(_GetPackedTypeAccessor(type, true), false) << "("
            << name << "[index]);\n}\n";
    } else {
        // non-indexed, only makes sense for uniform or vertex.
        if (binding.GetType() == HdBinding::UNIFORM ||
            binding.GetType() == HdBinding::VERTEX_ATTR ||
            binding.GetType() == HdBinding::SSBO) {
            emitIndexlessVariant = true;
            str << _GetUnpackedType(type, false)
                << " HdGet_" << name << "(int localIndex) { return ";
            str << _GetUnpackedType(_GetPackedTypeAccessor(type, true), false) << "(" << name << ");}\n";
        }
    }
    
    // GLSL spec doesn't allow default parameter. use function overload instead.
    // default to locaIndex=0
    if(emitIndexlessVariant)
        str << _GetUnpackedType(type, false) << " HdGet_" << name << "()"
            << " { return HdGet_" << name << "(0); }\n";
    
}

static void _EmitTextureAccessors(
    std::stringstream &accessors,
    HdSt_ResourceBinder::MetaData::ShaderParameterAccessor const &acc,
    std::string const &swizzle,
    int const dim,
    bool const hasTextureTransform,
    bool const hasTextureScaleAndBias,
    bool const isBindless)
{
    GarchContextCaps const &caps = GarchResourceFactory::GetInstance()->GetContextCaps();

    TfToken const &name = acc.name;
    
    char const* textureStr = "texture";
    if (name == TfToken("depthReadback")) {
        textureStr = "depth";
    }

    if (!isBindless) {
        // a function returning sampler requires bindless_texture
        if (caps.bindlessTextureEnabled) {
            accessors
                << textureStr << dim << "d<float>\n"
                << "HdGetSampler_" << name << "() {\n"
                << "  return textureBind_" << name << ";"
                << "}\n";
        } else {
            accessors
                << "#define HdGetSampler_" << name << "()"
                << " textureBind_" << name << "\n";
        }
    } else {
        if (caps.bindlessTextureEnabled) {
            accessors
                << "sampler" << dim << "D\n"
                << "HdGetSampler_" << name << "() {\n"
                << "  int shaderCoord = GetDrawingCoord().shaderCoord; \n"
                << "  return sampler" << dim << "D("
                << "    materialParams[shaderCoord]." << name << ");\n"
                << "}\n";
        }
    }

    TfToken const &dataType = acc.dataType;

    accessors
        << _GetUnpackedType(dataType, false)
        << " HdGet_" << name << "(vec" << dim << " coord) {\n"
        << "  int shaderCoord = GetDrawingCoord().shaderCoord; \n";

    if (hasTextureTransform) {
        accessors
            << "   vec4 c = vec4(\n"
            << "     materialParams[shaderCoord]."
            << name << HdSt_ResourceBindingSuffixTokens->samplingTransform
            << " * vec4(coord, 1));\n"
            << "   vec3 sampleCoord = c.xyz / c.w;\n";
    } else {
        accessors
            << "  vec" << dim << " sampleCoord = coord;\n";
    }

    if (hasTextureScaleAndBias) {
        accessors
            << "  " << textureStr << dim << "d<float> tex = HdGetSampler_" << name << "();\n"
            << "  " << _GetUnpackedType(dataType, false)
            << " result = is_null_texture(tex) ? wrapped_float(0.0f):"
            << _GetUnpackedType(_GetPackedTypeAccessor(dataType, false), false)
            << "((tex.sample(samplerBind_" << name << ", sampleCoord)\n"
            << "#ifdef HD_HAS_" << name << "_" << HdStTokens->scale << "\n"
            << "    * HdGet_" << name << "_" << HdStTokens->scale << "()\n"
            << "#endif\n"
            << "#ifdef HD_HAS_" << name << "_" << HdStTokens->bias << "\n"
            << "    + HdGet_" << name << "_" << HdStTokens->bias  << "()\n"
            << "#endif\n"
            << ")" << swizzle << ");\n";
    } else {
        accessors
            << "  " << textureStr << dim << "d<float> tex = HdGetSampler_" << name << "();\n"
            << "  " << _GetUnpackedType(dataType, false)
            << "  result = is_null_texture(tex) ? wrapped_float(0.0f):"
            << _GetUnpackedType(_GetPackedTypeAccessor(dataType, false), false)
            << "(tex.sample(samplerBind_" << name << ", sampleCoord)"
            << swizzle << ");\n";
    }

    if (acc.processTextureFallbackValue) {

        if (isBindless) {
            accessors
                << "  if (materialParams[shaderCoord]." << name
                << " != uvec2(0, 0)) {\n";
        } else {
            accessors
                << "  if (materialParams[shaderCoord]." << name
                << HdSt_ResourceBindingSuffixTokens->valid
                << ") {\n";
        }

        if (hasTextureScaleAndBias) {
            accessors
                << "    return result;\n"
                << "  } else {\n"
                << "    return ("
                << _GetUnpackedType(_GetPackedTypeAccessor(dataType, false), false)
                << "(materialParams[shaderCoord]."
                << name
                << HdSt_ResourceBindingSuffixTokens->fallback << swizzle << ")\n"
                << "#ifdef HD_HAS_" << name << "_" << HdStTokens->scale << "\n"
                << "        * HdGet_" << name << "_" << HdStTokens->scale
                << "()" << swizzle << "\n"
                << "#endif\n"
                << "#ifdef HD_HAS_" << name << "_" << HdStTokens->bias << "\n"
                << "        + HdGet_" << name << "_" << HdStTokens->bias
                << "()" << swizzle << "\n"
                << "#endif\n"
                << ");\n"
                << "  }\n";
        } else {
            accessors
                << "    return result;\n"
                << "  } else {\n"
                << "    return "
                << _GetUnpackedType(_GetPackedTypeAccessor(dataType, false), false)
                << "(materialParams[shaderCoord]."
                << name
                << HdSt_ResourceBindingSuffixTokens->fallback << ");\n"
                << "  }\n";
        }
    } else {
        accessors
            << "  return result;\n";
    }
    
    accessors
        << "}\n";
    
    TfTokenVector const &inPrimvars = acc.inPrimvars;

    // vec4 HdGet_name(int localIndex)
    accessors
        << _GetUnpackedType(dataType, false)
        << " HdGet_" << name
        << "(int localIndex) { return HdGet_" << name << "(";
    if (!inPrimvars.empty()) {
        accessors
            << "\n"
            << "#if defined(HD_HAS_" << inPrimvars[0] << ")\n"
            << "HdGet_" << inPrimvars[0]
            << "(localIndex).xy\n"
            << "#else\n"
            << "vec" << dim << "(0.0)\n"
            << "#endif\n";
    } else {
        accessors
            << "vec" << dim << "(0.0)";
    }
    accessors << "); }\n";

    // vec4 HdGet_name()
    accessors
        << _GetUnpackedType(dataType, false)
        << " HdGet_" << name
        << "() { return HdGet_" << name << "(0); }\n";
}

void
HdSt_CodeGenMSL::_GenerateConfigComments(std::stringstream& vsCfg, std::stringstream& fsCfg, std::stringstream& gsCfg)
{
    std::vector<std::string> commonSourceKeys = _geometricShader->GetSourceKeys(HdShaderTokens->commonShaderSource);
    vsCfg << "\n//\n//\tCommon GLSLFX Config:\n//\n";
    fsCfg << "\n//\n//\tCommon GLSLFX Config:\n//\n";
    gsCfg << "\n//\n//\tCommon GLSLFX Config:\n//\n";
    for(auto key : commonSourceKeys) {
        vsCfg << "//\t\t" << key << "\n";
        fsCfg << "//\t\t" << key << "\n";
        gsCfg << "//\t\t" << key << "\n";
    }
    
    { //VS
        std::vector<std::string> sourceKeys = _geometricShader->GetSourceKeys(HdShaderTokens->vertexShader);
        vsCfg << "//\n\n//\n//\tVertex GLSLFX Config:\n//\n";
        for(auto key : sourceKeys)
            vsCfg << "//\t\t" << key << "\n";
        vsCfg << "//\n\n";
    }
    { //FS
        std::vector<std::string> sourceKeys = _geometricShader->GetSourceKeys(HdShaderTokens->fragmentShader);
        fsCfg << "//\n\n//\n//\tFragment GLSLFX Config:\n//\n";
        for(auto key : sourceKeys)
            fsCfg << "//\t\t" << key << "\n";
        fsCfg << "//\n\n";
    }
    { //GS
        std::vector<std::string> sourceKeys = _geometricShader->GetSourceKeys(HdShaderTokens->geometryShader);
        gsCfg << "//\n\n//\n//\tGeometry GLSLFX Config:\n//\n";
        for(auto key : sourceKeys)
            gsCfg << "//\t\t" << key << "\n";
        gsCfg   << "//\n//\tIgnored Geometry Shader Exports via MTL_HINTs:\n//\n";
        for(auto it : _gsIgnoredExports)
            gsCfg << "//\t\t" << it << "\n";
        gsCfg << "//\n\n";
    }
}

void
HdSt_CodeGenMSL::_GenerateCommonDefinitions()
{
    // Used in glslfx files to determine if it is using new/old
    // imaging system. It can also be used as API guards when
    // we need new versions of Hydra shading.
    _genDefinitions  << GetComputeHeader();

    // primvar existence macros
    
    // XXX: this is temporary, until we implement the fallback value definition
    // for any primvars used in glslfx.
    // Note that this #define has to be considered in the hash computation
    // since it changes the source code. However we have already combined the
    // entries of instanceData into the hash value, so it's not needed to be
    // added separately, at least in current usage.
    TF_FOR_ALL (it, _metaData.constantData) {
        TF_FOR_ALL (pIt, it->second.entries) {
            _genDefinitions << "#define HD_HAS_" << pIt->name << " 1\n";
        }
    }
    TF_FOR_ALL (it, _metaData.instanceData) {
        _genDefinitions << "#define HD_HAS_INSTANCE_" << it->second.name << " 1\n"
                        << "#define HD_HAS_" << it->second.name << "_" << it->second.level << " 1\n";
    }
    _genDefinitions << "#define HD_INSTANCER_NUM_LEVELS "
                    << _metaData.instancerNumLevels << "\n"
                    << "#define HD_INSTANCE_INDEX_WIDTH "
                    << (_metaData.instancerNumLevels+1) << "\n";
    if (!_geometricShader->IsPrimTypePoints()) {
        TF_FOR_ALL (it, _metaData.elementData) {
            _genDefinitions << "#define HD_HAS_" << it->second.name << " 1\n";
        }
        if (_hasGS) {
            TF_FOR_ALL (it, _metaData.fvarData) {
                _genDefinitions << "#define HD_HAS_" << it->second.name << " 1\n";
            }
        }
    }
    TF_FOR_ALL (it, _metaData.vertexData) {
        _genDefinitions << "#define HD_HAS_" << it->second.name << " 1\n";
    }
    TF_FOR_ALL (it, _metaData.shaderParameterBinding) {
        // XXX: HdBinding::PRIMVAR_REDIRECT won't define an accessor if it's
        // an alias of like-to-like, so we want to suppress the HD_HAS_* flag
        // as well.

        // For PRIMVAR_REDIRECT, the HD_HAS_* flag will be defined after
        // the corresponding HdGet_* function.
        
        // XXX: (HYD-1882) The #define HD_HAS_... for a primvar
        // redirect will be defined immediately after the primvar
        // redirect HdGet_... in the loop over
        // _metaData.shaderParameterBinding below.  Given that this
        // loop is not running in a canonical order (e.g., textures
        // first, then primvar redirects, ...) and that the texture is
        // picking up the HD_HAS_... flag, the answer to the following
        // question is random:
        //
        // If there is a texture trying to use a primvar called NAME
        // for coordinates and there is a primvar redirect called NAME,
        // will the texture use it or not?
        // 

        HdBinding::Type bindingType = it->first.GetType();
        if (bindingType != HdBinding::PRIMVAR_REDIRECT) {
            _genDefinitions << "#define HD_HAS_" << it->second.name << " 1\n";
        }
    }
    
    // HD_NUM_PATCH_VERTS, HD_NUM_PRIMTIIVE_VERTS
    if (_geometricShader->IsPrimTypePatches()) {
        _genDefinitions << "#define HD_NUM_PATCH_VERTS "
                        << _geometricShader->GetPrimitiveIndexSize() << "\n";
    }
    _genDefinitions << "#define HD_NUM_PRIMITIVE_VERTS "
                    << _geometricShader->GetNumPrimitiveVertsForGeometryShader()
                    << "\n";
}

void
HdSt_CodeGenMSL::_GenerateCommonCode()
{
    // check if surface shader has masked material tag
    for (auto const& shader : _shaders) {
        if (HdStSurfaceShaderSharedPtr surfaceShader =
            std::dynamic_pointer_cast<HdStSurfaceShader>(shader)) {
            if (surfaceShader->GetMaterialTag() ==
                HdStMaterialTagTokens->masked) {
                _genCommon << "#define HD_MATERIAL_TAG_MASKED 1\n";
            }
        }
    }

    _genCommon  << "class ProgramScope<st> {\n"
                << "public:\n";

    METAL_DEBUG_COMMENT(&_genCommon, "Start of special inputs\n"); //MTL_FIXME
    
    
    _EmitDeclaration(_genCommon,
                     TfToken("gl_VertexID"), TfToken("uint"), TfToken("[[vertex_id]]"),
                     HdBinding(HdBinding::VERTEX_ID, 0));
    _AddInputParam(  _mslVSInputParams,
                TfToken("gl_VertexID"), TfToken("uint"), TfToken("[[vertex_id]]"),
                HdBinding(HdBinding::VERTEX_ID, 0));
    
    _EmitDeclaration(   _genCommon,
                        TfToken("gl_BaseVertex"), TfToken("uint"), TfToken("[[base_vertex]]"),
                        HdBinding(HdBinding::BASE_VERTEX_ID, 0));
    _AddInputParam(  _mslVSInputParams,
                TfToken("gl_BaseVertex"), TfToken("uint"), TfToken("[[base_vertex]]"),
                HdBinding(HdBinding::BASE_VERTEX_ID, 0));
    
    _EmitDeclaration(_genCommon,
                     TfToken("gl_FrontFacing"), TfToken("bool"), TfToken("[[front_facing]]"),
                     HdBinding(HdBinding::FRONT_FACING, 0));
    _AddInputParam(  _mslPSInputParams,
                TfToken("gl_FrontFacing"), TfToken("bool"), TfToken("[[front_facing]]"),
                HdBinding(HdBinding::FRONT_FACING, 0));
    
    _EmitDeclaration(_genCommon,
                     TfToken("gl_InstanceID"), TfToken("uint"), TfToken("[[instance_id]]"),
                     HdBinding(HdBinding::INSTANCE_ID, 0));
    _AddInputParam(  _mslVSInputParams,
                TfToken("gl_InstanceID"), TfToken("uint"), TfToken("[[instance_id]]"),
                HdBinding(HdBinding::INSTANCE_ID, 0));

    METAL_DEBUG_COMMENT(&_genCommon, "End of special inputs\n"); //MTL_FIXME
    
    METAL_DEBUG_COMMENT(&_genCommon, "Start of vertex/fragment interface\n"); //MTL_FIXME
    
    _EmitOutput(_genCommon, TfToken("gl_Position"), TfToken("vec4"), TfToken("[[position]]"));
    {
        TParam &param = _AddOutputParam(_mslVSOutputParams, TfToken("gl_Position"), TfToken("vec4"));
        param.attribute = TfToken("[[position]]");
        param.usage |= TParam::VertexShaderOnly;
    }
    _AddInputParam(_mslGSInputParams, TfToken("gl_Position"),
        TfToken("vec4"), TfToken("[[position]]")).usage |= TParam::VertexData;
    {
        TParam &param = _AddOutputParam(_mslGSOutputParams, TfToken("gl_Position"), TfToken("vec4"));
        param.attribute = TfToken("[[position]]");
        param.usage |= TParam::VertexData;
    }
    _AddInputParam(  _mslPSInputParams,
       TfToken("gl_Position"), TfToken("vec4"), TfToken("[[position]]"),
       HdBinding(HdBinding::FRAG_COORD, 0)).usage |= TParam::VertexData;

    _EmitOutput(_genCommon, TfToken("gl_PointSize"), TfToken("float"), TfToken("[[point_size]]"));
    {
        TParam &param = _AddOutputParam(_mslVSOutputParams, TfToken("gl_PointSize"), TfToken("float"));
        param.attribute = TfToken("[[point_size]]");
        param.usage |= TParam::VertexShaderOnly;
    }

    _genCommon  << "#if defined(HD_VERTEX_SHADER)\n"
                << "#define VTXCONST const\n"
                << "#if !defined(HD_NUM_clipPlanes)\n"
                << "#define HD_NUM_clipPlanes 1\n"
                << "#endif\n"
                << "float gl_ClipDistance[HD_NUM_clipPlanes];\n"
                << "#elif defined(HD_GEOMETRY_SHADER)\n"
                << "#define VTXCONST const\n"
                << "#else\n"
                << "#define VTXCONST\n"
                << "#endif\n";
    
    {
        // HD_NUM_clipPlanes
        TParam &param = _AddOutputParam(
            _mslVSOutputParams,  TfToken("gl_ClipDistance"), TfToken("float"));
        param.attribute = TfToken("[[clip_distance]]");
        param.usage |= TParam::VertexShaderOnly;
        param.arraySize = 1;
        param.arraySizeStr = "HD_NUM_clipPlanes";
        param.defineWrapperStr = "HD_HAS_clipPlanes";
    }

    _genCommon << "uint gl_PrimitiveID = 0;\n"
               << "uint gl_PrimitiveIDIn = 0;\n"
               << "int gl_MaxTessGenLevel = 64;\n"
               << "#if defined(HD_FRAGMENT_SHADER)\n"
               << "vec4 gl_FragCoord;\n"
               << "#endif\n";
    
    METAL_DEBUG_COMMENT(&_genCommon, "End of vertex/fragment interface\n"); //MTL_FIXME
   
    METAL_DEBUG_COMMENT(&_genCommon, "_metaData.customBindings\n"); //MTL_FIXME
}

void
HdSt_CodeGenMSL::_GenerateBindingsCode()
{
    // ------------------
    // Custom Buffer Bindings
    // ----------------------
    // For custom buffer bindings, more code can be generated; a full spec is
    // emitted based on the binding declaration.
    // MTL_IMPROVE - In Metal we're going to end up with a binding per buffer even though these will (all?) effectively be uniforms, perhaps it might be better to pack all into a single struct
    if(_metaData.customBindings.size()) {

        TF_FOR_ALL(binDecl, _metaData.customBindings) {
            _genDefinitions << "#define "
            << binDecl->name << "_Binding "
            << binDecl->binding.GetLocation() << "\n";
            _genDefinitions << "#define HD_HAS_" << binDecl->name << " 1\n";
            
            // typeless binding doesn't need declaration nor accessor.
            if (binDecl->dataType.IsEmpty()) continue;

            char const* indexStr = NULL;
            if (binDecl->binding.GetType() == HdBinding::SSBO)
            {
                indexStr = "localIndex";
                if (binDecl->typeIsAtomic || binDecl->writable)
                {
                    _EmitDeclarationMutablePtr(_genCommon, *binDecl);
                }
                else
                {
                    _EmitDeclarationPtr(_genCommon, *binDecl);
                }
                _AddInputPtrParam(_mslVSInputParams, *binDecl);
                _AddInputPtrParam(_mslPSInputParams, *binDecl);
            }
            else
            {
                _EmitDeclaration(_genCommon, *binDecl);
                _AddInputParam(_mslVSInputParams, *binDecl);
                _AddInputParam(_mslPSInputParams, *binDecl);
            }

            // Accessor are currently only emitted for non-atomic types because Metal requires all accesses (even simple reads)
            // of atomics to go via the atomic_read functions, which does not play well with the way the accessor functions
            // work. This could possibly be refactored to allow accessors for atomics.
            if (!binDecl->typeIsAtomic)
            {
                _EmitAccessor(_genCommon,
                              binDecl->name,
                              binDecl->dataType,
                              binDecl->binding,
                              indexStr);
            }
        }
    }
    
    METAL_DEBUG_COMMENT(&_genCommon, "END OF _metaData.customBindings\n"); //MTL_FIXME
    
    std::stringstream declarations;
    std::stringstream accessors;
    METAL_DEBUG_COMMENT(&_genCommon, "_metaData.customInterleavedBindings\n"); //MTL_FIXME
    
    TF_FOR_ALL(it, _metaData.customInterleavedBindings) {
        // note: _constantData has been sorted by offset in HdSt_ResourceBinder.
        // XXX: not robust enough, should consider padding and layouting rules
        // to match with the logic in HdInterleavedMemoryManager if we
        // want to use a layouting policy other than default padding.
        
        HdBinding binding = it->first;
        TfToken typeName(TfStringPrintf("CustomBlockData%d", binding.GetValue()));
        TfToken varName = it->second.blockName;
        
        declarations << "struct " << typeName << " {\n";
        
        // dbIt is StructEntry { name, dataType, offset, numElements }
        TF_FOR_ALL (dbIt, it->second.entries) {
            _genDefinitions << "#define HD_HAS_" << dbIt->name << " 1\n";
            declarations << "  " << _GetPackedType(dbIt->dataType, false)
            << " " << dbIt->name;
            if (dbIt->arraySize > 1) {
                _genDefinitions << "#define HD_NUM_" << dbIt->name
                << " " << dbIt->arraySize << "\n";
                declarations << "[" << dbIt->arraySize << "]";
            }
            declarations <<  ";\n";
            
            _EmitStructAccessor(accessors, varName,
                                dbIt->name, dbIt->dataType, dbIt->arraySize,
                                true, NULL);
        }
        
        declarations << "};\n";
        _EmitDeclarationPtr(declarations, varName, typeName, TfToken(), binding, 0, true);
        _AddInputPtrParam(_mslVSInputParams, varName, typeName, TfToken(), binding, 0, true);
        _AddInputPtrParam(_mslGSInputParams, varName, typeName, TfToken(), binding, 0, true);
        _AddInputPtrParam(_mslPSInputParams, varName, typeName, TfToken(), binding, 0, true);
    }
    _genCommon << declarations.str() << accessors.str();
    METAL_DEBUG_COMMENT(&_genCommon, "END OF _metaData.customInterleavedBindings\n"); //MTL_FIXME
}

void
HdSt_CodeGenMSL::_GenerateDrawingCoord()
{
    METAL_DEBUG_COMMENT(&_genCommon, "_GenerateDrawingCoord Common\n"); //MTL_FIXME
    METAL_DEBUG_COMMENT(&_genVS,     "_GenerateDrawingCoord VS\n"); //MTL_FIXME
    METAL_DEBUG_COMMENT(&_genFS,     "_GenerateDrawingCoord PS\n"); //MTL_FIXME
    
    TF_VERIFY(_metaData.drawingCoord0Binding.binding.IsValid());
    TF_VERIFY(_metaData.drawingCoord1Binding.binding.IsValid());
    TF_VERIFY(_metaData.drawingCoord2Binding.binding.IsValid());

    /*
       hd_drawingCoord is a struct of integer offsets to locate the primvars
       in buffer arrays at the current rendering location.

       struct hd_drawingCoord {
           int modelCoord;          // (reserved) model parameters
           int constantCoord;       // constant primvars (per object)
           int vertexCoord;         // vertex primvars   (per vertex)
           int elementCoord;        // element primvars  (per face/curve)
           int primitiveCoord;      // primitive ids     (per tri/quad/line)
           int fvarCoord;           // fvar primvars     (per face-vertex)
           int shaderCoord;         // shader parameters (per shader/object)
           int instanceIndex[];     // (see below)
           int instanceCoords[];    // (see below)
       };

          instanceIndex[0]  : global instance ID (used for ID rendering)
                       [1]  : instance index for level = 0
                       [2]  : instance index for level = 1
                       ...
          instanceCoords[0] : instanceDC for level = 0
          instanceCoords[1] : instanceDC for level = 1
                       ...

       We also have a drawingcoord for vertex primvars. Currently it's not
       being passed into shader since the vertex shader takes pre-offsetted
       vertex arrays and no needs to apply offset in shader (except gregory
       patch drawing etc. In that case gl_BaseVertexARB can be used under
       GL_ARB_shader_draw_parameters extention)

       gl_InstanceID is available only in vertex shader, so codegen
       takes care of applying an offset for each instance for the later
       stage. On the other hand, gl_PrimitiveID is available in all stages
       except vertex shader, and since tess/geometry shaders may or may not
       exist, we don't apply an offset of primitiveID during interstage
       plumbing to avoid overlap. Instead, GetDrawingCoord() applies
       primitiveID if necessary.

       XXX:
       Ideally we should use an interface block like:

         in DrawingCoord {
             flat hd_drawingCoord drawingCoord;
         } inDrawingCoord;
         out DrawingCoord {
             flat hd_drawingCoord drawingCoord;
         } outDrawingCoord;

      then the fragment shader can take the same input regardless the
      existence of tess/geometry shaders. However it seems the current
      driver (331.79) doesn't handle multiple interface blocks
      appropriately, it fails matching and ends up undefined results at
      consuming shader.

      > OpenGL 4.4 Core profile
      > 7.4.1 Shader Interface Matching
      >
      > When multiple shader stages are active, the outputs of one stage form
      > an interface with the inputs of the next stage. At each such
      > interface, shader inputs are matched up against outputs from the
      > previous stage:
      >
      > An output block is considered to match an input block in the
      > subsequent shader if the two blocks have the same block name, and
      > the members of the block match exactly in name, type, qualification,
      > and declaration order.
      >
      > An output variable is considered to match an input variable in the
      > subsequent shader if:
      >  - the two variables match in name, type, and qualification; or
      >  - the two variables are declared with the same location and
      >     component layout qualifiers and match in type and qualification.

      We use non-block variable for drawingCoord as a workaround of this
      problem for now. There is a caveat we can't use the same name for input
      and output, the subsequent shader has to be aware which stage writes
      the drawingCoord.

      for example:
        drawingCoord--(VS)--vsDrawingCoord--(GS)--gsDrawingCoord--(FS)
        drawingCoord--(VS)------------------------vsDrawingCoord--(FS)

      Fortunately the compiler is smart enough to optimize out unused
      attributes. If the VS writes the same value into two attributes:

        drawingCoord--(VS)--vsDrawingCoord--(GS)--gsDrawingCoord--(FS)
                      (VS)--gsDrawingCoord--------gsDrawingCoord--(FS)

      The fragment shader can always take gsDrawingCoord. The following code
      does such a plumbing work.

     */

    // common
    //
    // note: instanceCoords should be [HD_INSTANCER_NUM_LEVELS], but since
    //       GLSL doesn't allow [0] declaration, we use +1 value (WIDTH)
    //       for the sake of simplicity.
    _genCommon << "struct hd_drawingCoord {                       \n"
               << "  int modelCoord;                              \n"
               << "  int constantCoord;                           \n"
               << "  int vertexCoord;                             \n"
               << "  int elementCoord;                            \n"
               << "  int primitiveCoord;                          \n"
               << "  int fvarCoord;                               \n"
               << "  int shaderCoord;                             \n"
               << "  int topologyVisibilityCoord;                 \n"
               << "  int instanceIndex[HD_INSTANCE_INDEX_WIDTH];  \n"
               << "  int instanceCoords[HD_INSTANCE_INDEX_WIDTH]; \n"
               << "};\n";

    //_genCommon << "hd_drawingCoord GetDrawingCoord();\n"; // forward declaration

    // vertex shader

    // [immediate]
    //   layout (location=x) uniform ivec4 drawingCoord0;
    //   layout (location=y) uniform ivec4 drawingCoord1;
    //   layout (location=z) uniform int   drawingCoordI[N];
    // [indirect]
    //   layout (location=x) in ivec4 drawingCoord0
    //   layout (location=y) in ivec4 drawingCoord1
    //   layout (location=z) in int   drawingCoordI[N]

    _EmitDeclaration(_genVS, _metaData.drawingCoord0Binding);
    _AddInputParam(_mslVSInputParams, _metaData.drawingCoord0Binding);
    _EmitDeclaration(_genVS, _metaData.drawingCoord1Binding);
    _AddInputParam(_mslVSInputParams, _metaData.drawingCoord1Binding);
    _EmitDeclaration(_genVS, _metaData.drawingCoord2Binding);
    _AddInputParam(_mslVSInputParams, _metaData.drawingCoord2Binding);
    
    if (_metaData.drawingCoordIBinding.binding.IsValid()) {
        _EmitDeclaration(_genVS, _metaData.drawingCoordIBinding, TfToken(), /*arraySize=*/std::max(1, _metaData.instancerNumLevels));
        _AddInputParam(_mslVSInputParams, _metaData.drawingCoordIBinding, TfToken(), /*arraySize=*/std::max(1, _metaData.instancerNumLevels));
    }

    // instance index indirection
    _genCommon << "struct hd_instanceIndex { int indices[HD_INSTANCE_INDEX_WIDTH]; };\n";

    if (_metaData.instanceIndexArrayBinding.binding.IsValid()) {
        // << layout (location=x) uniform (int|ivec[234]) *instanceIndices;
        _EmitDeclarationPtr(_genCommon, _metaData.instanceIndexArrayBinding);
        _AddInputPtrParam(_mslVSInputParams, _metaData.instanceIndexArrayBinding);

        // << layout (location=x) uniform (int|ivec[234]) *culledInstanceIndices;
        _EmitDeclarationPtr(_genCommon, _metaData.culledInstanceIndexArrayBinding);
        _AddInputPtrParam(_mslVSInputParams, _metaData.culledInstanceIndexArrayBinding);

        /// if \p cullingPass is true, CodeGen generates GetInstanceIndex()
        /// such that it refers instanceIndices buffer (before culling).
        /// Otherwise, GetInstanceIndex() looks up culledInstanceIndices.

        _genVS << "int GetInstanceIndexCoord() {\n"
               << "  return drawingCoord1.y + gl_InstanceID * HD_INSTANCE_INDEX_WIDTH; \n"
               << "}\n";

        if (_geometricShader->IsFrustumCullingPass()) {
            // for frustum culling:  use instanceIndices.
            _genVS << "hd_instanceIndex GetInstanceIndex() {\n"
                   << "  int offset = GetInstanceIndexCoord();\n"
                   << "  hd_instanceIndex r;\n"
                   << "  for (int i = 0; i < HD_INSTANCE_INDEX_WIDTH; ++i)\n"
                   << "    r.indices[i] = instanceIndices[offset+i];\n"
                   << "  return r;\n"
                   << "}\n";
            _genVS << "void SetCulledInstanceIndex(uint instanceID) {\n"
                   << "  for (int i = 0; i < HD_INSTANCE_INDEX_WIDTH; ++i)\n"
                   << "    culledInstanceIndices[drawingCoord1.y + instanceID*HD_INSTANCE_INDEX_WIDTH+i]"
                   << "        = instanceIndices[drawingCoord1.y + gl_InstanceID*HD_INSTANCE_INDEX_WIDTH+i];\n"
                   << "}\n";
        } else {
            // for drawing:  use culledInstanceIndices.
            _EmitAccessor(_genVS, _metaData.culledInstanceIndexArrayBinding.name,
                          _metaData.culledInstanceIndexArrayBinding.dataType,
                          _metaData.culledInstanceIndexArrayBinding.binding,
                          "GetInstanceIndexCoord()+localIndex");
            _genVS << "hd_instanceIndex GetInstanceIndex() {\n"
                   << "  int offset = GetInstanceIndexCoord();\n"
                   << "  hd_instanceIndex r;\n"
                   << "  for (int i = 0; i < HD_INSTANCE_INDEX_WIDTH; ++i)\n"
                   << "    r.indices[i] = HdGet_culledInstanceIndices(/*localIndex=*/i);\n"
                   << "  return r;\n"
                   << "}\n";
        }
    } else {
        _genVS << "hd_instanceIndex GetInstanceIndex() {"
               << "  hd_instanceIndex r; r.indices[0] = 0; return r; }\n";
        if (_geometricShader->IsFrustumCullingPass()) {
            _genVS << "void SetCulledInstanceIndex(uint instance) "
                      "{ /*no-op*/ };\n";
        }
    }

    TfToken drawingCoordType("hd_drawingCoord");
    TfToken intType("int");
    TfToken tkn_flat("[[flat]]");
    
    _genVS  << "hd_drawingCoord vsDrawingCoord;\n"
            << "hd_drawingCoord gsDrawingCoord;\n";
    
    _genGS  << "hd_drawingCoord vsDrawingCoord[1];\n"
            << "hd_drawingCoord gsDrawingCoord;\n";

    TfToken tkn_modelCoord("__dc_modelCoord");
    TfToken tkn_constantCoord("__dc_constantCoord");
    TfToken tkn_elementCoord("__dc_elementCoord");
    TfToken tkn_primitiveCoord("__dc_primitiveCoord");
    TfToken tkn_fvarCoord("__dc_fvarCoord");
    TfToken tkn_shaderCoord("__dc_shaderCoord");
    
    //We add the input/output params here. Glue code is generated from these.
    bool _mslBuildComputeGS = _buildTarget == kMSL_BuildTarget_MVA_ComputeGS;
    
    _EmitStructMemberOutput(_mslVSOutputParams, tkn_modelCoord,
        TfToken("vsDrawingCoord.modelCoord"), intType, tkn_flat).usage |= HdSt_CodeGenMSL::TParam::DrawingCoord;
    if(_mslBuildComputeGS) {
        _AddInputParam(_mslGSInputParams, tkn_modelCoord, intType, tkn_flat, HdBinding(HdBinding::UNKNOWN, 0), 0, TfToken("vsDrawingCoord[i].modelCoord")).usage |= HdSt_CodeGenMSL::TParam::DrawingCoord;
        //_AddOutputParam(_mslGSOutputParams, tkn_modelCoord, intType, tkn_flat, TfToken("gsDrawingCoord.modelCoord")).usage |= HdSt_CodeGenMSL::TParam::DrawingCoord;
    }
    _AddInputParam(_mslPSInputParams, tkn_modelCoord, intType, tkn_flat, HdBinding(HdBinding::UNKNOWN, 0), 0, TfToken("gsDrawingCoord.modelCoord")).usage |= HdSt_CodeGenMSL::TParam::DrawingCoord;

    _EmitStructMemberOutput(_mslVSOutputParams, tkn_constantCoord,
        TfToken("vsDrawingCoord.constantCoord"), intType, tkn_flat).usage |= HdSt_CodeGenMSL::TParam::DrawingCoord;
    if(_mslBuildComputeGS) {
        _AddInputParam(_mslGSInputParams, tkn_constantCoord, intType, tkn_flat, HdBinding(HdBinding::UNKNOWN, 0), 0, TfToken("vsDrawingCoord[i].constantCoord")).usage |= HdSt_CodeGenMSL::TParam::DrawingCoord;
        //_AddOutputParam(_mslGSOutputParams, tkn_constantCoord, intType, tkn_flat, TfToken("gsDrawingCoord.constantCoord")).usage |= HdSt_CodeGenMSL::TParam::DrawingCoord;
    }
    _AddInputParam(_mslPSInputParams, tkn_constantCoord, intType, tkn_flat, HdBinding(HdBinding::UNKNOWN, 0), 0, TfToken("gsDrawingCoord.constantCoord")).usage |= HdSt_CodeGenMSL::TParam::DrawingCoord;
    
    _EmitStructMemberOutput(_mslVSOutputParams, tkn_elementCoord,
        TfToken("vsDrawingCoord.elementCoord"), intType, tkn_flat).usage |= HdSt_CodeGenMSL::TParam::DrawingCoord;
    if(_mslBuildComputeGS) {
        _AddInputParam(_mslGSInputParams, tkn_elementCoord, intType, tkn_flat, HdBinding(HdBinding::UNKNOWN, 0), 0, TfToken("vsDrawingCoord[i].elementCoord")).usage |= HdSt_CodeGenMSL::TParam::DrawingCoord;
        //_AddOutputParam(_mslGSOutputParams, tkn_elementCoord, intType, tkn_flat, TfToken("gsDrawingCoord.elementCoord")).usage |= HdSt_CodeGenMSL::TParam::DrawingCoord;
    }
    _AddInputParam(_mslPSInputParams, tkn_elementCoord, intType, tkn_flat, HdBinding(HdBinding::UNKNOWN, 0), 0, TfToken("gsDrawingCoord.elementCoord")).usage |= HdSt_CodeGenMSL::TParam::DrawingCoord;
    
    _EmitStructMemberOutput(_mslVSOutputParams, tkn_primitiveCoord,
        TfToken("vsDrawingCoord.primitiveCoord"), intType, tkn_flat).usage |= HdSt_CodeGenMSL::TParam::DrawingCoord;
    if(_mslBuildComputeGS) {
        _AddInputParam(_mslGSInputParams, tkn_primitiveCoord, intType, tkn_flat, HdBinding(HdBinding::UNKNOWN, 0), 0, TfToken("vsDrawingCoord[i].primitiveCoord")).usage |= HdSt_CodeGenMSL::TParam::DrawingCoord;
        //_AddOutputParam(_mslGSOutputParams, tkn_primitiveCoord, intType, tkn_flat, TfToken("gsDrawingCoord.primitiveCoord")).usage |= HdSt_CodeGenMSL::TParam::DrawingCoord;
    }
    _AddInputParam(_mslPSInputParams, tkn_primitiveCoord, intType, tkn_flat, HdBinding(HdBinding::UNKNOWN, 0), 0, TfToken("gsDrawingCoord.primitiveCoord")).usage |= HdSt_CodeGenMSL::TParam::DrawingCoord;
    
    _EmitStructMemberOutput(_mslVSOutputParams, tkn_fvarCoord,
        TfToken("vsDrawingCoord.fvarCoord"), intType, tkn_flat).usage |= HdSt_CodeGenMSL::TParam::DrawingCoord;
    if(_mslBuildComputeGS) {
        _AddInputParam(_mslGSInputParams, tkn_fvarCoord, intType, tkn_flat, HdBinding(HdBinding::UNKNOWN, 0), 0, TfToken("vsDrawingCoord[i].fvarCoord")).usage |= HdSt_CodeGenMSL::TParam::DrawingCoord;
        //_AddOutputParam(_mslGSOutputParams, tkn_fvarCoord, intType, tkn_flat, TfToken("gsDrawingCoord.fvarCoord")).usage |= HdSt_CodeGenMSL::TParam::DrawingCoord;
    }
    _AddInputParam(_mslPSInputParams, tkn_fvarCoord, intType, tkn_flat, HdBinding(HdBinding::UNKNOWN, 0), 0, TfToken("gsDrawingCoord.fvarCoord")).usage |= HdSt_CodeGenMSL::TParam::DrawingCoord;
    
    _EmitStructMemberOutput(_mslVSOutputParams, tkn_shaderCoord,
        TfToken("vsDrawingCoord.shaderCoord"), intType, tkn_flat).usage |= HdSt_CodeGenMSL::TParam::DrawingCoord;
    if(_mslBuildComputeGS) {
        _AddInputParam(_mslGSInputParams, tkn_shaderCoord, intType, tkn_flat, HdBinding(HdBinding::UNKNOWN, 0), 0, TfToken("vsDrawingCoord[i].shaderCoord")).usage |= HdSt_CodeGenMSL::TParam::DrawingCoord;
        //_AddOutputParam(_mslGSOutputParams, tkn_shaderCoord, intType, tkn_flat, TfToken("gsDrawingCoord.shaderCoord")).usage |= HdSt_CodeGenMSL::TParam::DrawingCoord;
    }
    _AddInputParam(_mslPSInputParams, tkn_shaderCoord, intType, tkn_flat, HdBinding(HdBinding::UNKNOWN, 0), 0, TfToken("gsDrawingCoord.shaderCoord")).usage |= HdSt_CodeGenMSL::TParam::DrawingCoord;
    
    for(int i = 0; i <= _metaData.instancerNumLevels; i++)
    {
        TfToken tkn___dc_instanceIndex(TfStringPrintf("__dc_instanceIndex%d", i));
        TfToken tkn_vs_instanceIndex(TfStringPrintf("vsDrawingCoord.instanceIndex[%d]", i));
        TfToken tkn_gs_instanceIndex(TfStringPrintf("gsDrawingCoord.instanceIndex[%d]", i));
        _EmitStructMemberOutput(_mslVSOutputParams, tkn___dc_instanceIndex, tkn_vs_instanceIndex, intType, tkn_flat).usage |= HdSt_CodeGenMSL::TParam::DrawingCoord;
        if(_mslBuildComputeGS) {
            _AddInputParam(_mslGSInputParams, tkn___dc_instanceIndex, intType, tkn_flat, HdBinding(HdBinding::UNKNOWN, 0), 0, TfToken(TfStringPrintf("vsDrawingCoord[i].instanceIndex[%d]", i))).usage |= HdSt_CodeGenMSL::TParam::DrawingCoord;
            //_AddOutputParam(_mslGSOutputParams, tkn___dc_instanceIndex, intType, tkn_flat, tkn_gs_instanceIndex).usage |= HdSt_CodeGenMSL::TParam::DrawingCoord;
        }
        _AddInputParam(_mslPSInputParams, tkn___dc_instanceIndex, intType, tkn_flat, HdBinding(HdBinding::UNKNOWN, 0), 0, tkn_gs_instanceIndex).usage |= HdSt_CodeGenMSL::TParam::DrawingCoord;
        
        TfToken tkn___dc_instanceCoords(TfStringPrintf("__dc_instanceCoords%d", i));
        TfToken tkn_vs_instanceCoords(TfStringPrintf("vsDrawingCoord.instanceCoords[%d]", i));
        TfToken tkn_gs_instanceCoords(TfStringPrintf("gsDrawingCoord.instanceCoords[%d]", i));
        _EmitStructMemberOutput(_mslVSOutputParams, tkn___dc_instanceCoords, tkn_vs_instanceCoords, intType, tkn_flat).usage |= HdSt_CodeGenMSL::TParam::DrawingCoord;
        if(_mslBuildComputeGS) {
            _AddInputParam(_mslGSInputParams, tkn___dc_instanceCoords, intType, tkn_flat, HdBinding(HdBinding::UNKNOWN, 0), 0, TfToken(TfStringPrintf("vsDrawingCoord[i].instanceCoords[%d]", i))).usage |= HdSt_CodeGenMSL::TParam::DrawingCoord;
            //_AddOutputParam(_mslGSOutputParams, tkn___dc_instanceCoords, intType, tkn_flat, tkn_gs_instanceCoords).usage |= HdSt_CodeGenMSL::TParam::DrawingCoord;
        }
        _AddInputParam(_mslPSInputParams, tkn___dc_instanceCoords, intType, tkn_flat, HdBinding(HdBinding::UNKNOWN, 0), 0, tkn_gs_instanceCoords).usage |= HdSt_CodeGenMSL::TParam::DrawingCoord;
    }
    
    _genVS << "hd_drawingCoord thread &GetDrawingCoord() { return vsDrawingCoord; } \n";
    _genVS << "void _GetDrawingCoord(hd_drawingCoord thread &dc) { \n"
           << "  dc.modelCoord     = drawingCoord0.x; \n"
           << "  dc.constantCoord  = drawingCoord0.y; \n"
           << "  dc.elementCoord   = drawingCoord0.z; \n"
           << "  dc.primitiveCoord = drawingCoord0.w; \n"
           << "  dc.fvarCoord      = drawingCoord1.x; \n"
           << "  dc.shaderCoord    = drawingCoord1.z; \n"
           << "  dc.vertexCoord    = drawingCoord1.w; \n"
           << "  dc.topologyVisibilityCoord = drawingCoord2; \n"
           << "  hd_instanceIndex r = GetInstanceIndex();\n"
           << "  for(int i = 0; i < HD_INSTANCE_INDEX_WIDTH; ++i)\n"
           << "    dc.instanceIndex[i]  = r.indices[i];\n";

    if (_metaData.drawingCoordIBinding.binding.IsValid()) {
        _genVS << "  for (int i = 0; i < HD_INSTANCER_NUM_LEVELS; ++i) {\n"
               << "    dc.instanceCoords[i] = drawingCoordI[i] \n"
               << "      + dc.instanceIndex[i+1]; \n"
               << "  }\n";
    }

    _genVS << "  return;\n"
           << "}\n";

    // note: GL spec says tessellation input array size must be equal to
    //       gl_MaxPatchVertices, which is used for intrinsic declaration
    //       of built-in variables:
    //       in gl_PerVertex {} gl_in[gl_MaxPatchVertices];

//    // tess control shader
//    _genTCS << "flat in hd_drawingCoord vsDrawingCoord[gl_MaxPatchVertices];\n"
//            << "flat out hd_drawingCoord tcsDrawingCoord[HD_NUM_PATCH_VERTS];\n"
//            << "hd_drawingCoord GetDrawingCoord() { \n"
//            << "  hd_drawingCoord dc = vsDrawingCoord[gl_InvocationID];\n"
//            << "  dc.primitiveCoord += gl_PrimitiveID;\n"
//            << "  return dc;\n"
//            << "}\n";
//    // tess eval shader
//    _genTES << "flat in hd_drawingCoord tcsDrawingCoord[gl_MaxPatchVertices];\n"
//            << "flat out hd_drawingCoord vsDrawingCoord;\n"
//            << "flat out hd_drawingCoord gsDrawingCoord;\n"
//            << "hd_drawingCoord GetDrawingCoord() { \n"
//            << "  hd_drawingCoord dc = tcsDrawingCoord[0]; \n"
//            << "  dc.primitiveCoord += gl_PrimitiveID; \n"
//            << "  return dc;\n"
//            << "}\n";

    // geometry shader ( VSdc + gl_PrimitiveIDIn )
    _genGS << "hd_drawingCoord gsDrawingCoordCached;\n"
           << "void CacheDrawingCoord() {\n"
           << "  _GetDrawingCoord(gsDrawingCoordCached);\n"
           << "}\n"
           << "hd_drawingCoord thread &GetDrawingCoord() { return gsDrawingCoordCached; }\n"
           << "void _GetDrawingCoord(hd_drawingCoord thread &dc) { \n"
           << "  dc = vsDrawingCoord[0]; \n"
           << "  dc.primitiveCoord += gl_PrimitiveIDIn; \n"
           << "  return; \n"
           << "}\n";

    // fragment shader ( VSdc + gl_PrimitiveID )
    // note that gsDrawingCoord isn't offsetted by gl_PrimitiveIDIn
    _genFS << "hd_drawingCoord gsDrawingCoord;\n"
           << "hd_drawingCoord gsDrawingCoordCached;\n"
           << "void CacheDrawingCoord() {\n"
           << "  _GetDrawingCoord(gsDrawingCoordCached);\n"
           << "}\n"
           << "hd_drawingCoord thread &GetDrawingCoord() { return gsDrawingCoordCached; }\n"
           << "void _GetDrawingCoord(hd_drawingCoord thread &dc) { \n"
           << "  dc = gsDrawingCoord; \n"
           << "  dc.primitiveCoord += gl_PrimitiveID; \n"
           << "  return; \n"
           << "}\n";

    // drawing coord plumbing.
    // Note that copying from [0] for multiple input source since the
    // drawingCoord is flat (no interpolation required).
    _procVS  << "    _GetDrawingCoord(vsDrawingCoord);\n";
             //<< "    gsDrawingCoord = vsDrawingCoord;\n";
//    _procTCS << "  tcsDrawingCoord[gl_InvocationID] = "
//             << "  vsDrawingCoord[gl_InvocationID];\n";
//    _procTES << "  vsDrawingCoord = tcsDrawingCoord[0];\n"
//             << "  gsDrawingCoord = tcsDrawingCoord[0];\n";
    _procGS  << "    gsDrawingCoord = vsDrawingCoord[0];\n";
    
    METAL_DEBUG_COMMENT(&_genCommon, "End _GenerateDrawingCoord Common\n"); //MTL_FIXME
    METAL_DEBUG_COMMENT(&_genVS,     "End _GenerateDrawingCoord VS\n"); //MTL_FIXME
    METAL_DEBUG_COMMENT(&_genFS,     "End _GenerateDrawingCoord FS\n"); //MTL_FIXME
}
void
HdSt_CodeGenMSL::_GenerateConstantPrimvar()
{
    /*
      // --------- constant data declaration ----------
      struct ConstantData0 {
          mat4 transform;
          mat4 transformInverse;
          mat4 instancerTransform[2];
          vec3 displayColor;
          vec4 primID;
      };
      // bindless
      layout (location=0) uniform ConstantData0 *constantData0;
      // not bindless
      layout (std430, binding=0) buffer {
          constantData0 constantData0[];
      };

      // --------- constant data accessors ----------
      mat4 HdGet_transform(int localIndex) {
          return constantData0[GetConstantCoord()].transform;
      }
      vec3 HdGet_displayColor(int localIndex) {
          return constantData0[GetConstantCoord()].displayColor;
      }

    */

    std::stringstream declarations;
    std::stringstream accessors;
    METAL_DEBUG_COMMENT(&declarations, "_GenerateConstantPrimvar()\n"); //MTL_FIXME
    METAL_DEBUG_COMMENT(&accessors,    "_GenerateConstantPrimvar()\n"); //MTL_FIXME
    
    TF_FOR_ALL (it, _metaData.constantData) {
        // note: _constantData has been sorted by offset in HdSt_ResourceBinder.
        // XXX: not robust enough, should consider padding and layouting rules
        // to match with the logic in HdInterleavedMemoryManager if we
        // want to use a layouting policy other than default padding.

        HdBinding binding = it->first;
        TfToken typeName(TfStringPrintf("ConstantData%d", binding.GetValue()));
        TfToken varName = it->second.blockName;

        {
            std::string ptrName = "*";
            ptrName += it->second.blockName;
            HdSt_CodeGenMSL::TParam in(TfToken(ptrName), typeName,
                TfToken(), TfToken(), HdSt_CodeGenMSL::TParam::Unspecified, binding);
            in.usage |= HdSt_CodeGenMSL::TParam::EntryFuncArgument | HdSt_CodeGenMSL::TParam::ProgramScope;
            
            _mslVSInputParams.push_back(in);
            _mslGSInputParams.push_back(in);
            _mslPSInputParams.push_back(in);
        }

        declarations << "struct " << typeName << " {\n";

        TF_FOR_ALL (dbIt, it->second.entries) {
            if (!TF_VERIFY(!dbIt->dataType.IsEmpty(),
                              "Unknown dataType for %s",
                              dbIt->name.GetText())) {
                continue;
            }

            declarations << "  " << _GetPackedType(dbIt->dataType, false)
                         << " " << dbIt->name;
            if (dbIt->arraySize > 1) {
                declarations << "[" << dbIt->arraySize << "]";
            }

            declarations << ";\n";

            _EmitStructAccessor(accessors, varName, dbIt->name, dbIt->dataType,
                                dbIt->arraySize, true,
                                "GetDrawingCoord().constantCoord");
        }
        declarations << "};\n"
                     << "device const " << typeName << " *" << varName << ";\n";
    }
    _genCommon << declarations.str()
               << accessors.str();
}

void
HdSt_CodeGenMSL::_GenerateInstancePrimvar()
{
    /*
      // --------- instance data declaration ----------
      // bindless
      layout (location=X) uniform vec4 *data;
      // not bindless
      layout (std430, binding=X) buffer buffer_X {
          vec4 data[];
      };

      // --------- instance data accessors ----------
      vec3 HdGet_translate(int localIndex=0) {
          return instanceData0[GetInstanceCoord()].translate;
      }
    */

    std::stringstream declarations;
    std::stringstream accessors;
    METAL_DEBUG_COMMENT(&declarations, "_GenerateInstancePrimvar() declarations\n"); //MTL_FIXME
    METAL_DEBUG_COMMENT(&accessors,    "_GenerateInstancePrimvar() accessors\n"); //MTL_FIXME
    
    struct LevelEntries {
        TfToken dataType;
        std::vector<int> levels;
    };
    std::map<TfToken, LevelEntries> nameAndLevels;

    TF_FOR_ALL (it, _metaData.instanceData) {
        HdBinding binding = it->first;
        TfToken const &dataType = it->second.dataType;
        int level = it->second.level;

        nameAndLevels[it->second.name].dataType = dataType;
        nameAndLevels[it->second.name].levels.push_back(level);

        std::stringstream _n;
        _n << it->second.name << "_" << level;
        TfToken name(_n.str());

        std::stringstream n;
        n << "GetDrawingCoord().instanceCoords[" << level << "]";

        // << layout (location=x) uniform float *translate_0;
        _EmitDeclarationPtr(declarations, name, dataType, TfToken(), binding);
        _AddInputPtrParam(_mslVSInputParams, name, dataType, TfToken(), binding);
        _AddInputPtrParam(_mslGSInputParams, name, dataType, TfToken(), binding);
        _AddInputPtrParam(_mslPSInputParams, name, dataType, TfToken(), binding);
        _EmitAccessor(accessors, name, dataType, binding, n.str().c_str());
    }

    /*
      accessor taking level as a parameter.
      note that instance primvar may or may not be defined for each level.
      we expect level is an unrollable constant to optimize out branching.

      vec3 HdGetInstance_translate(int level, vec3 defaultValue) {
          if (level == 0) return HdGet_translate_0();
          // level==1 is not defined. use default
          if (level == 2) return HdGet_translate_2();
          if (level == 3) return HdGet_translate_3();
          return defaultValue;
      }
    */
    TF_FOR_ALL (it, nameAndLevels) {
        accessors << _GetUnpackedType(it->second.dataType, false)
                  << " HdGetInstance_" << it->first << "(int level, "
                  << _GetUnpackedType(it->second.dataType, false)
                  << " defaultValue) {\n";
        TF_FOR_ALL (levelIt, it->second.levels) {
            accessors << "  if (level == " << *levelIt << ") "
                      << "return HdGet_" << it->first << "_" << *levelIt << "();\n";
        }
        
        accessors << "  return defaultValue;\n"
                  << "}\n";
    }

    /*
      common accessor, if the primvar is defined on the instancer but not
      the rprim.

      #if !defined(HD_HAS_translate)
      #define HD_HAS_translate 1
      vec3 HdGet_translate(int localIndex) {
          // 0 is the lowest level for which this is defined
          return HdGet_translate_0();
      }
      vec3 HdGet_translate() {
          return HdGet_translate(0);
      }
      #endif
    */
    TF_FOR_ALL (it, nameAndLevels) {
        accessors << "#if !defined(HD_HAS_" << it->first << ")\n"
                  << "#define HD_HAS_" << it->first << " 1\n"
                  << _GetUnpackedType(it->second.dataType, false)
                  << " HdGet_" << it->first << "(int localIndex) {\n"
                  << "  return HdGet_" << it->first << "_"
                                       << it->second.levels.front() << "();\n"
                  << "}\n"
                  << _GetUnpackedType(it->second.dataType, false)
                  << " HdGet_" << it->first << "() { return HdGet_"
                  << it->first << "(0); }\n"
                  << "#endif\n";
    }
    
    METAL_DEBUG_COMMENT(&declarations, "End _GenerateInstancePrimvar() declarations\n"); //MTL_FIXME
    METAL_DEBUG_COMMENT(&accessors,    "End _GenerateInstancePrimvar() accessors\n"); //MTL_FIXME


    _genCommon << declarations.str()
               << accessors.str();
}

void
HdSt_CodeGenMSL::_GenerateElementPrimvar()
{
    /*
    Accessing uniform primvar data:
    ===============================
    Uniform primvar data is authored at the subprimitive (also called element or
    face below) granularity.
    To access uniform primvar data (say color), there are two indirections in
    the lookup because of aggregation in the buffer layout.
          ----------------------------------------------------
    color | prim0 colors | prim1 colors | .... | primN colors|
          ----------------------------------------------------
    For each prim, GetDrawingCoord().elementCoord holds the start index into
    this buffer.

    For an unrefined prim, the subprimitive ID s simply the gl_PrimitiveID.
    For a refined prim, gl_PrimitiveID corresponds to the refined element ID.

    To map a refined face to its coarse face, Storm builds a "primitive param"
    buffer (more details in the section below). This buffer is also aggregated,
    and for each subprimitive, GetDrawingCoord().primitiveCoord gives us the
    index into this buffer (meaning it has already added the gl_PrimitiveID)

    To have a single codepath for both cases, we build the primitive param
    buffer for unrefined prims as well, and effectively index the uniform
    primvar using:
    drawCoord.elementCoord + primitiveParam[ drawCoord.primitiveCoord ]

    The code generated looks something like:

      // --------- primitive param declaration ----------
      struct PrimitiveData { int elementID; }
      layout (std430, binding=?) buffer PrimitiveBuffer {
          PrimitiveData primitiveData[];
      };

      // --------- indirection accessors ---------
      // Gives us the "coarse" element ID
      int GetElementID() {
          return primitiveData[GetPrimitiveCoord()].elementID;
      }
      
      // Adds the offset to the start of the uniform primvar data for the prim
      int GetAggregatedElementID() {
          return GetElementID() + GetDrawingCoord().elementCoord;\n"
      }

      // --------- uniform primvar declaration ---------
      struct ElementData0 {
          vec3 displayColor;
      };
      layout (std430, binding=?) buffer buffer0 {
          ElementData0 elementData0[];
      };

      // ---------uniform primvar data accessor ---------
      vec3 HdGet_displayColor(int localIndex) {
          return elementData0[GetAggregatedElementID()].displayColor;
      }

    */

    // Primitive Param buffer layout:
    // ==============================
    // Depending on the prim, one of following is used:
    // 
    // 1. basis curves
    //     1 int  : curve index 
    //     
    //     This lets us translate a basis curve segment to its curve id.
    //     A basis curve is made up for 'n' curves, each of which have a varying
    //     number of segments.
    //     (see hdSt/basisCurvesComputations.cpp)
    //     
    // 2. mesh specific
    // a. tris
    //     1 int  : coarse face index + edge flag
    //     (see hd/meshUtil.h,cpp)
    //     
    // b. quads coarse
    //     2 ints : coarse face index + edge flag
    //              ptex index
    //     (see hd/meshUtil.h,cpp)
    //
    // c. tris & quads uniformly refined
    //     3 ints : coarse face index + edge flag
    //              Far::PatchParam::field0 (includes ptex index)
    //              Far::PatchParam::field1
    //     (see hdSt/subdivision3.cpp)
    //
    // d. patch adaptively refined
    //     4 ints : coarse face index + edge flag
    //              Far::PatchParam::field0 (includes ptex index)
    //              Far::PatchParam::field1
    //              sharpness (float)
    //     (see hdSt/subdivision3.cpp)
    // -----------------------------------------------------------------------
    // note: decoding logic of primitiveParam has to match with
    // HdMeshTopology::DecodeFaceIndexFromPrimitiveParam()
    //
    // PatchParam is defined as ivec3 (see opensubdiv/far/patchParam.h)
    //  Field0     | Bits | Content
    //  -----------|:----:|---------------------------------------------------
    //  faceId     | 28   | the faceId of the patch (Storm uses ptexIndex)
    //  transition | 4    | transition edge mask encoding
    //
    //  Field1     | Bits | Content
    //  -----------|:----:|---------------------------------------------------
    //  level      | 4    | the subdivision level of the patch
    //  nonquad    | 1    | whether the patch is the child of a non-quad face
    //  unused     | 3    | unused
    //  boundary   | 4    | boundary edge mask encoding
    //  v          | 10   | log2 value of u parameter at first patch corner
    //  u          | 10   | log2 value of v parameter at first patch corner
    //
    //  Field2     (float)  sharpness
    //
    // whereas adaptive patches have PatchParams computed by OpenSubdiv,
    // we need to construct PatchParams for coarse tris and quads.
    // Currently it's enough to fill just faceId for coarse quads for
    // ptex shading.

    std::stringstream declarations;
    std::stringstream accessors;
    
    METAL_DEBUG_COMMENT(&declarations, "_GenerateElementPrimvar() declarations \n"); //MTL_FIXME
    METAL_DEBUG_COMMENT(&accessors,    "_GenerateElementPrimvar() accessors\n"); //MTL_FIXME

    if (_metaData.primitiveParamBinding.binding.IsValid()) {
        const HdSt_ResourceBinder::MetaData::BindingDeclaration& primParamBinding = _metaData.primitiveParamBinding;
        _EmitDeclarationPtr(declarations, primParamBinding);
        TParam& entryPS(_AddInputPtrParam(_mslPSInputParams, primParamBinding));
        entryPS.usage |= TParam::EntryFuncArgument;
        TParam& entryGS(_AddInputPtrParam(_mslGSInputParams, primParamBinding));
        entryGS.usage |= TParam::EntryFuncArgument;

        _EmitAccessor(  accessors,
                        primParamBinding.name, primParamBinding.dataType, primParamBinding.binding,
                        "GetDrawingCoord().primitiveCoord");

        if (_geometricShader->IsPrimTypePoints()) {
            // do nothing. 
            // e.g. if a prim's geomstyle is points and it has a valid
            // primitiveParamBinding, we don't generate any of the 
            // accessor methods.
            ;            
        }
        else if (_geometricShader->IsPrimTypeBasisCurves()) {
            // straight-forward indexing to get the segment's curve id
            accessors
                << "int GetElementID() {\n"
                << "  return (hd_int_get(HdGet_primitiveParam()));\n"
                << "}\n";
            accessors
                << "int GetAggregatedElementID() {\n"
                << "  return GetElementID()\n"
                << "  + GetDrawingCoord().elementCoord;\n"
                << "}\n";
        }
        else if (_geometricShader->IsPrimTypeMesh()) {
            // GetPatchParam, GetEdgeFlag
            switch (_geometricShader->GetPrimitiveType()) {
                case HdSt_GeometricShader::PrimitiveType::PRIM_MESH_REFINED_QUADS:
                case HdSt_GeometricShader::PrimitiveType::PRIM_MESH_REFINED_TRIANGLES:
                {
                    // refined quads or tris (loop subdiv)
                    accessors
                        << "ivec3 GetPatchParam() {\n"
                        << "  return ivec3(HdGet_primitiveParam().y, \n"
                        << "               HdGet_primitiveParam().z, 0);\n"
                        << "}\n";
                    accessors
                        << "int GetEdgeFlag(int localIndex) {\n"
                        << "  return (HdGet_primitiveParam().x & 3);\n"
                        << "}\n";
                    break;
                }

                case HdSt_GeometricShader::PrimitiveType::PRIM_MESH_BSPLINE:
                case HdSt_GeometricShader::PrimitiveType::PRIM_MESH_BOXSPLINETRIANGLE:
                {
                    // refined patches (tessellated triangles)
                    accessors
                        << "ivec3 GetPatchParam() {\n"
                        << "  return ivec3(HdGet_primitiveParam().y, \n"
                        << "               HdGet_primitiveParam().z, \n"
                        << "               HdGet_primitiveParam().w);\n"
                        << "}\n";
                    accessors
                        << "int GetEdgeFlag(int localIndex) {\n"
                        << "  return localIndex;\n"
                        << "}\n";
                    break;
                }

                case HdSt_GeometricShader::PrimitiveType::PRIM_MESH_COARSE_QUADS:
                {
                    // coarse quads (for ptex)
                    // put ptexIndex into the first element of PatchParam.
                    // (transition flags in MSB can be left as 0)
                    accessors
                        << "ivec3 GetPatchParam() {\n"
                        << "  return ivec3(HdGet_primitiveParam().y, 0, 0);\n"
                        << "}\n";
                    accessors
                        << "int GetEdgeFlag(int localIndex) {\n"
                        << "  return localIndex; \n"
                        << "}\n";
                    break;
                }

                case HdSt_GeometricShader::PrimitiveType::PRIM_MESH_COARSE_TRIANGLES:
                {
                    // coarse triangles                
                    // note that triangulated meshes don't have ptexIndex.
                    // Here we're passing primitiveID as ptexIndex PatchParam
                    // since HdSt_TriangulateFaceVaryingComputation unrolls facevaring
                    // primvars for each triangles.
                    accessors
                        << "ivec3 GetPatchParam() {\n"
                        << "  return ivec3(gl_PrimitiveID, 0, 0);\n"
                        << "}\n";
                    accessors
                        << "int GetEdgeFlag(int localIndex) {\n"
                        << "  return HdGet_primitiveParam() & 3;\n"
                        << "}\n";
                    break;
                }

                default:
                {
                    TF_CODING_ERROR("HdSt_GeometricShader::PrimitiveType %d is "
                      "unexpected in _GenerateElementPrimvar().",
                      (int)_geometricShader->GetPrimitiveType());
                }
            }

            // GetFVarIndex
            if (_geometricShader->IsPrimTypeTriangles() ||
                (_geometricShader->GetPrimitiveType() ==
                 HdSt_GeometricShader::PrimitiveType::PRIM_MESH_BOXSPLINETRIANGLE)) {
                // note that triangulated meshes don't have ptexIndex.
                // Here we're passing primitiveID as ptexIndex PatchParam
                // since HdSt_TriangulateFaceVaryingComputation unrolls facevaring
                // primvars for each triangles.
                accessors
                    << "int GetFVarIndex(int localIndex) {\n"
                    << "  int fvarCoord = GetDrawingCoord().fvarCoord;\n"
                    << "  int ptexIndex = GetPatchParam().x & 0xfffffff;\n"
                    << "  return fvarCoord + ptexIndex * 3 + localIndex;\n"
                    << "}\n";    
            }
            else {
                accessors
                    << "int GetFVarIndex(int localIndex) {\n"
                    << "  int fvarCoord = GetDrawingCoord().fvarCoord;\n"
                    << "  int ptexIndex = GetPatchParam().x & 0xfffffff;\n"
                    << "  return fvarCoord + ptexIndex * 4 + localIndex;\n"
                    << "}\n";
            }

            // ElementID getters
            accessors
                << "int GetElementID() {\n"
                << "  return (hd_int_get(HdGet_primitiveParam()) >> 2);\n"
                << "}\n";

            accessors
                << "int GetAggregatedElementID() {\n"
                << "  return GetElementID()\n"
                << "  + GetDrawingCoord().elementCoord;\n"
                << "}\n";
        }
        else {
            TF_CODING_ERROR("HdSt_GeometricShader::PrimitiveType %d is "
                  "unexpected in _GenerateElementPrimvar().",
                  (int)_geometricShader->GetPrimitiveType());
        }
    } else {
        // no primitiveParamBinding

        // XXX: this is here only to keep the compiler happy, we don't expect
        // users to call them -- we really should restructure whatever is
        // necessary to avoid having to do this and thus guarantee that users
        // can never call bogus versions of these functions.
        
        // Use a fallback of -1, so that points aren't selection highlighted
        // when face 0 is selected. This would be the case if we returned 0,
        // since the selection highlighting code is repr-agnostic.
        // It is safe to do this for points, since  we don't generate accessors
        // for element primvars, and thus don't use it as an index into
        // elementCoord.
        if (_geometricShader->IsPrimTypePoints()) {
            accessors
            << "int GetElementID() {\n"
            << "  return -1;\n"
            << "}\n";
        } else {
            accessors
            << "int GetElementID() {\n"
            << "  return 0;\n"
            << "}\n";
        }

        accessors
            << "int GetAggregatedElementID() {\n"
            << "  return GetElementID();\n"
            << "}\n";
        accessors
            << "int GetEdgeFlag(int localIndex) {\n"
            << "  return 0;\n"
            << "}\n";
        accessors
            << "ivec3 GetPatchParam() {\n"
            << "  return ivec3(0, 0, 0);\n"
            << "}\n";
        accessors
            << "int GetFVarIndex(int localIndex) {\n"
            << "  return 0;\n"
            << "}\n";
    }
    
    if (_metaData.edgeIndexBinding.binding.IsValid()) {
        
        HdBinding binding = _metaData.edgeIndexBinding.binding;
        
        _EmitDeclarationPtr(declarations, _metaData.edgeIndexBinding);
        _AddInputPtrParam(_mslPSInputParams, _metaData.edgeIndexBinding);
        TParam& entryGS(_AddInputPtrParam(_mslGSInputParams, _metaData.edgeIndexBinding));
        entryGS.usage |= TParam::EntryFuncArgument;
        
        _EmitAccessor(accessors, _metaData.edgeIndexBinding.name,
                      _metaData.edgeIndexBinding.dataType, binding,
                      "GetDrawingCoord().primitiveCoord");
        
        // Authored EdgeID getter
        // abs() is needed below, since both branches may get executed, and
        // we need to guard against array oob indexing.
        accessors
            << "int GetAuthoredEdgeId(int primitiveEdgeID) {\n"
            << "  if (primitiveEdgeID == -1) {\n"
            << "    return -1;\n"
            << "  }\n"
            << "  "
            << _GetUnpackedType(_metaData.edgeIndexBinding.dataType, false)
            << " edgeIndices = HdGet_edgeIndices();\n"
            << "  int coord = abs(primitiveEdgeID);\n"
            << "  return edgeIndices[coord];\n"
            << "}\n";
        
        // Primitive EdgeID getter
        if (_geometricShader->IsPrimTypePoints()) {
            // we get here only if we're rendering a mesh with the edgeIndices
            // binding and using a points repr. since there is no GS stage, we
            // generate fallback versions.
            // note: this scenario can't be handled in meshShaderKey, since it
            // doesn't know whether an edgeIndices binding exists.
            accessors
                << "int GetPrimitiveEdgeId() {\n"
                << "  return -1;\n"
                << "}\n";
            accessors
                << "bool IsFragmentOnEdge() {\n"
                << "  return false;\n"
                << "}\n";
        }
        else if (_geometricShader->IsPrimTypeBasisCurves()) {
            // basis curves don't have an edge indices buffer bound, so we
            // shouldn't ever get here.
            TF_VERIFY(false, "edgeIndexBinding shouldn't be found on a "
                      "basis curve");
        }
        else if (_geometricShader->IsPrimTypeMesh()) {
            // nothing to do. meshShaderKey takes care of it.
        }
    } else {
        // The functions below are used in picking (id render) and selection
        // highlighting, and are expected to be defined. Generate fallback
        // versions when we don't bind an edgeIndices buffer.
        accessors
            << "int GetAuthoredEdgeId(int primitiveEdgeID) {\n"
            << "  return -1;\n"
            << "}\n";
        accessors
            << "int GetPrimitiveEdgeId() {\n"
            << "  return -1;\n"
            << "}\n";
        accessors
            << "bool IsFragmentOnEdge() {\n"
            << "  return false;\n"
            << "}\n";
            accessors
            << "float GetSelectedEdgeOpacity() {\n"
            << "  return 0.0;\n"
            << "}\n";
    }

    if (!_geometricShader->IsPrimTypePoints()) {
        TF_FOR_ALL (it, _metaData.elementData) {
            HdBinding binding = it->first;
            TfToken const &name = it->second.name;
            TfToken dataType = _GetPackedType(it->second.dataType, false);
                
            if (it->second.dataType == _tokens->packed_2_10_10_10) {
                dataType = _tokens->packed_2_10_10_10;
            }

            // MTL_FIXME - changing from VS Input params to PS because none of this appaears to be associated with vertex shaders at all... (so possibly nothing to fix)
            _EmitDeclarationPtr(declarations, name, dataType, TfToken(), binding);
            _AddInputPtrParam(_mslPSInputParams, name, dataType, TfToken(), binding);
            TParam& entryGS(_AddInputPtrParam(_mslGSInputParams, name, dataType, TfToken(), binding));
            entryGS.usage |= TParam::EntryFuncArgument;
            
            // AggregatedElementID gives us the buffer index post batching, which
            // is what we need for accessing element (uniform) primvar data.
            _EmitAccessor(accessors, name, dataType, binding,"GetAggregatedElementID()");
        }
    }
    
    METAL_DEBUG_COMMENT(&declarations, "End _GenerateElementPrimvar() declarations \n"); //MTL_FIXME
    METAL_DEBUG_COMMENT(&accessors,    "End _GenerateElementPrimvar() accessors\n"); //MTL_FIXME

    // Emit primvar declarations and accessors.
    _genTCS << declarations.str()
            << accessors.str();
    _genTES << declarations.str()
            << accessors.str();
    _genGS << declarations.str()
           << accessors.str();
    _genFS << declarations.str()
           << accessors.str();
}

void
HdSt_CodeGenMSL::_GenerateVertexAndFaceVaryingPrimvar(bool hasGS)
{
    // Vertex and FVar primvar flow into the fragment shader as per-fragment
    // attribute data that has been interpolated by the rasterizer, and hence
    // have similarities for code gen.
    // While vertex primvar are authored per vertex and require plumbing
    // through all shader stages, fVar is emitted only in the GS stage.
    /*
      // --------- vertex data declaration (VS) ----------
      layout (location = 0) in vec3 normals;
      layout (location = 1) in vec3 points;

      struct Primvars {
          vec3 normals;
          vec3 points;
      };

      void ProcessPrimvars() {
          outPrimvars.normals = normals;
          outPrimvars.points = points;
      }

      // --------- geometry stage plumbing -------
      in Primvars {
          vec3 normals;
          vec3 points;
      } inPrimvars[];
      out Primvars {
          vec3 normals;
          vec3 points;
      } outPrimvars;

      void ProcessPrimvars(int index) {
          outPrimvars = inPrimvars[index];
      }

      // --------- vertex data accessors (used in geometry/fragment shader) ---
      in Primvars {
          vec3 normals;
          vec3 points;
      } inPrimvars;
      vec3 HdGet_normals(int localIndex=0) {
          return inPrimvars.normals;
      }
    */

    std::stringstream vertexInputs;
    std::stringstream interstageStruct;
    std::stringstream accessorsVS, accessorsTCS, accessorsTES,
        accessorsGS, accessorsFS;
    
    METAL_DEBUG_COMMENT(&interstageStruct,"_GenerateVertexPrimvar() interstageStruct\n"); //MTL_FIXME
    METAL_DEBUG_COMMENT(&vertexInputs,    "_GenerateVertexPrimvar() vertexInputs\n"); //MTL_FIXME
    METAL_DEBUG_COMMENT(&accessorsVS,     "_GenerateVertexPrimvar() accessorsVS\n"); //MTL_FIXME
    METAL_DEBUG_COMMENT(&accessorsFS,     "_GenerateVertexPrimvar() accessorsFS\n"); //MTL_FIXME
    
    
    TfToken structName("Primvars");
    interstageStruct << "struct " << structName << " {\n";
    
    // vertex varying
    TF_FOR_ALL (it, _metaData.vertexData) {
        HdBinding binding = it->first;
        TfToken const &name = it->second.name;
        TfToken const &dataType = it->second.dataType;

        _EmitDeclaration(vertexInputs, name, dataType, TfToken(), binding);
        
        {
            std::stringstream vtxOutName;
            vtxOutName << MTL_PRIMVAR_PREFIX << name;
            TfToken vtxOutName_Token(vtxOutName.str());
            
            _AddInputParam(_mslVSInputParams, name, _GetPackedType(dataType, false), TfToken(), binding);
            {
                TParam &param = _AddOutputParam(
                    _mslVSOutputParams, vtxOutName_Token, dataType);
                param.accessorStr = name;
                param.usage |= HdSt_CodeGenMSL::TParam::Usage::VPrimVar;
            }
            
            std::string inAccessorGS = "inPrimvars[i].";
            inAccessorGS += name.GetString();
            _AddInputParam(_mslGSInputParams, vtxOutName_Token, dataType, TfToken(), HdBinding(HdBinding::UNKNOWN, 0), 0, TfToken(inAccessorGS)).usage
                |= HdSt_CodeGenMSL::TParam::Usage::VPrimVar;
            std::string outAccessorGS = "outPrimvars.";
            outAccessorGS += name.GetString();
            {
                TParam &param = _AddOutputParam(_mslGSOutputParams, vtxOutName_Token, dataType);
                param.accessorStr = TfToken(outAccessorGS);
                param.usage |= HdSt_CodeGenMSL::TParam::Usage::VPrimVar;
            }
            
            _AddInputParam(_mslPSInputParams, name, dataType, TfToken(), HdBinding(HdBinding::UNKNOWN, 0), 0, vtxOutName_Token).usage |= HdSt_CodeGenMSL::TParam::Usage::VPrimVar;
        }

        interstageStruct << "  " << dataType << " " << name << ";\n";

        // primvar accessors
        _EmitAccessor(accessorsVS, name, dataType, binding);

        TfToken readStructName(std::string("in") + structName.GetString());
        _EmitStructAccessor(accessorsTCS, readStructName,
                            name, dataType, /*arraySize=*/1, false, "gl_InvocationID");
        _EmitStructAccessor(accessorsTES, readStructName,
                            name, dataType, /*arraySize=*/1, false, "localIndex");
        _EmitStructAccessor(accessorsGS,  readStructName,
                            name, dataType, /*arraySize=*/1, false, "localIndex");
        _EmitStructAccessor(accessorsFS,  readStructName,
                            name, dataType, /*arraySize=*/1, false);

        // interstage plumbing
        _procVS << "  outPrimvars." << name
                << " = " << name << ";\n";
        _procTCS << "  outPrimvars[gl_InvocationID]." << name
                 << " = inPrimvars[gl_InvocationID]." << name << ";\n";
        // procTES linearly interpolate vertex/varying primvars here.
        // XXX: needs smooth interpolation for vertex primvars?
        _procTES << "  outPrimvars." << name
                 << " = mix(mix(inPrimvars[i3]." << name
                 << "         , inPrimvars[i2]." << name << ", u),"
                 << "       mix(inPrimvars[i1]." << name
                 << "         , inPrimvars[i0]." << name << ", u), v);\n";
        
        _procGS  << "    // MTL_HINT EXPORTS:outPrimvars." << name << " PASSTHROUGH\n"
                 << "    outPrimvars." << name
                 << " = inPrimvars[index]." << name << ";\n";
    }
    
    /*
      // --------- facevarying data declaration ----------------
      // we use separate structs to avoid std430 padding problem of vec3 array.
      struct FaceVaryingData0 {
          vec2 map1;
      };
      struct FaceVaryingData1 {
          float map2_u;
      };
      layout (std430, binding=?) buffer buffer0 {
          FaceVaryingData0 faceVaryingData0[];
      };
      layout (std430, binding=?) buffer buffer1 {
          FaceVaryingData1 faceVaryingData1[];
      };

      // --------- geometry stage plumbing -------
      void ProcessPrimvars(int index) {
          outPrimvars = inPrimvars[index];
      }

      // --------- facevarying data accessors ----------
      // in geometry shader
      vec2 HdGet_map1(int localIndex) {
          return faceVaryingData0[GetFaceVaryingIndex(localIndex)].map1;
      }
      // in fragment shader
      vec2 HdGet_map1() {
          return inPrimvars.map1;
      }

    */

    // face varying
    std::stringstream fvarDeclarations;

    if (hasGS) {
        TF_FOR_ALL (it, _metaData.fvarData) {
            HdBinding binding = it->first;
            TfToken const &name = it->second.name;
            std::string dataType = _GetPackedType(it->second.dataType, false).GetString();

            interstageStruct << "  " << dataType << " " << name << ";\n";
            
            // primvar accessors (only in GS and FS)
            TfToken readStructName(std::string("in") + structName.GetString());
            _EmitAccessor(accessorsGS, name, TfToken(dataType), binding, "GetFVarIndex(localIndex)");
            _EmitStructAccessor(accessorsFS, readStructName, name, TfToken(dataType),
                                /*arraySize=*/1, false, NULL);

            _EmitDeclarationPtr(fvarDeclarations, name, TfToken(_GetPackedMSLType(dataType)), TfToken(), binding);

//            // interstage plumbing
//            _procVS << "  outPrimvars." << name
//                    << " = " << name << ";\n";
//            _procTCS << "  outPrimvars[gl_InvocationID]." << name
//                     << " = inPrimvars[gl_InvocationID]." << name << ";\n";
//            // TODO: facevarying tessellation
//            _procTES << "  outPrimvars->" << name
//                     << " = mix(mix(inPrimvars[i3]." << name
//                     << "         , inPrimvars[i2]." << name << ", u),"
//                     << "       mix(inPrimvars[i1]." << name
//                     << "         , inPrimvars[i0]." << name << ", u), v);\n";


            switch(_geometricShader->GetPrimitiveType())
            {
                case HdSt_GeometricShader::PrimitiveType::PRIM_MESH_COARSE_QUADS:
                case HdSt_GeometricShader::PrimitiveType::PRIM_MESH_REFINED_QUADS:
                case HdSt_GeometricShader::PrimitiveType::PRIM_MESH_BSPLINE:
                {
                    // linear interpolation within a quad.
                    _procGS << "    // MTL_HINT EXPORTS:outPrimvars." << name << "\n"
                            << "    outPrimvars." << name
                            << "  = mix("
                            << "mix(" << "HdGet_" << name << "(0),"
                            <<           "HdGet_" << name << "(1), localST.x),"
                            << "mix(" << "HdGet_" << name << "(3),"
                            <<           "HdGet_" << name << "(2), localST.x), localST.y);\n";
                    break;
                }

                case HdSt_GeometricShader::PrimitiveType::PRIM_MESH_REFINED_TRIANGLES:
                case HdSt_GeometricShader::PrimitiveType::PRIM_MESH_COARSE_TRIANGLES:
                case HdSt_GeometricShader::PrimitiveType::PRIM_MESH_BOXSPLINETRIANGLE:
                {
                    // barycentric interpolation within a triangle.
                    _procGS << "    // MTL_HINT EXPORTS:outPrimvars." << name << "\n"
                            << "    outPrimvars." << name
                            << "  = HdGet_" << name << "(0) * (1-localST.x-localST.y) "
                            << "  + HdGet_" << name << "(1) * localST.x "
                            << "  + HdGet_" << name << "(2) * localST.y;\n";
                    break;
                }

                case HdSt_GeometricShader::PrimitiveType::PRIM_POINTS:
                {
                    // do nothing.
                    // e.g. if a prim's geomstyle is points and it has valid
                    // fvarData, we don't generate any of the
                    // accessor methods.
                    break;
                }

                default:
                    TF_CODING_ERROR("Face varing bindings for unexpected for"
                                    " HdSt_GeometricShader::PrimitiveType %d",
                                    (int)_geometricShader->GetPrimitiveType());
            }
            
            {
                std::stringstream vtxOutName;
                vtxOutName << MTL_PRIMVAR_PREFIX << name;

                TfToken vtxOutName_Token(vtxOutName.str());
                std::string outAccessorGS = "outPrimvars.";
                outAccessorGS += name.GetString();
                std::string inAccessorPS = "inPrimvars.";
                inAccessorPS += name.GetString();

                _AddInputPtrParam(_mslGSInputParams, name, TfToken(dataType), TfToken(), HdBinding(HdBinding::UNKNOWN, 0)).usage |= HdSt_CodeGenMSL::TParam::Usage::FPrimVar;
                TParam &param = _AddOutputParam(_mslGSOutputParams, vtxOutName_Token, TfToken(dataType));
                param.accessorStr = TfToken(outAccessorGS);
                param.usage |= HdSt_CodeGenMSL::TParam::Usage::FPrimVar;
                
                _AddInputParam(_mslPSInputParams, name, TfToken(dataType), TfToken(), HdBinding(HdBinding::UNKNOWN, 0), 0, TfToken(inAccessorPS)).usage
                    |= HdSt_CodeGenMSL::TParam::Usage::FPrimVar;
            }
        }
    }

    interstageStruct << "}";
    
    METAL_DEBUG_COMMENT(&interstageStruct,"End _GenerateVertexPrimvar() interstageStruct\n"); //MTL_FIXME
    METAL_DEBUG_COMMENT(&vertexInputs,    "End _GenerateVertexPrimvar() vertexInputs\n"); //MTL_FIXME
    METAL_DEBUG_COMMENT(&accessorsVS,     "End _GenerateVertexPrimvar() accessorsVS\n"); //MTL_FIXME
    METAL_DEBUG_COMMENT(&accessorsFS,     "End _GenerateVertexPrimvar() accessorsFS\n"); //MTL_FIXME

    _genVS << fvarDeclarations.str()
           << vertexInputs.str()
           << interstageStruct.str()
           << " outPrimvars;\n"
           << accessorsVS.str();

    _genTCS << interstageStruct.str()
            << " inPrimvars[gl_MaxPatchVertices];\n"
            << interstageStruct.str()
            << " outPrimvars[HD_NUM_PATCH_VERTS];\n"
            << accessorsTCS.str();

    _genTES << interstageStruct.str()
            << " inPrimvars[gl_MaxPatchVertices];\n"
            << interstageStruct.str()
            << " outPrimvars;\n"
            << accessorsTES.str();

    _genGS << fvarDeclarations.str()
           << interstageStruct.str() << ";\n"
           << structName << " inPrimvars[HD_NUM_PRIMITIVE_VERTS];\n"
           << structName << " outPrimvars;\n"
           << accessorsGS.str();

    _genFS << interstageStruct.str()
           << " inPrimvars;\n"
           << accessorsFS.str();

    // ---------
    _genFS << "vec4 GetPatchCoord() { return GetPatchCoord(0); }\n";

    //_genGS << "vec4 GetPatchCoord(int localIndex);\n";  ..... No 
    
    // VS specific accessor for the "vertex drawing coordinate"
    // Even though we currently always plumb vertexCoord as part of the drawing
    // coordinate, we expect clients to use this accessor when querying the base
    // vertex offset for a draw call.
    _genVS << "int GetBaseVertexOffset() {\n";
    _genVS << "  return gl_BaseVertex;\n";
    _genVS << "}\n";
}

void
HdSt_CodeGenMSL::_GenerateShaderParameters()
{
    /*
      ------------- Declarations -------------

      // shader parameter buffer
      struct ShaderData {
          <type>          <name>;
          vec4            diffuseColor;     // fallback uniform
          sampler2D       kdTexture;        // uv texture    (bindless texture)
          sampler2DArray  ptexTexels;       // ptex texels   (bindless texture)
          isamplerBuffer  ptexLayouts;      // ptex layouts  (bindless texture)
      };

      // bindless buffer
      layout (location=0) uniform ShaderData *shaderData;
      // not bindless buffer
      layout (std430, binding=0) buffer {
          ShaderData shaderData[];
      };

      // non bindless textures
      uniform sampler2D      samplers_2d[N];
      uniform sampler2DArray samplers_2darray[N];
      uniform isamplerBuffer isamplerBuffers[N];

      ------------- Accessors -------------

      * fallback value
      <type> HdGet_<name>(int localIndex=0) {
          return shaderData[GetDrawingCoord().shaderCoord].<name>
      }

      * primvar redirect
      <type> HdGet_<name>(int localIndex=0) {
          return HdGet_<inPrimvars>().xxx;
      }

      * bindless 2D texture
      <type> HdGet_<name>(int localIndex=0) {
          return texture(sampler2D(shaderData[GetDrawingCoord().shaderCoord].<name>), <inPrimvars>).xxx;
      }

      * non-bindless 2D texture
      <type> HdGet_<name>(int localIndex=0) {
          return texture(samplers_2d[<offset> + drawIndex * <stride>], <inPrimvars>).xxx;
      }

      * bindless Ptex texture
      <type> HdGet_<name>(int localIndex=0) {
          return GlopPtexTextureLookup(<name>_Data, <name>_Packing, GetPatchCoord()).xxx;
      }

      * non-bindless Ptex texture
      <type> HdGet_<name>(int localIndex=0) {
          return GlopPtexTextureLookup(
              samplers_2darray[<offset_ptex_texels> + drawIndex * <stride>],
              isamplerBuffers[<offset_ptex_layouts> + drawIndex * <stride>],
              GetPatchCoord()).xxx;
      }

      * bindless Ptex texture with patchcoord
      <type> HdGet_<name>(vec4 patchCoord) {
          return GlopPtexTextureLookup(<name>_Data, <name>_Packing, patchCoord).xxx;
      }

      * non-bindless Ptex texture
      <type> HdGet_<name>(vec4 patchCoord) {
          return GlopPtexTextureLookup(
              samplers_2darray[<offset_ptex_texels> + drawIndex * <stride>],
              isamplerBuffers[<offset_ptex_layouts> + drawIndex * <stride>],
              patchCoord).xxx;
      }
     
     * transform2d
     vec2 HdGet_<name>(int localIndex=0) {
         float angleRad = HdGet_<name>_rotation() * 3.1415926f / 180.f;
         mat2 rotMat = mat2(cos(angleRad), sin(angleRad),
                            -sin(angleRad), cos(angleRad));
     #if defined(HD_HAS_<primvarName>)
         return vec2(HdGet_<name>_translation() + rotMat *
           (HdGet_<name>_scale() * HdGet_<primvarName>(localIndex)));
     #else
         int shaderCoord = GetDrawingCoord().shaderCoord;
         return vec2(HdGet_<name>_translation() + rotMat *
          (HdGet_<name>_scale() * shaderData[shaderCoord].<name>_fallback.xy));
     #endif
     }

    */

    std::stringstream declarations;
    std::stringstream accessors;

    METAL_DEBUG_COMMENT(&_genFS, "_GenerateShaderParameters()\n"); //MTL_FIXME
    METAL_DEBUG_COMMENT(&_genVS, "_GenerateShaderParameters()\n"); //MTL_FIXME

    
    GarchContextCaps const &caps = GarchResourceFactory::GetInstance()->GetContextCaps();

    TfToken typeName("ShaderData");
    TfToken varName("materialParams");
    
    // for shader parameters, we create declarations and accessors separetely.
    TF_FOR_ALL (it, _metaData.shaderData) {
        HdBinding binding = it->first;

        declarations << "#define float wrapped_float\n";
        declarations << "#define int wrapped_int\n";
        //declarations << "#undef vec3\n";
        //declarations << "#define vec3 packed_float3\n";
        declarations << "struct " << typeName << " {\n";

        TF_FOR_ALL (dbIt, it->second.entries) {
            declarations << "  " << _GetPackedType(dbIt->dataType, false)
                         << " " << dbIt->name
                         << ";\n";
            
        }
        declarations << "};\n";
        //declarations << "#undef vec3\n";
        //declarations << "#define vec3 float3\n";
        declarations << "#undef float\n";
        declarations << "#undef int\n";

        // for array delaration, SSBO and bindless uniform can use [].
        // UBO requires the size [N].
        // XXX: [1] is a hack to cheat driver not telling the actual size.
        //      may not work some GPUs.
        // XXX: we only have 1 shaderData entry (interleaved).
        int arraySize = (binding.GetType() == HdBinding::UBO) ? 1 : 0;
        _EmitDeclarationPtr(declarations, varName, typeName, TfToken(), binding, arraySize, true);
        _AddInputPtrParam(_mslVSInputParams, varName, typeName, TfToken(), binding, arraySize, true);
        _AddInputPtrParam(_mslGSInputParams, varName, typeName, TfToken(), binding, arraySize, true);
        _AddInputPtrParam(_mslPSInputParams, varName, typeName, TfToken(), binding, arraySize, true);
        break;
    }
    
    _genVS << declarations.str()
           << accessors.str();

    // Non-field redirect accessors.
    TF_FOR_ALL (it, _metaData.shaderParameterBinding) {

        bool dup = false;
        TF_FOR_ALL (sub_it, _metaData.shaderParameterBinding) {
            if (sub_it == it)
                break;
            
            if (it->second.name != sub_it->second.name)
                continue;
                
            dup = true;
            break;
        }

        if (dup) {
            continue;
        }
        // adjust datatype
        std::string swizzle = _GetSwizzleString(it->second.dataType,
                                                it->second.swizzle);
        bool addScalarAccessor = true;
        bool isTextureSource = false;

        HdBinding::Type bindingType = it->first.GetType();
        if (bindingType == HdBinding::FALLBACK) {
            if (swizzle != ".x")
                swizzle = "";
            // vec4 HdGet_name(int localIndex)
            accessors
                << _GetUnpackedType(it->second.dataType, false)
                << " HdGet_" << it->second.name << "(int localIndex) {\n"
                << "  int shaderCoord = GetDrawingCoord().shaderCoord; \n"
                << "  return "
                << _GetUnpackedType(_GetPackedTypeAccessor(it->second.dataType, false), false)
                << "(materialParams[shaderCoord]."
                << it->second.name << HdSt_ResourceBindingSuffixTokens->fallback
                << swizzle
                << ");\n"
                << "}\n";
            // vec4 HdGet_name()
            accessors
                << _GetUnpackedType(it->second.dataType, false)
                << " HdGet_" << it->second.name
                << "() { return HdGet_" << it->second.name << "(0); }\n";
        } else if (bindingType == HdBinding::BINDLESS_TEXTURE_2D) {
            TF_FATAL_CODING_ERROR("Not Implemented");

            _EmitTextureAccessors(
                accessors, it->second, swizzle,
                /* dim = */ 2,
                /* hasTextureTransform = */ false,
                /* hasTextureScaleAndBias = */ true,
                /* isBindless = */ true);
            
            isTextureSource = true;
        } else if (bindingType == HdBinding::TEXTURE_2D) {
            char const* textureStr = "texture";
            TfToken textureTypeStr = TfToken("texture2d<float>");
            if (it->second.name == TfToken("depthReadback")) {
                textureStr = "depth";
                textureTypeStr = TfToken("depth2d<float>");
            }

            declarations
                << "sampler samplerBind_" << it->second.name << ";\n"
                << textureStr << "2d<float> textureBind_" << it->second.name << ";\n";
            
            _AddInputParam(_mslPSInputParams, TfToken("samplerBind_" + it->second.name.GetString()), TfToken("sampler"), TfToken()).usage
                |= HdSt_CodeGenMSL::TParam::Sampler;
            _AddInputParam(_mslPSInputParams, TfToken("textureBind_" + it->second.name.GetString()), textureTypeStr, TfToken()).usage
                |= HdSt_CodeGenMSL::TParam::Texture;

            _EmitTextureAccessors(
                accessors, it->second, swizzle,
                /* dim = */ 2,
                /* hasTextureTransform = */ false,
                /* hasTextureScaleAndBias = */ true,
                /* isBindless = */ false);

            isTextureSource = true;
        } else if (bindingType == HdBinding::BINDLESS_TEXTURE_FIELD) {

            _EmitTextureAccessors(
                accessors, it->second, swizzle,
                /* dim = */ 3,
                /* hasTextureTransform = */ true,
                /* hasTextureScaleAndBias = */ false,
                /* isBindless = */ true);

            isTextureSource = true;
        } else if (bindingType == HdBinding::TEXTURE_FIELD) {
            declarations
                << "sampler samplerBind_" << it->second.name << ";\n"
                << "texture3d<float> textureBind_" << it->second.name << ";\n";
            
            _AddInputParam(_mslPSInputParams, TfToken("samplerBind_" + it->second.name.GetString()), TfToken("sampler"), TfToken()).usage
                |= HdSt_CodeGenMSL::TParam::Sampler;
            _AddInputParam(_mslPSInputParams, TfToken("textureBind_" + it->second.name.GetString()), TfToken("texture3d<float>"), TfToken()).usage
                |= HdSt_CodeGenMSL::TParam::Texture;
            
            _EmitTextureAccessors(
                accessors, it->second, swizzle,
                /* dim = */ 3,
                /* hasTextureTransform = */ true,
                /* hasTextureScaleAndBias = */ false,
                /* isBindless = */ false);

            isTextureSource = true;
        } else if (bindingType == HdBinding::BINDLESS_TEXTURE_UDIM_ARRAY) {
            // a function returning sampler requires bindless_texture
            if (caps.bindlessTextureEnabled) {
                accessors
                    << "sampler2DArray\n"
                    << "HdGetSampler_" << it->second.name << "() {\n"
                    << "  int shaderCoord = GetDrawingCoord().shaderCoord; \n"
                    << "  return sampler2DArray(materialParams[shaderCoord]."
                    << it->second.name << ");\n"
                    << "  }\n";
            } else {
                accessors
                    << "#define HdGetSampler_" << it->second.name << "()"
                    << " sampler2dArray_" << it->second.name << "\n";
            }
            accessors
                << it->second.dataType
                << " HdGet_" << it->second.name << "()" << " {\n"
                << "  int shaderCoord = GetDrawingCoord().shaderCoord;\n";
            
            if (!it->second.inPrimvars.empty()) {
                accessors
                    << "#if defined(HD_HAS_"
                    << it->second.inPrimvars[0] << ")\n"
                    << "  vec3 c = hd_sample_udim(HdGet_"
                    << it->second.inPrimvars[0] << "().xy);\n"
                    << "  c.z = texelFetch(sampler1D(materialParams[shaderCoord]."
                    << it->second.name
                    << HdSt_ResourceBindingSuffixTokens->layout
                    << "), int(c.z), 0).x - 1;\n"
                    << "#else\n"
                    << "  vec3 c = vec3(0.0, 0.0, 0.0);\n"
                    << "#endif\n";
            } else {
                accessors
                    << "  vec3 c = vec3(0.0, 0.0, 0.0);\n";
            }
            accessors
                << "if (c.z < -0.5) { return vec4(0, 0, 0, 0)" << swizzle
                << "; } else { \n"
                << "  return texture(sampler2DArray(materialParams[shaderCoord]."
                << it->second.name << "), c)" << swizzle << ";}\n}\n";
        } else if (bindingType == HdBinding::TEXTURE_UDIM_ARRAY) {
            declarations
                << "sampler samplerBind_" << it->second.name << ";\n"
                << "texture2d_array<float> textureBind_" << it->second.name << ";\n";
            
            _AddInputParam(_mslPSInputParams, TfToken("samplerBind_" + it->second.name.GetString()), TfToken("sampler"), TfToken()).usage
                |= HdSt_CodeGenMSL::TParam::Sampler;
            _AddInputParam(_mslPSInputParams, TfToken("textureBind_" + it->second.name.GetString()), TfToken("texture2d_array<float>"), TfToken()).usage
                |= HdSt_CodeGenMSL::TParam::Texture;
            
            if (caps.glslVersion >= 430) {
                accessors
                    << "texture2d_array<float>\n"
                    << "HdGetSampler_" << it->second.name << "() {\n"
                    << "  return textureBind_" << it->second.name << ";"
                    << "}\n";
            }
            // vec4 HdGet_name(vec2 coord) { vec3 c = hd_sample_udim(coord);
            // c.z = texelFetch(sampler1d_name_layout, int(c.z), 0).x - 1;
            // if (c.z < -0.5) { return vec4(0, 0, 0, 0).xyz; } else {
            // return texture(sampler2dArray_name, hd_sample_udim(coord)).xyz;}}
            accessors
                << it->second.dataType
                << " HdGet_" << it->second.name
                << "(vec2 coord) { vec3 c = hd_sample_udim(coord);\n"
                << "  c.z = textureBind_"
                << it->second.name << HdSt_ResourceBindingSuffixTokens->layout
                << ".read(uint(c.z), 0).x - 1;\n"
                << "if (c.z < -0.5) { return vec4(0, 0, 0, 0)"
                << swizzle << "; } else {\n"
                << "  return textureBind_" << it->second.name
                << ".sample(samplerBind_"
                << it->second.name << ", c.xy, c.z)" << swizzle << ";}}\n";
                // vec4 HdGet_name() { return HdGet_name(HdGet_st().xy); }
            accessors
                << it->second.dataType
                << " HdGet_" << it->second.name
                << "() { return HdGet_" << it->second.name << "(";
                if (!it->second.inPrimvars.empty()) {
                    accessors
                        << "\n"
                        << "#if defined(HD_HAS_"
                        << it->second.inPrimvars[0] << ")\n"
                        << "HdGet_" << it->second.inPrimvars[0] << "().xy\n"
                        << "#else\n"
                        << "vec2(0.0, 0.0)\n"
                        << "#endif\n";
                } else {
                    accessors
                        << "vec2(0.0, 0.0)";
            }
            accessors << "); }\n";
        } else if (bindingType == HdBinding::TEXTURE_UDIM_LAYOUT) {
            declarations
                << "texture1d<float> textureBind_" << it->second.name << ";\n";
            _AddInputParam(_mslPSInputParams,
                TfToken("textureBind_" + it->second.name.GetString()),
                TfToken("texture1d<float>"),
                TfToken(),
                it->first).usage |= HdSt_CodeGenMSL::TParam::Texture;
            addScalarAccessor = false;

        } else if (bindingType == HdBinding::BINDLESS_TEXTURE_PTEX_TEXEL) {
            accessors
                << _GetUnpackedType(it->second.dataType, false)
                << " HdGet_" << it->second.name << "(int localIndex) {\n"
                << "  int shaderCoord = GetDrawingCoord().shaderCoord; \n"
                << "  return " << _GetPackedTypeAccessor(it->second.dataType, false)
                << "(GlopPtexTextureLookup("
                << "sampler2DArray(materialParams[shaderCoord]."
                << it->second.name << "),"
                << "isamplerBuffer(materialParams[shaderCoord]."
                << it->second.name << HdSt_ResourceBindingSuffixTokens->layout
                <<"), "
                << "GetPatchCoord(localIndex))" << swizzle << ");\n"
                << "}\n"
                << _GetUnpackedType(it->second.dataType, false)
                << " HdGet_" << it->second.name << "()"
                << "{ return HdGet_" << it->second.name << "(0); }\n"
                << _GetUnpackedType(it->second.dataType, false)
                << " HdGet_" << it->second.name << "(vec4 patchCoord) {\n"
                << "  int shaderCoord = GetDrawingCoord().shaderCoord; \n"
                << "  return " << _GetPackedTypeAccessor(it->second.dataType, false)
                << "(GlopPtexTextureLookup("
                << "sampler2DArray(materialParams[shaderCoord]."
                << it->second.name << "),"
                << "isamplerBuffer(materialParams[shaderCoord]."
                << it->second.name << HdSt_ResourceBindingSuffixTokens->layout
                << "), "
                << "patchCoord)" << swizzle << ");\n"
                << "}\n";
        } else if (bindingType == HdBinding::TEXTURE_PTEX_TEXEL) {
            // appending '_layout' for layout is by convention.
            std::string texelBindName("textureBind_" + it->second.name.GetString());
            std::string samplerBindName("samplerBind_" + it->second.name.GetString());
            std::string layoutBindName("bufferBind_" + it->second.name.GetString() + "_layout");
            
            declarations
                << "texture2d_array<float> " << texelBindName << ";\n"
                << "const device ushort * " << layoutBindName << ";\n"
                << "sampler " << samplerBindName << ";\n";
            
            _AddInputParam(_mslPSInputParams, TfToken(samplerBindName),
                           TfToken("sampler"), TfToken(), it->first).usage
                |= HdSt_CodeGenMSL::TParam::Sampler;
            _AddInputParam(_mslPSInputParams, TfToken(texelBindName),
                           TfToken("texture2d_array<float>"), TfToken(), it->first).usage
                |= HdSt_CodeGenMSL::TParam::Texture;

            HdBinding layoutBinding(HdBinding::TEXTURE_PTEX_LAYOUT,
                it->first.GetLocation(),
                it->first.GetTextureUnit());
            _AddInputPtrParam(_mslPSInputParams, TfToken(layoutBindName),
                TfToken("ushort"), TfToken(), HdBinding(HdBinding::UNIFORM, 0));

            accessors
                << _GetUnpackedType(it->second.dataType, false)
                << " HdGet_" << it->second.name << "(int localIndex) {\n"
                << "  return " << _GetPackedTypeAccessor(it->second.dataType, false)
                << "(GlopPtexTextureLookup("
                << texelBindName << ","
                << layoutBindName << ","
                << samplerBindName << ","
                << "GetPatchCoord(localIndex))" << swizzle << ");\n"
                << "}\n"
                << _GetUnpackedType(it->second.dataType, false)
                << " HdGet_" << it->second.name << "()"
                << "{ return HdGet_" << it->second.name << "(0); }\n"
                << _GetUnpackedType(it->second.dataType, false)
                << " HdGet_" << it->second.name << "(vec4 patchCoord) {\n"
                << "  return " << _GetPackedTypeAccessor(it->second.dataType, false)
                << "(GlopPtexTextureLookup("
                << texelBindName<< ","
                << layoutBindName << ","
                << samplerBindName << ","
                << "patchCoord)" << swizzle << ");\n"
                << "}\n";
            addScalarAccessor = false;
        } else if (bindingType == HdBinding::BINDLESS_TEXTURE_PTEX_LAYOUT) {
            addScalarAccessor = false;
            //accessors << _GetUnpackedType(it->second.dataType) << "(0)";
        } else if (bindingType == HdBinding::TEXTURE_PTEX_LAYOUT) {
            addScalarAccessor = false;
//            declarations
//                << LayoutQualifier(HdBinding(it->first.GetType(),
//                                            it->first.GetLocation(),
//                                            it->first.GetTextureUnit()))
//                << "uniform isamplerBuffer isamplerbuffer_"
//                << it->second.name << ";\n";
        } else if (bindingType == HdBinding::PRIMVAR_REDIRECT) {
            // Create an HdGet_INPUTNAME for the shader to access a primvar
            // for which a HdGet_PRIMVARNAME was already generated earlier.
            
            // XXX: shader and primvar name collisions are a problem!
            // (see, e.g., HYD-1800).
            if (it->second.name == it->second.inPrimvars[0]) {
                // Avoid the following:
                // If INPUTNAME and PRIMVARNAME are the same and the
                // primvar exists, we would generate two functions
                // both called HdGet_PRIMVAR, one to read the primvar
                // (based on _metaData.constantData) and one for the
                // primvar redirect here.
                accessors
                    << "#if !defined(HD_HAS_" << it->second.name << ")\n";
            }

            if (it->second.name != it->second.inPrimvars[0]) {
                accessors
                    << _GetUnpackedType(it->second.dataType, false)
                    << " HdGet_" << it->second.name << "(int localIndex) {\n"
                    // If primvar exists, use it
                    << "#if defined(HD_HAS_" << it->second.inPrimvars[0] << ")\n"
                    << "  return HdGet_" << it->second.inPrimvars[0] << "();\n"
                    << "#else\n"
                    // Otherwise use default value.
                    << "  int shaderCoord = GetDrawingCoord().shaderCoord;\n"
                    << "  return "
                    << _GetUnpackedType(_GetPackedTypeAccessor(it->second.dataType, false), false)
                    << "(materialParams[shaderCoord]."
                    << it->second.name << HdSt_ResourceBindingSuffixTokens->fallback
                    << swizzle << ";\n"
                    << "#endif\n"
                    << "\n}\n"
                    << "#define HD_HAS_" << it->second.name << " 1\n";
                
                accessors
                    << _GetUnpackedType(it->second.dataType, false)
                    << " HdGet_" << it->second.name << "() {\n"
                    // If primvar exists, use it
                    << "#if defined(HD_HAS_" << it->second.inPrimvars[0] << ")\n"
                    << "  return HdGet_" << it->second.inPrimvars[0] << "(0);\n"
                    << "#else\n"
                    // Otherwise use default value.
                    << "  int shaderCoord = GetDrawingCoord().shaderCoord;\n"
                    << "  return "
                    << _GetUnpackedType(_GetPackedTypeAccessor(it->second.dataType, false), false)
                    << "(materialParams[shaderCoord]."
                    << it->second.name << HdSt_ResourceBindingSuffixTokens->fallback
                    << swizzle <<  ");\n"
                    << "#endif\n"
                    << "\n}\n"
                    << "#define HD_HAS_" << it->second.name << " 1\n";
            }
            
            if (it->second.name == it->second.inPrimvars[0]) {
                accessors
                    << "#endif\n";
            }
        } else if (bindingType == HdBinding::TRANSFORM_2D) {
            // Forward declare rotation, scale, and translation
//            accessors
//                << "float HdGet_" << it->second.name << "_"
//                << HdStTokens->rotation  << "();\n"
//                << "vec2 HdGet_" << it->second.name << "_"
//                << HdStTokens->scale  << "();\n"
//                << "vec2 HdGet_" << it->second.name << "_"
//                << HdStTokens->translation  << "();\n";

            // vec2 HdGet_name(int localIndex)
            accessors
                << _GetUnpackedType(it->second.dataType, false)
                << " HdGet_" << it->second.name << "(int localIndex) {\n"
                << "  float angleRad = HdGet_" << it->second.name << "_"
                << HdStTokens->rotation  << "()"
                << " * 3.1415926f / 180.f;\n"
                << "  mat2 rotMat = mat2(cos(angleRad), sin(angleRad), "
                << "-sin(angleRad), cos(angleRad)); \n";
            // If primvar exists, use it
            if (!it->second.inPrimvars.empty()) {
                accessors
                    << "#if defined(HD_HAS_" << it->second.inPrimvars[0] << ")\n"
                    << "  return vec2(HdGet_" << it->second.name << "_"
                    << HdStTokens->translation << "() + rotMat * (HdGet_"
                    << it->second.name << "_" << HdStTokens->scale << "() * "
                    << "HdGet_" << it->second.inPrimvars[0] << "(localIndex)));\n"
                    << "#else\n";
            }
            // Otherwise use default value.
            accessors
                << "  int shaderCoord = GetDrawingCoord().shaderCoord;\n"
                << "  return vec2(HdGet_" << it->second.name << "_"
                << HdStTokens->translation << "() + rotMat * (HdGet_"
                << it->second.name << "_" << HdStTokens->scale << "() * "
                << "shaderData[shaderCoord]." << it->second.name
                << HdSt_ResourceBindingSuffixTokens->fallback << swizzle
                << "));\n";
            if (!it->second.inPrimvars.empty()) {
                accessors << "#endif\n";
            }
            accessors << "}\n";

            // vec2 HdGet_name()
            accessors
                << _GetUnpackedType(it->second.dataType, false)
                << " HdGet_" << it->second.name << "() {\n"
                << "  return HdGet_" << it->second.name << "(0);\n"
                << "}\n";
        }
        
        if (addScalarAccessor) {
            // Scalar accessor - to work around GLSL allowing .x to access a float
            TfToken componentType = _GetComponentType(it->second.dataType);
            if (componentType == it->second.dataType) {
                accessors
                    << it->second.dataType
                    << " HdGet_" << it->second.name << "_Scalar(int localIndex) {\n"
                    << "  return HdGet_" << it->second.name << "(localIndex); \n"
                    << "}\n";
                accessors
                    << it->second.dataType
                    << " HdGet_" << it->second.name << "_Scalar() {\n"
                    << "  return HdGet_" << it->second.name << "(0); \n"
                    << "}\n";
            } else {
                accessors
                    << _GetComponentType(it->second.dataType)
                    << " HdGet_" << it->second.name << "_Scalar(int localIndex) {\n"
                    << "  return HdGet_" << it->second.name << "(localIndex).x; \n"
                    << "}\n";
                accessors
                    << _GetComponentType(it->second.dataType)
                    << " HdGet_" << it->second.name << "_Scalar() {\n"
                    << "  return HdGet_" << it->second.name << "(0).x; \n"
                    << "}\n";
            }
        }
        
        accessors
            << "bool HdGet_" << it->second.name << "_IsTextureSource() {\n"
            << "  return " << isTextureSource << "; \n"
            << "}\n";
    }
    
    // Field redirect accessors, need to access above field textures.
    TF_FOR_ALL (it, _metaData.shaderParameterBinding) {
        HdBinding::Type bindingType = it->first.GetType();

        if (bindingType == HdBinding::FIELD_REDIRECT) {
            
            // adjust datatype
            std::string swizzle = _GetSwizzleString(it->second.dataType);

            const TfToken fieldName =
                it->second.inPrimvars.empty()
                ? TfToken("FIELDNAME_WAS_NOT_SPECIFIED")
                : it->second.inPrimvars[0];

            // Create an HdGet_INPUTNAME(vec3) for the shader to access a
            // field texture HdGet_FIELDNAMETexture(vec3).

            accessors
                << _GetUnpackedType(it->second.dataType, false)
                << " HdGet_" << it->second.name << "(vec3 coord) {\n"
                // If field exists, use it
                << "#if defined(HD_HAS_"
                << fieldName << HdSt_ResourceBindingSuffixTokens->texture
                << ")\n"
                << "  return HdGet_"
                << fieldName << HdSt_ResourceBindingSuffixTokens->texture
                << "(coord)"
                << swizzle << ";\n"
                << "#else\n"
                // Otherwise use default value.
                << "  int shaderCoord = GetDrawingCoord().shaderCoord;\n"
                << "  return "
                << _GetUnpackedType(_GetPackedTypeAccessor(it->second.dataType, false), false)
                << "(materialParams[shaderCoord]."
                << it->second.name << HdSt_ResourceBindingSuffixTokens->fallback
                <<  ");\n"
                << "#endif\n"
                << "\n}\n";
        }
    }
    
    _genFS << declarations.str()
           << accessors.str();

    _genGS << declarations.str()
           << accessors.str();
    
    METAL_DEBUG_COMMENT(&_genFS, "END OF _GenerateShaderParameters()\n"); //MTL_FIXME
    METAL_DEBUG_COMMENT(&_genVS, "END OF _GenerateShaderParameters()\n"); //MTL_FIXME

}

void
HdSt_CodeGenMSL::_GenerateTopologyVisibilityParameters()
{
    std::stringstream declarations;
    std::stringstream accessors;
    
    METAL_DEBUG_COMMENT(&_genFS, "_GenerateTopologyVisibilityParameters()\n"); //MTL_FIXME
    METAL_DEBUG_COMMENT(&_genVS, "_GenerateTopologyVisibilityParameters()\n"); //MTL_FIXME

    TF_FOR_ALL (it, _metaData.topologyVisibilityData) {
        // See note in _GenerateConstantPrimvar re: padding.
        HdBinding binding = it->first;
        TfToken typeName(TfStringPrintf("TopologyVisibilityData%d",
                                        binding.GetValue()));
        TfToken varName = it->second.blockName;
        
        declarations << "struct " << typeName << " {\n";
        
        TF_FOR_ALL (dbIt, it->second.entries) {
            if (!TF_VERIFY(!dbIt->dataType.IsEmpty(),
                           "Unknown dataType for %s",
                           dbIt->name.GetText())) {
                continue;
            }
            
            declarations << "  " << _GetPackedType(dbIt->dataType, false)
                         << " " << dbIt->name;
            if (dbIt->arraySize > 1) {
                declarations << "[" << dbIt->arraySize << "]";
            }
            
            declarations << ";\n";
            
            _EmitStructAccessor(accessors, varName, dbIt->name, dbIt->dataType,
                                dbIt->arraySize,
                                "GetDrawingCoord().topologyVisibilityCoord");
        }
        declarations << "};\n";
        
        _EmitDeclaration(declarations, varName, typeName, TfToken(), binding,
                         /*arraySize=*/1);
    }
    _genCommon << declarations.str()
               << accessors.str();
    
    METAL_DEBUG_COMMENT(&_genFS, "END OF _GenerateTopologyVisibilityParameters()\n"); //MTL_FIXME
    METAL_DEBUG_COMMENT(&_genVS, "END OF _GenerateTopologyVisibilityParameters()\n"); //MTL_FIXME
}

PXR_NAMESPACE_CLOSE_SCOPE

