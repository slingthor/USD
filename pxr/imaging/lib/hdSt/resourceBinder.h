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
#ifndef HDST_RESOURCE_BINDER_H
#define HDST_RESOURCE_BINDER_H

#include "pxr/pxr.h"
#include "pxr/imaging/hdSt/api.h"
#include "pxr/imaging/hd/version.h"

#include "pxr/imaging/hd/binding.h"
#include "pxr/base/tf/token.h"
#include "pxr/base/tf/stl.h"

#include "pxr/base/tf/hashmap.h"

PXR_NAMESPACE_OPEN_SCOPE


class HdStDrawItem;
class HdStShaderCode;
class HdResource;

typedef boost::shared_ptr<class HdBufferResource> HdBufferResourceSharedPtr;
typedef boost::shared_ptr<class HdBufferArrayRange> HdBufferArrayRangeSharedPtr;
typedef boost::shared_ptr<class HdSt_ResourceBinder> HdSt_ResourceBinderSharedPtr;
typedef boost::shared_ptr<class HdStProgram> HdStProgramSharedPtr;

typedef boost::shared_ptr<class HdStShaderCode> HdStShaderCodeSharedPtr;
typedef std::vector<HdStShaderCodeSharedPtr> HdStShaderCodeSharedPtrVector;
typedef std::vector<class HdBindingRequest> HdBindingRequestVector;


/// \class HdSt_ResourceBinder
/// 
/// A helper class to maintain all vertex/buffer/uniform binding points to be
/// used for both codegen time and rendering time.
///
/// Storm uses 6 different types of coherent buffers.
///
/// 1. Constant buffer
///   constant primvars, which is uniform for all instances/elements/vertices.
///     ex. transform, object color
//    [SSBO, BindlessUniform]
///
/// 2. Instance buffer
///   instance primvars, one-per-instance.
///     ex. translate/scale/rotate, instanceIndices
//    [SSBO, BindlessUniform]
///
/// 3. Element buffer
///   element primvars. one-per-element (face, line).
///     ex. face color
///   [SSBO]
///
/// 4. Vertex buffer
///   vertex primvars. one-per-vertex.
///     ex. positions, normals, vertex color
///   [VertexAttribute]
///
/// 5. Index buffer
///   points/triangles/quads/lines/patches indices.
///     ex. indices, primitive param.
///   [IndexAttribute, SSBO]
///
/// 6. DrawIndex buffer
///   draw command data. one-per-drawitem (gl_DrawID equivalent)
///     ex. drawing coordinate, instance counts
///   [VertexAttribute]
///
///
///
/// For instance index indirection, three bindings are needed:
///
///    +-----------------------------------------+
///    |  instance indices buffer resource       | <-- <arrayBinding>
///    +-----------------------------------------+
///    |* culled instance indices buffer resource| <-- <culledArrayBinding>
///    +-----------------------------------------+  (bindless uniform
///                  ^   ^      ^                      or SSBO)
/// DrawCalls +---+  |   |      |
///          0|   |---   |      |
///           +---+      |      |
///          1|   |-------      |
///           +---+             |
///          2|   |--------------
///           +---+
///             ^
///             --- <baseBinding>
///                  (immediate:uniform, indirect:vertex attrib)
///
/// (*) GPU frustum culling shader shuffles instance indices into
///     culled indices buffer.
///
///
/// HdSt_ResourceBinder also takes custom bindings.
///
/// Custom bindings are used to manage bindable resources for
/// glsl shader code which is not itself generated by codegen.
///
/// For each custom binding, codegen will emit a binding definition
/// that can be used as the value of a glsl \a binding or
/// \a location layout qualifier.
///
/// e.g. Adding a custom binding of 2 for "paramsBuffer", will
/// cause codegen to emit the definition:
/// \code
/// #define paramsBuffer_Binding 2
/// \endcode
/// which can be used in a custom glsl resource declaration as:
/// \code
/// layout (binding = paramsBuffer_Binding) buffer ParamsBuffer { ... };
/// \endcode
///
class HdSt_ResourceBinder {
public:
    /// binding metadata for codegen
    class MetaData {
    public:
        MetaData() : instancerNumLevels(0) {}

        typedef size_t ID;
        /// Returns the hash value of this metadata.
        HDST_API
        ID ComputeHash() const;

        // -------------------------------------------------------------------
        // for a primvar in interleaved buffer array (Constant, ShaderData)
        struct StructEntry {
            StructEntry(TfToken const &name,
                        TfToken const &dataType,
                        int offset, int arraySize)
                : name(name)
                , dataType(dataType)
                , offset(offset)
                , arraySize(arraySize)
            {}

            TfToken name;
            TfToken dataType;
            int offset;
            int arraySize;

