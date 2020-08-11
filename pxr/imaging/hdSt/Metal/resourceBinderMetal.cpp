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
#include "pxr/imaging/mtlf/mtlDevice.h"

#include "pxr/imaging/hdSt/Metal/resourceBinderMetal.h"
#include "pxr/imaging/hdSt/bufferResource.h"
#include "pxr/imaging/hdSt/drawItem.h"
#include "pxr/imaging/hdSt/textureHandle.h"
#include "pxr/imaging/hdSt/textureObject.h"
#include "pxr/imaging/hdSt/samplerObject.h"
#include "pxr/imaging/hdSt/shaderCode.h"

#include "pxr/imaging/hdSt/Metal/metalConversions.h"
#include "pxr/imaging/hdSt/Metal/glslProgramMetal.h"

#include "pxr/imaging/hd/bufferArrayRange.h"
#include "pxr/imaging/hd/bufferSpec.h"
#include "pxr/imaging/hd/resource.h"
#include "pxr/imaging/hd/tokens.h"

#include "pxr/imaging/hgiMetal/buffer.h"
#include "pxr/imaging/hgiMetal/texture.h"
#include "pxr/imaging/hgiMetal/sampler.h"

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

HdSt_ResourceBinderMetal::HdSt_ResourceBinderMetal()
{
}

void
HdSt_ResourceBinderMetal::BindBuffer(TfToken const &name,
                                     HdStBufferResourceSharedPtr const &buffer,
                                     int offset,
                                     int level) const
{
    HD_TRACE_FUNCTION();
    
    //NSLog(@"Binding buffer %s", name.GetText());
    // it is possible that the buffer has not been initialized when
    // the instanceIndex is empty (e.g. FX points. see bug 120354)
    if (!buffer->GetId())
        return;
    
    MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();
    HdTupleType tupleType = buffer->GetTupleType();

    auto shaderBindings = MSL_FindBinding(_shaderBindingMap, name, level);
    auto it = shaderBindings.first;
    
    for(; it != shaderBindings.second; ++it) {
        MSL_ShaderBinding const* const shaderBinding = (*it).second;
        id<MTLBuffer> metalBuffer = HgiMetalBuffer::MTLBuffer(buffer->GetId());

        switch(shaderBinding->_type)
        {
        case kMSL_BindingType_VertexAttribute:
            context->SetVertexAttribute(
                shaderBinding->_index,
                _GetNumComponents(tupleType.type),
                HdStMetalConversions::GetGLAttribType(tupleType.type),  // ??!??!
                buffer->GetStride(),
                offset,
                name);
            MtlfMetalContext::GetMetalContext()->SetBuffer(shaderBinding->_index, metalBuffer, name);
            break;
        case kMSL_BindingType_UniformBuffer:
            context->SetUniformBuffer(shaderBinding->_index, metalBuffer, name, shaderBinding->_stage, offset);
            break;
        case kMSL_BindingType_IndexBuffer:
            if(offset != 0)
                TF_FATAL_CODING_ERROR("Not implemented!");
            context->SetIndexBuffer(metalBuffer);
            break;
        default:
            TF_FATAL_CODING_ERROR("Not allowed!");
        }
    }
}

void
HdSt_ResourceBinderMetal::UnbindBuffer(TfToken const &name,
                                  HdStBufferResourceSharedPtr const &buffer,
                                  int level) const
{
    HD_TRACE_FUNCTION();
}

void
HdSt_ResourceBinderMetal::BindUniformi(TfToken const &name,
                                int count, const int *value) const
{
    auto shaderBindings = MSL_FindBinding(_shaderBindingMap, name, -1);
    auto it = shaderBindings.first;
    
    MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();
    
    int found = 0;
    for(; it != shaderBindings.second; ++it) {
        MSL_ShaderBinding const* const shaderBinding = (*it).second;
        
        context->SetUniform(value, count * sizeof(int), name, shaderBinding->_offsetWithinResource, shaderBinding->_stage);
        found++;
    }
    
    if(found == 0) { //If we tried searching but couldn't find a single uniform.
        TF_FATAL_CODING_ERROR("Could not find uniform!");
    }
}

