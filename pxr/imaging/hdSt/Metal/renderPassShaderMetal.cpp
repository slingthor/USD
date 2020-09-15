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
#include "pxr/imaging/hgiMetal/texture.h"
#include "pxr/imaging/hgiMetal/sampler.h"
#include "pxr/imaging/hdSt/resourceBinder.h"
#include "pxr/imaging/hdSt/package.h"
#include "pxr/imaging/hdSt/Metal/renderPassShaderMetal.h"
#include "pxr/imaging/hdSt/Metal/glslProgramMetal.h"

#include "pxr/imaging/hio/glslfx.h"

#include <boost/functional/hash.hpp>

#include "pxr/imaging/hgiMetal/hgi.h"
#include "pxr/imaging/mtlf/mtlDevice.h"

#include <string>

PXR_NAMESPACE_OPEN_SCOPE

HdStRenderPassShaderMetal::HdStRenderPassShaderMetal()
    : HdStRenderPassShader()
{
}

HdStRenderPassShaderMetal::HdStRenderPassShaderMetal(TfToken const &glslfxFile)
    : HdStRenderPassShader(glslfxFile)
{
}

/*virtual*/
HdStRenderPassShaderMetal::~HdStRenderPassShaderMetal()
{
    // nothing
}

// Helper to bind texture from given AOV to GLSL program identified
// by \p program.
void
HdStRenderPassShaderMetal::_BindTexture(HdStGLSLProgram const &program,
                                        const HdRenderPassAovBinding &aov,
                                        const TfToken &bindName,
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
    VtValue rv = buffer->GetResource(multiSampled);
    
    HgiMetalTexture * const texture = rv.IsHolding<HgiTextureHandle>() ?
        dynamic_cast<HgiMetalTexture*>(rv.Get<HgiTextureHandle>().Get()) :
        nullptr;

    if (!texture) {
        TF_CODING_ERROR("When binding readback for aov '%s', AOV is not backed "
                        "by HgiMetalTexture.", aov.aovName.GetString().c_str());
        return;
    }
    
    if (!_sampler) {
        HgiSamplerDesc sampDesc;

        sampDesc.magFilter = HgiSamplerFilterLinear;
        sampDesc.minFilter = HgiSamplerFilterLinear;

        sampDesc.addressModeU = HgiSamplerAddressModeClampToEdge;
        sampDesc.addressModeV = HgiSamplerAddressModeClampToEdge;

        _sampler = MtlfMetalContext::GetMetalContext()->GetHgi()->CreateSampler(sampDesc);
    }

    // Get Metal texture Id.
    id<MTLTexture> textureId = texture->GetTextureId();
    id<MTLSamplerState> samplerId =
        dynamic_cast<HgiMetalSampler*>(_sampler.Get())->GetSamplerId();

    HdStGLSLProgramMSL const &mslProgram(
        static_cast<HdStGLSLProgramMSL const&>(program));
    mslProgram.BindTexture(bindName, textureId);
    mslProgram.BindSampler(bindName, samplerId);
}

// Helper to unbind what was bound with _BindTexture.
void
HdStRenderPassShaderMetal::_UnbindTexture(const HdBinding &binding)
{
    if (binding.GetType() != HdBinding::TEXTURE_2D) {
        // Coding error already issued in _BindTexture.
        return;
    }
    
    TF_FATAL_CODING_ERROR("Not Implemented");
/*
    const int samplerUnit = binding.GetTextureUnit();
    glActiveTexture(GL_TEXTURE0 + samplerUnit);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindSampler(samplerUnit, 0);
 */
}    

PXR_NAMESPACE_CLOSE_SCOPE
