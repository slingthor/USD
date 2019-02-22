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
#ifndef HDST_RENDER_DELEGATE_METAL_H
#define HDST_RENDER_DELEGATE_METAL_H

#include "pxr/pxr.h"
#include "pxr/imaging/hdSt/api.h"
#include "pxr/imaging/hd/engine.h"
#include "pxr/imaging/hdSt/renderDelegate.h"

PXR_NAMESPACE_OPEN_SCOPE

///
/// HdStRenderDelegateMetal
///
/// The Stream Render Delegate provides a Hydra render that uses a
/// streaming graphics implementation to draw the scene.
///
class HdStRenderDelegateMetal final : public HdStRenderDelegate {
public:
    HDST_API
    HdStRenderDelegateMetal();
    HDST_API
    HdStRenderDelegateMetal(HdRenderSettingsMap const& settingsMap);

    HDST_API
    virtual ~HdStRenderDelegateMetal();

    // Returns whether or not HdStRenderDelegateMetal can run on the current
    // hardware.
    HDST_API
    static bool IsSupported();
    
    ///
    /// Set a custom render setting on this render delegate.
    ///
    HDST_API
    virtual void SetRenderSetting(TfToken const& key, VtValue const& value);
    
    ///
    /// Get the current value for a render setting.
    ///
    HDST_API
    virtual VtValue GetRenderSetting(TfToken const& key) const;

    virtual HdRenderSettingDescriptorList
        GetRenderSettingDescriptors() const override;

    HDST_API
    virtual void PrepareRender(DelegateParams const &params) override;
    
    HDST_API
    virtual void FinalizeRender() override;

private:
    
    TfToken _deviceDesc;

    HdStRenderDelegateMetal(
        const HdStRenderDelegateMetal &) = delete;
    HdStRenderDelegateMetal &operator =(
        const HdStRenderDelegateMetal &) = delete;

    DelegateParams::RenderOutput _renderOutput;
    MTLRenderPassDescriptor *_mtlRenderPassDescriptorForInterop;
    MTLRenderPassDescriptor *_mtlRenderPassDescriptor;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDST_RENDER_DELEGATE_METAL_H
