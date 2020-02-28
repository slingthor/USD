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

#include "pxr/imaging/hd/aov.h"
#include "pxr/imaging/hd/binding.h"
#include "pxr/imaging/hd/engine.h"
#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/tokens.h"
#include "pxr/imaging/hd/renderBuffer.h"
#include "pxr/imaging/hd/renderPassState.h"
#include "pxr/imaging/hgiGL/texture.h"
#include "pxr/imaging/hdSt/package.h"
#include "pxr/imaging/hdSt/GL/renderPassShaderGL.h"
#include "pxr/imaging/hdSt/GL/glslProgram.h"

#include "pxr/imaging/hio/glslfx.h"

#include <boost/functional/hash.hpp>

#include <string>

PXR_NAMESPACE_OPEN_SCOPE

// Name shader uses to read AOV, i.e., shader calls
// HdGet_AOVNAMEReadback().
static
TfToken
_GetReadbackName(const TfToken &aovName)
{
    return TfToken(aovName.GetString() + "Readback");
}

HdStRenderPassShaderGL::HdStRenderPassShaderGL()
    : HdStRenderPassShader()
{
}

HdStRenderPassShaderGL::HdStRenderPassShaderGL(TfToken const &glslfxFile)
    : HdStRenderPassShader(glslfxFile)
{
}

/*virtual*/
HdStRenderPassShaderGL::~HdStRenderPassShaderGL()
{
    // nothing
}

// Helper to bind texture from given AOV to GLSL program identified
// by \p program.
static
void
_BindTexture(const HdRenderPassAovBinding &aov,
             const HdBinding &binding)
{
    if (binding.GetType() != HdBinding::TEXTURE_2D) {
        TF_CODING_ERROR("When binding readback for aov '%s', binding is "
                        "not of type TEXTURE_2D.",
                        aov.aovName.GetString().c_str());
        return;
    }
    
    HdRenderBuffer * const buffer = aov.renderBuffer;
    if (!buffer) {
        TF_CODING_ERROR("When binding readback for aov '%s', AOV has invalid "
                        "render buffer.", aov.aovName.GetString().c_str());
        return;
    }

    // Get texture from AOV's render buffer.
    const bool multiSampled = false;
    HgiGLTexture * const texture = dynamic_cast<HgiGLTexture*>(
        buffer->GetHgiTextureHandle(multiSampled));
    if (!texture) {
        TF_CODING_ERROR("When binding readback for aov '%s', AOV is not backed "
                        "by HgiGLTexture.", aov.aovName.GetString().c_str());
        return;
    }

    // Get OpenGL texture Id.
    const int textureId = texture->GetTextureId();

    // XXX:-matthias
    // Some of this code is duplicated, see HYD-1788.

    // Sampler unit was determined during binding resolution.
    // Use it to bind texture.
    const int samplerUnit = binding.GetTextureUnit();
    glActiveTexture(GL_TEXTURE0 + samplerUnit);
    glBindTexture(GL_TEXTURE_2D, (GLuint) textureId);
    glBindSampler(samplerUnit, 0);
}

/*virtual*/
void
HdStRenderPassShaderGL::BindResources(HdStProgram const &program,
                                      HdSt_ResourceBinder const &binder,
                                      HdRenderPassState const &state)
{
    HdStRenderPassShader::BindResources(program, binder, state);

    // Count how many textures we bind for check at the end.
    size_t numFulfilled = 0;

    // Loop over all AOVs for which a read back was requested.
    for (const HdRenderPassAovBinding &aovBinding : state.GetAovBindings()) {
        const TfToken &aovName = aovBinding.aovName;
        if (_aovReadbackRequests.count(aovName) > 0) {
            // Bind the texture.
            _BindTexture(aovBinding,
                         binder.GetBinding(_GetReadbackName(aovName)));

            numFulfilled++;
        }
    }

    if (numFulfilled != _aovReadbackRequests.size()) {
        TF_CODING_ERROR("AOV bindings missing for requested readbacks.");
    }
}

// Helper to unbind what was bound with _BindTexture.
static
void
_UnbindTexture(const HdBinding &binding)
{
    if (binding.GetType() != HdBinding::TEXTURE_2D) {
        // Coding error already issued in _BindTexture.
        return;
    }

    const int samplerUnit = binding.GetTextureUnit();
    glActiveTexture(GL_TEXTURE0 + samplerUnit);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindSampler(samplerUnit, 0);
}    


/*virtual*/
void
HdStRenderPassShaderGL::UnbindResources(HdStProgram const &program,
                                      HdSt_ResourceBinder const &binder,
                                      HdRenderPassState const &state)
{
    HdStRenderPassShader::UnbindResources(program, binder, state);

    // Unbind all textures that were requested for AOV read back
    for (const HdRenderPassAovBinding &aovBinding : state.GetAovBindings()) {
        const TfToken &aovName = aovBinding.aovName;
        if (_aovReadbackRequests.count(aovName) > 0) {
            _UnbindTexture(binder.GetBinding(_GetReadbackName(aovName)));
        }
    }    

    glActiveTexture(GL_TEXTURE0);
}

PXR_NAMESPACE_CLOSE_SCOPE
