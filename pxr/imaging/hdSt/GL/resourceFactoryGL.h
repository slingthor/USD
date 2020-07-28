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
#ifndef HDST_RESOURCEFACTORY_GL_H
#define HDST_RESOURCEFACTORY_GL_H

/// \file hdSt/GL/resourceFactoryGL.h

#include "pxr/pxr.h"
#include "pxr/imaging/hdSt/api.h"
#include "pxr/imaging/hdSt/resourceFactory.h"
#include "pxr/imaging/hdSt/bufferResourceGL.h"

#include "pxr/imaging/glf/resourceFactory.h"

PXR_NAMESPACE_OPEN_SCOPE

class HdStResourceFactoryGL
: public GlfResourceFactory,
  public HdStResourceFactoryInterface {
public:
    HDST_API
    HdStResourceFactoryGL();
    
    HDST_API
    virtual ~HdStResourceFactoryGL();
    
    // Temp
    virtual bool IsOpenGL() const override { return true; }

    // HdSt_CodeGen for OpenGL
    HDST_API
    virtual HdSt_CodeGen *NewCodeGen(
        HdSt_GeometricShaderPtr const &geometricShader,
        HdStShaderCodeSharedPtrVector const &shaders) const override;
    
    HDST_API
    virtual HdSt_CodeGen *NewCodeGen(
        HdStShaderCodeSharedPtrVector const &shaders) const override;

    /// Creates a draw target texture resource for OpenGL
    HDST_API
    virtual HdStTextureResourceSharedPtr
        NewDrawTargetTextureResource() const override;
    
    /// Creates an indirect draw batch for OpenGL
    HDST_API
    virtual HdSt_DrawBatchSharedPtr NewIndirectDrawBatch(
        HdStDrawItemInstance * drawItemInstance) const override;
    
    /// Creates a graphics API specific GPU quadrangulate computation
    /// This computaion doesn't generate buffer source (i.e. 2nd phase)
    HDST_API
    virtual HdSt_QuadrangulateComputationGPU *NewQuadrangulateComputationGPU(
        HdSt_MeshTopology *topology,
        TfToken const &sourceName,
        HdType dataType,
        SdfPath const &id) const override;
    
    /// Creates a new smooth normals GPU computation for OpenGL
    HDST_API
    virtual HdSt_SmoothNormalsComputationGPU *NewSmoothNormalsComputationGPU(
        Hd_VertexAdjacency const *adjacency,
        TfToken const &srcName, TfToken const &dstName,
        HdType srcDataType, bool packed) const override;
    
    /// Creates a GPU flat normals computation for OpenGL
    HDST_API
    virtual HdSt_FlatNormalsComputationGPU *NewFlatNormalsComputationGPU(
        HdBufferArrayRangeSharedPtr const &topologyRange,
        HdBufferArrayRangeSharedPtr const &vertexRange,
        int numFaces, TfToken const &srcName, TfToken const &dstName,
        HdType srcDataType, bool packed) const override;
    
    /// Creates a new ExtCompGPUComputation computation
    HDST_API
    virtual HdStExtCompGpuComputation *NewExtCompGPUComputationGPU(
        SdfPath const &id,
        HdStExtCompGpuComputationResourceSharedPtr const &resource,
        HdExtComputationPrimvarDescriptorVector const &compPrimvars,
        int dispatchCount,
        int elementCount) const override;
      
    /// Creates a new HdSt_DomeLightComputationGPU computation
    HDST_API
    virtual HdSt_DomeLightComputationGPU *NewDomeLightComputationGPU(
        const TfToken & shaderToken,
        HdStSimpleLightingShaderPtr const &lightingShader,
        unsigned int numLevels,
        unsigned int level,
        float roughness) const override;

    /// Creates a new render pass state for OpenGL
    HDST_API
    virtual HdStRenderPassState *NewRenderPassState() const override;
    
    /// Creates a new render pass state for OpenGL
    HDST_API
    virtual HdStRenderPassState *NewRenderPassState(
        HdStRenderPassShaderSharedPtr const &renderPassShader) const override;
    
    /// Creates a resource binder for OpenGL
    HDST_API
    virtual HdSt_ResourceBinder *NewResourceBinder() const override;
    
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
        size_t memoryRequest) const override;
    
    HDST_API
    virtual HdStSimpleTextureResource *NewSimpleTextureResource(
        GarchTextureHandleRefPtr const &textureHandle,
        HdTextureType textureType,
        HdWrap wrapS, HdWrap wrapT, HdWrap wrapR,
        HdMinFilter minFilter, HdMagFilter magFilter,
        size_t memoryRequest = 0) const override;
    
    HDST_API
    virtual const char* const GetComputeShaderFilename() const override {
        return "compute.glslfx";
    }
    
    HDST_API
    virtual const char* const GetPtexTextureShaderFilename() const override {
        return "ptexTextureGL.glslfx";
    }
      
    HDST_API
    virtual HdStProgram *NewProgram(
        TfToken const &role, HdStResourceRegistry *const registry) const override;
      
    HDST_API
    virtual HdStRenderPassShaderSharedPtr NewRenderPassShader() const override;

    HDST_API
    virtual HdStRenderPassShaderSharedPtr NewRenderPassShader(
        TfToken const &glslfxFile) const override;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDST_RESOURCEFACTORY_GL_H
