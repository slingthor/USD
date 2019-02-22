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
#include "pxr/pxr.h"
#include "pxr/imaging/hdSt/Metal/renderDelegateMetal.h"
#include "pxr/imaging/hdSt/tokens.h"

#include "pxr/usdImaging/usdImagingGL/rendererSettings.h"
#include "pxr/usdImaging/usdImagingGL/renderParams.h"

#include "pxr/imaging/garch/resourceFactory.h"

#include "pxr/imaging/mtlf/contextCaps.h"
#include "pxr/imaging/mtlf/diagnostic.h"
#include "pxr/imaging/mtlf/mtlDevice.h"

PXR_NAMESPACE_OPEN_SCOPE

namespace {
static
std::string
_MetalDeviceDescriptor(id<MTLDevice> device)
{
    return [[device name] UTF8String];
}

} // anonymous namespace

HdStRenderDelegateMetal::HdStRenderDelegateMetal()
    : HdStRenderDelegate()
    , _mtlRenderPassDescriptorForInterop(nil)
    , _mtlRenderPassDescriptor(nil)
{
    _deviceDesc = TfToken(_MetalDeviceDescriptor(MtlfMetalContext::GetMetalContext()->device));
//    _Initialize();
}

HdStRenderDelegateMetal::HdStRenderDelegateMetal(HdRenderSettingsMap const& settingsMap)
    : HdStRenderDelegate(settingsMap)
    , _mtlRenderPassDescriptorForInterop(nil)
    , _mtlRenderPassDescriptor(nil)
{
    _deviceDesc = TfToken(_MetalDeviceDescriptor(MtlfMetalContext::GetMetalContext()->device));
}

HdRenderSettingDescriptorList
HdStRenderDelegateMetal::GetRenderSettingDescriptors() const
{
    HdRenderSettingDescriptorList ret(
        HdStRenderDelegate::GetRenderSettingDescriptors());

    // Metal Device Options
    std::vector<std::string> apiDevices;
    
#if defined(ARCH_OS_MACOS)
    NSArray<id<MTLDevice>> *_deviceList = MTLCopyAllDevices();
#else
    NSMutableArray<id<MTLDevice>> *_deviceList = [[NSMutableArray alloc] init];
    [_deviceList addObject:MTLCreateSystemDefaultDevice()];
#endif
    apiDevices.resize(_deviceList.count);

    int i = 0;
    for (id<MTLDevice> dev in _deviceList) {
        apiDevices[i++] = _MetalDeviceDescriptor(dev);
    }

    ret.push_back({ "GPU",
        HdStRenderSettingsTokens->graphicsAPI,
        VtValue(apiDevices) });

    return ret;
}

void HdStRenderDelegateMetal::SetRenderSetting(TfToken const& key, VtValue const& value)
{
    if (key == HdStRenderSettingsTokens->graphicsAPI) {
        _deviceDesc = TfToken(value.Get<std::string>());
        
#if defined(ARCH_OS_MACOS)
        NSArray<id<MTLDevice>> *_deviceList = MTLCopyAllDevices();
#else
        NSMutableArray<id<MTLDevice>> *_deviceList = [[NSMutableArray alloc] init];
        [_deviceList addObject:MTLCreateSystemDefaultDevice()];
#endif
        
        for (id<MTLDevice> dev in _deviceList) {
            if (value == _MetalDeviceDescriptor(dev)) {
                // Recreate the underlying Metal context
                MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();
                MtlfMetalContext::RecreateInstance(dev, context->mtlColorTexture.width, context->mtlColorTexture.height);
                break;
            }
        }

        return;
    }

    HdStRenderDelegateMetal::SetRenderSetting(key, value);
}

VtValue HdStRenderDelegateMetal::GetRenderSetting(TfToken const& key) const
{
    if (key == HdStRenderSettingsTokens->graphicsAPI) {
        return VtValue(std::string(_deviceDesc.GetText()));
    }
    
    return  HdStRenderDelegate::GetRenderSetting(key);
}

HdStRenderDelegateMetal::~HdStRenderDelegateMetal()
{
    // Nothing
}

bool
HdStRenderDelegateMetal::IsSupported()
{
    if (MtlfContextCaps::GetAPIVersion() >= 400)
        return true;

    return false;
}

