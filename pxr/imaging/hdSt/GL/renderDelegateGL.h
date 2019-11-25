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
#ifndef HDST_RENDER_DELEGATE_GL_H
#define HDST_RENDER_DELEGATE_GL_H

#include "pxr/pxr.h"
#include "pxr/imaging/hdSt/renderDelegate.h"

PXR_NAMESPACE_OPEN_SCOPE

///
/// HdStRenderDelegateGL
///
/// The Stream Render Delegate provides a Hydra render that uses a
/// streaming graphics implementation to draw the scene.
///
class HdStRenderDelegateGL final : public HdStRenderDelegate {
public:
    HDST_API
    HdStRenderDelegateGL();
    HDST_API
    HdStRenderDelegateGL(HdRenderSettingsMap const& settingsMap);

    HDST_API
    virtual ~HdStRenderDelegateGL();

    // Returns whether or not HdStRenderDelegate can run on the current
    // hardware.
    HDST_API
    static bool IsSupported();
    
    HDST_API
    virtual void PrepareRender(DelegateParams const &params) override;
    
    HDST_API
    virtual void FinalizeRender() override;

private:

    HdStRenderDelegateGL(
        const HdStRenderDelegateGL &) = delete;
    HdStRenderDelegateGL &operator =(
        const HdStRenderDelegateGL &) = delete;
    
    bool _isCoreProfileContext;
    GLuint _vao;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDST_RENDER_DELEGATE_GL_H
