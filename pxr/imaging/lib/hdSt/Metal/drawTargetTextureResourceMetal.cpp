//
// Copyright 2017 Pixar
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
#include "pxr/imaging/hdSt/Metal/drawTargetTextureResourceMetal.h"
#include "pxr/imaging/hd/conversions.h"

PXR_NAMESPACE_OPEN_SCOPE



HdSt_DrawTargetTextureResourceMetal::HdSt_DrawTargetTextureResourceMetal()
 : HdSt_DrawTargetTextureResource()
{
}

HdSt_DrawTargetTextureResourceMetal::~HdSt_DrawTargetTextureResourceMetal()
{
}

void
HdSt_DrawTargetTextureResourceMetal::SetSampler(HdWrap wrapS,
                                          HdWrap wrapT,
                                          HdMinFilter minFilter,
                                          HdMagFilter magFilter)
{
    static const float borderColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    TF_CODING_ERROR("Not Implemented");
    /*
    GLenum glWrapS = HdConversions::GetWrap(wrapS);
    GLenum glWrapT = HdConversions::GetWrap(wrapT);
    GLenum glMinFilter = HdConversions::GetMinFilter(minFilter);
    GLenum glMagFilter = HdConversions::GetMagFilter(magFilter);

    glSamplerParameteri(_sampler, GL_TEXTURE_WRAP_S, glWrapS);
    glSamplerParameteri(_sampler, GL_TEXTURE_WRAP_T, glWrapT);
    glSamplerParameteri(_sampler, GL_TEXTURE_MIN_FILTER, glMinFilter);
    glSamplerParameteri(_sampler, GL_TEXTURE_MAG_FILTER, glMagFilter);
    glSamplerParameterf(_sampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.0);
    glSamplerParameterfv(_sampler, GL_TEXTURE_BORDER_COLOR, borderColor);
     */
}

GarchTextureGPUHandle
HdSt_DrawTargetTextureResourceMetal::GetTexelsTextureHandle()
{
    GarchTextureGPUHandle textureId = GetTexelsTextureId();

    if (textureId == 0) {
        return 0;
    }

    TF_CODING_ERROR("Not Implemented");
    return textureId;
}

PXR_NAMESPACE_CLOSE_SCOPE

