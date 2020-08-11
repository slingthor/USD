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

#include "pxr/imaging/hdSt/bufferResource.h"
#include "pxr/imaging/hdSt/dispatchBuffer.h"
#include "pxr/imaging/hdSt/GL/codeGenGLSL.h"
#include "pxr/imaging/hdSt/GL/domeLightComputationsGL.h"
#include "pxr/imaging/hdSt/GL/drawTargetTextureResourceGL.h"
#include "pxr/imaging/hdSt/GL/extCompGpuComputationGL.h"
#include "pxr/imaging/hdSt/GL/glslProgramGL.h"
#include "pxr/imaging/hdSt/GL/indirectDrawBatchGL.h"
#include "pxr/imaging/hdSt/GL/renderPassStateGL.h"
#include "pxr/imaging/hdSt/GL/renderPassShaderGL.h"
#include "pxr/imaging/hdSt/GL/resourceBinderGL.h"
#include "pxr/imaging/hdSt/GL/textureResourceGL.h"

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

HdStTextureResourceSharedPtr
HdStResourceFactoryGL::NewDrawTargetTextureResource() const
{
    return HdStTextureResourceSharedPtr(new HdSt_DrawTargetTextureResourceGL());
}

HdSt_DrawBatchSharedPtr HdStResourceFactoryGL::NewIndirectDrawBatch(
    HdStDrawItemInstance * drawItemInstance) const
{
    return HdSt_DrawBatchSharedPtr(
        new HdSt_IndirectDrawBatchGL(drawItemInstance));
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
    HdWrap wrapS, HdWrap wrapT, HdWrap wrapR,
    HdMinFilter minFilter, HdMagFilter magFilter,
    size_t memoryRequest) const
{
    return new HdStSimpleTextureResourceGL(
        textureHandle, textureType, wrapS, wrapT, wrapR, minFilter, magFilter,
        memoryRequest);
}

HdStGLSLProgram *HdStResourceFactoryGL::NewProgram(
    TfToken const &role, HdStResourceRegistry *const registry) const
{
    return new HdStglslProgramGLSL(role, registry);
}

HdStExtCompGpuComputation *
HdStResourceFactoryGL::NewExtCompGPUComputationGPU(
    SdfPath const &id,
    HdStExtCompGpuComputationResourceSharedPtr const &resource,
    HdExtComputationPrimvarDescriptorVector const &compPrimvars,
    int dispatchCount,
    int elementCount) const
{
    return new HdStExtCompGpuComputationGL(
                    id, resource, compPrimvars, dispatchCount, elementCount);
}

HdSt_DomeLightComputationGPU*
HdStResourceFactoryGL::NewDomeLightComputationGPU(
    const TfToken & shaderToken,
    HdStSimpleLightingShaderPtr const &lightingShader,
    unsigned int numLevels,
    unsigned int level,
    float roughness) const
{
    return new HdSt_DomeLightComputationGPUGL(shaderToken,
        lightingShader, numLevels, level, roughness);
}

HdStRenderPassShaderSharedPtr HdStResourceFactoryGL::NewRenderPassShader() const
{
    return std::make_shared<HdStRenderPassShaderGL>();
}

HdStRenderPassShaderSharedPtr HdStResourceFactoryGL::NewRenderPassShader(
    TfToken const &glslfxFile) const
{
    return std::make_shared<HdStRenderPassShaderGL>(glslfxFile);
}

PXR_NAMESPACE_CLOSE_SCOPE