void
HdSt_ResourceBinderMetal::BindUniformArrayi(TfToken const &name,
                                 int count, const int *value) const
{
    HdBinding uniformLocation = GetBinding(name);
    if (uniformLocation.GetLocation() == HdBinding::NOT_EXIST) return;

    auto shaderBindings = MSL_FindBinding(_shaderBindingMap, name, -1);
    auto it = shaderBindings.first;
    
    MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();
    
    int found = 0;
    for(; it != shaderBindings.second; ++it) {
        MSL_ShaderBinding const* const shaderBinding = (*it).second;
        if (shaderBinding->_type != kMSL_BindingType_Uniform)
            continue;
        
        context->SetUniform(value, count * sizeof(int), name, shaderBinding->_offsetWithinResource, shaderBinding->_stage);
        found++;
    }
    
    if(found == 0) { //If we tried searching but couldn't find a single uniform.
        TF_FATAL_CODING_ERROR("Could not find uniform!");
    }
}

void
HdSt_ResourceBinderMetal::BindUniformui(TfToken const &name,
                                int count, const unsigned int *value) const
{
    BindUniformi(name, count, (const int*)value);
}

void
HdSt_ResourceBinderMetal::BindUniformf(TfToken const &name,
                                int count, const float *value) const
{
    BindUniformi(name, count, (const int*)value);
}

void
HdSt_ResourceBinderMetal::IntrospectBindings(HdStGLSLProgramSharedPtr programResource) const
{
    HdStGLSLProgramMSLSharedPtr program(std::dynamic_pointer_cast<HdStGLSLProgramMSL>(programResource));
    
    //Copy the all shader bindings from the program.
    _shaderBindingMap = program->GetBindingMap();

    TF_FOR_ALL(it, _bindingMap) {
        HdBinding binding = it->second;
        HdBinding::Type type = binding.GetType();
        TfToken name = it->first.name;
        int level = it->first.level;
        if (level >=0) {
            // follow nested instancing naming convention.
            std::stringstream n;
            n << name << "_" << level;
            name = TfToken(n.str());
        }

        int loc = -1;
        {
            auto it = _shaderBindingMap.find(name.Hash());
            if(it != _shaderBindingMap.end())
                loc = (*it).second->_index; //Multiple entries in the shaderBindingMap ultimately resolve to the same thing.
        }

        // update location in resource binder.
        // some uniforms may be optimized out.
        if (loc < 0) loc = HdBinding::NOT_EXIST;
        it->second.Set(type, loc, binding.GetTextureUnit());
    }
}

namespace {

class _BindTextureFunctor {
public:
    static void Compute(
        TfToken const &name,
        HdStUvTextureObject const &texture,
        HdStUvSamplerObject const &sampler,
        HdStGLSLProgramMSL const &mslProgram,
        const bool bind)
    {
        auto metalTexture = dynamic_cast<HgiMetalTexture const*>(
            texture.GetTexture().Get());
        auto metalSampler = dynamic_cast<HgiMetalSampler const*>(
            sampler.GetSampler().Get());
        
        mslProgram.BindTexture(name, metalTexture?metalTexture->GetTextureId():nil);
        mslProgram.BindSampler(name,  metalSampler?metalSampler->GetSamplerId():nil);
    }

    static void Compute(
        TfToken const &name,
        HdStFieldTextureObject const &texture,
        HdStFieldSamplerObject const &sampler,
        HdStGLSLProgramMSL const &mslProgram,
        const bool bind)
    {
        auto metalTexture = dynamic_cast<HgiMetalTexture const*>(
            texture.GetTexture().Get());
        auto metalSampler = dynamic_cast<HgiMetalSampler const*>(
            sampler.GetSampler().Get());

        mslProgram.BindTexture(name, metalTexture?metalTexture->GetTextureId():nil);
        mslProgram.BindSampler(name, metalSampler?metalSampler->GetSamplerId():nil);
    }
    
    static void Compute(
        TfToken const &name,
        HdStPtexTextureObject const &texture,
        HdStPtexSamplerObject const &sampler,
        HdStGLSLProgramMSL const &mslProgram,
        const bool bind)
    {
        // Bind the texels
        GarchTextureGPUHandle texelHandle = texture.GetTexelGLTextureName();
        mslProgram.BindTexture(name, texelHandle);
        
        // Bind the layout
        TfToken layoutName = HdSt_ResourceBinder::_Concat(name, HdSt_ResourceBindingSuffixTokens->layout);
        GarchTextureGPUHandle layoutHandle = texture.GetLayoutGLTextureName();
        mslProgram.BindTexture(layoutName, layoutHandle);
    }

