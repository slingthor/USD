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
#include "pxr/imaging/glf/glew.h"

#include "pxr/imaging/hdSt/GL/domeLightComputationsGL.h"
#include "pxr/imaging/hdSt/resourceRegistry.h"
#include "pxr/imaging/hdSt/package.h"
#include "pxr/imaging/hdSt/tokens.h"

#include "pxr/imaging/hdSt/GL/glslProgram.h"

#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hf/perfLog.h"

#include "pxr/base/tf/token.h"

PXR_NAMESPACE_OPEN_SCOPE


HdSt_DomeLightComputationGPUGL::HdSt_DomeLightComputationGPUGL(
    TfToken token,
    GarchTextureGPUHandle const &sourceId, GarchTextureGPUHandle const &destId, 
    int width, int height, unsigned int numLevels, unsigned int level, 
    float roughness) 
    : HdSt_DomeLightComputationGPU(token, sourceId, destId,
        width, height, numLevels, level, roughness)
{
}


void
HdSt_DomeLightComputationGPUGL::_Execute(HdStProgramSharedPtr computeProgram)
{
    HdStGLSLProgramSharedPtr const &glslProgram(
        boost::dynamic_pointer_cast<HdStGLSLProgram>(computeProgram));
    GLuint programId = glslProgram->GetGLProgram();

    // bind the input and output textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _sourceTextureId);
    
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, _destTextureId);
    glBindImageTexture(1, _destTextureId, _level, _layered, _layer, 
                        GL_WRITE_ONLY, GL_RGBA16F);

    glUseProgram(programId);

    // if we are calculating the irradiance map we do not need to send over
    // the roughness value to the shader
    // flagged this with a negative roughness value
    if (_roughness >= 0.0) {
        glUniform1f(glGetUniformLocation(programId, "roughness"), _roughness);
    }

    // dispatch compute kernel
    glDispatchCompute(  (GLuint)_textureWidth / 32, 
                        (GLuint)_textureHeight / 32, 1);

    glUseProgram(0);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
}

PXR_NAMESPACE_CLOSE_SCOPE
