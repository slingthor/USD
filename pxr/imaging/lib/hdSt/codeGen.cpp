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

#include "pxr/imaging/hdSt/codeGen.h"
#include "pxr/imaging/hdSt/GL/codeGenGLSL.h"
#include "pxr/imaging/hdSt/Metal/codeGenMSL.h"

#include "pxr/imaging/hd/engine.h"

PXR_NAMESPACE_OPEN_SCOPE

HdSt_CodeGen *HdSt_CodeGen::New(HdSt_GeometricShaderPtr const &geometricShader,
                                   HdStShaderCodeSharedPtrVector const &shaders)
{
    
    HdEngine::RenderAPI api = HdEngine::GetRenderAPI();
    switch(api)
    {
        case HdEngine::OpenGL:
            return new HdSt_CodeGenGLSL(geometricShader, shaders);
#if defined(ARCH_GFX_METAL)
        case HdEngine::Metal:
            return new HdSt_CodeGenMSL(geometricShader, shaders);
#endif
        default:
            TF_FATAL_CODING_ERROR("No HdStBufferResource for this API");
    }
    return NULL;
}

HdSt_CodeGen *HdSt_CodeGen::New(HdStShaderCodeSharedPtrVector const &shaders)
{
    HdEngine::RenderAPI api = HdEngine::GetRenderAPI();
    switch(api)
    {
        case HdEngine::OpenGL:
            return new HdSt_CodeGenGLSL(shaders);
#if defined(ARCH_GFX_METAL)
        case HdEngine::Metal:
            return new HdSt_CodeGenMSL(shaders);
#endif
        default:
            TF_FATAL_CODING_ERROR("No HdStBufferResource for this API");
    }
    return NULL;
}

PXR_NAMESPACE_CLOSE_SCOPE