void HdStRenderDelegateMetal::PrepareRender(
    DelegateParams const &params)
{
    GarchContextCaps const &caps =
        GarchResourceFactory::GetInstance()->GetContextCaps();
    
    MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();
    
    _renderOutput = params.renderOutput;

    if (_renderOutput == DelegateParams::RenderOutput::OpenGL &&
        params.mtlRenderPassDescriptorForNativeMetal) {
        TF_CODING_ERROR("SetMetalRenderPassDescriptor isn't valid to call "
                        "when using OpenGL as the output target");
        return;
    }
    if (params.mtlRenderPassDescriptorForNativeMetal) {
        _mtlRenderPassDescriptor = [params.mtlRenderPassDescriptorForNativeMetal copy];
    }

#if defined(ARCH_GFX_OPENGL)
    // Make sure the Metal render targets, and GL interop textures match the GL viewport size
    GLint viewport[4];
    glGetIntegerv( GL_VIEWPORT, viewport );
    
    if (context->mtlColorTexture.width != viewport[2] ||
        context->mtlColorTexture.height != viewport[3]) {
        context->AllocateAttachments(viewport[2], viewport[3]);
    }
    
    if (_renderOutput == DelegateParams::RenderOutput::OpenGL) {
        if (_mtlRenderPassDescriptorForInterop == nil)
            _mtlRenderPassDescriptorForInterop =
                [[MTLRenderPassDescriptor alloc] init];
        
        //Set this state every frame because it may have changed
        // during rendering.
        
        // create a color attachment every frame since we have to
        // recreate the texture every frame
        MTLRenderPassColorAttachmentDescriptor *colorAttachment =
            _mtlRenderPassDescriptorForInterop.colorAttachments[0];
        
        // make sure to clear every frame for best performance
        colorAttachment.loadAction = MTLLoadActionClear;
        
        // store only attachments that will be presented to the
        // screen, as in this case
        colorAttachment.storeAction = MTLStoreActionStore;
        
        MTLRenderPassDepthAttachmentDescriptor *depthAttachment =
            _mtlRenderPassDescriptorForInterop.depthAttachment;
        depthAttachment.loadAction = MTLLoadActionClear;
        depthAttachment.storeAction = MTLStoreActionStore;
        depthAttachment.clearDepth = 1.0f;
        
        colorAttachment.texture = context->mtlColorTexture;
        
        GLfloat clearColor[4];
        glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor);
        clearColor[3] = 1.0f;
        
        colorAttachment.clearColor = MTLClearColorMake(clearColor[0],
                                                       clearColor[1],
                                                       clearColor[2],
                                                       clearColor[3]);
        depthAttachment.texture = context->mtlDepthTexture;
        
        _mtlRenderPassDescriptor = _mtlRenderPassDescriptorForInterop;
    }
    else
#else
    if (false) {}
    else
#endif
    {
        if (_mtlRenderPassDescriptor == nil) {
            TF_FATAL_CODING_ERROR(
                "SetMetalRenderPassDescriptor must be called prior "
                "to rendering when render output is set to Metal");
        }
    }
    
    context->StartFrame();
    
    // Create a new command buffer for each render pass to the current drawable
    context->CreateCommandBuffer(METALWORKQUEUE_DEFAULT);
    context->LabelCommandBuffer(@"HdEngine::Render", METALWORKQUEUE_DEFAULT);
    
    // Set the render pass descriptor to use for the render encoders
    context->SetRenderPassDescriptor(_mtlRenderPassDescriptor);
    if (_renderOutput == DelegateParams::RenderOutput::Metal) {
        [_mtlRenderPassDescriptor release];
        _mtlRenderPassDescriptor = nil;
    }
    // hydra orients all geometry during topological processing so that
    // front faces have ccw winding. We disable culling because culling
    // is handled by fragment shader discard.
    if (params.flipFrontFacing) {
        context->SetFrontFaceWinding(MTLWindingClockwise);
    } else {
        context->SetFrontFaceWinding(MTLWindingCounterClockwise);
    }
    context->SetCullMode(MTLCullModeNone);
    
    if (params.applyRenderState) {
        // drawmode.
        // XXX: Temporary solution until shader-based styling implemented.
        switch (params.drawMode) {
            case HdStDrawMode::DRAW_POINTS:
                context->SetTempPointWorkaround(true);
                break;
            default:
                context->SetPolygonFillMode(MTLTriangleFillModeFill);
                context->SetTempPointWorkaround(false);
                break;
        }
    }
    // TODO:
    //  * forceRefresh
    //  * showGuides, showRender, showProxy
    //  * gammaCorrectColors
}
void HdStRenderDelegateMetal::FinalizeRender()
{
    MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();

    if (_renderOutput == DelegateParams::RenderOutput::OpenGL) {
        // Depth texture copy
        context->CopyDepthTextureToOpenGL();
    }
    
    if (context->GeometryShadersActive()) {
        // Complete the GS command buffer if we have one
        context->CommitCommandBuffer(true, false, METALWORKQUEUE_GEOMETRY_SHADER);
    }
    
    // Commit the render buffer (will wait for GS to complete if present)
    // We wait until scheduled, because we're about to consume the Metal
    // generated textures in an OpenGL blit
    context->CommitCommandBuffer(
        _renderOutput == DelegateParams::RenderOutput::OpenGL, false);
    
    context->EndFrame();
    
    // Finalize rendering here & push the command buffer to the GPU
    if (_renderOutput == DelegateParams::RenderOutput::OpenGL) {
        context->BlitColorTargetToOpenGL();
        GLF_POST_PENDING_GL_ERRORS();
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
