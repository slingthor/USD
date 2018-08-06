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
#include "pxr/imaging/garch/glslfx.h"
#include "pxr/imaging/garch/resourceFactory.h"

#include "pxr/imaging/hdSt/Metal/codeGenMSL.h"
#include "pxr/imaging/hdSt/Metal/mslProgram.h"

#include "pxr/imaging/hdSt/geometricShader.h"
#include "pxr/imaging/hdSt/package.h"
#include "pxr/imaging/hdSt/resourceBinder.h"
#include "pxr/imaging/hdSt/resourceRegistry.h"
#include "pxr/imaging/hdSt/shaderCode.h"

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

#include <opensubdiv/osd/glslPatchShaderSource.h>

#define MTL_PRIMVAR_PREFIX "__primVar_"

#if GENERATE_METAL_DEBUG_SOURCE_CODE
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
    (hd_vec3)
    (hd_vec3_get)
    (hd_vec3_set)
    (hd_ivec3)
    (hd_ivec3_get)
    (hd_ivec3_set)
    (hd_dvec3)
    (hd_dvec3_get)
    (hd_dvec3_set)
    (hd_mat3)
    (hd_mat3_get)
    (hd_mat3_set)
    (hd_dmat3)
    (hd_dmat3_get)
    (hd_dmat3_set)
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
    (mat3)
    (mat4)
    (dmat3)
    (dmat4)
    ((ptexTextureSampler, "ptexTextureSampler"))
    (isamplerBuffer)
    (samplerBuffer)
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
    : _geometricShader(geometricShader), _shaders(shaders), _mslVSOutputStructSize(0)
{
    TF_VERIFY(geometricShader);
}

HdSt_CodeGenMSL::HdSt_CodeGenMSL(HdStShaderCodeSharedPtrVector const &shaders)
    : _geometricShader(), _shaders(shaders), _mslVSOutputStructSize(0)
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
        GLSLFX(HdStPackagePtexTextureShader()).GetSource(
            _tokens->ptexTextureSampler);
    return source;
}

static bool InDeviceMemory(const HdBinding binding)
{
    switch (binding.GetType()) {
        case HdBinding::SSBO:
        case HdBinding::UBO:
        case HdBinding::TBO:
            return true;
        default:
            return false;
    }
}

// TODO: Shuffle code to remove these declarations.
static HdSt_CodeGenMSL::TParam& _EmitDeclaration(std::stringstream &str,
                                                 HdSt_CodeGenMSL::InOutParams &inputParams,
                                                 TfToken const &name,
                                                 TfToken const &type,
                                                 TfToken const &attribute,
                                                 HdBinding const &binding,
                                                 int arraySize=0);

static HdSt_CodeGenMSL::TParam& _EmitDeclarationPtr(std::stringstream &str,
                                                    HdSt_CodeGenMSL::InOutParams &inputParams,
                                                    TfToken const &name,
                                                    TfToken const &type,
                                                    TfToken const &attribute,
                                                    HdBinding const &binding,
                                                    int arraySize=0,
                                                    bool programScope=false);

static void _EmitStructAccessor(std::stringstream &str,
                                TfToken const &structMemberName,
                                TfToken const &name,
                                TfToken const &type,
                                int arraySize,
                                bool pointerDereference,
                                const char *index);

static void _EmitComputeAccessor(std::stringstream &str,
                                 TfToken const &name,
                                 TfToken const &type,
                                 HdBinding const &binding,
                                 const char *index);

static void _EmitComputeMutator(std::stringstream &str,
                                TfToken const &name,
                                TfToken const &type,
                                HdBinding const &binding,
                                const char *index);

static void _EmitAccessor(std::stringstream &str,
                          TfToken const &name,
                          TfToken const &type,
                          HdBinding const &binding,
                          const char *index=NULL);

static HdSt_CodeGenMSL::TParam& _EmitOutput(std::stringstream &str,
                                            HdSt_CodeGenMSL::InOutParams &outputParams,
                                            TfToken const &name,
                                            TfToken const &type,
                                            TfToken const &attribute = TfToken(),
                                            HdSt_CodeGenMSL::TParam::Usage usage = HdSt_CodeGenMSL::TParam::Unspecified);

static HdSt_CodeGenMSL::TParam& _EmitStructMemberOutput(HdSt_CodeGenMSL::InOutParams &outputParams,
                                                        TfToken const &name,
                                                        TfToken const &accessor,
                                                        TfToken const &type,
                                                        TfToken const &attribute = TfToken(),
                                                        HdSt_CodeGenMSL::TParam::Usage usage = HdSt_CodeGenMSL::TParam::Unspecified);
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

static const char *
_GetPackedTypeDefinitions()
{
    return "#define hd_ivec3 packed_int3\n"
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
           "int hd_int_get<st>(int v)          { return v; }\n"
           "int hd_int_get<st>(ivec2 v)        { return v[0]; }\n"
           "int hd_int_get<st>(ivec3 v)        { return v[0]; }\n"
           "int hd_int_get<st>(ivec4 v)        { return v[0]; }\n"
           "mat4 inverse(mat4 a) { return transpose(a); }  // MTL_FIXME - Required for AlGhadeer scene, need proper implementation of this;\n";
}

static TfToken const &
_GetPackedType(TfToken const &token)
{
    if (token == _tokens->ivec3) {
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
    return token;
}

static TfToken const &
_GetPackedTypeAccessor(TfToken const &token)
{
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
    return token;
}

static TfToken const &
_GetPackedTypeMutator(TfToken const &token)
{
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
    }
    return token;
}

