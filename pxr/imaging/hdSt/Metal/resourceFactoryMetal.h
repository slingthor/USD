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
#ifndef HDST_RESOURCEFACTORY_METAL_H
#define HDST_RESOURCEFACTORY_METAL_H

/// \file hdSt/Metal/resourceFactoryMetal.h

#include "pxr/pxr.h"
#include "pxr/imaging/hdSt/api.h"
#include "pxr/imaging/hdSt/resourceFactory.h"

#include "pxr/imaging/mtlf/resourceFactory.h"

PXR_NAMESPACE_OPEN_SCOPE

class HdStResourceFactoryMetal
: public MtlfResourceFactory,
  public HdStResourceFactoryInterface {
public:
    HDST_API
    HdStResourceFactoryMetal();

    HDST_API
    virtual ~HdStResourceFactoryMetal();
      
    // Temp
    virtual bool IsOpenGL() const override { return false; }
    
    // HdSt_CodeGen for Metal
    HDST_API
    virtual HdSt_CodeGen *NewCodeGen(
        HdSt_GeometricShaderPtr const &geometricShader,
        HdStShaderCodeSharedPtrVector const &shaders) const override;
    
    HDST_API
    virtual HdSt_CodeGen *NewCodeGen(
        HdStShaderCodeSharedPtrVector const &shaders) const override;

    /// Creates an indirect draw batch for Metal
    HDST_API
    virtual HdSt_DrawBatchSharedPtr NewIndirectDrawBatch(
        HdStDrawItemInstance * drawItemInstance) const override;

    /// Creates a new render pass state for Metal
    HDST_API
    virtual HdStRenderPassState *NewRenderPassState() const override;

    /// Creates a new render pass state for Metal
    HDST_API
    virtual HdStRenderPassState *NewRenderPassState(
        HdStRenderPassShaderSharedPtr const &renderPassShader) const override;

    /// Creates a resource binder for Metal
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
        return "compute.metal";
    }
    
    HDST_API
    virtual const char* const GetPtexTextureShaderFilename() const override {
        return "ptexTextureMetal.glslfx";
    }
      
    HDST_API
    virtual HdStGLSLProgram *NewProgram(
        TfToken const &role, HdStResourceRegistry *const registry) const override;
      
    HDST_API
    virtual HdStRenderPassShaderSharedPtr NewRenderPassShader() const override;

    HDST_API
    virtual HdStRenderPassShaderSharedPtr NewRenderPassShader(
        TfToken const &glslfxFile) const override;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDST_RESOURCEFACTORY_METAL_H