    static void Compute(
        TfToken const &name,
        HdStUdimTextureObject const &texture,
        HdStUdimSamplerObject const &sampler,
        HdStGLSLProgramMSL const &mslProgram,
        const bool bind)
    {
        // Bind the texels
        GarchTextureGPUHandle texelHandle = texture.GetTexelGLTextureName();
        mslProgram.BindTexture(name, texelHandle);
        
        // Bind the layout
        GarchTextureGPUHandle layoutHandle = texture.GetLayoutGLTextureName();
        mslProgram.BindTexture(
            HdSt_ResourceBinder::_Concat(
                name, HdSt_ResourceBindingSuffixTokens->layout),
            layoutHandle);

        // Bind the sampler
        auto metalSampler = dynamic_cast<HgiMetalSampler const*>(
            sampler.GetTexelsSampler().Get());
        id<MTLSamplerState> samplerID = metalSampler?metalSampler->GetSamplerId():nil;
        mslProgram.BindSampler(name, samplerID);
    }
};

template<HdTextureType textureType, typename ...Args>
void _CastAndCompute(
    HdStShaderCode::NamedTextureHandle const &namedTexture,
    HdStGLSLProgram const &program,
    Args&& ...args)
{
    // e.g. HdStUvTextureObject
    using TextureObject = HdStTypedTextureObject<textureType>;
    // e.g. HdStUvSamplerObject
    using SamplerObject = HdStTypedSamplerObject<textureType>;
    
    const HdStTextureHandleSharedPtr textureHandle = namedTexture.handle;

    const TextureObject * const typedTexture =
        dynamic_cast<TextureObject *>(textureHandle->GetTextureObject().get());
    if (!typedTexture) {
        TF_CODING_ERROR("Bad texture object");
        return;
    }

    const SamplerObject * const typedSampler =
        dynamic_cast<SamplerObject *>(textureHandle->GetSamplerObject().get());
    if (!typedSampler) {
        TF_CODING_ERROR("Bad sampler object");
        return;
    }

    HdStGLSLProgramMSL const &mslProgram(
        dynamic_cast<const HdStGLSLProgramMSL&>(program));

    _BindTextureFunctor::Compute(
        namedTexture.name,
        *typedTexture,
        *typedSampler,
        mslProgram,
        std::forward<Args>(args)...);
}

void _BindTextureDispatch(
    HdStShaderCode::NamedTextureHandle const &namedTexture,
    HdSt_ResourceBinder const &binder,
    HdStGLSLProgram const &program,
    bool bind)
{
    HdStGLSLProgramMSL const &mslProgram(
        dynamic_cast<const HdStGLSLProgramMSL&>(program));

    switch (namedTexture.type) {
    case HdTextureType::Uv:
        _CastAndCompute<HdTextureType::Uv>(
            namedTexture, mslProgram, bind);
        break;
    case HdTextureType::Field:
            _CastAndCompute<HdTextureType::Field>(
                namedTexture, mslProgram, bind);
        break;
    case HdTextureType::Ptex:
            _CastAndCompute<HdTextureType::Ptex>(
                namedTexture, mslProgram, bind);
        break;
    case HdTextureType::Udim:
        _CastAndCompute<HdTextureType::Udim>(
            namedTexture, mslProgram, bind);
        break;
    }
}
} // end anonymous namespace

void
HdSt_ResourceBinderMetal::BindShaderResources(
    HdStShaderCode const *shader,
    HdStGLSLProgram const &shaderProgram) const
{
    auto const & textures = shader->GetNamedTextureHandles();
    for (const HdStShaderCode::NamedTextureHandle & texture : textures) {
        _BindTextureDispatch(texture, *this, shaderProgram, /* bind = */ true);
    }
}

void
HdSt_ResourceBinderMetal::UnbindShaderResources(
    HdStShaderCode const *shader,
    HdStGLSLProgram const &shaderProgram) const
{
    auto const & textures = shader->GetNamedTextureHandles();
    for (const HdStShaderCode::NamedTextureHandle & texture : textures) {
        _BindTextureDispatch(texture, *this, shaderProgram, /* bind = */ false);
    }
}

PXR_NAMESPACE_CLOSE_SCOPE