static TfToken const &
_GetSamplerBufferType(TfToken const &token)
{
    if (token == _tokens->_int  ||
        token == _tokens->ivec2 ||
        token == _tokens->ivec3 ||
        token == _tokens->ivec4) {
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
        case HdBinding::TBO:
        case HdBinding::SSBO:
        case HdBinding::BINDLESS_UNIFORM:
        case HdBinding::TEXTURE_2D:
        case HdBinding::BINDLESS_TEXTURE_2D:
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
HdSt_CodeGenMSL::_ParseGLSL(std::stringstream &source, InOutParams& inParams, InOutParams& outParams)
{
    static std::regex regex_word("(\\S+)");

    std::string result = source.str();
    std::stringstream dummy;
    
    struct TagSpec {
        TagSpec(char const* const _tag, InOutParams& _params)
        : glslTag(_tag),
          params(_params)
        {}

        std::string glslTag;
        InOutParams& params;
    };
    
    std::vector<TagSpec> tags;
    
    tags.push_back(TagSpec("\nout ", outParams));
    tags.push_back(TagSpec("\nin ", inParams));
    tags.push_back(TagSpec("\nuniform ", inParams));
    tags.push_back(TagSpec("\nlayout(std140) uniform ", inParams));
    
    TfTokenVector mslAttribute;
    
    int firstPerspectiveIndex = tags.size();
    tags.push_back(TagSpec("\nflat out ", outParams));
    mslAttribute.push_back(TfToken("[[flat]]"));
    tags.push_back(TagSpec("\nflat in ", inParams));
    mslAttribute.push_back(TfToken("[[flat]]"));
    
    tags.push_back(TagSpec("\ncentroid out ", outParams));
    mslAttribute.push_back(TfToken("[[centroid_perspective]]"));
    tags.push_back(TagSpec("\ncentroid in ", inParams));
    mslAttribute.push_back(TfToken("[[centroid_perspective]]"));

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
                std::string structName = result.substr(pos + tag.glslTag.length(), endOfName - (pos + tag.glslTag.length()));
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
                if (std::regex_search(line, match, regex_word)) {
                    const std::string::size_type s   = match[0].first  - line.begin();
                    const std::string::size_type count = match[0].second - match[0].first;
                    
                    parent = line.substr(s, count);
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
                            if(!isUniform)
                                _EmitStructMemberOutput(tag.params, name, accessor, type);
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

                    if (nameStr[0] == '*') {
                        result.replace(pos, 0, std::string("\ndevice const "));
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
                    
                    
                    _EmitOutput(dummy, tag.params, name, type,
                        pass >= firstPerspectiveIndex?mslAttribute[pass - firstPerspectiveIndex]:TfToken(""),
                        usage);
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
}

void HdSt_CodeGenMSL::_GenerateGlue(std::stringstream& glueVS, std::stringstream& gluePS, HdStMSLProgramSharedPtr mslProgram)
{
    std::stringstream glueCommon, copyInputsVtx, copyOutputsVtx, copyInputsVtxStruct_Compute, copyInputsVtx_Compute;
    std::stringstream copyInputsFrag, copyOutputsFrag;

    glueCommon.str("");
    METAL_DEBUG_COMMENT(&glueCommon, "_GenerateGlue(glueCommon)\n"); //MTL_FIXME
    copyInputsVtx.str("");
    copyInputsVtx_Compute.str("");
    copyInputsVtxStruct_Compute.str("");
    copyInputsFrag.str("");
    copyOutputsVtx.str("");
    copyOutputsFrag.str("");
    
    glueCommon << "struct MSLVtxOutputs {\n";
    uint32 vtxOutputStructSize = 0;
    TF_FOR_ALL(it, _mslVSOutputParams) {
        HdSt_CodeGenMSL::TParam const &output = *it;
        
        bool generateCSVersion = !output.attribute.IsEmpty();
        if(generateCSVersion) glueCommon << "#if HD_MTL_COMPUTESHADER == 0\n";
        glueCommon << output.dataType << " " << output.name << output.attribute << ";\n";
        if(generateCSVersion) {
            glueCommon << "#else\n";
            glueCommon << output.dataType << " " << output.name << "; //" << output.attribute << "\n";
            glueCommon << "#endif\n";
        }
        
        copyOutputsVtx << "vtxOut." << output.name << "=scope.";
        if (output.accessorStr.IsEmpty()) {
            copyOutputsVtx << output.name << ";\n";
        }
        else {
            copyOutputsVtx << output.accessorStr << ";\n";
        }
        
        //Update struct size + apply alignment rules
        const std::string dataTypeStr = output.dataType.GetString();
        uint32 size = 4;
        if(dataTypeStr.find("mat") != std::string::npos) TF_FATAL_CODING_ERROR("Not implemented!");
        else if(dataTypeStr.find("2") != std::string::npos) size = 8;
        else if(dataTypeStr.find("3") != std::string::npos) size = 12;
        else if(dataTypeStr.find("4") != std::string::npos) size = 16;
        uint32 regStart = vtxOutputStructSize / 16;
        uint32 regEnd = (vtxOutputStructSize + size - 1) / 16;
        if(regStart != regEnd && vtxOutputStructSize % 16 != 0) vtxOutputStructSize += 16 - (vtxOutputStructSize % 16);

        vtxOutputStructSize += size;
    }
    //Round up size of uniform buffer to next 16 byte boundary.
    vtxOutputStructSize = ((vtxOutputStructSize + 15) / 16) * 16;
    _mslVSOutputStructSize = vtxOutputStructSize;
    glueCommon << "};\n";
    
    glueVS << glueCommon.str();
    gluePS << glueCommon.str();
    
    METAL_DEBUG_COMMENT(&glueVS, "_GenerateGlue(glueVS)\n"); //MTL_FIXME
    METAL_DEBUG_COMMENT(&gluePS, "_GenerateGlue(gluePS)\n"); //MTL_FIXME
    
    std::stringstream glueCompute;
    glueCompute << "kernel void computeEntryPoint(\n"
                << "uint threadPositionInGrid[[thread_position_in_grid]]\n"
                << "    , uint threadPositionInThreadgroup[[thread_position_in_threadgroup]]\n"
                << "    , uint threadIndexInThreadgroup[[thread_index_in_threadgroup]]\n"
                << "    , uint threadgroupPositionInGrid[[threadgroup_position_in_grid]]";
    
    glueVS << "struct MSLVtxInputs {\n";
    int location = 0;
    int vertexAttribsLocation = 0;
    TF_FOR_ALL(it, _mslVSInputParams) {
        HdSt_CodeGenMSL::TParam const &input = *it;
        TfToken attrib;
        
        if (input.usage & HdSt_CodeGenMSL::TParam::Uniform)
            continue;
        
        if (input.usage & HdSt_CodeGenMSL::TParam::EntryFuncArgument) {
            std::string n = (input.name.GetText()[0] == '*') ? input.name.GetString().substr(1) : input.name;
            copyInputsVtx << "scope." << n << "=" << n << ";\n";
            copyInputsVtx_Compute << "            , " << n << "\n";
            continue;
        }

        copyInputsVtx << "scope." << input.name << "=input." << input.name << ";\n";
        
        if (input.name.GetText()[0] == '*') {
            //Does this case ever hit? A pointer as vertexattrib? - jsoet
            glueVS << "device const ";
            mslProgram->AddBinding(input.name.GetText() + 1, vertexAttribsLocation, kMSL_BindingType_VertexAttribute, kMSL_ProgramStage_Vertex);
        }
        else {
            mslProgram->AddBinding(input.name.GetString(), vertexAttribsLocation, kMSL_BindingType_VertexAttribute, kMSL_ProgramStage_Vertex);
        }

//        if (!input.attribute.IsEmpty())
//            attrib = input.attribute;
//        else
            attrib = TfToken(TfStringPrintf("[[attribute(%d)]]", vertexAttribsLocation));

        ////////////////////////////////////////////////////////
        
        const bool isPackedNormals = input.name == HdTokens->packedNormals; //MTL_FIXME: A horrible hack to fix loading packedNormals, we should consider StageIn but that would mean we can't do VS and GS in a single pass....
        
        bool generateCSVersion = !attrib.IsEmpty();
        if(generateCSVersion) glueVS << "#if HD_MTL_VERTEXSHADER\n";
        glueVS << input.dataType << " " << input.name << attrib << ";"  << "// Binding to [[buffer(" << vertexAttribsLocation << ")]]\n";
        if(generateCSVersion) {
            glueVS << "#else\n";
            glueVS << input.dataType << " " << input.name << "; // *" << attrib << "* Binding to [[buffer(" << vertexAttribsLocation << ")]]\n";
            glueVS << "#endif\n";
            
            //Add the attribute as a buffer input to the Compute function definition
            glueCompute << "\n    , device const ";
            if(isPackedNormals)
                glueCompute << "uint";
            else
                glueCompute << input.dataType;
            glueCompute << " *" << input.name << "[[buffer(" << vertexAttribsLocation << ")]] // " << attrib;
        }
        if(isPackedNormals)
            copyInputsVtxStruct_Compute << ",   \n            float4(Unpack10_10_10_2_ToVec3(" << input.name << "[__vertexIDs[i]]), 0.0)";
        else
            copyInputsVtxStruct_Compute << ",   \n            " << input.name << "[__vertexIDs[i]]";
        ////////////////////////////////////////////////////////
        
        vertexAttribsLocation++;
    }
    glueVS << "};\n";
    
    //This binding for indices is not an necessarily a required binding. It's here so that
    //it propagates to the binding system and can be retrieved there. You don't have to bind it.
    mslProgram->AddBinding("indices", vertexAttribsLocation, kMSL_BindingType_IndexBuffer, kMSL_ProgramStage_Vertex);
    mslProgram->AddBinding("indices", vertexAttribsLocation, kMSL_BindingType_IndexBuffer, kMSL_ProgramStage_Compute);
    //Add it for ComputeVS. Increment vertexAttribsLocation (this means that in the VS the buffers are offset by 1,
    //with 1 slot skipped, the indices)
    glueCompute << "\n    , device const uint *indices[[buffer(" << vertexAttribsLocation++ << ")]]";
    
    /////////////////////////////////Uniform Buffer///////////////////////////////////
    
    int vtxUniformBufferSize = 0;
    TF_FOR_ALL(it, _mslVSInputParams) {
        HdSt_CodeGenMSL::TParam const &input = *it;
        if ((input.usage & HdSt_CodeGenMSL::TParam::Usage::Uniform) == 0)
            continue;
        
        //Apply alignment rules
        uint32 size = 4;
        if(input.dataType.GetString().find("vec2") != std::string::npos) size = 8;
        else if(input.dataType.GetString().find("vec3") != std::string::npos) size = 12;
        else if(input.dataType.GetString().find("vec4") != std::string::npos) size = 16;
        uint32 regStart = vtxUniformBufferSize / 16;
        uint32 regEnd = (vtxUniformBufferSize + size - 1) / 16;
        if(regStart != regEnd && vtxUniformBufferSize % 16 != 0)
            vtxUniformBufferSize += 16 - (vtxUniformBufferSize % 16);
        
        mslProgram->AddBinding(input.name, -1, kMSL_BindingType_Uniform, kMSL_ProgramStage_Vertex, vtxUniformBufferSize);

        vtxUniformBufferSize += size;
    }

    //Round up size of uniform buffer to next 16 byte boundary.
    vtxUniformBufferSize = ((vtxUniformBufferSize + 15) / 16) * 16;

#define CODEGENMSL_VTXUNIFORMSTRUCTNAME "MSLVtxUniforms"
#define CODEGENMSL_VTXUNIFORMINPUTNAME "vtxUniforms"

    if(vtxUniformBufferSize != 0)
    {
        glueVS << "struct " CODEGENMSL_VTXUNIFORMSTRUCTNAME " {\n";
        {
            TF_FOR_ALL(it, _mslVSInputParams) {
                HdSt_CodeGenMSL::TParam const &input = *it;
                if ((input.usage & HdSt_CodeGenMSL::TParam::Usage::Uniform) == 0)
                    continue;

                if (input.arraySize) {
                    glueVS << input.dataType << " " << input.name << "[" << input.arraySize << "]" << ";\n";
                }
                else {
                glueVS << input.dataType << " " << input.name << ";\n";
                }

                copyInputsVtx << "scope." << input.name << "=" CODEGENMSL_VTXUNIFORMINPUTNAME "->" << input.name << ";\n";
            }
        }
        glueVS << "};\n";

        HdSt_CodeGenMSL::TParam in(TfToken("*" CODEGENMSL_VTXUNIFORMINPUTNAME), TfToken(CODEGENMSL_VTXUNIFORMSTRUCTNAME), TfToken(), TfToken(), HdSt_CodeGenMSL::TParam::Unspecified);
        
        copyInputsVtx_Compute <<   "            , " << CODEGENMSL_VTXUNIFORMINPUTNAME << "\n";

        in.usage |= HdSt_CodeGenMSL::TParam::EntryFuncArgument;

        _mslVSInputParams.push_back(in);
    }
    
    /////////////////////////////////Frag Outputs///////////////////////////////////
    
    gluePS << "struct MSLFragOutputs {\n";
    location = 0;
    TF_FOR_ALL(it, _mslPSOutputParams) {
        HdSt_CodeGenMSL::TParam const &output = *it;
        gluePS << output.dataType << " " << output.name << "[[color(" << location++ << ")]];\n";
        
        copyOutputsFrag << "fragOut." << output.name << "=scope.";
        if (output.accessorStr.IsEmpty()) {
            copyOutputsFrag << output.name << ";\n";
        }
        else {
            copyOutputsFrag << output.accessorStr << ";\n";
        }
    }
    gluePS << "};\n";
    
    // Check if there's any texturing parameters
    bool hasTexturing = false;
    TF_FOR_ALL(it, _mslPSInputParams) {
        HdSt_CodeGenMSL::TParam const &input = *it;
        auto usage = input.usage & HdSt_CodeGenMSL::TParam::maskShaderUsage;
        if (usage == HdSt_CodeGenMSL::TParam::Texture ||
            usage == HdSt_CodeGenMSL::TParam::Sampler) {
            hasTexturing = true;
            break;
        }
    }
    if (hasTexturing) {
        gluePS << "struct MSLTexturing {\n";
        int textureLocation = 0;
        int samplerLocation = 0;
        std::stringstream attribute;

        TF_FOR_ALL(it, _mslPSInputParams) {
            HdSt_CodeGenMSL::TParam const &input = *it;

            attribute.str("");

            switch (input.usage & HdSt_CodeGenMSL::TParam::maskShaderUsage) {
                case HdSt_CodeGenMSL::TParam::Unspecified:
                    continue;
                case HdSt_CodeGenMSL::TParam::Texture:
                    location = textureLocation++;
                    attribute << "[[texture(" << location << ")]]";
                    break;
                case HdSt_CodeGenMSL::TParam::Sampler:
                    location = samplerLocation++;
                    attribute << "[[sampler(" << location << ")]]";
                    break;
                default:
                    TF_FATAL_CODING_ERROR("Not Implemented");
            }

            gluePS << input.dataType << " " << input.name << attribute.str() << ";\n";

            std::string n;
            if (input.name.GetText()[0] == '*') {
                n = input.name.GetString().substr(1);
            }
            else {
                n = input.name.GetString();
            }
            
            switch (input.usage & HdSt_CodeGenMSL::TParam::maskShaderUsage) {
                case HdSt_CodeGenMSL::TParam::Texture:
                    mslProgram->AddBinding(n, location, kMSL_BindingType_Texture, kMSL_ProgramStage_Fragment);
                    break;
                case HdSt_CodeGenMSL::TParam::Sampler:
                    mslProgram->AddBinding(n, location, kMSL_BindingType_Sampler, kMSL_ProgramStage_Fragment);
                    break;
                default:
                    TF_FATAL_CODING_ERROR("Not Implemented");
            }
            
            copyInputsFrag << "scope." << n << "=texturing." << n << ";\n";
        }
        gluePS << "};\n";
    }

#define CODEGENMSL_FRAGUNIFORMINPUTNAME "fragUniforms"

    gluePS << "struct MSLFragInputs {\n";
    location = 0;
    uint32 byteOffset = 0;
    uint32 inputUniformBufferSize = 0;
    TF_FOR_ALL(it, _mslPSInputParams) {
        HdSt_CodeGenMSL::TParam const &input = *it;
        TfToken attrib;
        
        if ((input.usage & HdSt_CodeGenMSL::TParam::maskShaderUsage) != 0 ||
            ((input.usage & HdSt_CodeGenMSL::TParam::UniformBlock) != 0 && input.name.GetText()[0] == '*')) {
            continue;
        }
        else if (input.usage & HdSt_CodeGenMSL::TParam::EntryFuncArgument) {
            if(input.usage & HdSt_CodeGenMSL::TParam::UniformBlock) {
                copyInputsFrag << "scope." << input.name << "=*" << input.name << ";\n";
            }
            else if (input.name.GetText()[0] == '*') {
                std::string n = input.name.GetString().substr(1);
                copyInputsFrag << "scope." << n << "=" << n << ";\n";
            }
            else {
                copyInputsFrag << "scope." << input.name << "=" << input.name << ";\n";
            }
            continue;
        }

        // Look for the input name in the vertex outputs and if so, wire it up to the [[stage_in]]
        bool bFound = false;
        for (auto output : _mslVSOutputParams) {
            if (input.name == output.name) {
                bFound = true;
                break;
            }
        }
        TfToken accessor;
        if (input.accessorStr.IsEmpty()) {
            accessor = input.name;
        }
        else {
            accessor = input.accessorStr;
        }
        if (bFound) {
            if (input.usage & HdSt_CodeGenMSL::TParam::VertexShaderOnly) {
                continue;
            }
            copyInputsFrag << "scope." << accessor << "=vsInput." << input.name << ";\n";
            continue;
        }
        else if(input.usage & HdSt_CodeGenMSL::TParam::UniformBlockMember) {
            std::string inputName = input.name.GetString();
            std::string::size_type openingBracket = inputName.find_first_of("[");
            if(openingBracket != std::string::npos) {
                inputName = input.name.GetString().substr(0, openingBracket);
            }
            copyInputsFrag << "scope." << inputName << "=" << input.accessorStr << "->" << inputName << ";\n";
            continue;
        }
        else if(input.usage & HdSt_CodeGenMSL::TParam::PrimVar) {
            copyInputsFrag << "scope.inPrimvars." << input.name << "=vsInput." << accessor << ";\n";
        }
        else {
            copyInputsFrag << "scope." << accessor << "=" CODEGENMSL_FRAGUNIFORMINPUTNAME "->" << input.name << ";\n";
        }

        attrib = input.attribute;
        gluePS << input.dataType << " " << input.name << attrib << ";\n";
        
        //Register these uniforms. They're part of the "input" buffer which is hardcoded to be bound at slot 0
        
        //Apply alignment rules
        uint32 size = 4;
        if(input.dataType.GetString().find("vec2") != std::string::npos) size = 8;
        else if(input.dataType.GetString().find("vec3") != std::string::npos) size = 12;
        else if(input.dataType.GetString().find("vec4") != std::string::npos) size = 16;
        uint32 regStart = byteOffset / 16;
        uint32 regEnd = (byteOffset + size - 1) / 16;
        if(regStart != regEnd && byteOffset % 16 != 0) byteOffset += 16 - (byteOffset % 16);
        
        mslProgram->AddBinding(input.name, 0, kMSL_BindingType_Uniform, kMSL_ProgramStage_Fragment, byteOffset);
        
        //Size
         byteOffset += size;
    }
    inputUniformBufferSize = ((byteOffset + 15) / 16) * 16;
    gluePS << "};\n";
    
    //Vertex Glue Function//
    
    std::stringstream glueCVS;
    glueVS << "#if HD_MTL_VERTEXSHADER\n";
    glueVS << "vertex MSLVtxOutputs vertexEntryPoint(MSLVtxInputs input[[stage_in]]\n";
    glueCVS << "#else\n";
    glueCVS << "MSLVtxOutputs compute_vertexEntryPoint(MSLVtxInputs input //[[stage_in]]\n";
    // Uniform buffers must start after any allcoated for vertex attributes
    location = vertexAttribsLocation;
    int vtxUniformBufferSlot = 0;
    TF_FOR_ALL(it, _mslVSInputParams) {
        HdSt_CodeGenMSL::TParam const &input = *it;
        TfToken attrib;
        bool useInCompute = true; //Used to remove things like vertexID etc.
        if (!(input.usage & HdSt_CodeGenMSL::TParam::EntryFuncArgument)) {
            continue;
        }
        if (!input.attribute.IsEmpty()) {
            attrib = input.attribute;
            useInCompute = false;    //Any attribute other than [[buffer(X)]] can't be present in CVS sig.
        }
        else {
            std::string n;
            if (input.name.GetText()[0] == '*') {
                if ((input.usage & HdSt_CodeGenMSL::TParam::UniformBlock) != 0)
                    n = input.name.GetText() + 4; //Because of "*___<NAME>"
                else
                    n = input.name.GetText() + 1;
            }
            else {
                n = input.name.GetString();
            }
            int uniformBufferSize = 0;
            if(n == CODEGENMSL_VTXUNIFORMINPUTNAME) {
                uniformBufferSize = vtxUniformBufferSize;
                vtxUniformBufferSlot = location;
            }
            mslProgram->AddBinding(n, location, kMSL_BindingType_UniformBuffer, kMSL_ProgramStage_Vertex, 0, uniformBufferSize);
            attrib = TfToken(TfStringPrintf("[[buffer(%d)]]", location++));
        }
        glueVS << ", ";
        glueCVS << ", ";
        if(useInCompute) glueCompute << "\n    , ";
        if (input.name.GetText()[0] == '*') {
            glueVS << "device const ";
            glueCVS << "device const ";
            if(useInCompute) glueCompute << "device const ";
        }
        if (input.usage & HdSt_CodeGenMSL::TParam::ProgramScope) {
            glueVS << "ProgramScope<st>::";
            glueCVS << "ProgramScope<st>::";
            if(useInCompute) glueCompute << "ProgramScope<st>::";
        }
        glueVS << input.dataType << " " << input.name << attrib << "\n";
        glueCVS << input.dataType << " " << input.name << " //" << attrib << "\n";
        if(useInCompute) glueCompute << input.dataType << " " << input.name << attrib;
    }
    
    //Create VS Input binding
    TF_FOR_ALL(it, _mslVSInputParams) {
        HdSt_CodeGenMSL::TParam const &input = *it;
        TfToken attrib;
        if (!(input.usage & HdSt_CodeGenMSL::TParam::Uniform))
            continue;
        std::string name = (input.name.GetText()[0] == '*') ? input.name.GetText() + 1 : input.name.GetText();
        mslProgram->UpdateUniformBinding(name, vtxUniformBufferSlot);
    }

    glueCVS << ")\n"
            << "#endif\n";
    glueVS  << ")\n"
            << glueCVS.str();
    
    glueVS  << "{\n"
            << "ProgramScope<st> scope;\n"
            << copyInputsVtx.str()
            << "scope.main();\n"
            << "MSLVtxOutputs vtxOut;\n"
            << copyOutputsVtx.str()
            << "return vtxOut;\n"
            << "}\n";
    
    //Compute Function Body
    {
        //TODO:
        //  -   Add a way to get __baseVertex, this is built in for VS. Can't use setStageInRegion because we want to access more than 1 vertex
        //  -   Add a way to get __instanceID
        //  -   Add proper support for quads. Most code allows using N vertices per prim but there is no triangulation for quads etc.
        //  -   Do we need to handle any additional vertex/index buffer data offsets in-shader? I don't think so?
        
        //NOTES:
        //  -   Need to dispatch numPrimitives amount of threads.
        //  -   Need to bind the index buffer to appropriate index.
        
        const bool expandVertexData = false;    //Whether to expand vertex data to N unique verts per prim or use the regalur shared vertex approach
        //NOTE: We don't know exactly which verts are going to be used in the call. We also don't yet have a mechanism to prevent processing the same
        //vertex multiple times. For now, this means not expanding data is almost as costly as expanding it apart from reduced memory requirements.
        
        //Add a binding for the Vertex output generated in compute, and a binding for compute argument buffer
        const UInt32 computeVSArgSlot = location++;
        const UInt32 computeVSOutputSlot = location++;
        mslProgram->AddBinding("computeVSArg", computeVSArgSlot, kMSL_BindingType_ComputeVSArg, kMSL_ProgramStage_Compute);
        mslProgram->AddBinding("computeVSOutput", computeVSOutputSlot, kMSL_BindingType_ComputeVSOutput, kMSL_ProgramStage_Compute);
        //Generate the compute shader that calls the vertex shader code and outputs the vertices.
        UInt32 numVerticesPerThread = 3;
        glueVS  << "#if HD_MTL_COMPUTESHADER\n"
                << "vec3 Unpack10_10_10_2_ToVec3(uint u_packedNormal) {\n"
                << "    int i_packedNormal = *(thread int*)&u_packedNormal;\n"
                << "    return vec3(i_packedNormal & 0x3FF, (i_packedNormal >> 10) & 0x3FF,(i_packedNormal >> 20) & 0x3FF) / 511.0;\n"
                << "}\n"
                << "#define vec2 packed_float2\n"
                << "#define vec3 packed_float3\n"
                << "struct MSLComputeVSArgs { uint indexCount, baseVertex; };"
                << glueCompute.str()
                << "\n    , device const MSLComputeVSArgs *computeVSArg[[buffer(" << computeVSArgSlot << ")]]\n"
                << "    , device MSLVtxOutputs *computeVSOutput[[buffer(" << computeVSOutputSlot << ")]])\n"
                << "{\n"
                << "#undef vec3\n"  //Need to be a bit careful, vec3 used to be degined as float3 before this.
                << "#undef vec2\n"
                << "    uint __baseIndexID = threadPositionInGrid * " << numVerticesPerThread << ";\n"
                << "    uint __instanceID = __baseIndexID % computeVSArg[0].indexCount;\n" // <-- MTL_FIXME
                << "    uint __baseVertex = computeVSArg[0].baseVertex;\n" // <-- MTL_FIXME
                << "\n"
                << "    uint __vertexIDs[" << numVerticesPerThread << "];\n"
                << "    for(uint i = 0; i < " << numVerticesPerThread << "; i++) {\n"
                << "        uint __indexID = __baseIndexID + i;\n"
                << "        __vertexIDs[i] = __baseVertex + indices[__indexID];\n"
                << "    }\n"
                << "\n"
                << "    for(uint i = 0; i < " << numVerticesPerThread << "; i++) {\n"
                << "        uint __indexID = __baseIndexID + i;\n"
                << "        uint gl_VertexID = __vertexIDs[i];\n"
                << "        uint gl_InstanceID = __instanceID;\n"
                << "        uint gl_BaseVertex = __baseVertex;\n"
                << "        MSLVtxInputs vtxInputs = { //"
                << copyInputsVtxStruct_Compute.str() << "\n"
                << "        };\n";
        if(expandVertexData)
            glueVS << "        computeVSOutput[__indexID] = ";
        else
            glueVS << "        computeVSOutput[__vertexIDs[i] - __baseVertex] = ";
        glueVS  << "compute_vertexEntryPoint(vtxInputs\n"
                << copyInputsVtx_Compute.str()
                << "        );\n"
                << "    }\n"
                << "}\n"
                << "#endif //HD_MTL_COMPUTESHADER\n";
        
        //Add new pass-through Vertex Shader that just reads compute output and passes it on to Rasterizer/FS
        glueVS  << "#if HD_MTL_VERTEXSHADER\n"
                << "vertex MSLVtxOutputs vertexPassThroughEntryPoint( uint baseVertex[[base_vertex]],\n"
                << "                             uint vertexID[[vertex_id]],\n"
                << "                             uint baseInstance[[base_instance]],\n"
                << "                             uint instanceID[[instance_id]],\n"
                << "                             device const uint *indices[[buffer(0)]],\n"
                << "                             device const MSLVtxOutputs *cvsData[[buffer(1)]]) {\n";
        if(expandVertexData)
            glueVS << "    uint cvsDataIndex = vertexID;\n";
        else
            glueVS << "    uint cvsDataIndex = indices[vertexID];\n";
        glueVS  << "    return cvsData[cvsDataIndex];\n"
                << "}\n"
                << "#endif //HD_MTL_VERTEXSHADER\n";
    }
    
    //Fragment Glue Function
    
    gluePS << "fragment MSLFragOutputs fragmentEntryPoint(MSLVtxOutputs vsInput[[stage_in]]\n"
           << ", device const MSLFragInputs *" CODEGENMSL_FRAGUNIFORMINPUTNAME "[[buffer(0)]]\n";
    mslProgram->AddBinding(CODEGENMSL_FRAGUNIFORMINPUTNAME, 0, kMSL_BindingType_UniformBuffer, kMSL_ProgramStage_Fragment, 0, inputUniformBufferSize);

    location = 1;

    if (hasTexturing) {
        gluePS << ", MSLTexturing texturing\n";
    }

    // This is the fragment entry point argument list. This takes all inputs that are individual bound buffers
    TF_FOR_ALL(it, _mslPSInputParams) {
        HdSt_CodeGenMSL::TParam const &input = *it;
        TfToken attrib;
        bool isBuffer = true;
        
        if (! (input.usage & HdSt_CodeGenMSL::TParam::EntryFuncArgument)) {
            continue;
        }
//        if (!input.attribute.IsEmpty()) {
//            attrib = input.attribute;
//        }
        if (input.binding.GetType() == HdBinding::FRONT_FACING) {
            attrib = TfToken("[[front_facing]]");
            isBuffer = false;
        }
        else {
            attrib = TfToken(TfStringPrintf("[[buffer(%d)]]", location));
        }
        gluePS << ", ";
        
        std::string n;
        if (input.name.GetText()[0] == '*') {
            gluePS << "device const ";
            if ((input.usage & HdSt_CodeGenMSL::TParam::UniformBlock) != 0)
                n = input.name.GetText() + 4; //Because of "*___<NAME>"
            else
                n = input.name.GetText() + 1;
        }
        else if(input.usage & HdSt_CodeGenMSL::TParam::UniformBlock) {
            gluePS << "device const ";
            n = input.dataType.GetText();
        }
        else
        {
            n = input.name.GetString();
        }

        mslProgram->AddBinding(n, location++, kMSL_BindingType_UniformBuffer, kMSL_ProgramStage_Fragment);

        if (input.usage & HdSt_CodeGenMSL::TParam::ProgramScope) {
            gluePS << " ProgramScope<st>::";
        }
        if (input.name.GetText()[0] != '*' && input.usage & HdSt_CodeGenMSL::TParam::UniformBlock)
            gluePS << input.dataType << "* " << input.name << attrib << "\n";
        else
            gluePS << input.dataType << " " << input.name << attrib << "\n";
    }

    gluePS  << ") {\n"
            << "ProgramScope<st> scope;\n"
            << copyInputsFrag.str()
            << "scope.main();\n"
            << "MSLFragOutputs fragOut;\n"
            << copyOutputsFrag.str()
            << "return fragOut;\n"
            << "}\n";
    
    METAL_DEBUG_COMMENT(&glueVS, "End of _GenerateGlue(glueVS)\n"); //MTL_FIXME
    METAL_DEBUG_COMMENT(&gluePS, "End of _GenerateGlue(gluePS)\n"); //MTL_FIXME
    
}

HdStProgramSharedPtr
HdSt_CodeGenMSL::Compile()
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();
    
    // shader sources
    // geometric shader owns main()
    std::string vertexShader =
    _geometricShader->GetSource(HdShaderTokens->vertexShader);
    std::string tessControlShader =
    _geometricShader->GetSource(HdShaderTokens->tessControlShader);
    std::string tessEvalShader =
    _geometricShader->GetSource(HdShaderTokens->tessEvalShader);
    std::string geometryShader =
    _geometricShader->GetSource(HdShaderTokens->geometryShader);
    std::string fragmentShader =
    _geometricShader->GetSource(HdShaderTokens->fragmentShader);
    
    bool hasVS  = (!vertexShader.empty());
    bool hasTCS = (!tessControlShader.empty());
    bool hasTES = (!tessEvalShader.empty());
    bool hasGS  = (!geometryShader.empty());
    bool hasFS  = (!fragmentShader.empty());

    
    // create MSL program.
    HdStMSLProgramSharedPtr mslProgram(new HdStMSLProgram(HdTokens->drawingShader));
    
    // initialize autogen source buckets
    _genCommon.str(""); _genVS.str(""); _genTCS.str(""); _genTES.str("");
    _genGS.str(""); _genFS.str(""); _genCS.str("");
    _procVS.str(""); _procTCS.str(""), _procTES.str(""), _procGS.str("");
    
    // Metal conversion defines

    GarchContextCaps const &caps = GarchResourceFactory::GetInstance()->GetContextCaps();
    
    METAL_DEBUG_COMMENT(&_genCommon, "Compile()\n"); //MTL_FIXME
    
    // Used in glslfx files to determine if it is using new/old
    // imaging system. It can also be used as API guards when
    // we need new versions of Hydra shading.
    _genCommon  << "#define HD_SHADER_API " << HD_SHADER_API << "\n";
    _genCommon  << "#define ARCH_GFX_METAL\n";
    
    _genCommon  << "#include <metal_stdlib>\n"
                << "#include <simd/simd.h>\n"
                << "#include <metal_pack>\n"
                << "using namespace metal;\n";
    
    _genCommon  << "#define double float\n"
                << "#define vec2 float2\n"
                << "#define vec3 float3\n"
                << "#define vec4 float4\n"
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
                << "#define dmat3 float3x3\n"
                << "#define dmat4 float4x4\n";
    
    // XXX: this macro is still used in GlobalUniform.
    _genCommon  << "#define MAT4 mat4\n";
    
    // a trick to tightly pack vec3 into SSBO/UBO.
    _genCommon  << _GetPackedTypeDefinitions();
    
    _genCommon  << "#define in /*in*/\n"
                << "#define out /*out*/\n"
                << "#define discard discard_fragment();\n"
                << "#define radians(d) (d * 0.01745329252)\n"
                << "#define noperspective /*center_no_perspective MTL_FIXME*/\n"
                << "#define greaterThan(a,b) (a > b)\n"
                << "#define lessThan(a,b)    (a < b)\n";
    
    // wrapper for type float and int to deal with .x accessors and the like that are valid in GLSL
    _genCommon  << "struct wrapped_float {\n"
                << "    union {\n"
                << "        float x;\n"
                << "        float xx;\n"
                << "        float xxx;\n"
                << "        float xxxx;\n"
                << "    };\n"
                << "    operator float () {\n"
                << "        return x;\n"
                << "    }\n"
                << "};\n";
    
    _genCommon  << "struct wrapped_int {\n"
                << "    union {\n"
                << "        int x;\n"
                << "        int xx;\n"
                << "        int xxx;\n"
                << "        int xxxx;\n"
                << "    };\n"
                << "    operator int () {\n"
                << "        return x;\n"
                << "    }\n"
                << "};\n";

    // Fixes for Geometry shader functionality
    std::stringstream vsConfigString;
    std::stringstream fsConfigString;
    std::stringstream gsConfigString;
    bool enableFragmentNormalReconstruction = false;
    {
        std::vector<std::string> commonSourceKeys = _geometricShader->GetSourceKeys(HdShaderTokens->commonShaderSource);
        vsConfigString << "\n//\n//\tCommon GLSLFX Config:\n//\n";
        fsConfigString << "\n//\n//\tCommon GLSLFX Config:\n//\n";
        gsConfigString << "\n//\n//\tCommon GLSLFX Config:\n//\n";
        for(auto key : commonSourceKeys) {
            vsConfigString << "//\t\t" << key << "\n";
            fsConfigString << "//\t\t" << key << "\n";
            gsConfigString << "//\t\t" << key << "\n";
        }
        
        { //VS
            std::vector<std::string> sourceKeys = _geometricShader->GetSourceKeys(HdShaderTokens->vertexShader);
            vsConfigString << "//\n\n//\n//\tVertex GLSLFX Config:\n//\n";
            for(auto key : sourceKeys) {
                vsConfigString << "//\t\t" << key << "\n";
            }
            vsConfigString << "//\n\n";
        }
        { //FS
            std::vector<std::string> sourceKeys = _geometricShader->GetSourceKeys(HdShaderTokens->fragmentShader);
            fsConfigString << "//\n\n//\n//\tFragment GLSLFX Config:\n//\n";
            for(auto key : sourceKeys) {
                fsConfigString << "//\t\t" << key << "\n";
            }
            fsConfigString << "//\n\n";
        }
        { //GS
            std::vector<std::string> sourceKeys = _geometricShader->GetSourceKeys(HdShaderTokens->geometryShader);
            gsConfigString << "//\n\n//\n//\tGeometry GLSLFX Config:\n//\n";
            for(auto key : sourceKeys) {
                if(key == "MeshNormal.Flat") enableFragmentNormalReconstruction = true;
                
                gsConfigString << "//\t\t" << key << "\n";
            }
            gsConfigString << "//\n\n";
        }
    }

    // Start of Program Scope
    
    _genCommon  << "class ProgramScope<st> {\n"
                << "public:\n";
    
    METAL_DEBUG_COMMENT(&_genCommon, "Start of special inputs\n"); //MTL_FIXME
    
    _EmitDeclaration(_genCommon,
                     _mslVSInputParams,
                     TfToken("gl_VertexID"),
                     TfToken("uint"),
                     TfToken("[[vertex_id]]"),
                     HdBinding(HdBinding::VERTEX_ID, 0));
    
    _EmitDeclaration(_genCommon,
                     _mslVSInputParams,
                     TfToken("gl_BaseVertex"),
                     TfToken("uint"),
                     TfToken("[[base_vertex]]"),
                     HdBinding(HdBinding::BASE_VERTEX_ID, 0));
    
    _EmitDeclaration(_genCommon,
                     _mslPSInputParams,
                     TfToken("gl_FrontFacing"),
                     TfToken("bool"),
                     TfToken("[[front_facing]]"),
                     HdBinding(HdBinding::FRONT_FACING, 0));
    
    _EmitDeclaration(_genCommon,
                     _mslVSInputParams,
                     TfToken("gl_InstanceID"),
                     TfToken("uint"),
                     TfToken("[[instance_id]]"),
                     HdBinding(HdBinding::INSTANCE_ID, 0));
    
    METAL_DEBUG_COMMENT(&_genCommon, "End of special inputs\n"); //MTL_FIXME
    
    METAL_DEBUG_COMMENT(&_genCommon, "Start of vertex/fragment interface\n"); //MTL_FIXME
    
    _EmitOutput(_genCommon,
                _mslVSOutputParams,
                TfToken("gl_Position"),
                TfToken("vec4"),
                TfToken("[[position]]")).usage |= TParam::VertexShaderOnly;
    
    _EmitOutput(_genCommon,
                _mslVSOutputParams,
                TfToken("gl_PointSize"),
                TfToken("float"),
                TfToken("[[point_size]]")).usage |= TParam::VertexShaderOnly;
    
    _EmitOutput(_genCommon,
                _mslVSOutputParams,
                TfToken("gl_ClipDistance"),
                TfToken("float"),
                // XXX - Causes an internal error on Lobo - fixed in Liberty 18A281+
                //TfToken("[[clip_distance]]")).usage |= TParam::VertexShaderOnly;
                TfToken("")).usage |= TParam::VertexShaderOnly;

    // _EmitOutput(_genCommon, _mslVSOutputParams, TfToken("gl_PrimitiveID"), TfToken("uint"), TfToken("[[flat]]"));
    // XXX - Hook this up somehow. Output from the vertex shader perhaps?
    _genCommon << "uint gl_PrimitiveID = 0;\n";
    
    METAL_DEBUG_COMMENT(&_genCommon, "End of vertex/fragment interface\n"); //MTL_FIXME
   
    METAL_DEBUG_COMMENT(&_genCommon, "_metaData.customBindings\n"); //MTL_FIXME
    
    // ------------------
    // Custom Buffer Bindings
    // ----------------------
    // For custom buffer bindings, more code can be generated; a full spec is
    // emitted based on the binding declaration.
    // MTL_IMPROVE - In Metal we're going to end up with a binding per buffer even though these will (all?) effectively be uniforms, perhaps it might be better to pack all into a single struct
    if(_metaData.customBindings.size()) {

        TF_FOR_ALL(binDecl, _metaData.customBindings) {
            _genCommon << "#define "
            << binDecl->name << "_Binding "
            << binDecl->binding.GetLocation() << "\n";
            _genCommon << "#define HD_HAS_" << binDecl->name << " 1\n";
            
            // typeless binding doesn't need declaration nor accessor.
            if (binDecl->dataType.IsEmpty()) continue;

            char const* indexStr = NULL;
            if (binDecl->binding.GetType() == HdBinding::SSBO)
            {
                indexStr = "localIndex";
                _EmitDeclarationPtr(_genCommon, _mslVSInputParams, binDecl->name, binDecl->dataType, TfToken(), binDecl->binding);
                std::stringstream dummy;
                _EmitDeclarationPtr(dummy, _mslPSInputParams, binDecl->name, binDecl->dataType, TfToken(), binDecl->binding);
            }
            else
            {
                _EmitDeclaration(_genCommon, _mslVSInputParams, binDecl->name, binDecl->dataType, TfToken(), binDecl->binding);
                std::stringstream dummy;
                _EmitDeclaration(dummy, _mslPSInputParams, binDecl->name, binDecl->dataType, TfToken(), binDecl->binding);
            }

            _EmitAccessor(_genCommon,
                          binDecl->name,
                          binDecl->dataType,
                          binDecl->binding,
                          indexStr);
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
            _genCommon << "#define HD_HAS_" << dbIt->name << " 1\n";
            declarations << "  " << dbIt->dataType
            << " " << dbIt->name;
            if (dbIt->arraySize > 1) {
                _genCommon << "#define HD_NUM_" << dbIt->name
                << " " << dbIt->arraySize << "\n";
                declarations << "[" << dbIt->arraySize << "]";
            }
            declarations <<  ";\n";
            
            _EmitStructAccessor(accessors, varName,
                                dbIt->name, dbIt->dataType, dbIt->arraySize,
                                true, NULL);
        }
        
        declarations << "};\n";
        _EmitDeclarationPtr(declarations, _mslVSInputParams, varName, typeName, TfToken(), binding, 0, true);
        std::stringstream dummy;
        _EmitDeclarationPtr(dummy, _mslPSInputParams, varName, typeName, TfToken(), binding, 0, true);
        
    }
    _genCommon << declarations.str() << accessors.str();
    METAL_DEBUG_COMMENT(&_genCommon, "END OF _metaData.customInterleavedBindings\n"); //MTL_FIXME
    
    
    // HD_NUM_PATCH_VERTS, HD_NUM_PRIMTIIVE_VERTS
    if (_geometricShader->IsPrimTypePatches()) {
        _genCommon << "#define HD_NUM_PATCH_VERTS "
        << _geometricShader->GetPrimitiveIndexSize() << "\n";
    }
    _genCommon << "#define HD_NUM_PRIMITIVE_VERTS "
    << _geometricShader->GetNumPrimitiveVertsForGeometryShader()
    << "\n";
    
    // include Mtlf ptex utility (if needed)
    TF_FOR_ALL (it, _metaData.shaderParameterBinding) {
        HdBinding::Type bindingType = it->first.GetType();
        if (bindingType == HdBinding::TEXTURE_PTEX_TEXEL ||
            bindingType == HdBinding::BINDLESS_TEXTURE_PTEX_TEXEL) {
            _genCommon << _GetPtexTextureShaderSource();
            break;
        }
    }
    
    // primvar existence macros
    
    // XXX: this is temporary, until we implement the fallback value definition
    // for any primvars used in glslfx.
    // Note that this #define has to be considered in the hash computation
    // since it changes the source code. However we have already combined the
    // entries of instanceData into the hash value, so it's not needed to be
    // added separately, at least in current usage.
    TF_FOR_ALL (it, _metaData.constantData) {
        TF_FOR_ALL (pIt, it->second.entries) {
            _genCommon << "#define HD_HAS_" << pIt->name << " 1\n";
        }
    }
    TF_FOR_ALL (it, _metaData.instanceData) {
        _genCommon << "#define HD_HAS_INSTANCE_" << it->second.name << " 1\n";
        _genCommon << "#define HD_HAS_"
        << it->second.name << "_" << it->second.level << " 1\n";
    }
    _genCommon << "#define HD_INSTANCER_NUM_LEVELS "
               << _metaData.instancerNumLevels << "\n"
               << "#define HD_INSTANCE_INDEX_WIDTH "
               << (_metaData.instancerNumLevels+1) << "\n";
    if (!_geometricShader->IsPrimTypePoints()) {
        TF_FOR_ALL (it, _metaData.elementData) {
            _genCommon << "#define HD_HAS_" << it->second.name << " 1\n";
        }
        if (hasGS) {
            TF_FOR_ALL (it, _metaData.fvarData) {
                _genCommon << "#define HD_HAS_" << it->second.name << " 1\n";
            }
        }
    }
    TF_FOR_ALL (it, _metaData.vertexData) {
        _genCommon << "#define HD_HAS_" << it->second.name << " 1\n";
    }
    TF_FOR_ALL (it, _metaData.shaderParameterBinding) {
        _genCommon << "#define HD_HAS_" << it->second.name << " 1\n";
    }
    
    // prep interstage plumbing function
    _procVS  << "void ProcessPrimvars() {\n";
    _procTCS << "void ProcessPrimvars() {\n";
    _procTES << "void ProcessPrimvars(float u, float v, int i0, int i1, int i2, int i3) {\n";
    
    // generate drawing coord and accessors
    _GenerateDrawingCoord();

    // mixin shaders
    _genCommon << _geometricShader->GetSource(HdShaderTokens->commonShaderSource);
    TF_FOR_ALL(it, _shaders) {
        _genCommon << (*it)->GetSource(HdShaderTokens->commonShaderSource);
    }
    
    // geometry shader plumbing
    switch(_geometricShader->GetPrimitiveType())
    {
        case HdSt_GeometricShader::PrimitiveType::PRIM_MESH_REFINED_QUADS:
        case HdSt_GeometricShader::PrimitiveType::PRIM_MESH_REFINED_TRIANGLES:
        case HdSt_GeometricShader::PrimitiveType::PRIM_MESH_PATCHES:
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
            << "   vec2 localST = vec2[](vec2(0,0), vec2(1,0), vec2(1,1), vec2(0,1))[index];\n";
            break;
        }
            
        case HdSt_GeometricShader::PrimitiveType::PRIM_MESH_COARSE_TRIANGLES:
        {
            // barycentric interpolation
            _procGS  << "void ProcessPrimvars(int index) {\n"
            << "   vec2 localST = vec2[](vec2(1,0), vec2(0,1), vec2(0,0))[index];\n";
            break;
        }
            
        default: // points, basis curves
            // do nothing. no additional code needs to be generated.
            ;
    }
    
    // generate primvars
    _GenerateConstantPrimvar();
    _GenerateInstancePrimvar();
    _GenerateElementPrimvar();
    _GenerateVertexAndFaceVaryingPrimvar(hasGS);
    
    //generate shader parameters
    _GenerateShaderParameters();
    
    // finalize buckets
    _procVS  << "}\n";
    _procGS  << "}\n";
    _procTCS << "}\n";
    _procTES << "}\n";
    
    // insert interstage primvar plumbing procs into genVS/TCS/TES/GS
    _genVS  << _procVS.str();
    _genTCS << _procTCS.str();
    _genTES << _procTES.str();
    _genGS  << _procGS.str();
    
    // insert fixes for unsupported functionality
    
    if(enableFragmentNormalReconstruction) {
        _genFS << "#define __RECONSTRUCT_FLAT_NORMAL 1\n";  //See meshNormal.glslfx
    }
    
    // other shaders (renderpass, lighting, surface) first
    TF_FOR_ALL(it, _shaders) {
        HdStShaderCodeSharedPtr const &shader = *it;
        if (hasVS)
            _genVS  << shader->GetSource(HdShaderTokens->vertexShader);
        if (hasTCS)
            _genTCS << shader->GetSource(HdShaderTokens->tessControlShader);
        if (hasTES)
            _genTES << shader->GetSource(HdShaderTokens->tessEvalShader);
        if (hasGS)
            _genGS  << shader->GetSource(HdShaderTokens->geometryShader);
        if (hasFS)
            _genFS  << shader->GetSource(HdShaderTokens->fragmentShader);
    }
    
    // OpenSubdiv tessellation shader (if required)
    if (tessControlShader.find("OsdPerPatchVertexBezier") != std::string::npos) {
        _genTCS << OpenSubdiv::Osd::GLSLPatchShaderSource::GetCommonShaderSource();
        _genTCS << "MAT4 GetWorldToViewMatrix();\n";
        _genTCS << "MAT4 GetProjectionMatrix();\n";
        _genTCS << "float GetTessLevel();\n";
        // we apply modelview in the vertex shader, so the osd shaders doesn't need
        // to apply again.
        _genTCS << "mat4 OsdModelViewMatrix() { return mat4(1); }\n";
        _genTCS << "mat4 OsdProjectionMatrix() { return mat4(GetProjectionMatrix()); }\n";
        _genTCS << "int OsdPrimitiveIdBase() { return 0; }\n";
        _genTCS << "float OsdTessLevel() { return GetTessLevel(); }\n";
    }
    if (tessEvalShader.find("OsdPerPatchVertexBezier") != std::string::npos) {
        _genTES << OpenSubdiv::Osd::GLSLPatchShaderSource::GetCommonShaderSource();
        _genTES << "mat4 OsdModelViewMatrix() { return mat4(1); }\n";
    }
    if (geometryShader.find("OsdInterpolatePatchCoord") != std::string::npos) {
        _genGS <<  OpenSubdiv::Osd::GLSLPatchShaderSource::GetCommonShaderSource();
    }
    if (fragmentShader.find("vec4 GetPatchCoord(int ") == std::string::npos) {
        _genFS << "vec4 GetPatchCoord(int localIndex) { return vec4(1); }\n";
    }
    
    // geometric shader
    _genVS  << vertexShader;
    _genTCS << tessControlShader;
    _genTES << tessEvalShader;
    _genGS  << geometryShader;
    _genFS  << fragmentShader;
    
    // Sanity check that if you provide a control shader, you have also provided
    // an evaluation shader (and vice versa)
    if (hasTCS ^ hasTES) {
        TF_CODING_ERROR(
                        "tessControlShader and tessEvalShader must be provided together.");
        hasTCS = hasTES = false;
    };
    
    std::stringstream termination;
    termination << "}; // ProgramScope<st>\n";
    
    // Externally sourced glslfx translation to MSL
    _ParseGLSL(_genVS, _mslVSInputParams, _mslVSOutputParams);
    _ParseGLSL(_genFS, _mslPSInputParams, _mslPSOutputParams);

    // MSL<->Metal API plumbing
    std::stringstream glueVS, gluePS;
    glueVS.str(""); gluePS.str("");
    
    _GenerateGlue(glueVS, gluePS, mslProgram);

    bool shaderCompiled = true;
    // compile shaders
    // note: _vsSource, _fsSource etc are used for diagnostics (see header)
    if (hasVS) {
        _vsSource = vsConfigString.str() + _genCommon.str() + _genVS.str() + termination.str() + glueVS.str();
        _vsSource = replaceStringAll(_vsSource, "<st>", "_Vert");

        if (!mslProgram->CompileShader(GL_VERTEX_SHADER, _vsSource)) {
            shaderCompiled = false;
        }
        mslProgram->SetVertexOutputStructSize(_mslVSOutputStructSize);
    }
    if (hasFS) {
        _fsSource = fsConfigString.str() + _genCommon.str() + _genFS.str() + termination.str() + gluePS.str();
        _fsSource = replaceStringAll(_fsSource, "<st>", "_Frag");

        if (!mslProgram->CompileShader(GL_FRAGMENT_SHADER, _fsSource)) {
            shaderCompiled = false;
        }
    }
    if (hasTCS) {
        _tcsSource = _genCommon.str() + _genTCS.str() + termination.str();
        if (!mslProgram->CompileShader(GL_TESS_CONTROL_SHADER, _tcsSource)) {
            shaderCompiled = false;
        }
    }
    if (hasTES) {
        _tesSource = _genCommon.str() + _genTES.str() + termination.str();
        if (!mslProgram->CompileShader(GL_TESS_EVALUATION_SHADER, _tesSource)) {
            shaderCompiled = false;
        }
    }
    if (hasGS) {
        _gsSource = gsConfigString.str() + _genCommon.str() + _genGS.str() + termination.str();
        if (!mslProgram->CompileShader(GL_GEOMETRY_SHADER, _gsSource)) {
            shaderCompiled = false;
        }
    }
    
    if (!shaderCompiled) {
        return HdStProgramSharedPtr();
    }
    
    return mslProgram;
}

HdStProgramSharedPtr
HdSt_CodeGenMSL::CompileComputeProgram()
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    // initialize autogen source buckets
    _genCommon.str(""); _genVS.str(""); _genTCS.str(""); _genTES.str("");
    _genGS.str(""); _genFS.str(""); _genCS.str("");
    _procVS.str(""); _procTCS.str(""), _procTES.str(""), _procGS.str("");
    
    // GLSL version.
    GarchContextCaps const &caps = GarchResourceFactory::GetInstance()->GetContextCaps();
    _genCommon << "#version " << caps.glslVersion << "\n";

    // Used in glslfx files to determine if it is using new/old
    // imaging system. It can also be used as API guards when
    // we need new versions of Hydra shading. 
    _genCommon << "#define HD_SHADER_API " << HD_SHADER_API << "\n";    
        
    // a trick to tightly pack unaligned data (vec3, etc) into SSBO/UBO.
    _genCommon << _GetPackedTypeDefinitions();

    std::stringstream uniforms;
    std::stringstream declarations;
    std::stringstream accessors;
    
    uniforms << "// Uniform block\n";

    HdBinding uboBinding(HdBinding::UBO, 0);
    uniforms << AddressSpace(uboBinding);
    uniforms << "uniform ubo_" << uboBinding.GetLocation() << " {\n";

    accessors << "// Read-Write Accessors & Mutators\n";
    uniforms << "    int vertexOffset;       // offset in aggregated buffer\n";
    TF_FOR_ALL(it, _metaData.computeReadWriteData) {
        TfToken const &name = it->second.name;
        HdBinding const &binding = it->first;
        TfToken const &dataType = it->second.dataType;
        
        uniforms << "    int " << name << "Offset;\n";
        uniforms << "    int " << name << "Stride;\n";
        
        _EmitDeclaration(declarations,
                _mslVSInputParams,
                name,
                //compute shaders need vector types to be flat arrays
                _GetFlatType(dataType),
                TfToken(),
                binding, 0);
        // getter & setter
        {
            std::stringstream indexing;
            indexing << "(localIndex + vertexOffset)"
                     << " * " << name << "Stride"
                     << " + " << name << "Offset";
            _EmitComputeAccessor(accessors, name, dataType, binding,
                    indexing.str().c_str());
            _EmitComputeMutator(accessors, name, dataType, binding,
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
        _EmitDeclaration(declarations,
                _mslVSInputParams,
                name,
                //compute shaders need vector types to be flat arrays
                _GetFlatType(dataType),
                TfToken(),
                binding, 0);
        // getter
        {
            std::stringstream indexing;
            // no vertex offset for constant data
            indexing << "(localIndex)"
                     << " * " << name << "Stride"
                     << " + " << name << "Offset";
            _EmitComputeAccessor(accessors, name, dataType, binding,
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

    // main
    _genCS << "void main() {\n";
    _genCS << "  int computeCoordinate = int(gl_GlobalInvocationID.x);\n";
    _genCS << "  compute(computeCoordinate);\n";
    _genCS << "}\n";
    
    // create Metal function.
    HdStProgramSharedPtr program(
        new HdStMSLProgram(HdTokens->computeShader));
    
    TF_FATAL_CODING_ERROR("Not Implemented");
    /*
    // compile shaders
    {
        _csSource = _genCommon.str() + _genCS.str();
        if (!program->CompileShader(GL_COMPUTE_SHADER, _csSource)) {
            const char *shaderSources[1];
            shaderSources[0] = _csSource.c_str();
            GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
            glShaderSource(shader, 1, shaderSources, NULL);
            glCompileShader(shader);

            std::string logString;
            HdStGLUtils::GetShaderCompileStatus(shader, &logString);
            TF_WARN("Failed to compile compute shader:\n%s\n",
                    logString.c_str());
            glDeleteShader(shader);
            return HdProgramSharedPtr();
        }
    }
    */
    return program;
}

static std::string _GetSwizzleString(TfToken const& type)
{
    std::string swizzle = "";
    if (type == _tokens->vec4 || type == _tokens->ivec4) {
        // nothing
    } else if (type == _tokens->vec3 || type == _tokens->ivec3) {
        swizzle = ".xyz";
    } else if (type == _tokens->vec2 || type == _tokens->ivec2) {
        swizzle = ".xy";
    } else if (type == _tokens->_float || type == _tokens->_int) {
        swizzle = ".x";
    }
    
    return swizzle;
}

static HdSt_CodeGenMSL::TParam& _EmitDeclaration(std::stringstream &str,
                             HdSt_CodeGenMSL::InOutParams &inputParams,
                             TfToken const &name,
                             TfToken const &type,
                             TfToken const &attribute,
                             HdBinding const &binding,
                             int arraySize)
{
    if (!arraySize) {
          str << type << " " << name << ";\n";
    }
    else {
        str << "device const " << type << " *" << name /* << "[" << arraySize << "]"*/  << ";\n";
    }
    HdSt_CodeGenMSL::TParam in(name, type, TfToken(), attribute, HdSt_CodeGenMSL::TParam::Unspecified, binding, arraySize);
    HdBinding::Type bindingType = binding.GetType();
    if(bindingType == HdBinding::VERTEX_ID ||
       bindingType == HdBinding::BASE_VERTEX_ID ||
       bindingType == HdBinding::INSTANCE_ID ||
       bindingType == HdBinding::FRONT_FACING) {
        in.usage |= HdSt_CodeGenMSL::TParam::EntryFuncArgument;
    }
    
    if(bindingType == HdBinding::UNIFORM)
        in.usage |= HdSt_CodeGenMSL::TParam::Uniform;
 
    inputParams.push_back(in);
   return inputParams.back();
}

static HdSt_CodeGenMSL::TParam& _EmitDeclaration(
    std::stringstream &str,
    HdSt_CodeGenMSL::InOutParams &inputParams,
    HdSt_ResourceBinder::MetaData::BindingDeclaration const &bindingDeclaration,
    TfToken const &attribute = TfToken(),
    int arraySize=0)
{
    return _EmitDeclaration(str,
                            inputParams,
                            bindingDeclaration.name,
                            bindingDeclaration.dataType,
                            attribute,
                            bindingDeclaration.binding,
                            arraySize);
}

static HdSt_CodeGenMSL::TParam& _EmitDeclarationPtr(std::stringstream &str,
                                                    HdSt_CodeGenMSL::InOutParams &inputParams,
                                                    TfToken const &name,
                                                    TfToken const &type,
                                                    TfToken const &attribute,
                                                    HdBinding const &binding,
                                                    int arraySize,
                                                    bool programScope)
{
    // MTL_FIXME - we need to map vec3 device pointers to the packed variants as that's how HYDRA presents its buffers
    // but we should orobably alter type at source not do a last minute fix up here
    TfToken dataType = type;
    if (type == _tokens->vec3) {
        dataType = _tokens->hd_vec3;
    }
    
    TfToken ptrName(std::string("*") + name.GetString());
    str << "device const ";
    if (programScope) {
        str << "ProgramScope<st>::";
    }
    HdSt_CodeGenMSL::TParam& result(_EmitDeclaration(str, inputParams, ptrName, dataType, attribute, binding, arraySize));
    result.usage |= HdSt_CodeGenMSL::TParam::Usage::EntryFuncArgument;
    if (programScope) {
        result.usage |= HdSt_CodeGenMSL::TParam::Usage::ProgramScope;
    }
    return result;
}

static HdSt_CodeGenMSL::TParam& _EmitDeclarationPtr(std::stringstream &str,
                                HdSt_CodeGenMSL::InOutParams &inputParams,
                                HdSt_ResourceBinder::MetaData::BindingDeclaration const &bindingDeclaration,
                                TfToken const &attribute = TfToken(),
                                int arraySize = 0)
{
    return _EmitDeclarationPtr(str,
                               inputParams,
                               bindingDeclaration.name,
                               bindingDeclaration.dataType,
                               attribute,
                               bindingDeclaration.binding,
                               arraySize,
                               false);
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
            str << type << " HdGet_" << name << "(" << "int arrayIndex, int localIndex) {\n"
                << "  return " << structMemberName << "[" << index << "]." << name << "[arrayIndex];\n}\n";
        } else {
            str << type << " HdGet_" << name << "(" << "int localIndex) {\n"
                << "  return " << structMemberName << "[" << index << "]." << name << ";\n}\n";
        }
    } else {
        if (arraySize > 1) {
            str << type << " HdGet_" << name << "(" << "int arrayIndex, int localIndex) { return "
                << structMemberName << ptrAccessor << name << "[arrayIndex];}\n";
        } else {
            str << type << " HdGet_" << name << "(" << "int localIndex) { return "
                << structMemberName << ptrAccessor << name << ";}\n";
        }
    }
    // GLSL spec doesn't allow default parameter. use function overload instead.
    // default to localIndex=0
    if (arraySize > 1) {
        str << type << " HdGet_" << name << "(" << "int arrayIndex)"
            << " { return HdGet_" << name << "(arrayIndex, 0); }\n";
    } else {
        str << type << " HdGet_" << name << "()" << " { return HdGet_" << name << "(0); }\n";
    }
}

static void _EmitComputeAccessor(
                    std::stringstream &str,
                    TfToken const &name,
                    TfToken const &type,
                    HdBinding const &binding,
                    const char *index)
{
    METAL_DEBUG_COMMENT(&str,"_EmitComputeAccessor\n"); //MTL_FIXME
    if (index) {
        str << type
            << " HdGet_" << name << "(int localIndex) {\n"
            << "  int index = " << index << ";\n";
        if (binding.GetType() == HdBinding::TBO) {
            str << "  return texelFetch("
                << name << ", index)" << _GetSwizzleString(type) << ";\n}\n";
        } else if (binding.GetType() == HdBinding::SSBO) {
            str << "  return " << type << "(";
            int numComponents = 1;
            if (type == _tokens->vec2 || type == _tokens->ivec2) {
                numComponents = 2;
            } else if (type == _tokens->vec3 || type == _tokens->ivec3) {
                numComponents = 3;
            } else if (type == _tokens->vec4 || type == _tokens->ivec4) {
                numComponents = 4;
            }
            for (int c = 0; c < numComponents; ++c) {
                if (c > 0) {
                    str << ",\n              ";
                }
                str << name << "[index + " << c << "]";
            }
            str << ");\n}\n";
        } else {
            str << "  return " << _GetPackedTypeAccessor(type) << "("
                << name << "[index]);\n}\n";
        }
    } else {
        // non-indexed, only makes sense for uniform or vertex.
        if (binding.GetType() == HdBinding::UNIFORM || 
            binding.GetType() == HdBinding::VERTEX_ATTR) {
            str << type
                << " HdGet_" << name << "(int localIndex) { return ";
            str << _GetPackedTypeAccessor(type) << "(" << name << ");}\n";
        }
    }
    // GLSL spec doesn't allow default parameter. use function overload instead.
    // default to locaIndex=0
    str << type << " HdGet_" << name << "()"
        << " { return HdGet_" << name << "(0); }\n";
    
}

static void _EmitComputeMutator(
                    std::stringstream &str,
                    TfToken const &name,
                    TfToken const &type,
                    HdBinding const &binding,
                    const char *index)
{
    METAL_DEBUG_COMMENT(&str, "_EmitComputeMutator\n"); //MTL_FIXME
    if (index) {
        str << "void"
            << " HdSet_" << name << "(int localIndex, " << type << " value) {\n"
            << "  int index = " << index << ";\n";
        if (binding.GetType() == HdBinding::SSBO) {
            int numComponents = 1;
            if (type == _tokens->vec2 || type == _tokens->ivec2) {
                numComponents = 2;
            } else if (type == _tokens->vec3 || type == _tokens->ivec3) {
                numComponents = 3;
            } else if (type == _tokens->vec4 || type == _tokens->ivec4) {
                numComponents = 4;
            }
            if (numComponents == 1) {
                str << "  "
                    << name << "[index] = value;\n";
            } else {
                for (int c = 0; c < numComponents; ++c) {
                    str << "  "
                        << name << "[index + " << c << "] = "
                        << "value[" << c << "];\n";
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
        str << type
            << " HdGet_" << name << "(int localIndex) {\n"
            << "  int index = " << index << ";\n";
        if (binding.GetType() == HdBinding::TBO) {
            str << "  return texelFetch("
                << name << ", index)" << _GetSwizzleString(type) << ";\n}\n";
        } else {
            str << "  return " << _GetPackedTypeAccessor(type) << "("
                << name << "[index]);\n}\n";
        }
    } else {
        // non-indexed, only makes sense for uniform or vertex.
        if (binding.GetType() == HdBinding::UNIFORM ||
            binding.GetType() == HdBinding::VERTEX_ATTR ||
            binding.GetType() == HdBinding::SSBO) {
            emitIndexlessVariant = true;
            str << type
                << " HdGet_" << name << "(int localIndex) { return ";
            str << _GetPackedTypeAccessor(type) << "(" << name << ");}\n";
        }
    }
    
    // GLSL spec doesn't allow default parameter. use function overload instead.
    // default to locaIndex=0
    if(emitIndexlessVariant)
        str << type << " HdGet_" << name << "()"
            << " { return HdGet_" << name << "(0); }\n";
    
}

static HdSt_CodeGenMSL::TParam& _EmitOutput(std::stringstream &str,
                                            HdSt_CodeGenMSL::InOutParams &outputParams,
                                            TfToken const &name,
                                            TfToken const &type,
                                            TfToken const &attribute,
                                            HdSt_CodeGenMSL::TParam::Usage usage)
{
    METAL_DEBUG_COMMENT(&str, "_EmitOutput\n"); //MTL_FIXME
    str << type << " " << name << ";\n";
    std::string accessorStr = "";
    if(usage == HdSt_CodeGenMSL::TParam::Usage::PrimVar)
    {
        std::string nameStr = name.GetString();
        size_t prefixLen = strlen(MTL_PRIMVAR_PREFIX);
        accessorStr = nameStr.substr(prefixLen, nameStr.length() - prefixLen);
    }
    TfToken accessorToken(accessorStr);
    HdSt_CodeGenMSL::TParam out(name, type, accessorToken, attribute, usage);
    outputParams.push_back(out);
    return outputParams.back();
}

static HdSt_CodeGenMSL::TParam& _EmitStructMemberOutput(HdSt_CodeGenMSL::InOutParams &outputParams,
                                                        TfToken const &name,
                                                        TfToken const &accessor,
                                                        TfToken const &type,
                                                        TfToken const &attribute,
                                                        HdSt_CodeGenMSL::TParam::Usage usage)
{
    HdSt_CodeGenMSL::TParam out(name, type, accessor, attribute, usage);
    outputParams.push_back(out);
    return outputParams.back();
}

void
HdSt_CodeGenMSL::_GenerateDrawingCoord()
{
    METAL_DEBUG_COMMENT(&_genCommon, "_GenerateDrawingCoord Common\n"); //MTL_FIXME
    METAL_DEBUG_COMMENT(&_genVS,     "_GenerateDrawingCoord VS\n"); //MTL_FIXME
    METAL_DEBUG_COMMENT(&_genFS,     "_GenerateDrawingCoord PS\n"); //MTL_FIXME
    
    TF_VERIFY(_metaData.drawingCoord0Binding.binding.IsValid());
    TF_VERIFY(_metaData.drawingCoord1Binding.binding.IsValid());

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

    _EmitDeclaration(_genVS, _mslVSInputParams, _metaData.drawingCoord0Binding.name, _metaData.drawingCoord0Binding.dataType, TfToken(), _metaData.drawingCoord0Binding.binding);
    _EmitDeclaration(_genVS, _mslVSInputParams, _metaData.drawingCoord1Binding.name, _metaData.drawingCoord1Binding.dataType, TfToken(), _metaData.drawingCoord1Binding.binding);
    
    if (_metaData.drawingCoordIBinding.binding.IsValid()) {
        _EmitDeclaration(_genVS, _mslVSInputParams, _metaData.drawingCoordIBinding.name, _metaData.drawingCoordIBinding.dataType, TfToken(), _metaData.drawingCoord0Binding.binding, /*arraySize=*/std::max(1, _metaData.instancerNumLevels));
    }

    // instance index indirection
    _genCommon << "struct hd_instanceIndex { int indices[HD_INSTANCE_INDEX_WIDTH]; };\n";

    if (_metaData.instanceIndexArrayBinding.binding.IsValid()) {
        // << layout (location=x) uniform (int|ivec[234]) *instanceIndices;
        _EmitDeclarationPtr(_genCommon, _mslVSInputParams, _metaData.instanceIndexArrayBinding);

        // << layout (location=x) uniform (int|ivec[234]) *culledInstanceIndices;
        _EmitDeclarationPtr(_genCommon, _mslVSInputParams, _metaData.culledInstanceIndexArrayBinding);

        /// if \p cullingPass is true, CodeGen generates GetInstanceIndex()
        /// such that it refers instanceIndices buffer (before culling).
        /// Otherwise, GetInstanceIndex() looks up culledInstanceIndices.

        _genVS << "int GetInstanceIndexCoord() {\n"
               << "  return drawingCoord1.y + gl_InstanceID * HD_INSTANCE_INDEX_WIDTH; \n"
               << "}\n";

        if (_geometricShader->IsCullingPass()) {
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
        if (_geometricShader->IsCullingPass()) {
            _genVS << "void SetCulledInstanceIndex(uint instance) "
                      "{ /*no-op*/ };\n";
        }
    }

    TfToken drawingCoordType("hd_drawingCoord");
    TfToken intType("int");
    TfToken tkn_flat("[[flat]]");
    
    _genVS << "hd_drawingCoord vsDrawingCoord;\n";
    _genVS << "hd_drawingCoord gsDrawingCoord;\n";

    TfToken tkn_modelCoord("vsdc_modelCoord");
    TfToken tkn_constantCoord("vsdc_constantCoord");
    TfToken tkn_elementCoord("vsdc_elementCoord");
    TfToken tkn_primitiveCoord("vsdc_primitiveCoord");
    TfToken tkn_fvarCoord("vsdc_fvarCoord");
    TfToken tkn_shaderCoord("vsdc_shaderCoord");
    
    _EmitStructMemberOutput(_mslVSOutputParams, tkn_modelCoord,
        TfToken("vsDrawingCoord.modelCoord"), intType, tkn_flat);
    _mslPSInputParams.push_back(TParam(tkn_modelCoord, intType,
        TfToken("gsDrawingCoord.modelCoord"), tkn_flat, HdSt_CodeGenMSL::TParam::Unspecified));

    _EmitStructMemberOutput(_mslVSOutputParams, tkn_constantCoord,
        TfToken("vsDrawingCoord.constantCoord"), intType, tkn_flat);
    _mslPSInputParams.push_back(TParam(tkn_constantCoord, intType,
        TfToken("gsDrawingCoord.constantCoord"), tkn_flat, HdSt_CodeGenMSL::TParam::Unspecified));
    
    _EmitStructMemberOutput(_mslVSOutputParams, tkn_elementCoord,
        TfToken("vsDrawingCoord.elementCoord"), intType, tkn_flat);
    _mslPSInputParams.push_back(TParam(tkn_elementCoord, intType,
        TfToken("gsDrawingCoord.elementCoord"), tkn_flat, HdSt_CodeGenMSL::TParam::Unspecified));
    
    _EmitStructMemberOutput(_mslVSOutputParams, tkn_primitiveCoord,
        TfToken("vsDrawingCoord.primitiveCoord"), intType, tkn_flat);
    _mslPSInputParams.push_back(TParam(tkn_primitiveCoord, intType,
        TfToken("gsDrawingCoord.primitiveCoord"), tkn_flat, HdSt_CodeGenMSL::TParam::Unspecified));
    
    _EmitStructMemberOutput(_mslVSOutputParams, tkn_fvarCoord,
        TfToken("vsDrawingCoord.fvarCoord"), intType, tkn_flat);
    _mslPSInputParams.push_back(TParam(tkn_fvarCoord, intType,
        TfToken("gsDrawingCoord.fvarCoord"), tkn_flat, HdSt_CodeGenMSL::TParam::Unspecified));

    _EmitStructMemberOutput(_mslVSOutputParams, tkn_shaderCoord,
        TfToken("vsDrawingCoord.shaderCoord"), intType, tkn_flat);
    _mslPSInputParams.push_back(TParam(tkn_shaderCoord, intType,
        TfToken("gsDrawingCoord.shaderCoord"), tkn_flat, HdSt_CodeGenMSL::TParam::Unspecified));
    
    for(int i = 0; i <= _metaData.instancerNumLevels; i++)
    {
        TfToken tkn_instanceIndex(TfStringPrintf("vsdc_instanceIndex%d", i));
        _EmitStructMemberOutput(_mslVSOutputParams,
            tkn_instanceIndex,
            TfToken(TfStringPrintf("vsDrawingCoord.instanceIndex[%d]", i)), intType, tkn_flat);
        _mslPSInputParams.push_back(TParam(tkn_instanceIndex, intType,
            TfToken(TfStringPrintf("gsDrawingCoord.instanceIndex[%d]", i)), tkn_flat,
            HdSt_CodeGenMSL::TParam::Unspecified));
        
        TfToken tkn_instanceCoords(TfStringPrintf("vsdc_instanceCoord%d", i));
        _EmitStructMemberOutput(_mslVSOutputParams,
            tkn_instanceCoords,
            TfToken(TfStringPrintf("vsDrawingCoord.instanceCoords[%d]", i)), intType, tkn_flat);
        _mslPSInputParams.push_back(TParam(tkn_instanceCoords, intType,
            TfToken(TfStringPrintf("gsDrawingCoord.instanceCoords[%d]", i)), tkn_flat,
            HdSt_CodeGenMSL::TParam::Unspecified));
    }
    
    _genVS << "hd_drawingCoord GetDrawingCoord() { hd_drawingCoord dc; \n"
           << "  dc.modelCoord     = drawingCoord0.x; \n"
           << "  dc.constantCoord  = drawingCoord0.y; \n"
           << "  dc.elementCoord   = drawingCoord0.z; \n"
           << "  dc.primitiveCoord = drawingCoord0.w; \n"
           << "  dc.fvarCoord      = drawingCoord1.x; \n"
           << "  dc.shaderCoord    = drawingCoord1.z; \n"
           << "  dc.vertexCoord    = drawingCoord1.w; \n"
           << "  hd_instanceIndex r = GetInstanceIndex();\n"
           << "  for(int i = 0; i < HD_INSTANCER_NUM_LEVELS; ++i)\n"
           << "    dc.instanceIndex[i]  = r.indices[i];\n";

    if (_metaData.drawingCoordIBinding.binding.IsValid()) {
        _genVS << "  for (int i = 0; i < HD_INSTANCER_NUM_LEVELS; ++i) {\n"
               << "    dc.instanceCoords[i] = drawingCoordI[i] \n"
               << "      + GetInstanceIndex().indices[i+1]; \n"
               << "  }\n";
    }

    _genVS << "  return dc;\n"
           << "}\n";

    // note: GL spec says tessellation input array size must be equal to
    //       gl_MaxPatchVertices, which is used for intrinsic declaration
    //       of built-in variables:
    //       in gl_PerVertex {} gl_in[gl_MaxPatchVertices];

    // tess control shader
    _genTCS << "flat in hd_drawingCoord vsDrawingCoord[gl_MaxPatchVertices];\n"
            << "flat out hd_drawingCoord tcsDrawingCoord[HD_NUM_PATCH_VERTS];\n"
            << "hd_drawingCoord GetDrawingCoord() { \n"
            << "  hd_drawingCoord dc = vsDrawingCoord[gl_InvocationID];\n"
            << "  dc.primitiveCoord += gl_PrimitiveID;\n"
            << "  return dc;\n"
            << "}\n";
    // tess eval shader
    _genTES << "flat in hd_drawingCoord tcsDrawingCoord[gl_MaxPatchVertices];\n"
            << "flat out hd_drawingCoord vsDrawingCoord;\n"
            << "flat out hd_drawingCoord gsDrawingCoord;\n"
            << "hd_drawingCoord GetDrawingCoord() { \n"
            << "  hd_drawingCoord dc = tcsDrawingCoord[0]; \n"
            << "  dc.primitiveCoord += gl_PrimitiveID; \n"
            << "  return dc;\n"
            << "}\n";

    // geometry shader ( VSdc + gl_PrimitiveIDIn )
    _genGS << "flat in hd_drawingCoord vsDrawingCoord[HD_NUM_PRIMITIVE_VERTS];\n"
           << "flat out hd_drawingCoord gsDrawingCoord;\n"
           << "hd_drawingCoord GetDrawingCoord() { \n"
           << "  hd_drawingCoord dc = vsDrawingCoord[0]; \n"
           << "  dc.primitiveCoord += gl_PrimitiveIDIn; \n"
           << "  return dc; \n"
           << "}\n";

    // fragment shader ( VSdc + gl_PrimitiveID )
    // note that gsDrawingCoord isn't offsetted by gl_PrimitiveIDIn
    _genFS << "hd_drawingCoord gsDrawingCoord;\n"
           << "hd_drawingCoord GetDrawingCoord() { \n"
           << "  hd_drawingCoord dc = gsDrawingCoord; \n"
           << "  dc.primitiveCoord += gl_PrimitiveID; \n"
           << "  return dc; \n"
           << "}\n";

    // drawing coord plumbing.
    // Note that copying from [0] for multiple input source since the
    // drawingCoord is flat (no interpolation required).
    _procVS  << "  vsDrawingCoord = GetDrawingCoord();\n"
             << "  gsDrawingCoord = GetDrawingCoord();\n";
    _procTCS << "  tcsDrawingCoord[gl_InvocationID] = "
             << "  vsDrawingCoord[gl_InvocationID];\n";
    _procTES << "  vsDrawingCoord = tcsDrawingCoord[0];\n"
             << "  gsDrawingCoord = tcsDrawingCoord[0];\n";
    _procGS  << "  gsDrawingCoord = vsDrawingCoord[0];\n";
    
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
          vec4 color;
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
      vec4 HdGet_color(int localIndex) {
          return constantData0[GetConstantCoord()].color;
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
            _mslPSInputParams.push_back(in);
            _mslVSInputParams.push_back(in);
        }

        declarations << "struct " << typeName << " {\n";

        TF_FOR_ALL (dbIt, it->second.entries) {
            if (!TF_VERIFY(!dbIt->dataType.IsEmpty(),
                              "Unknown dataType for %s",
                              dbIt->name.GetText())) {
                continue;
            }

            declarations << "  " << dbIt->dataType
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

        std::stringstream n;
        n << it->second.name << "_" << level;
        TfToken name(n.str());
        n.str("");
        n << "GetDrawingCoord().instanceCoords[" << level << "]";

        // << layout (location=x) uniform float *translate_0;
        _EmitDeclarationPtr(declarations, _mslVSInputParams, name, dataType, TfToken(), binding);
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
        accessors << it->second.dataType
                  << " HdGetInstance_" << it->first << "(int level, "
                  << it->second.dataType << " defaultValue) {\n";
        TF_FOR_ALL (levelIt, it->second.levels) {
            accessors << "  if (level == " << *levelIt << ") "
                      << "return HdGet_" << it->first << "_" << *levelIt << "();\n";
        }

        accessors << "  return defaultValue;\n"
                  << "}\n";
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

    To map a refined face to its coarse face, Hydra builds a "primitive param"
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
          vec4 color;
      };
      layout (std430, binding=?) buffer buffer0 {
          ElementData0 elementData0[];
      };

      // ---------uniform primvar data accessor ---------
      vec4 HdGet_color(int localIndex) {
          return elementData0[GetAggregatedElementID()].color;
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
    //  faceId     | 28   | the faceId of the patch (Hydra uses ptexIndex)
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

        HdBinding binding = _metaData.primitiveParamBinding.binding;
        TParam& entry(_EmitDeclarationPtr(declarations, _mslPSInputParams, _metaData.primitiveParamBinding));
        entry.usage |= TParam::EntryFuncArgument;

        _EmitAccessor(accessors, _metaData.primitiveParamBinding.name,
                        _metaData.primitiveParamBinding.dataType, binding,
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
                << "  return (hd_int_get<st>(HdGet_primitiveParam()));\n"
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

                case HdSt_GeometricShader::PrimitiveType::PRIM_MESH_PATCHES:
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
                      _geometricShader->GetPrimitiveType());
                }
            }

            // GetFVarIndex
            if (_geometricShader->IsPrimTypeTriangles()) {
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
                << "  return (hd_int_get<st>(HdGet_primitiveParam()) >> 2);\n"
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
                  _geometricShader->GetPrimitiveType());
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
        
        _EmitDeclarationPtr(declarations, _mslPSInputParams, _metaData.edgeIndexBinding);
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
            << "  return HdGet_edgeIndices()[abs(primitiveEdgeID)];\n;"
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
            TfToken const &dataType = it->second.dataType;

        // MTL_FIXME - changing from VS Input params to PS because none of this appaears to be associated with vertex shaders at all... (so possibly nothing to fix)
        _EmitDeclarationPtr(declarations, _mslPSInputParams, name, dataType, TfToken(), binding);
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

        _EmitDeclaration(vertexInputs, _mslVSInputParams, name, dataType, TfToken(), binding);

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
        _procGS  << "  outPrimvars." << name
                 << " = inPrimvars[index]." << name << ";\n";
        
        //Pass-through for primvars to cover for missing geometry shaders in Metal
        {
            std::stringstream vtxOutName;
            vtxOutName << MTL_PRIMVAR_PREFIX << name;
            //Add primvars to vtxOut struct
            std::stringstream dummy;
            TfToken vtxOutName_Token(vtxOutName.str());
            _EmitOutput(dummy, _mslVSOutputParams, vtxOutName_Token, dataType, TfToken(), HdSt_CodeGenMSL::TParam::Usage::PrimVar);
        
            //Copy primvars to correct places in PS
            HdSt_CodeGenMSL::TParam in(name, dataType, vtxOutName_Token, TfToken(), HdSt_CodeGenMSL::TParam::Usage::PrimVar);
            _mslPSInputParams.push_back(in);
        }
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
            TfToken const &dataType = it->second.dataType;

            _EmitDeclaration(fvarDeclarations, _mslVSInputParams, name, dataType, TfToken(), binding);

            interstageStruct << "  " << dataType << " " << name << ";\n";

            // primvar accessors (only in GS and FS)
            TfToken readStructName(std::string("in") + structName.GetString());
            _EmitAccessor(accessorsGS, name, dataType, binding, "GetFVarIndex(localIndex)");
            _EmitStructAccessor(accessorsFS, readStructName, name, dataType,
                                /*arraySize=*/1, false, NULL);

            // interstage plumbing
            _procVS << "  outPrimvars." << name
                    << " = " << name << ";\n";
            _procTCS << "  outPrimvars[gl_InvocationID]." << name
                     << " = inPrimvars[gl_InvocationID]." << name << ";\n";
            // TODO: facevarying tessellation
            _procTES << "  outPrimvars->" << name
                     << " = mix(mix(inPrimvars[i3]." << name
                     << "         , inPrimvars[i2]." << name << ", u),"
                     << "       mix(inPrimvars[i1]." << name
                     << "         , inPrimvars[i0]." << name << ", u), v);\n";


            switch(_geometricShader->GetPrimitiveType())
            {
                case HdSt_GeometricShader::PrimitiveType::PRIM_MESH_COARSE_QUADS:
                case HdSt_GeometricShader::PrimitiveType::PRIM_MESH_REFINED_QUADS:
                case HdSt_GeometricShader::PrimitiveType::PRIM_MESH_PATCHES:
                {
                    // linear interpolation within a quad.
                    _procGS << "   outPrimvars->" << name
                        << "  = mix("
                        << "mix(" << "HdGet_" << name << "(0),"
                        <<           "HdGet_" << name << "(1), localST.x),"
                        << "mix(" << "HdGet_" << name << "(3),"
                        <<           "HdGet_" << name << "(2), localST.x), localST.y);\n";
                    break;
                }

                case HdSt_GeometricShader::PrimitiveType::PRIM_MESH_REFINED_TRIANGLES:
                case HdSt_GeometricShader::PrimitiveType::PRIM_MESH_COARSE_TRIANGLES:
                {
                    // barycentric interpolation within a triangle.
                    _procGS << "   outPrimvars->" << name
                        << "  = HdGet_" << name << "(0) * localST.x "
                        << "  + HdGet_" << name << "(1) * localST.y "
                        << "  + HdGet_" << name << "(2) * (1-localST.x-localST.y);\n";
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
                                    _geometricShader->GetPrimitiveType());
            }
            
            //Pass-through for primvars to cover for missing geometry shaders in Metal
            {
                std::stringstream vtxOutName;
                vtxOutName << MTL_PRIMVAR_PREFIX << name;
                //Add primvars to vtxOut struct
                std::stringstream dummy;
                TfToken vtxOutName_Token(vtxOutName.str());
                _EmitOutput(dummy, _mslVSOutputParams, vtxOutName_Token, dataType, TfToken(), HdSt_CodeGenMSL::TParam::Usage::PrimVar);
                
                //Copy primvars to correct places in PS
                HdSt_CodeGenMSL::TParam in(name, dataType, vtxOutName_Token, TfToken(), HdSt_CodeGenMSL::TParam::Usage::PrimVar);
                _mslPSInputParams.push_back(in);
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
           << interstageStruct.str()
           << " inPrimvars[HD_NUM_PRIMITIVE_VERTS];\n"
           << interstageStruct.str()
           << " outPrimvars;\n"
           << accessorsGS.str();

    _genFS << interstageStruct.str()
           << " inPrimvars;\n"
           << accessorsFS.str();

    // ---------
    _genFS << "vec4 GetPatchCoord() { return GetPatchCoord(0); }\n";

    _genGS << "vec4 GetPatchCoord(int localIndex);\n";
    
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
            declarations << "  " << dbIt->dataType
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
        _EmitDeclarationPtr(declarations, _mslVSInputParams, varName, typeName, TfToken(), binding, arraySize, true);
        std::stringstream dummy;
        _EmitDeclarationPtr(dummy, _mslPSInputParams, varName, typeName, TfToken(), binding, arraySize, true);
        break;
    }
    
    _genVS << declarations.str()
           << accessors.str();

    // accessors.
    TF_FOR_ALL (it, _metaData.shaderParameterBinding) {

        // adjust datatype
        std::string swizzle = _GetSwizzleString(it->second.dataType);

        HdBinding::Type bindingType = it->first.GetType();
        if (bindingType == HdBinding::FALLBACK) {
            if (swizzle != ".x")
                swizzle = "";

            // TEMP: FIXME: THIS IS REALY BAD: Sort something better out here. I think there's a Hydra bug
            // causing the diffuseColour to be represented as a float rather than a vec3, which doesn't
            // cause an issue in GLSL but does in MSL
            if (it->second.name == "diffuseColor")
                swizzle = ".x";

            accessors
                << it->second.dataType
                << " HdGet_" << it->second.name << "() {\n"
                << "  int shaderCoord = GetDrawingCoord().shaderCoord; \n"
                << "  return materialParams[shaderCoord]." << it->second.name << swizzle << ";\n"
                << "}\n";
        } else if (bindingType == HdBinding::BINDLESS_TEXTURE_2D) {
            // a function returning sampler2D is allowed in 430 or later
            if (caps.glslVersion >= 430) {
                accessors
                    << "sampler\n"
                    << "HdGetSampler_" << it->second.name << "() {\n"
                    << "  int shaderCoord = GetDrawingCoord().shaderCoord; \n"
                    << "  return sampler2D(materialParams[shaderCoord]." << it->second.name << ");\n"
                    << "  }\n";
            }
            accessors
                << it->second.dataType
                << " HdGet_" << it->second.name << "() {\n"
                << "  int shaderCoord = GetDrawingCoord().shaderCoord; \n"
                << "  return texture(sampler2D(materialParams[shaderCoord]." << it->second.name << "), ";

            if (!it->second.inPrimvars.empty()) {
                accessors 
                    << "\n"
                    << "#if defined(HD_HAS_" << it->second.inPrimvars[0] << ")\n"
                    << " HdGet_" << it->second.inPrimvars[0] << "().xy\n"
                    << "#else\n"
                    << "vec2(0.0, 0.0)\n"
                    << "#endif\n";
            } else {
            // allow to fetch uv texture without sampler coordinate for convenience.
                accessors
                    << " vec2(0.0, 0.0)";
            }
            accessors
                << ")" << swizzle << ";\n"
                << "}\n";
        } else if (bindingType == HdBinding::TEXTURE_2D) {
            declarations
                << "sampler sampler2d_" << it->second.name << ";\n"
                << "texture2d<float> texture2d_" << it->second.name << ";\n";
            
            std::stringstream dummy;
            _EmitOutput(dummy, _mslPSInputParams, TfToken("sampler2d_" + it->second.name.GetString()), TfToken("sampler"), TfToken(), HdSt_CodeGenMSL::TParam::Sampler);
            _EmitOutput(dummy, _mslPSInputParams, TfToken("texture2d_" + it->second.name.GetString()), TfToken("texture2d<float>"), TfToken(), HdSt_CodeGenMSL::TParam::Texture);

            // a function returning sampler2D is allowed in 430 or later
            if (caps.glslVersion >= 430) {
                accessors
                    << "sampler\n"
                    << "HdGetSampler_" << it->second.name << "() {\n"
                    << "  return sampler2d_" << it->second.name << ";"
                    << "}\n";
            }
            // vec4 HdGet_name(vec2 coord) { return texture(sampler2d_name, coord).xyz; }
            accessors
                << it->second.dataType
                << " HdGet_" << it->second.name
                << "(vec2 coord) { return texture2d_" << it->second.name << ".sample(sampler2d_"
                << it->second.name << ", coord)" << swizzle << ";}\n";
            // vec4 HdGet_name() { return HdGet_name(HdGet_st().xy); }
            accessors
                << it->second.dataType
                << " HdGet_" << it->second.name
                << "() { return HdGet_" << it->second.name << "(";
            if (!it->second.inPrimvars.empty()) {
                accessors
                    << "\n"
                    << "#if defined(HD_HAS_" << it->second.inPrimvars[0] << ")\n"
                    << "HdGet_" << it->second.inPrimvars[0] << "().xy\n"
                    << "#else\n"
                    << "vec2(0.0, 0.0)\n"
                    << "#endif\n";
            } else {
                accessors
                    << "vec2(0.0, 0.0)";
            }
            accessors << "); }\n";
        } else if (bindingType == HdBinding::BINDLESS_TEXTURE_PTEX_TEXEL) {
            accessors
                << it->second.dataType
                << " HdGet_" << it->second.name << "(int localIndex) {\n"
                << "  int shaderCoord = GetDrawingCoord().shaderCoord; \n"
                << "  return " << it->second.dataType
                << "(GlopPtexTextureLookup("
                << "samplerArray(materialParams[shaderCoord]." << it->second.name <<"),"
                << "isamplerBuffer(materialParams[shaderCoord]." << it->second.name << "_layout), "
                << "GetPatchCoord(localIndex))" << swizzle << ");\n"
                << "}\n"
                << it->second.dataType
                << " HdGet_" << it->second.name << "()"
                << "{ return HdGet_" << it->second.name << "(0); }\n"
                << it->second.dataType
                << " HdGet_" << it->second.name << "(vec4 patchCoord) {\n"
                << "  int shaderCoord = GetDrawingCoord().shaderCoord; \n"
                << "  return " << it->second.dataType
                << "(GlopPtexTextureLookup("
                << "samplerArray(materialParams[shaderCoord]." << it->second.name <<"),"
                << "isamplerBuffer(materialParams[shaderCoord]." << it->second.name << "_layout), "
                << "patchCoord)" << swizzle << ");\n"
                << "}\n";
        } else if (bindingType == HdBinding::TEXTURE_PTEX_TEXEL) {
            // +1 for layout is by convention.
            declarations
                << "sampler2DArray sampler2darray_" << it->first.GetLocation() << ";\n"
                << "isamplerBuffer isamplerbuffer_" << (it->first.GetLocation()+1) << ";\n";
            accessors
                << it->second.dataType
                << " HdGet_" << it->second.name << "(int localIndex) {\n"
                << "  return " << it->second.dataType
                << "(GlopPtexTextureLookup("
                << "sampler2darray_" << it->first.GetLocation() << ","
                << "isamplerbuffer_" << (it->first.GetLocation()+1) << ","
                << "GetPatchCoord(localIndex))" << swizzle << ");\n"
                << "}\n"
                << it->second.dataType
                << " HdGet_" << it->second.name << "()"
                << "{ return HdGet_" << it->second.name << "(0); }\n"
                << it->second.dataType
                << " HdGet_" << it->second.name << "(vec4 patchCoord) {\n"
                << "  return " << it->second.dataType
                << "(GlopPtexTextureLookup("
                << "sampler2darray_" << it->first.GetLocation() << ","
                << "isamplerbuffer_" << (it->first.GetLocation()+1) << ","
                << "patchCoord)" << swizzle << ");\n"
                << "}\n";
        } else if (bindingType == HdBinding::BINDLESS_TEXTURE_PTEX_LAYOUT) {
            //accessors << it->second.dataType << "(0)";
        } else if (bindingType == HdBinding::TEXTURE_PTEX_LAYOUT) {
            //accessors << it->second.dataType << "(0)";
        } else if (bindingType == HdBinding::PRIMVAR_REDIRECT) {
            // XXX: shader and primvar name collisions are a problem!
            // If this shader and it's connected primvar have the same name, we
            // are good to go, else we must alias the parameter to the primvar
            // accessor.
            if (it->second.name != it->second.inPrimvars[0]) {
                accessors
                    << it->second.dataType
                    << " HdGet_" << it->second.name << "() {\n"
                    << "#if defined(HD_HAS_" << it->second.inPrimvars[0] << ")\n"
                    << "  return HdGet_" << it->second.inPrimvars[0] << "();\n"
                    << "#else\n"
                    << "  return " << it->second.dataType << "(0);\n"
                    << "#endif\n"
                    << "\n}\n"
                    ;
            }
        }
    }
    
    _genFS << declarations.str()
           << accessors.str();

    _genGS << declarations.str()
           << accessors.str();
    
    METAL_DEBUG_COMMENT(&_genFS, "END OF _GenerateShaderParameters()\n"); //MTL_FIXME
    METAL_DEBUG_COMMENT(&_genVS, "END OF _GenerateShaderParameters()\n"); //MTL_FIXME

}

PXR_NAMESPACE_CLOSE_SCOPE

