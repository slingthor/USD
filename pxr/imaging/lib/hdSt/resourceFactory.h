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
#ifndef HDST_RESOURCEFACTORY_H
#define HDST_RESOURCEFACTORY_H

/// \file hdSt/resourceFactory.h

#include "pxr/pxr.h"
#include "pxr/imaging/hdSt/api.h"

#include "pxr/imaging/hd/bufferArray.h"
#include "pxr/imaging/hd/resource.h"
#include "pxr/imaging/hd/types.h"

#include "pxr/base/tf/singleton.h"

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>

PXR_NAMESPACE_OPEN_SCOPE

class Hd_VertexAdjacency;

class HdStBufferRelocator;
class HdStBufferResource;
class HdSt_CodeGen;
class HdStDispatchBuffer;
class HdStDrawItemInstance;
class HdSt_FlatNormalsComputationGPU;
class HdSt_MeshTopology;
class HdStPersistentBuffer;
class HdStProgram;
class HdStRenderPassState;
class HdSt_ResourceBinder;
class HdSt_QuadrangulateComputationGPU;
class HdSt_SmoothNormalsComputationGPU;
class HdStSimpleTextureResource;

typedef boost::shared_ptr<class HdBufferArrayRange> HdBufferArrayRangeSharedPtr;
typedef boost::shared_ptr<class HdBufferArray> HdBufferArraySharedPtr;
typedef boost::shared_ptr<class HdTextureResource> HdTextureResourceSharedPtr;

typedef boost::shared_ptr<class HdSt_DrawBatch> HdSt_DrawBatchSharedPtr;
typedef boost::shared_ptr<class HdSt_GeometricShader> HdSt_GeometricShaderPtr;
typedef boost::shared_ptr<class HdStRenderPassShader>
                            HdStRenderPassShaderSharedPtr;
typedef boost::shared_ptr<class HdStShaderCode> HdStShaderCodeSharedPtr;
typedef std::vector<HdStShaderCodeSharedPtr> HdStShaderCodeSharedPtrVector;

class HdStResourceFactoryInterface {
public:

    HDST_API
    virtual ~HdStResourceFactoryInterface() {}

    // Temp
    virtual bool IsOpenGL() const = 0;

    // HdSt_CodeGen
    HDST_API
    virtual HdSt_CodeGen *NewCodeGen(
        HdSt_GeometricShaderPtr const &geometricShader,
        HdStShaderCodeSharedPtrVector const &shaders) const = 0;
    
    HDST_API
    virtual HdSt_CodeGen *NewCodeGen(
        HdStShaderCodeSharedPtrVector const &shaders) const = 0;
    
    /// commandNumUints is given in how many integers.
    HDST_API
    virtual HdStDispatchBuffer *NewDispatchBuffer(
        TfToken const &role, int count,
        unsigned int commandNumUints) const = 0;
    
    /// Creates a buffer relocator
    HDST_API
    virtual HdStBufferRelocator *NewBufferRelocator(
        HdResourceGPUHandle srcBuffer,
        HdResourceGPUHandle dstBuffer) const = 0;
    
    /// Creates a buffer resource
    HDST_API
    virtual HdStBufferResource *NewBufferResource(
        TfToken const &role,
        HdTupleType tupleType,
        int offset,
        int stride) const = 0;
    
    /// Creates a new draw target texture resource
    HDST_API
    virtual HdTextureResourceSharedPtr NewDrawTargetTextureResource() const = 0;
    
    /// Create a striped interleaved buffer
    HDST_API
    virtual HdBufferArraySharedPtr NewStripedInterleavedBuffer(
        TfToken const &role,
        HdBufferSpecVector const &bufferSpecs,
        HdBufferArrayUsageHint usageHint,
        int bufferOffsetAlignment,
        int structAlignment,
        size_t maxSize,
        TfToken const &garbageCollectionPerfToken) const = 0;
    
    /// Create a VBO simple memory buffer for Metal
    HDST_API
    virtual HdBufferArraySharedPtr NewVBOSimpleMemoryBuffer(
        TfToken const &role,
        HdBufferSpecVector const &bufferSpecs,
        HdBufferArrayUsageHint usageHint) const = 0;
    
