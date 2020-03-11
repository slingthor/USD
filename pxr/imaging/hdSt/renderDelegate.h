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
#ifndef PXR_IMAGING_HD_ST_RENDER_DELEGATE_H
#define PXR_IMAGING_HD_ST_RENDER_DELEGATE_H

#include "pxr/pxr.h"
#include "pxr/imaging/hdSt/api.h"
#include "pxr/imaging/hd/engine.h"
#include "pxr/imaging/hd/renderDelegate.h"

#include <mutex>

PXR_NAMESPACE_OPEN_SCOPE

class Hgi;

class Hgi;

typedef boost::shared_ptr<class HdStResourceRegistry>
    HdStResourceRegistrySharedPtr;

enum class HdStDrawMode
{
    DRAW_POINTS,
    DRAW_WIREFRAME,
    DRAW_WIREFRAME_ON_SURFACE,
    DRAW_SHADED_FLAT,
    DRAW_SHADED_SMOOTH,
    DRAW_GEOM_ONLY,
    DRAW_GEOM_FLAT,
    DRAW_GEOM_SMOOTH
};

///
/// HdStRenderDelegate
///
/// The Storm Render Delegate provides a Hydra render that uses a
/// streaming graphics implementation to draw the scene.
///
class HdStRenderDelegate : public HdRenderDelegate {
public:
    
    class DelegateParams {
    public:

        enum RenderOutput {
            /// Use output of the render will be blitted from Metal into the
            /// currently bound OpenGL FBO - if OpenGL is included in the build
            OpenGL,
            
            /// The output will be rendered using the application supplied
            /// MTLRenderPassDescriptor - if Metal is included in the build
            Metal,
        };
        
        bool flipFrontFacing;
        bool applyRenderState;
        bool enableIdRender;
        bool enableSampleAlphaToCoverage;
        unsigned long sampleCount;
        HdStDrawMode drawMode;
        RenderOutput renderOutput;
        
#if defined(PXR_METAL_SUPPORT_ENABLED)
        MTLRenderPassDescriptor *mtlRenderPassDescriptorForNativeMetal;
#endif
        
        DelegateParams(bool _flipFrontFacing,
                       bool _applyRenderState,
                       bool _enableIdRender,
                       bool _enableSampleAlphaToCoverage,
                       unsigned long _sampleCount,
                       HdStDrawMode _drawMode,
                       RenderOutput _renderOutput)
        : flipFrontFacing(_flipFrontFacing)
        , applyRenderState(_applyRenderState)
        , enableIdRender(_enableIdRender)
        , enableSampleAlphaToCoverage(_enableSampleAlphaToCoverage)
        , sampleCount(_sampleCount)
        , drawMode(_drawMode)
        , renderOutput(_renderOutput)
        {
#if defined(PXR_METAL_SUPPORT_ENABLED)
            mtlRenderPassDescriptorForNativeMetal = nil;
#endif
        }

    private:
        DelegateParams();
    };
    
    HDST_API
    virtual ~HdStRenderDelegate();

    HDST_API
    virtual void SetDrivers(HdDriverVector const& drivers) override;

    HDST_API
    virtual HdRenderParam *GetRenderParam() const override;

    HDST_API
    virtual const TfTokenVector &GetSupportedRprimTypes() const override;
    HDST_API
    virtual const TfTokenVector &GetSupportedSprimTypes() const override;
    HDST_API
    virtual const TfTokenVector &GetSupportedBprimTypes() const override;
    HDST_API
    virtual HdResourceRegistrySharedPtr GetResourceRegistry() const override;

    HDST_API
    virtual HdRenderPassSharedPtr CreateRenderPass(HdRenderIndex *index,
                HdRprimCollection const& collection) override;
    HDST_API
    virtual HdRenderPassStateSharedPtr CreateRenderPassState() const override;

    HDST_API
    virtual HdInstancer *CreateInstancer(HdSceneDelegate *delegate,
                                         SdfPath const& id,
                                         SdfPath const& instancerId) override;

    HDST_API
    virtual void DestroyInstancer(HdInstancer *instancer) override;

    HDST_API
    virtual HdRprim *CreateRprim(TfToken const& typeId,
                                 SdfPath const& rprimId,
                                 SdfPath const& instancerId) override;
    HDST_API
    virtual void DestroyRprim(HdRprim *rPrim) override;

    HDST_API
    virtual HdSprim *CreateSprim(TfToken const& typeId,
                                 SdfPath const& sprimId) override;
    HDST_API
    virtual HdSprim *CreateFallbackSprim(TfToken const& typeId) override;
    HDST_API
    virtual void DestroySprim(HdSprim *sPrim) override;

    HDST_API
    virtual HdBprim *CreateBprim(TfToken const& typeId,
                                 SdfPath const& bprimId) override;
    HDST_API
    virtual HdBprim *CreateFallbackBprim(TfToken const& typeId) override;
    HDST_API
    virtual void DestroyBprim(HdBprim *bPrim) override;

    HDST_API
    virtual void CommitResources(HdChangeTracker *tracker) override;

    HDST_API
    virtual TfToken GetMaterialNetworkSelector() const override;

    HDST_API
    virtual TfTokenVector GetShaderSourceTypes() const override;

    HDST_API
    virtual bool IsPrimvarFilteringNeeded() const override;

    // Returns whether or not HdStRenderDelegate can run on the current
    // hardware.
    HDST_API
    static bool IsSupported();

    HDST_API
    virtual HdRenderSettingDescriptorList
        GetRenderSettingDescriptors() const override;
    
    HDST_API
    virtual void PrepareRender(DelegateParams const &params) = 0;
    
    HDST_API
    virtual void FinalizeRender() = 0;

    HDST_API
    virtual VtDictionary GetRenderStats() const override;

    HDST_API
    virtual HdAovDescriptor
        GetDefaultAovDescriptor(TfToken const& name) const override;

    // Returns Hydra graphics interface
    HDST_API
    Hgi* GetHgi();

protected:
    HDST_API
    HdStRenderDelegate();
    HDST_API
    HdStRenderDelegate(HdRenderSettingsMap const& settingsMap);

    Hgi *_hgi;
    
private:
    static const TfTokenVector SUPPORTED_RPRIM_TYPES;
    static const TfTokenVector SUPPORTED_SPRIM_TYPES;
    static const TfTokenVector SUPPORTED_BPRIM_TYPES;

    /// Resource registry used in this render delegate
    static std::mutex _mutexResourceRegistry;
    static std::atomic_int _counterResourceRegistry;
    static HdStResourceRegistrySharedPtr _resourceRegistry;

    HdRenderSettingDescriptorList _settingDescriptors;

    void _Initialize();

    HdSprim *_CreateFallbackMaterialPrim();

    HdStRenderDelegate(const HdStRenderDelegate &)             = delete;
    HdStRenderDelegate &operator =(const HdStRenderDelegate &) = delete;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_IMAGING_HD_ST_RENDER_DELEGATE_H
