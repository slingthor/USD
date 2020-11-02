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

#include "pxr/imaging/hdSt/Metal/codeGenMSL.h"
#include "pxr/imaging/hdSt/Metal/indirectDrawBatchMetal.h"
#include "pxr/imaging/hdSt/Metal/glslProgramMetal.h"
#include "pxr/imaging/hdSt/Metal/renderPassShaderMetal.h"
#include "pxr/imaging/hdSt/Metal/renderPassStateMetal.h"
#include "pxr/imaging/hdSt/Metal/resourceBinderMetal.h"
#include "pxr/imaging/hdSt/Metal/textureResourceMetal.h"

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

HdSt_DrawBatchSharedPtr HdStResourceFactoryMetal::NewIndirectDrawBatch(
    HdStDrawItemInstance * drawItemInstance) const
{
    return HdSt_DrawBatchSharedPtr(
        new HdSt_IndirectDrawBatchMetal(drawItemInstance));
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
    HdWrap wrapS, HdWrap wrapT, HdWrap wrapR,
    HdMinFilter minFilter, HdMagFilter magFilter,
    size_t memoryRequest) const
{
    return new HdStSimpleTextureResourceMetal(
        textureHandle, textureType, wrapS, wrapT, wrapR, minFilter, magFilter,
        memoryRequest);
}

HdStGLSLProgram *HdStResourceFactoryMetal::NewProgram(
    TfToken const &role, HdStResourceRegistry *const registry) const
{
    return new HdStGLSLProgramMSL(role, registry);
}

HdStRenderPassShaderSharedPtr HdStResourceFactoryMetal::NewRenderPassShader() const
{
    return std::make_shared<HdStRenderPassShaderMetal>();
}

HdStRenderPassShaderSharedPtr HdStResourceFactoryMetal::NewRenderPassShader(
    TfToken const &glslfxFile) const
{
    return std::make_shared<HdStRenderPassShaderMetal>(glslfxFile);
}

PXR_NAMESPACE_CLOSE_SCOPE