    /// Create a VBO memory buffer for Metal
    HDST_API
    virtual HdBufferArraySharedPtr NewVBOMemoryBuffer(
        TfToken const &role,
        HdBufferSpecVector const &bufferSpecs,
        HdBufferArrayUsageHint usageHint) const = 0;
    
    /// Creates an indirect draw batch
    HDST_API
    virtual HdSt_DrawBatchSharedPtr NewIndirectDrawBatch(
        HdStDrawItemInstance * drawItemInstance) const = 0;
    
    /// Creates a persistent buffer
    HDST_API
    virtual HdStPersistentBuffer *NewPersistentBuffer(
        TfToken const &role, size_t dataSize, void* data) const = 0;

    /// Creates a graphics API specific GPU quadrangulate computation
    /// This computaion doesn't generate buffer source (i.e. 2nd phase)
    HDST_API
    virtual HdSt_QuadrangulateComputationGPU *NewQuadrangulateComputationGPU(
        HdSt_MeshTopology *topology,
        TfToken const &sourceName,
        HdType dataType,
        SdfPath const &id) const = 0;
    
    /// Creates a new smooth normals GPU computation
    HDST_API
    virtual HdSt_SmoothNormalsComputationGPU *NewSmoothNormalsComputationGPU(
        Hd_VertexAdjacency const *adjacency,
        TfToken const &srcName, TfToken const &dstName,
        HdType srcDataType, bool packed) const = 0;
    
    /// Creates a new flat normals GPU computation
    HDST_API
    virtual HdSt_FlatNormalsComputationGPU *NewFlatNormalsComputationGPU(
        HdBufferArrayRangeSharedPtr const &topologyRange,
        HdBufferArrayRangeSharedPtr const &vertexRange,
        int numFaces, TfToken const &srcName, TfToken const &dstName,
        HdType srcDataType, bool packed) const = 0;
    
    /// Creates a new render pass state
    HDST_API
    virtual HdStRenderPassState *NewRenderPassState() const = 0;
    
    /// Creates a new render pass state
    HDST_API
    virtual HdStRenderPassState *NewRenderPassState(
        HdStRenderPassShaderSharedPtr const &renderPassShader) const = 0;
    
    /// Creates a resource binder
    HDST_API
    virtual HdSt_ResourceBinder *NewResourceBinder() const = 0;

    /// Create a texture resource around a Garch handle.
    /// While the texture handle maybe shared between many references to a
    /// texture.
    /// The texture resource represents a single texture binding.
    ///
    /// The memory request can be used to limit, the amount of texture memory
    /// this reference requires of the texture.  Set to 0 for unrestricted.
    HDST_API
    virtual HdStSimpleTextureResource *NewSimpleTextureResource(
        GarchTextureHandleRefPtr const &textureHandle,
        HdTextureType textureType,
        size_t memoryRequest) const = 0;
    
    HDST_API
    virtual HdStSimpleTextureResource *NewSimpleTextureResource(
        GarchTextureHandleRefPtr const &textureHandle,
        HdTextureType textureType,
        HdWrap wrapS, HdWrap wrapT,
        HdMinFilter minFilter, HdMagFilter magFilter,
        size_t memoryRequest = 0) const = 0;

    HDST_API
    virtual char const *const GetComputeShaderFilename() const = 0;
    
    HDST_API
    virtual char const *const GetPtexTextureShaderFilename() const = 0;

    /// Creates a graphics API specific program
    HDST_API
    virtual HdStProgram *NewProgram(
        TfToken const &role) const = 0;

protected:
    HDST_API
    HdStResourceFactoryInterface() {}

};

class HdStResourceFactory : boost::noncopyable {
public:
    HDST_API
    static HdStResourceFactory& GetInstance();
    
    HDST_API
    HdStResourceFactoryInterface *operator ->() const;
    
    HDST_API
    void SetResourceFactory(HdStResourceFactoryInterface *factory);
    
private:
    HDST_API
    HdStResourceFactory();
    
    HDST_API
    ~HdStResourceFactory();
    
    HdStResourceFactoryInterface *factory;
    
    friend class TfSingleton<HdStResourceFactory>;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDST_RESOURCEFACTORY_H
