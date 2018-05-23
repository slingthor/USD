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

#include "pxr/imaging/hdSt/GL/resourceBinderGL.h"
#include "pxr/imaging/hdSt/GL/glslProgram.h"
#include "pxr/imaging/hdSt/bufferResource.h"
#include "pxr/imaging/hdSt/renderContextCaps.h"
#include "pxr/imaging/hdSt/shaderCode.h"
#include "pxr/imaging/hdSt/drawItem.h"
#include "pxr/imaging/hdSt/GL/glConversions.h"

#include "pxr/imaging/hd/bufferArrayRange.h"
#include "pxr/imaging/hd/bufferSpec.h"
#include "pxr/imaging/hd/resource.h"
#include "pxr/imaging/hd/tokens.h"

#include "pxr/base/tf/staticTokens.h"

#include <boost/functional/hash.hpp>

PXR_NAMESPACE_OPEN_SCOPE


TF_DEFINE_PRIVATE_TOKENS(
    _tokens,
    ((_double, "double"))
    ((_float, "float"))
    ((_int, "int"))
    (vec2)
    (vec3)
    (vec4)
    (dvec2)
    (dvec3)
    (dvec4)
    (ivec2)
    (ivec3)
    (ivec4)
    (primitiveParam)
);

namespace {
    struct BindingLocator {
        BindingLocator() :
            uniformLocation(0), uboLocation(0),
            ssboLocation(0), attribLocation(0),
            textureUnit(0) {}

        HdBinding GetBinding(HdBinding::Type type, TfToken const &debugName) {
            switch(type) {
            case HdBinding::UNIFORM:
                return HdBinding(HdBinding::UNIFORM, uniformLocation++);
                break;
            case HdBinding::UBO:
                return HdBinding(HdBinding::UBO, uboLocation++);
                break;
            case HdBinding::SSBO:
                return HdBinding(HdBinding::SSBO, ssboLocation++);
                break;
            case HdBinding::TBO:
                return HdBinding(HdBinding::TBO, uniformLocation++, textureUnit++);
                break;
            case HdBinding::BINDLESS_UNIFORM:
                return HdBinding(HdBinding::BINDLESS_UNIFORM, uniformLocation++);
                break;
            case HdBinding::VERTEX_ATTR:
                return HdBinding(HdBinding::VERTEX_ATTR, attribLocation++);
                break;
            case HdBinding::DRAW_INDEX:
                return HdBinding(HdBinding::DRAW_INDEX, attribLocation++);
                break;
            case HdBinding::DRAW_INDEX_INSTANCE:
                return HdBinding(HdBinding::DRAW_INDEX_INSTANCE, attribLocation++);
                break;
            default:
                TF_CODING_ERROR("Unknown binding type %d for %s",
                                type, debugName.GetText());
                return HdBinding();
                break;
            }
        }

        int uniformLocation;
        int uboLocation;
        int ssboLocation;
        int attribLocation;
        int textureUnit;
    };

    static inline GLboolean _ShouldBeNormalized(HdType type) {
        return type == HdTypeInt32_2_10_10_10_REV;
    }

    // GL has special handling for the "number of components" for
    // packed vectors.  Handle that here.
    static inline int _GetNumComponents(HdType type) {
        if (type == HdTypeInt32_2_10_10_10_REV) {
            return 4;
        } else {
            return HdGetComponentCount(type);
        }
    }
}

HdSt_ResourceBinderGL::HdSt_ResourceBinderGL()
{
}

