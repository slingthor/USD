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
#ifndef PXR_IMAGING_HD_ST_RENDER_PASS_SHADER_METAL_H
#define PXR_IMAGING_HD_ST_RENDER_PASS_SHADER_METAL_H

#include "pxr/pxr.h"
#include "pxr/imaging/hdSt/api.h"
#include "pxr/imaging/hdSt/renderPassShader.h"
#include "pxr/imaging/hgi/sampler.h"


PXR_NAMESPACE_OPEN_SCOPE

using HdStRenderPassShaderSharedPtr =
    std::shared_ptr<class HdStRenderPassShader>;

/// \class HdStRenderPassShader
///
/// A shader that supports common renderPass functionality.
///
class HdStRenderPassShaderMetal : public HdStRenderPassShader {
public:
    HDST_API
    HdStRenderPassShaderMetal();
    HDST_API
    HdStRenderPassShaderMetal(TfToken const &glslfxFile);
    HDST_API
    virtual ~HdStRenderPassShaderMetal() override;

protected:
    void
    _BindTexture(HdStGLSLProgram const &program,
                 const HdRenderPassAovBinding &aov,
                 const TfToken &bindName,
                 const HdBinding &binding) override;
    void
    _UnbindTexture(const HdBinding &binding) override;
    
private:

    // No copying
    HdStRenderPassShaderMetal(const HdStRenderPassShader &) = delete;
    HdStRenderPassShaderMetal &operator =(const HdStRenderPassShader &)=delete;
    
    HgiSamplerHandle _sampler;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_IMAGING_HD_ST_RENDER_PASS_SHADER_METAL_H