            bool operator < (StructEntry const &other) const {
                return offset < other.offset;
            }
        };
        struct StructBlock {
            StructBlock(TfToken const &name)
                : blockName(name) {}
            TfToken blockName;
            std::vector<StructEntry> entries;
        };
        typedef std::map<HdBinding, StructBlock> StructBlockBinding;

        // -------------------------------------------------------------------
        // for a primvar in non-interleaved buffer array (Vertex, Element, ...)
        struct Primvar {
            Primvar() {}
            Primvar(TfToken const &name, TfToken const &dataType)
                : name(name), dataType(dataType) {}
            TfToken name;
            TfToken dataType;
        };
        typedef std::map<HdBinding, Primvar> PrimvarBinding;

        // -------------------------------------------------------------------
        // for instance primvars
        struct NestedPrimvar {
            NestedPrimvar() {}
            NestedPrimvar(TfToken const &name, TfToken const &dataType,
                        int level)
                : name(name), dataType(dataType), level(level) {}
            TfToken name;
            TfToken dataType;
            int level;
        };
        typedef std::map<HdBinding, NestedPrimvar> NestedPrimvarBinding;

        // -------------------------------------------------------------------
        // for shader parameter accessors
        struct ShaderParameterAccessor {
             ShaderParameterAccessor() {}
             ShaderParameterAccessor(TfToken const &name,
                                     TfToken const &dataType,
                                     TfTokenVector const &inPrimvars=TfTokenVector())
                 : name(name), dataType(dataType), inPrimvars(inPrimvars) {}
             TfToken name;        // e.g. Kd
             TfToken dataType;    // e.g. vec4
             TfTokenVector inPrimvars;  // for primvar renaming and texture coordinates,
        };
        typedef std::map<HdBinding, ShaderParameterAccessor> ShaderParameterBinding;

        // -------------------------------------------------------------------
        // for accessor with fallback value sampling a 3d texture at
        // coordinates transformed by an associated matrix
        // 
        // The accessor will be named NAME (i.e., vec3 HdGet_NAME(vec3 p)) and
        // will sample FIELDNAMETexture at the coordinate obtained by transforming
        // p by FIELDNAMESamplineTransform. If FIELDNAMETexture does not exist,
        // NAMEFallback is used.
        struct FieldRedirectAccessor {
            FieldRedirectAccessor() {}
            FieldRedirectAccessor(TfToken const &name,
                                  TfToken const &fieldName)
                : name(name), fieldName(fieldName) {}
            TfToken name;
            TfToken fieldName;
        };
        typedef std::map<HdBinding, FieldRedirectAccessor> FieldRedirectBinding;

        // -------------------------------------------------------------------
        // for specific buffer array (drawing coordinate, instance indices)
        struct BindingDeclaration {
            BindingDeclaration() {}
            BindingDeclaration(TfToken const &name,
                               TfToken const &dataType,
                               HdBinding binding)
            : name(name), dataType(dataType), binding(binding), typeIsAtomic(false) {}
            
            BindingDeclaration(TfToken const &name,
                               TfToken const &dataType,
                               HdBinding binding,
                               bool isAtomic)
            : name(name), dataType(dataType), binding(binding), typeIsAtomic(isAtomic) {}
            
            TfToken name;
            TfToken dataType;
            HdBinding binding;
            bool typeIsAtomic;
        };

        // -------------------------------------------------------------------

        StructBlockBinding constantData;
        StructBlockBinding shaderData;
        StructBlockBinding topologyVisibilityData;
        PrimvarBinding elementData;
        PrimvarBinding vertexData;
        PrimvarBinding fvarData;
        PrimvarBinding computeReadWriteData;
        PrimvarBinding computeReadOnlyData;
        NestedPrimvarBinding instanceData;
        int instancerNumLevels;

        ShaderParameterBinding shaderParameterBinding;
        FieldRedirectBinding fieldRedirectBinding;

        BindingDeclaration drawingCoord0Binding;
        BindingDeclaration drawingCoord1Binding;
        BindingDeclaration drawingCoord2Binding;
        BindingDeclaration drawingCoordIBinding;
        BindingDeclaration instanceIndexArrayBinding;
        BindingDeclaration culledInstanceIndexArrayBinding;
        BindingDeclaration instanceIndexBaseBinding;
        BindingDeclaration primitiveParamBinding;
        BindingDeclaration edgeIndexBinding;

        StructBlockBinding customInterleavedBindings;
        std::vector<BindingDeclaration> customBindings;
    };
    
    /// Destructor
    HDST_API
    virtual ~HdSt_ResourceBinder() {}

    /// Assign all binding points used in drawitem and custom bindings.
    /// Returns metadata to be used for codegen.
    HDST_API
    void ResolveBindings(HdStDrawItem const *drawItem,
                         HdStShaderCodeSharedPtrVector const &shaders,
                         MetaData *metaDataOut,
                         bool indirect,
                         bool instanceDraw,
                         HdBindingRequestVector const &customBindings);

