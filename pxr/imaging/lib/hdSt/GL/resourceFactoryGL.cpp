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
// resourceFactoryGL.cpp
//
#include "pxr/imaging/hdSt/GL/resourceFactoryGL.h"

#include "pxr/imaging/hdSt/GL/bufferRelocatorGL.h"
#include "pxr/imaging/hdSt/GL/bufferResourceGL.h"
#include "pxr/imaging/hdSt/GL/codeGenGLSL.h"
#include "pxr/imaging/hdSt/GL/dispatchBufferGL.h"
#include "pxr/imaging/hdSt/GL/drawTargetTextureResourceGL.h"
#include "pxr/imaging/hdSt/GL/flatNormalsGL.h"
#include "pxr/imaging/hdSt/GL/glslProgram.h"
#include "pxr/imaging/hdSt/GL/indirectDrawBatchGL.h"
#include "pxr/imaging/hdSt/GL/interleavedMemoryBufferGL.h"
#include "pxr/imaging/hdSt/GL/persistentBufferGL.h"
#include "pxr/imaging/hdSt/GL/renderPassStateGL.h"
#include "pxr/imaging/hdSt/GL/resourceBinderGL.h"
#include "pxr/imaging/hdSt/GL/quadrangulateGL.h"
#include "pxr/imaging/hdSt/GL/smoothNormalsGL.h"
#include "pxr/imaging/hdSt/GL/textureResourceGL.h"
#include "pxr/imaging/hdSt/GL/vboMemoryBufferGL.h"
#include "pxr/imaging/hdSt/GL/vboSimpleMemoryBufferGL.h"

#include <boost/smart_ptr.hpp>

PXR_NAMESPACE_OPEN_SCOPE

HdStResourceFactoryGL::HdStResourceFactoryGL()
{
    // Empty
}

HdStResourceFactoryGL::~HdStResourceFactoryGL()
{
    // Empty
}

HdSt_CodeGen *HdStResourceFactoryGL::NewCodeGen(
    HdSt_GeometricShaderPtr const &geometricShader,
    HdStShaderCodeSharedPtrVector const &shaders) const
{
    return new HdSt_CodeGenGLSL(geometricShader, shaders);
}

HdSt_CodeGen *HdStResourceFactoryGL::NewCodeGen(
    HdStShaderCodeSharedPtrVector const &shaders) const
{
    return new HdSt_CodeGenGLSL(shaders);
}

HdStDispatchBuffer *HdStResourceFactoryGL::NewDispatchBuffer(
    TfToken const &role, int count,
    unsigned int commandNumUints) const
{
    return new HdStDispatchBufferGL(role, count, commandNumUints);
}

HdStBufferRelocator *HdStResourceFactoryGL::NewBufferRelocator(
    HdResourceGPUHandle srcBuffer,
    HdResourceGPUHandle dstBuffer) const
{
    return new HdStBufferRelocatorGL(srcBuffer, dstBuffer);
}

HdStBufferResource *HdStResourceFactoryGL::NewBufferResource(
    TfToken const &role,
    HdTupleType tupleType,
    int offset,
    int stride) const
{
    return new HdStBufferResourceGL(role, tupleType, offset, stride);
}

HdTextureResourceSharedPtr
HdStResourceFactoryGL::NewDrawTargetTextureResource() const
{
    return HdTextureResourceSharedPtr(new HdSt_DrawTargetTextureResourceGL());
}

HdSt_FlatNormalsComputationGPU*
HdStResourceFactoryGL::NewFlatNormalsComputationGPU(
    HdBufferArrayRangeSharedPtr const &topologyRange,
    HdBufferArrayRangeSharedPtr const &vertexRange,
    int numFaces, TfToken const &srcName, TfToken const &dstName,
    HdType srcDataType, bool packed) const
{
    return new HdSt_FlatNormalsComputationGL(
        topologyRange, vertexRange, numFaces, srcName, dstName, srcDataType,
        packed);
}

