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

#include "pxr/imaging/hdSt/Metal/bufferResourceMetal.h"
#include "pxr/imaging/hdSt/Metal/resourceBinderMetal.h"
#include "pxr/imaging/hdSt/Metal/mslProgram.h"
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

HdSt_ResourceBinderMetal::HdSt_ResourceBinderMetal()
{
}

void
HdSt_ResourceBinderMetal::BindBuffer(TfToken const &name,
                                     HdBufferResourceSharedPtr const &buffer,
                                     int offset,
                                     int level) const
{
    HD_TRACE_FUNCTION();

    // it is possible that the buffer has not been initialized when
    // the instanceIndex is empty (e.g. FX points. see bug 120354)
    if (!buffer->GetId().IsSet()) return;

    HdBinding binding = GetBinding(name, level);
    HdBinding::Type type = binding.GetType();
    HdStBufferResourceMetalSharedPtr const metalBuffer = boost::dynamic_pointer_cast<HdStBufferResourceMetal>(buffer);
    int loc              = binding.GetLocation();
    int textureUnit      = binding.GetTextureUnit();

    HdTupleType tupleType = buffer->GetTupleType();

    switch(type) {
        case HdBinding::VERTEX_ATTR:
            MtlfMetalContext::GetMetalContext()->SetVertexAttribute(
                    loc,
                    _GetNumComponents(tupleType.type),
                    HdStGLConversions::GetGLAttribType(tupleType.type),
                    buffer->GetStride(),
                    offset);
            break;
        case HdBinding::SSBO:
        case HdBinding::UBO:
            MtlfMetalContext::GetMetalContext()->SetBuffer(
                    loc,
                    metalBuffer->GetId());
            break;
        case HdBinding::INDEX_ATTR:
            MtlfMetalContext::GetMetalContext()->SetIndexBuffer(metalBuffer->GetId());
            break;
        default:
            TF_FATAL_CODING_ERROR("Not Implemented");
    }
}

void
HdSt_ResourceBinderMetal::UnbindBuffer(TfToken const &name,
                                  HdBufferResourceSharedPtr const &buffer,
                                  int level) const
{
    HD_TRACE_FUNCTION();
}

void
HdSt_ResourceBinderMetal::BindShaderResources(HdStShaderCode const *shader) const
{
    // Nothing
}

void
HdSt_ResourceBinderMetal::UnbindShaderResources(HdStShaderCode const *shader) const
{
    // Nothing
}

void
HdSt_ResourceBinderMetal::BindUniformi(TfToken const &name,
                                int count, const int *value) const
{
    HdBinding uniformLocation = GetBinding(name);
    if (uniformLocation.GetLocation() == HdBinding::NOT_EXIST) return;

    TF_VERIFY(uniformLocation.IsValid());
    TF_VERIFY(uniformLocation.GetType() == HdBinding::UNIFORM);

    TF_FATAL_CODING_ERROR("Not Implemented");
}

void
HdSt_ResourceBinderMetal::BindUniformArrayi(TfToken const &name,
                                 int count, const int *value) const
{
    HdBinding uniformLocation = GetBinding(name);
    if (uniformLocation.GetLocation() == HdBinding::NOT_EXIST) return;

    TF_VERIFY(uniformLocation.IsValid());
    TF_VERIFY(uniformLocation.GetType() == HdBinding::UNIFORM_ARRAY);

    TF_FATAL_CODING_ERROR("Not Implemented");
}

void
HdSt_ResourceBinderMetal::BindUniformui(TfToken const &name,
                                int count, const unsigned int *value) const
{
    HdBinding uniformLocation = GetBinding(name);
    if (uniformLocation.GetLocation() == HdBinding::NOT_EXIST) return;

    TF_VERIFY(uniformLocation.IsValid());
    TF_VERIFY(uniformLocation.GetType() == HdBinding::UNIFORM);

    int loc = uniformLocation.GetLocation();
    //MtlfMetalContext::GetMetalContext()->SetAttribute(loc, value);

    //TF_FATAL_CODING_ERROR("Not Implemented");
}

void
HdSt_ResourceBinderMetal::BindUniformf(TfToken const &name,
                                int count, const float *value) const
{
    HdBinding uniformLocation = GetBinding(name);
    if (uniformLocation.GetLocation() == HdBinding::NOT_EXIST) return;

    if (!TF_VERIFY(uniformLocation.IsValid())) return;
    if (!TF_VERIFY(uniformLocation.GetType() == HdBinding::UNIFORM)) return;

    TF_FATAL_CODING_ERROR("Not Implemented");
}

void
HdSt_ResourceBinderMetal::IntrospectBindings(HdStProgramSharedPtr programResource)
{
    HdStMSLProgramSharedPtr program(boost::dynamic_pointer_cast<HdStMSLProgram>(programResource));
    HdStMSLProgram::BindingLocationMap const& locationMap(program->GetBindingLocations());

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

        int loc = -1;
        auto item = locationMap.find(name);
        if (item != locationMap.end()) {
            loc = item->second;
        }
        // update location in resource binder.
        // some uniforms may be optimized out.
        if (loc < 0) loc = HdBinding::NOT_EXIST;
        it->second.Set(type, loc, binding.GetTextureUnit());
    }
}

PXR_NAMESPACE_CLOSE_SCOPE

