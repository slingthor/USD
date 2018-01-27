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

#include "pxr/imaging/hd/program.h"
#include "pxr/imaging/hd/engine.h"
#include "pxr/imaging/hd/instanceRegistry.h"
#include "pxr/imaging/hd/package.h"
#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/resourceRegistry.h"
#include "pxr/imaging/hd/tokens.h"

#include "pxr/imaging/garch/glslfx.h"

PXR_NAMESPACE_OPEN_SCOPE

HdProgram::HdProgram(TfToken const &role) {

}

HdProgram::~HdProgram() {
    
}

/* static */
HdProgram::ID
HdProgram::ComputeHash(TfToken const &sourceFile)
{
    HD_TRACE_FUNCTION();

    uint32_t hash = 0;
    std::string const &filename = sourceFile.GetString();
    hash = ArchHash(filename.c_str(), filename.size(), hash);

    return hash;
}

HdProgramSharedPtr
HdProgram::GetComputeProgram(
    TfToken const &shaderToken,
    HdResourceRegistry *resourceRegistry)
{
    TF_CODING_ERROR("Not Implemented");

    // Find the program from registry
    HdInstance<HdProgram::ID, HdProgramSharedPtr> programInstance;

    std::unique_lock<std::mutex> regLock = 
        resourceRegistry->RegisterProgram(
            HdProgram::ComputeHash(shaderToken), &programInstance);

    if (programInstance.IsFirstInstance()) {
        // if not exists, create new one
        
        HdProgramSharedPtr newProgram(HdEngine::CreateProgram(HdTokens->computeShader));
        boost::scoped_ptr<GLSLFX> glslfx(HdEngine::CreateGLSLFX(HdPackageComputeShader()));

        std::string version = "#version 430\n";
        if (!newProgram->CompileShader(
                GL_COMPUTE_SHADER, version + glslfx->GetSource(shaderToken))) {
            TF_CODING_ERROR("Fail to compile " + shaderToken.GetString());
            return HdProgramSharedPtr();
        }
        if (!newProgram->Link()) {
            TF_CODING_ERROR("Fail to link " + shaderToken.GetString());
            return HdProgramSharedPtr();
        }
        programInstance.SetValue(newProgram);
    }

    return programInstance.GetValue();
}


PXR_NAMESPACE_CLOSE_SCOPE