    /// Assign all binding points used in computation.
    /// Returns metadata to be used for codegen.
    HDST_API
    void ResolveComputeBindings(HdBufferSpecVector const &readWriteBufferSpecs,
                                HdBufferSpecVector const &readOnlyBufferSpecs,
                                HdStShaderCodeSharedPtrVector const &shaders,
                                MetaData *metaDataOut);
    
    /// call introspection APIs and fix up binding locations,
    /// in case if explicit resource location qualifier is not available
    /// (GL 4.2 or before)
    HDST_API
    virtual void IntrospectBindings(HdStProgramSharedPtr programResource) const = 0;

    HDST_API
    void Bind(HdBindingRequest const& req) const;
    HDST_API
    void Unbind(HdBindingRequest const& req) const;

    /// bind/unbind BufferArray
    HDST_API
    void BindBufferArray(HdBufferArrayRangeSharedPtr const &bar) const;
    HDST_API
    void UnbindBufferArray(HdBufferArrayRangeSharedPtr const &bar) const;

    /// bind/unbind interleaved constant buffer
    HDST_API
    void BindConstantBuffer(
        HdBufferArrayRangeSharedPtr const & constantBar) const;
    HDST_API
    void UnbindConstantBuffer(
        HdBufferArrayRangeSharedPtr const &constantBar) const;

    /// bind/unbind interleaved buffer
    HDST_API
    void BindInterleavedBuffer(
        HdBufferArrayRangeSharedPtr const & constantBar,
        TfToken const &name) const;
    HDST_API
    void UnbindInterleavedBuffer(
        HdBufferArrayRangeSharedPtr const &constantBar,
        TfToken const &name) const;

    /// bind/unbind nested instance BufferArray
    HDST_API
    void BindInstanceBufferArray(
        HdBufferArrayRangeSharedPtr const &bar, int level) const;
    HDST_API
    void UnbindInstanceBufferArray(
        HdBufferArrayRangeSharedPtr const &bar, int level) const;

    /// bind/unbind shader parameters and textures
    HDST_API
    virtual void BindShaderResources(HdStShaderCode const *shader) const = 0;
    HDST_API
    virtual void UnbindShaderResources(HdStShaderCode const *shader) const = 0;

    /// piecewise buffer binding utility
    /// (to be used for frustum culling, draw indirect result)
    HDST_API
    void BindBuffer(TfToken const &name,
                    HdBufferResourceSharedPtr const &resource) const;
    HDST_API
    virtual void BindBuffer(TfToken const &name,
                    HdBufferResourceSharedPtr const &resource,
                    int offset, int level=-1) const = 0;
    HDST_API
    virtual void UnbindBuffer(TfToken const &name,
                      HdBufferResourceSharedPtr const &resource,
                      int level=-1) const = 0;

    /// bind(update) a standalone uniform (unsigned int)
    HDST_API
    virtual void BindUniformui(TfToken const &name, int count,
                       const unsigned int *value) const = 0;

    /// bind a standalone uniform (signed int, ivec2, ivec3, ivec4)
    HDST_API
    virtual void BindUniformi(TfToken const &name, int count, const int *value) const = 0;

    /// bind a standalone uniform array (int[N])
    HDST_API
    virtual void BindUniformArrayi(TfToken const &name, int count, const int *value) const = 0;

    /// bind a standalone uniform (float, vec2, vec3, vec4, mat4)
    HDST_API
    virtual void BindUniformf(TfToken const &name, int count, const float *value) const = 0;

    /// Returns binding point.
    /// XXX: exposed temporarily for drawIndirectResult
    /// see Hd_IndirectDrawBatch::_BeginGPUCountVisibleInstances()
    HdBinding GetBinding(TfToken const &name, int level=-1) const {
        HdBinding binding;
        TfMapLookup(_bindingMap, NameAndLevel(name, level), &binding);
        return binding;
    }

    int GetNumReservedTextureUnits() const {
        return _numReservedTextureUnits;
    }

protected:
    /// Constructor
    HDST_API
    HdSt_ResourceBinder();

    // for batch execution
    struct NameAndLevel {
        NameAndLevel(TfToken const &n, int lv=-1) :
            name(n), level(lv) {}
        TfToken name;
        int level;

        bool operator < (NameAndLevel const &other) const {
            return name  < other.name ||
                  (name == other.name && level < other.level);
        }
    };
    typedef std::map<NameAndLevel, HdBinding> _BindingMap;
    mutable _BindingMap _bindingMap;
    int _numReservedTextureUnits;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif  // HDST_RESOURCE_BINDER_H