void
HdSt_ResourceBinderGL::BindBuffer(TfToken const &name,
                                  HdBufferResourceSharedPtr const &buffer,
                                  int offset,
                                  int level) const
{
    HD_TRACE_FUNCTION();

    // it is possible that the buffer has not been initialized when
    // the instanceIndex is empty (e.g. FX points. see bug 120354)
    GLuint bufferId = (GLuint)(uint64_t)buffer->GetId();
    if (bufferId == 0) return;

    HdBinding binding = GetBinding(name, level);
    HdBinding::Type type = binding.GetType();
    int loc              = binding.GetLocation();
    int textureUnit      = binding.GetTextureUnit();

    HdTupleType tupleType = buffer->GetTupleType();

    void const* offsetPtr =
        reinterpret_cast<const void*>(
            static_cast<intptr_t>(offset));
    switch(type) {
    case HdBinding::VERTEX_ATTR:
        glBindBuffer(GL_ARRAY_BUFFER, bufferId);
        glVertexAttribPointer(loc,
                  _GetNumComponents(tupleType.type),
                  HdStGLConversions::GetGLAttribType(tupleType.type),
                  _ShouldBeNormalized(tupleType.type),
                              buffer->GetStride(),
                              offsetPtr);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glEnableVertexAttribArray(loc);
        break;
    case HdBinding::DRAW_INDEX:
        glBindBuffer(GL_ARRAY_BUFFER, bufferId);
        glVertexAttribIPointer(loc,
                               HdGetComponentCount(tupleType.type),
                               GL_INT,
                               buffer->GetStride(),
                               offsetPtr);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glEnableVertexAttribArray(loc);
        break;
    case HdBinding::DRAW_INDEX_INSTANCE:
        glBindBuffer(GL_ARRAY_BUFFER, bufferId);
        glVertexAttribIPointer(loc,
                               HdGetComponentCount(tupleType.type),
                               GL_INT,
                               buffer->GetStride(),
                               offsetPtr);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        // set the divisor to uint-max so that the same base value is used
        // for all instances.
        glVertexAttribDivisor(loc,
                              std::numeric_limits<GLint>::max());
        glEnableVertexAttribArray(loc);
        break;
    case HdBinding::DRAW_INDEX_INSTANCE_ARRAY:
        glBindBuffer(GL_ARRAY_BUFFER, bufferId);
        // instancerNumLevels is represented by the tuple size.
        // We unroll this to an array of int[1] attributes.
        for (size_t i = 0; i < buffer->GetTupleType().count; ++i) {
            offsetPtr = reinterpret_cast<const void*>(offset + i*sizeof(int));
            glVertexAttribIPointer(loc, 1, GL_INT, buffer->GetStride(),
                                   offsetPtr);
            // set the divisor to uint-max so that the same base value is used
            // for all instances.
            glVertexAttribDivisor(loc, std::numeric_limits<GLint>::max());
            glEnableVertexAttribArray(loc);
            ++loc;
        }
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        break;
    case HdBinding::INDEX_ATTR:
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bufferId);
        break;
    case HdBinding::BINDLESS_UNIFORM:
        // at least in nvidia driver 346.59, this query call doesn't show
        // any pipeline stall.
        if (!glIsNamedBufferResidentNV(bufferId)) {
            glMakeNamedBufferResidentNV(bufferId, GL_READ_WRITE);
        }
        glUniformui64NV(loc, buffer->GetGPUAddress());
        break;
    case HdBinding::SSBO:
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, loc,
                         bufferId);
        break;
    case HdBinding::DISPATCH:
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, bufferId);
        break;
    case HdBinding::UBO:
    case HdBinding::UNIFORM:
        glBindBufferRange(GL_UNIFORM_BUFFER, loc,
                          bufferId,
                          offset,
                          buffer->GetStride());
        break;
    case HdBinding::TBO:
        if (loc != HdBinding::NOT_EXIST) {
            glUniform1i(loc, textureUnit);
            glActiveTexture(GL_TEXTURE0 + textureUnit);
            glBindSampler(textureUnit, 0);
            glBindTexture(GL_TEXTURE_BUFFER, buffer->GetTextureBuffer());
        }
        break;
    case HdBinding::TEXTURE_2D:
        // nothing
        break;
    default:
        TF_CODING_ERROR("binding type %d not found for %s",
                        type, name.GetText());
        break;
    }
}

void
HdSt_ResourceBinderGL::UnbindBuffer(TfToken const &name,
                                    HdBufferResourceSharedPtr const &buffer,
                                    int level) const
{
    HD_TRACE_FUNCTION();

    // it is possible that the buffer has not been initialized when
    // the instanceIndex is empty (e.g. FX points)
    if (buffer->GetId() == 0) return;

    HdBinding binding = GetBinding(name, level);
    HdBinding::Type type = binding.GetType();
    int loc = binding.GetLocation();

    switch(type) {
    case HdBinding::VERTEX_ATTR:
        glDisableVertexAttribArray(loc);
        break;
    case HdBinding::DRAW_INDEX:
        glDisableVertexAttribArray(loc);
        break;
    case HdBinding::DRAW_INDEX_INSTANCE:
        glDisableVertexAttribArray(loc);
        glVertexAttribDivisor(loc, 0);
        break;
    case HdBinding::DRAW_INDEX_INSTANCE_ARRAY:
        // instancerNumLevels is represented by the tuple size.
        for (size_t i = 0; i < buffer->GetTupleType().count; ++i) {
            glDisableVertexAttribArray(loc);
            glVertexAttribDivisor(loc, 0);
            ++loc;
        }
        break;
    case HdBinding::INDEX_ATTR:
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        break;
    case HdBinding::BINDLESS_UNIFORM:
        if (glIsNamedBufferResidentNV((GLuint)(uint64_t)buffer->GetId())) {
            glMakeNamedBufferNonResidentNV((GLuint)(uint64_t)buffer->GetId());
        }
        break;
    case HdBinding::SSBO:
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, loc, 0);
        break;
    case HdBinding::DISPATCH:
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
        break;
    case HdBinding::UBO:
    case HdBinding::UNIFORM:
        glBindBufferBase(GL_UNIFORM_BUFFER, loc, 0);
        break;
    case HdBinding::TBO:
        if (loc != HdBinding::NOT_EXIST) {
            glActiveTexture(GL_TEXTURE0 + binding.GetTextureUnit());
            glBindTexture(GL_TEXTURE_BUFFER, 0);
        }
        break;
    case HdBinding::TEXTURE_2D:
        // nothing
        break;
    default:
        TF_CODING_ERROR("binding type %d not found for %s",
                        type, name.GetText());
        break;
    }
}

