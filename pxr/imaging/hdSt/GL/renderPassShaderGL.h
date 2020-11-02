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
#ifndef PXR_IMAGING_HD_ST_RENDER_PASS_SHADER_GL_H
#define PXR_IMAGING_HD_ST_RENDER_PASS_SHADER_GL_H

#include "pxr/pxr.h"
#include "pxr/imaging/hdSt/api.h"
#include "pxr/imaging/hdSt/renderPassShader.h"

PXR_NAMESPACE_OPEN_SCOPE


/// \class HdStRenderPassShaderGL
///
/// A shader that supports common renderPass functionality.
///
class HdStRenderPassShaderGL : public HdStRenderPassShader {
public:
    HDST_API
    HdStRenderPassShaderGL();
    HDST_API
    HdStRenderPassShaderGL(TfToken const &glslfxFile);

    HDST_API
    virtual ~HdStRenderPassShaderGL() override;

protected:
    HDST_API
    void UnbindResources(HdStGLSLProgram const &program,
                         HdSt_ResourceBinder const &binder,
                         HdRenderPassState const &state) override;
    void
    _BindTexture(HdStGLSLProgram const &program,
                 const HdRenderPassAovBinding &aov,
                 const TfToken &bindName,
                 const HdBinding &binding) override;
    void
    _UnbindTexture(const HdBinding &binding) override;

private:

    // No copying
    HdStRenderPassShaderGL(const HdStRenderPassShader &)             = delete;
    HdStRenderPassShaderGL &operator =(const HdStRenderPassShader &) = delete;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_IMAGING_HD_ST_RENDER_PASS_SHADER_GL_H
