//
// Copyright 2019 Pixar
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
#ifndef HDST_DOME_LIGHT_COMPUTATIONS_GL_H
#define HDST_DOME_LIGHT_COMPUTATIONS_GL_H

#include "pxr/pxr.h"
#include "pxr/imaging/hdSt/api.h"
#include "pxr/imaging/hdSt/domeLightComputations.h"

PXR_NAMESPACE_OPEN_SCOPE

class HdSt_DomeLightComputationGPUGL : public HdSt_DomeLightComputationGPU {
protected:
    /// Constructor
    HDST_API
    HdSt_DomeLightComputationGPUGL(const TfToken & shaderToken,
                                   HgiTextureHandle const& sourceGLTextureName,
                                   HdStSimpleLightingShaderPtr const &lightingShader,
                                   unsigned int numLevels,
                                   unsigned int level,
                                   float roughness);
    
    friend class HdStResourceFactoryGL;

    HDST_API
    void _Execute(HdStProgramSharedPtr computeProgram) override;
    
    HDST_API
    GarchTextureGPUHandle
    _CreateGLTexture(const int32_t width, const int32_t height) const override;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDST_DOME_LIGHT_COMPUTATIONS_H