void
HdSt_ResourceBinderGL::BindShaderResources(HdStShaderCode const *shader) const
{
    // bind fallback values and sampler uniforms (unit#? or bindless address)

    // this is bound in batches.
    //BindBufferArray(shader->GetShaderData());

    // bind textures
    HdStShaderCode::TextureDescriptorVector textures = shader->GetTextures();
    TF_FOR_ALL(it, textures) {
        HdBinding binding = GetBinding(it->name);
        HdBinding::Type type = binding.GetType();

        if (type == HdBinding::TEXTURE_2D) {
        } else if (type == HdBinding::BINDLESS_TEXTURE_2D
                || type == HdBinding::BINDLESS_TEXTURE_PTEX_TEXEL
                || type == HdBinding::BINDLESS_TEXTURE_PTEX_LAYOUT) {
            // nothing? or make it resident?? but it only binds the first one.
            // XXX: it looks like this function should take all textures in the batch.

//            if (!glIsTextureHandleResidentNV(it->handle)) {
//                glMakeTextureHandleResidentNV(it->handle);
//            }
        }
    }
}

void
HdSt_ResourceBinderGL::UnbindShaderResources(HdStShaderCode const *shader) const
{
//    UnbindBufferArray(shader->GetShaderData());

    HdStShaderCode::TextureDescriptorVector textures = shader->GetTextures();
    TF_FOR_ALL(it, textures) {
        HdBinding binding = GetBinding(it->name);
        HdBinding::Type type = binding.GetType();

        if (type == HdBinding::TEXTURE_2D) {
        } else if (type == HdBinding::BINDLESS_TEXTURE_2D
                || type == HdBinding::BINDLESS_TEXTURE_PTEX_TEXEL
                   || type == HdBinding::BINDLESS_TEXTURE_PTEX_LAYOUT) {
//            if (glIsTextureHandleResidentNV(it->handle)) {
//                glMakeTextureHandleNonResidentNV(it->handle);
//            }
        }
        // XXX: unbind
    }
}

void
HdSt_ResourceBinderGL::BindUniformi(TfToken const &name,
                                int count, const int *value) const
{
    HdBinding uniformLocation = GetBinding(name);
    if (uniformLocation.GetLocation() == HdBinding::NOT_EXIST) return;

    TF_VERIFY(uniformLocation.IsValid());
    TF_VERIFY(uniformLocation.GetType() == HdBinding::UNIFORM);

    if (count == 1) {
        glUniform1iv(uniformLocation.GetLocation(), 1, value);
    } else if (count == 2) {
        glUniform2iv(uniformLocation.GetLocation(), 1, value);
    } else if (count == 3) {
        glUniform3iv(uniformLocation.GetLocation(), 1, value);
    } else if (count == 4) {
        glUniform4iv(uniformLocation.GetLocation(), 1, value);
    } else {
        TF_CODING_ERROR("Invalid count %d.\n", count);
    }
}

void
HdSt_ResourceBinderGL::BindUniformArrayi(TfToken const &name,
                                 int count, const int *value) const
{
    HdBinding uniformLocation = GetBinding(name);
    if (uniformLocation.GetLocation() == HdBinding::NOT_EXIST) return;

    TF_VERIFY(uniformLocation.IsValid());
    TF_VERIFY(uniformLocation.GetType() == HdBinding::UNIFORM_ARRAY);

    glUniform1iv(uniformLocation.GetLocation(), count, value);
}

