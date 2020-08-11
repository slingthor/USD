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
#include "pxr/imaging/hdSt/GL/glslProgramGL.h"
#include "pxr/imaging/hdSt/simpleLightingShader.h"
#include "pxr/imaging/hdSt/resourceRegistry.h"
#include "pxr/imaging/hdSt/package.h"
#include "pxr/imaging/hdSt/tokens.h"
#include "pxr/imaging/hdSt/textureObject.h"
#include "pxr/imaging/hdSt/textureHandle.h"
#include "pxr/imaging/hdSt/dynamicUvTextureObject.h"
#include "pxr/imaging/hgiGL/texture.h"

#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hf/perfLog.h"

#include "pxr/base/tf/token.h"

PXR_NAMESPACE_OPEN_SCOPE


HdSt_DomeLightComputationGPUGL::HdSt_DomeLightComputationGPUGL(
    const TfToken &shaderToken,
    HdStSimpleLightingShaderPtr const &lightingShader,
    const unsigned int numLevels,
    const unsigned int level, 
    const float roughness) 
    : HdSt_DomeLightComputationGPU(
        shaderToken,
        lightingShader,
        numLevels,
        level,
        roughness)
{
}

GarchTextureGPUHandle
HdSt_DomeLightComputationGPUGL::_GetGlTextureName(const HgiTexture * const hgiTexture)
{
    const HgiGLTexture * const glTexture = 
        dynamic_cast<const HgiGLTexture*>(hgiTexture);
    if (!glTexture) {
        TF_CODING_ERROR(
            "Texture in dome light computation is not HgiGLTexture");
        return GarchTextureGPUHandle();
    }
    const GarchTextureGPUHandle textureName = glTexture->GetTextureId();
    if (!textureName.IsSet()) {
        TF_CODING_ERROR(
            "Texture in dome light computation has zero GL name");
    }
    return textureName;
}

void
HdSt_DomeLightComputationGPUGL::_Execute(HdStGLSLProgramSharedPtr computeProgram)
{
    HdStglslProgramGLSL const *glslProgram(
        dynamic_cast<const HdStglslProgramGLSL*>(computeProgram.get()));
    const GLuint programId = glslProgram->GetGLProgram();

    HdStSimpleLightingShaderSharedPtr const shader = _lightingShader.lock();
    if (!TF_VERIFY(shader)) {
        return;
    }

    // Size of source texture (the dome light environment map)
    GfVec3i srcDim;
    // GL name of source texture
    GarchTextureGPUHandle srcGLTextureName;
    if (!_GetSrcTextureDimensionsAndGLName(
            shader, &srcDim, &srcGLTextureName)) {
        return;
    }

    // Size of texture to be created.
    const GLint width  = srcDim[0] / 2;
    const GLint height = srcDim[1] / 2;

    // Get texture object from lighting shader that this
    // computation is supposed to populate
    HdStTextureHandleSharedPtr const &dstTextureHandle =
        shader->GetTextureHandle(_shaderToken);
    if (!TF_VERIFY(dstTextureHandle)) {
        return;
    }

    HdStDynamicUvTextureObject * const dstUvTextureObject =
        dynamic_cast<HdStDynamicUvTextureObject*>(
            dstTextureHandle->GetTextureObject().get());
    if (!TF_VERIFY(dstUvTextureObject)) {
        return;
    }
        
    if (_level == 0) {
        // Level zero is in charge of actually creating the
        // GPU resource.
        HgiTextureDesc desc;
        desc.debugName = _shaderToken.GetText();
        desc.format = HgiFormatFloat16Vec4;
        desc.dimensions = GfVec3i(width, height, 1);
        desc.layerCount = 1;
        desc.mipLevels = _numLevels;
        desc.usage =
            HgiTextureUsageBitsShaderRead | HgiTextureUsageBitsShaderWrite;
        _FillPixelsByteSize(&desc);
        dstUvTextureObject->CreateTexture(desc);
    }

    const GarchTextureGPUHandle dstGLTextureName = _GetGlTextureName(
        dstUvTextureObject->GetTexture().Get());

    // Now bind the textures and launch GPU computation
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, srcGLTextureName);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, dstGLTextureName);
    glBindImageTexture(1, dstGLTextureName,
                       _level,
                       /* layered = */ GL_FALSE,
                       /* layer = */ 0,
                       GL_WRITE_ONLY, GL_RGBA16F);

    glUseProgram(programId);

    // if we are calculating the irradiance map we do not need to send over
    // the roughness value to the shader
    // flagged this with a negative roughness value
    if (_roughness >= 0.0) {
        glUniform1f(glGetUniformLocation(programId, "roughness"), _roughness);
    }

    // dispatch compute kernel
    glDispatchCompute( (GLuint)width / 32, (GLuint)height / 32, 1);

    glUseProgram(0);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
}

PXR_NAMESPACE_CLOSE_SCOPE
