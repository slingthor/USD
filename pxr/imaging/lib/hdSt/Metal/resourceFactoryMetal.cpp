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
// resourceFactoryMetal.cpp
//
#include "pxr/imaging/mtlf/mtlDevice.h"

#include "pxr/imaging/hdSt/Metal/resourceFactoryMetal.h"

#include "pxr/imaging/hdSt/Metal/bufferRelocatorMetal.h"
#include "pxr/imaging/hdSt/Metal/bufferResourceMetal.h"
#include "pxr/imaging/hdSt/Metal/codeGenMSL.h"
#include "pxr/imaging/hdSt/Metal/dispatchBufferMetal.h"
#include "pxr/imaging/hdSt/Metal/drawTargetTextureResourceMetal.h"
#include "pxr/imaging/hdSt/Metal/flatNormalsMetal.h"
#include "pxr/imaging/hdSt/Metal/indirectDrawBatchMetal.h"
#include "pxr/imaging/hdSt/Metal/interleavedMemoryBufferMetal.h"
#include "pxr/imaging/hdSt/Metal/mslProgram.h"
#include "pxr/imaging/hdSt/Metal/persistentBufferMetal.h"
#include "pxr/imaging/hdSt/Metal/renderPassStateMetal.h"
#include "pxr/imaging/hdSt/Metal/resourceBinderMetal.h"
#include "pxr/imaging/hdSt/Metal/quadrangulateMetal.h"
#include "pxr/imaging/hdSt/Metal/smoothNormalsMetal.h"
#include "pxr/imaging/hdSt/Metal/textureResourceMetal.h"
#include "pxr/imaging/hdSt/Metal/vboMemoryBufferMetal.h"
#include "pxr/imaging/hdSt/Metal/vboSimpleMemoryBufferMetal.h"

#include <boost/smart_ptr.hpp>

PXR_NAMESPACE_OPEN_SCOPE

HdStResourceFactoryMetal::HdStResourceFactoryMetal()
{
    // Empty
}

HdStResourceFactoryMetal::~HdStResourceFactoryMetal()
{
    // Empty
}

HdSt_CodeGen *HdStResourceFactoryMetal::NewCodeGen(
    HdSt_GeometricShaderPtr const &geometricShader,
    HdStShaderCodeSharedPtrVector const &shaders) const
{
    return new HdSt_CodeGenMSL(geometricShader, shaders);
}

HdSt_CodeGen *HdStResourceFactoryMetal::NewCodeGen(
    HdStShaderCodeSharedPtrVector const &shaders) const
{
    return new HdSt_CodeGenMSL(shaders);
}

HdStDispatchBuffer *HdStResourceFactoryMetal::NewDispatchBuffer(
    TfToken const &role, int count,
    unsigned int commandNumUints) const
{
    return new HdStDispatchBufferMetal(role, count, commandNumUints);
}

HdStBufferRelocator *HdStResourceFactoryMetal::NewBufferRelocator(
    HdResourceGPUHandle srcBuffer,
    HdResourceGPUHandle dstBuffer) const
{
    return new HdStBufferRelocatorMetal(srcBuffer, dstBuffer);
}

HdStBufferResource *HdStResourceFactoryMetal::NewBufferResource(
    TfToken const &role,
    HdTupleType tupleType,
    int offset,
    int stride) const
{
    return new HdStBufferResourceMetal(role, tupleType, offset, stride);
}

HdTextureResourceSharedPtr
HdStResourceFactoryMetal::NewDrawTargetTextureResource() const
{
    return HdTextureResourceSharedPtr(
        new HdSt_DrawTargetTextureResourceMetal());
}

HdSt_FlatNormalsComputationGPU*
HdStResourceFactoryMetal::NewFlatNormalsComputationGPU(
    HdBufferArrayRangeSharedPtr const &topologyRange,
    HdBufferArrayRangeSharedPtr const &vertexRange,
    int numFaces, TfToken const &srcName, TfToken const &dstName,
    HdType srcDataType, bool packed) const
{
    return new HdSt_FlatNormalsComputationMetal(
        topologyRange, vertexRange, numFaces, srcName, dstName, srcDataType,
        packed);
}