void
HdSt_ResourceBinderGL::BindUniformui(TfToken const &name,
                                int count, const unsigned int *value) const
{
    HdBinding uniformLocation = GetBinding(name);
    if (uniformLocation.GetLocation() == HdBinding::NOT_EXIST) return;

    TF_VERIFY(uniformLocation.IsValid());
    TF_VERIFY(uniformLocation.GetType() == HdBinding::UNIFORM);

    if (count == 1) {
        glUniform1uiv(uniformLocation.GetLocation(), 1, value);
    } else if (count == 2) {
        glUniform2uiv(uniformLocation.GetLocation(), 1, value);
    } else if (count == 3) {
        glUniform3uiv(uniformLocation.GetLocation(), 1, value);
    } else if (count == 4) {
        glUniform4uiv(uniformLocation.GetLocation(), 1, value);
    } else {
        TF_CODING_ERROR("Invalid count %d.", count);
    }
}

void
HdSt_ResourceBinderGL::BindUniformf(TfToken const &name,
                                int count, const float *value) const
{
    HdBinding uniformLocation = GetBinding(name);
    if (uniformLocation.GetLocation() == HdBinding::NOT_EXIST) return;

    if (!TF_VERIFY(uniformLocation.IsValid())) return;
    if (!TF_VERIFY(uniformLocation.GetType() == HdBinding::UNIFORM)) return;
    GLint location = uniformLocation.GetLocation();

    if (count == 1) {
        glUniform1fv(location, 1, value);
    } else if (count == 2) {
        glUniform2fv(location, 1, value);
    } else if (count == 3) {
        glUniform3fv(location, 1, value);
    } else if (count == 4) {
        glUniform4fv(location, 1, value);
    } else if (count == 16) {
        glUniformMatrix4fv(location, 1, /*transpose=*/false, value);
    } else {
        TF_CODING_ERROR("Invalid count %d.", count);
    }
}

void
HdSt_ResourceBinderGL::IntrospectBindings(HdStProgramSharedPtr programResource)
{
    HdStRenderContextCaps const &caps = HdStRenderContextCaps::GetInstance();
    GLuint program = boost::dynamic_pointer_cast<HdStGLSLProgram>(programResource)->GetGLProgram();

    if (ARCH_UNLIKELY(!caps.shadingLanguage420pack)) {
        GLint numUBO = 0;
        glGetProgramiv(program, GL_ACTIVE_UNIFORM_BLOCKS, &numUBO);

        const int MAX_NAME = 256;
        int length = 0;
        char name[MAX_NAME+1];
        for (int i = 0; i < numUBO; ++i) {
            glGetActiveUniformBlockName(program, i, MAX_NAME, &length, name);
            // note: ubo_ prefix is added in HdCodeGen::_EmitDeclaration()
            if (strstr(name, "ubo_") == name) {
                HdBinding binding;
                if (TfMapLookup(_bindingMap, NameAndLevel(TfToken(name+4)), &binding)) {
                    // set uniform block binding.
                    glUniformBlockBinding(program, i, binding.GetLocation());
                }
            }
        }
    }

    if (ARCH_UNLIKELY(!caps.explicitUniformLocation)) {
        TF_FOR_ALL(it, _bindingMap) {
            HdBinding binding = it->second;
            HdBinding::Type type = binding.GetType();
            std::string name = it->first.name;
            int level = it->first.level;
            if (level >=0) {
                // follow nested instancing naming convention.
                std::stringstream n;
                n << name << "_" << level;
                name = n.str();
            }
            if (type == HdBinding::UNIFORM       ||
                type == HdBinding::UNIFORM_ARRAY ||
                type == HdBinding::TBO) {
                GLint loc = glGetUniformLocation(program, name.c_str());
                // update location in resource binder.
                // some uniforms may be optimized out.
                if (loc < 0) loc = HdBinding::NOT_EXIST;
                it->second.Set(type, loc, binding.GetTextureUnit());
            } else if (type == HdBinding::TEXTURE_2D) {
                // note: sampler2d_ prefix is added in
                // HdCodeGen::_GenerateShaderParameters()
                name = "sampler2d_" + name;
                GLint loc = glGetUniformLocation(program, name.c_str());
                if (loc < 0) loc = HdBinding::NOT_EXIST;
                it->second.Set(type, loc, binding.GetTextureUnit());
            }
        }
    }
}

PXR_NAMESPACE_CLOSE_SCOPE

