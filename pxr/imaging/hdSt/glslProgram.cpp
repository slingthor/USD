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
#include "pxr/imaging/garch/glApi.h"

#include "pxr/imaging/hdSt/glslProgram.h"
#include "pxr/imaging/hdSt/package.h"
#include "pxr/imaging/hdSt/resourceFactory.h"
#include "pxr/imaging/hdSt/resourceRegistry.h"

#include "pxr/imaging/hd/engine.h"
#include "pxr/imaging/hd/instanceRegistry.h"
#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/tokens.h"

#include "pxr/imaging/hio/glslfx.h"
#include "pxr/imaging/hgi/shaderProgram.h"

#include "pxr/base/arch/hash.h"

PXR_NAMESPACE_OPEN_SCOPE

HdStGLSLProgram::HdStGLSLProgram(TfToken const &role,
                         HdStResourceRegistry* resourceRegistry)
: _role(role)
, _registry(resourceRegistry)
{

}

HdStGLSLProgram::~HdStGLSLProgram() {
    
}

/* static */
HdStGLSLProgram::ID
HdStGLSLProgram::ComputeHash(TfToken const &sourceFile)
{
    HD_TRACE_FUNCTION();

    uint32_t hash = 0;
    std::string const &filename = sourceFile.GetString();
    hash = ArchHash(filename.c_str(), filename.size(), hash);

    return hash;
}

HdStGLSLProgramSharedPtr
HdStGLSLProgram::GetComputeProgram(
    TfToken const &shaderToken,
    HdStResourceRegistry *resourceRegistry)
{
    return GetComputeProgram(HdStPackageComputeShader(), shaderToken,
                             resourceRegistry);
}

HdStGLSLProgramSharedPtr
HdStGLSLProgram::GetComputeProgram(
    const TfToken& shaderToken,
    HdStResourceRegistry *resourceRegistry,
    PopulateDescriptorCallback callable)
{
    // Find the program from registry
    HdInstance<HdStGLSLProgramSharedPtr> programInstance =
                resourceRegistry->RegisterGLSLProgram(
                        HdStGLSLProgram::ComputeHash(shaderToken));

    if (programInstance.IsFirstInstance()) {
        TfToken const &shaderFileName = HdStPackageComputeShader();
        const HioGlslfx glslfx(shaderFileName, HioGlslfxTokens->defVal);
        std::string errorString;
        if (!glslfx.IsValid(&errorString)){
            TF_CODING_ERROR("Failed to parse " + shaderFileName.GetString()
                            + ": " + errorString);
            return HdStGLSLProgramSharedPtr();
        }

        Hgi *hgi = resourceRegistry->GetHgi();
        HgiShaderFunctionDesc computeDesc;
        std::string sourceCode;

        callable(computeDesc);

        sourceCode += glslfx.GetSource(shaderToken);
        computeDesc.shaderCode = sourceCode.c_str();
        HgiShaderFunctionHandle computeFn = hgi->CreateShaderFunction(computeDesc);
        
        // if not exists, create new one
        HdStGLSLProgramSharedPtr newProgram(
            HdStResourceFactory::GetInstance()->NewProgram(
                HdTokens->computeShader, resourceRegistry));

        newProgram->_programDesc.shaderFunctions.push_back(computeFn);
        newProgram->Link();
        
        programInstance.SetValue(newProgram);
    }

    return programInstance.GetValue();
}

HdStGLSLProgramSharedPtr
HdStGLSLProgram::GetComputeProgram(
    TfToken const &shaderFileName,
    TfToken const &shaderToken,
    HdStResourceRegistry *resourceRegistry)
{
    // Find the program from registry
    HdInstance<HdStGLSLProgramSharedPtr> programInstance =
                resourceRegistry->RegisterGLSLProgram(
                        HdStGLSLProgram::ComputeHash(shaderToken));

    if (programInstance.IsFirstInstance()) {
        // if not exists, create new one
        HdStGLSLProgramSharedPtr newProgram(
            HdStResourceFactory::GetInstance()->NewProgram(
                HdTokens->computeShader, resourceRegistry));
        
        std::unique_ptr<HioGlslfx> glslfx(
            new HioGlslfx(shaderFileName));
        std::string errorString;
        if (!glslfx->IsValid(&errorString)){
            TF_CODING_ERROR("Failed to parse " + shaderFileName.GetString()
                            + ": " + errorString);
            return HdStGLSLProgramSharedPtr();
        }

        std::string header = newProgram->GetComputeHeader();
        if (!newProgram->CompileShader(
                HgiShaderStageCompute, header + glslfx->GetSource(shaderToken))) {
            TF_CODING_ERROR("Fail to compile " + shaderToken.GetString());
            return HdStGLSLProgramSharedPtr();
        }
        if (!newProgram->Link()) {
            TF_CODING_ERROR("Fail to link " + shaderToken.GetString());
            return HdStGLSLProgramSharedPtr();
        }
        programInstance.SetValue(newProgram);
    }

    return programInstance.GetValue();
}


PXR_NAMESPACE_CLOSE_SCOPE