HdBufferArraySharedPtr
HdStResourceFactoryMetal::NewStripedInterleavedBuffer(
    TfToken const &role,
    HdBufferSpecVector const &bufferSpecs,
    HdBufferArrayUsageHint usageHint,
    int bufferOffsetAlignment,
    int structAlignment,
    size_t maxSize,
    TfToken const &garbageCollectionPerfToken) const
{
    return boost::make_shared<
            HdStStripedInterleavedBufferMetal>(role,
                                               bufferSpecs,
                                               usageHint,
                                               bufferOffsetAlignment,
                                               structAlignment,
                                               maxSize,
                                               garbageCollectionPerfToken);
}

HdSt_DrawBatchSharedPtr HdStResourceFactoryMetal::NewIndirectDrawBatch(
    HdStDrawItemInstance * drawItemInstance) const
{
    return HdSt_DrawBatchSharedPtr(
        new HdSt_IndirectDrawBatchMetal(drawItemInstance));
}

HdStPersistentBuffer *HdStResourceFactoryMetal::NewPersistentBuffer(
    TfToken const &role, size_t dataSize, void* data) const
{
    return new HdStPersistentBufferMetal(role, dataSize, data);
}

/// Create a GPU quadrangulation computation
HdSt_QuadrangulateComputationGPU *
HdStResourceFactoryMetal::NewQuadrangulateComputationGPU(
    HdSt_MeshTopology *topology,
    TfToken const &sourceName,
    HdType dataType,
    SdfPath const &id) const
{
    return new HdSt_QuadrangulateComputationGPUMetal(topology,
                                                     sourceName,
                                                     dataType,
                                                     id);
}

HdStRenderPassState *HdStResourceFactoryMetal::NewRenderPassState() const
{
    return new HdStRenderPassStateMetal();
}

HdStRenderPassState *HdStResourceFactoryMetal::NewRenderPassState(
    HdStRenderPassShaderSharedPtr const &renderPassShader) const
{
    return new HdStRenderPassStateMetal(renderPassShader);
}

HdSt_ResourceBinder *HdStResourceFactoryMetal::NewResourceBinder() const
{
    return new HdSt_ResourceBinderMetal();
}

/// Creates a new smooth normals GPU computation
HdSt_SmoothNormalsComputationGPU *
HdStResourceFactoryMetal::NewSmoothNormalsComputationGPU(
    Hd_VertexAdjacency const *adjacency,
    TfToken const &srcName, TfToken const &dstName,
    HdType srcDataType, bool packed) const
{
    return new HdSt_SmoothNormalsComputationMetal(
        adjacency, srcName, dstName, srcDataType, packed);
}

HdStSimpleTextureResource *
HdStResourceFactoryMetal::NewSimpleTextureResource(
    GarchTextureHandleRefPtr const &textureHandle,
    HdTextureType textureType,
    size_t memoryRequest) const
{
    return new HdStSimpleTextureResourceMetal(
        textureHandle, textureType, memoryRequest);
}

HdStSimpleTextureResource *
HdStResourceFactoryMetal::NewSimpleTextureResource(
    GarchTextureHandleRefPtr const &textureHandle,
    HdTextureType textureType,
    HdWrap wrapS, HdWrap wrapT,
    HdMinFilter minFilter, HdMagFilter magFilter,
    size_t memoryRequest) const
{
    return new HdStSimpleTextureResourceMetal(
        textureHandle, textureType, wrapS, wrapT, minFilter, magFilter,
        memoryRequest);
}

HdBufferArraySharedPtr HdStResourceFactoryMetal::NewVBOMemoryBuffer(
    TfToken const &role,
    HdBufferSpecVector const &bufferSpecs,
    HdBufferArrayUsageHint usageHint) const
{
    return boost::make_shared<HdStVBOMemoryBufferMetal>(
        role, bufferSpecs, usageHint);
}

HdBufferArraySharedPtr HdStResourceFactoryMetal::NewVBOSimpleMemoryBuffer(
    TfToken const &role,
    HdBufferSpecVector const &bufferSpecs,
    HdBufferArrayUsageHint usageHint) const
{
    return boost::make_shared<HdStVBOSimpleMemoryBufferMetal>(
        role, bufferSpecs, usageHint);
}

HdStProgram *HdStResourceFactoryMetal::NewProgram(
    TfToken const &role) const
{
    return new HdStMSLProgram(role);
}

PXR_NAMESPACE_CLOSE_SCOPE

