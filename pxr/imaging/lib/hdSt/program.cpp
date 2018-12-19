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

#include "pxr/imaging/hdSt/program.h"
#include "pxr/imaging/hdSt/package.h"
#include "pxr/imaging/hdSt/resourceRegistry.h"

#include "pxr/imaging/hd/engine.h"
#include "pxr/imaging/hd/instanceRegistry.h"
#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/tokens.h"

#include "pxr/imaging/garch/glslfx.h"

#if defined(ARCH_GFX_OPENGL)
#include "pxr/imaging/hdSt/GL/glslProgram.h"
#endif
#if defined(ARCH_GFX_METAL)
#include "pxr/imaging/hdSt/Metal/mslProgram.h"
#endif

PXR_NAMESPACE_OPEN_SCOPE

/// Creates a graphics API specific program
HdStProgram *HdStProgram::New(TfToken const &role)
{
    HdEngine::RenderAPI api = HdEngine::GetRenderAPI();
    switch(api)
    {
#if defined(ARCH_GFX_OPENGL)
        case HdEngine::OpenGL:
            return new HdStGLSLProgram(role);
#endif
#if defined(ARCH_GFX_METAL)
        case HdEngine::Metal:
            return new HdStMSLProgram(role);
#endif
        default:
            TF_FATAL_CODING_ERROR("No HdStBufferResource for this API");
    }
    return NULL;
}

HdStProgram::HdStProgram(TfToken const &role) {

}

HdStProgram::~HdStProgram() {
    
}

/* static */
HdStProgram::ID
HdStProgram::ComputeHash(TfToken const &sourceFile)
{
    HD_TRACE_FUNCTION();

    uint32_t hash = 0;
    std::string const &filename = sourceFile.GetString();
    hash = ArchHash(filename.c_str(), filename.size(), hash);

    return hash;
}

HdStProgramSharedPtr
HdStProgram::GetComputeProgram(
    TfToken const &shaderToken,
    HdStResourceRegistry *resourceRegistry)
{
    // Find the program from registry
    HdInstance<HdStProgram::ID, HdStProgramSharedPtr> programInstance;

    std::unique_lock<std::mutex> regLock = 
        resourceRegistry->RegisterProgram(
            HdStProgram::ComputeHash(shaderToken), &programInstance);

    if (programInstance.IsFirstInstance()) {
        // if not exists, create new one
        
        HdStProgramSharedPtr newProgram(HdStProgram::New(HdTokens->computeShader));
        boost::scoped_ptr<GLSLFX> glslfx(new GLSLFX(HdStPackageComputeShader()));

        std::string header = newProgram->GetComputeHeader();
        if (!newProgram->CompileShader(
                GL_COMPUTE_SHADER, header + glslfx->GetSource(shaderToken))) {
            TF_CODING_ERROR("Fail to compile " + shaderToken.GetString());
            return HdStProgramSharedPtr();
        }
        if (!newProgram->Link()) {
            TF_CODING_ERROR("Fail to link " + shaderToken.GetString());
            return HdStProgramSharedPtr();
        }
        programInstance.SetValue(newProgram);
    }

    return programInstance.GetValue();
}


PXR_NAMESPACE_CLOSE_SCOPE