HdBufferArraySharedPtr
HdStResourceFactoryGL::NewStripedInterleavedBuffer(
    TfToken const &role,
    HdBufferSpecVector const &bufferSpecs,
    HdBufferArrayUsageHint usageHint,
    int bufferOffsetAlignment,
    int structAlignment,
    size_t maxSize,
    TfToken const &garbageCollectionPerfToken) const
{
    return boost::make_shared<
    HdStStripedInterleavedBufferGL>(role,
                                    bufferSpecs,
                                    usageHint,
                                    bufferOffsetAlignment,
                                    structAlignment,
                                    maxSize,
                                    garbageCollectionPerfToken);
}

HdSt_DrawBatchSharedPtr HdStResourceFactoryGL::NewIndirectDrawBatch(
    HdStDrawItemInstance * drawItemInstance) const
{
    return HdSt_DrawBatchSharedPtr(
        new HdSt_IndirectDrawBatchGL(drawItemInstance));
}

HdStPersistentBuffer *HdStResourceFactoryGL::NewPersistentBuffer(
    TfToken const &role, size_t dataSize, void* data) const
{
    return new HdStPersistentBufferGL(role, dataSize, data);
}

/// Create a GPU quadrangulation computation
HdSt_QuadrangulateComputationGPU *
HdStResourceFactoryGL::NewQuadrangulateComputationGPU(
    HdSt_MeshTopology *topology,
    TfToken const &sourceName,
    HdType dataType,
    SdfPath const &id) const
{
    return new HdSt_QuadrangulateComputationGPUGL(topology,
                                                  sourceName,
                                                  dataType,
                                                  id);
}

HdStRenderPassState *HdStResourceFactoryGL::NewRenderPassState() const
{
    return new HdStRenderPassStateGL();
}

HdStRenderPassState *HdStResourceFactoryGL::NewRenderPassState(
    HdStRenderPassShaderSharedPtr const &renderPassShader) const
{
    return new HdStRenderPassStateGL(renderPassShader);
}

HdSt_ResourceBinder *HdStResourceFactoryGL::NewResourceBinder() const
{
    return new HdSt_ResourceBinderGL();
}

/// Creates a new smooth normals GPU computation
HdSt_SmoothNormalsComputationGPU *
HdStResourceFactoryGL::NewSmoothNormalsComputationGPU(
    Hd_VertexAdjacency const *adjacency,
    TfToken const &srcName, TfToken const &dstName,
    HdType srcDataType, bool packed) const
{
    return new HdSt_SmoothNormalsComputationGL(
        adjacency, srcName, dstName, srcDataType, packed);
}

HdStSimpleTextureResource *
HdStResourceFactoryGL::NewSimpleTextureResource(
    GarchTextureHandleRefPtr const &textureHandle,
    HdTextureType textureType,
    size_t memoryRequest) const
{
    return new HdStSimpleTextureResourceGL(
        textureHandle, textureType, memoryRequest);
}

HdStSimpleTextureResource *
HdStResourceFactoryGL::NewSimpleTextureResource(
    GarchTextureHandleRefPtr const &textureHandle,
    HdTextureType textureType,
    HdWrap wrapS, HdWrap wrapT,
    HdMinFilter minFilter, HdMagFilter magFilter,
    size_t memoryRequest) const
{
    return new HdStSimpleTextureResourceGL(
        textureHandle, textureType, wrapS, wrapT, minFilter, magFilter,
        memoryRequest);
}

HdBufferArraySharedPtr HdStResourceFactoryGL::NewVBOMemoryBuffer(
    TfToken const &role,
    HdBufferSpecVector const &bufferSpecs,
    HdBufferArrayUsageHint usageHint) const
{
    return boost::make_shared<HdStVBOMemoryBufferGL>(
                                role, bufferSpecs, usageHint);
}

HdBufferArraySharedPtr HdStResourceFactoryGL::NewVBOSimpleMemoryBuffer(
    TfToken const &role,
    HdBufferSpecVector const &bufferSpecs,
    HdBufferArrayUsageHint usageHint) const
{
    return boost::make_shared<HdStVBOSimpleMemoryBufferGL>(
                                role, bufferSpecs, usageHint);
}

HdStProgram *HdStResourceFactoryGL::NewProgram(
    TfToken const &role) const
{
    return new HdStGLSLProgram(role);
}

PXR_NAMESPACE_CLOSE_SCOPE

