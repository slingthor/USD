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

#include "pxr/imaging/hd/driver.h"

#include "pxr/imaging/garch/resourceFactory.h"

#include "pxr/base/tf/envSetting.h"

#include "pxr/imaging/mtlf/contextCaps.h"
#include "pxr/imaging/mtlf/drawTarget.h"
#include "pxr/imaging/mtlf/diagnostic.h"
#include "pxr/imaging/mtlf/mtlDevice.h"

#include "pxr/imaging/hgiMetal/hgi.h"

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
    , _mtlRenderPassDescriptor(nil)
{
//    _Initialize();
}

HdStRenderDelegateMetal::HdStRenderDelegateMetal(HdRenderSettingsMap const& settingsMap)
    : HdStRenderDelegate(settingsMap)
    , _mtlRenderPassDescriptor(nil)
{
}

void
HdStRenderDelegateMetal::SetDrivers(HdDriverVector const& drivers)
{
    HdStRenderDelegate::SetDrivers(drivers);

    id<MTLDevice> currentDevice = nil;

    if (!MtlfMetalContext::GetMetalContext()) {
        MtlfMetalContext::CreateMetalContext(static_cast<HgiMetal*>(_hgi));
    }
    
    _deviceDesc = TfToken(_MetalDeviceDescriptor(MtlfMetalContext::GetMetalContext()->currentDevice));
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

    return ret;
}

void HdStRenderDelegateMetal::SetRenderSetting(TfToken const& key, VtValue const& value)
{
    HdStRenderDelegate::SetRenderSetting(key, value);
}

void HdStRenderDelegateMetal::CommitResources(HdChangeTracker *tracker)
{
    MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();

    context->StartFrameForThread();
    context->PrepareBufferFlush();

    HdStRenderDelegate::CommitResources(tracker);

    context->FlushBuffers();

    if (context->GeometryShadersActive()) {
        // Complete the GS command buffer if we have one
        context->CommitCommandBufferForThread(false, METALWORKQUEUE_GEOMETRY_SHADER);
    }
    if (context->GetWorkQueue(METALWORKQUEUE_DEFAULT).commandBuffer != nil) {
        context->CommitCommandBufferForThread(false);
    }

    context->EndFrameForThread();
}

VtValue HdStRenderDelegateMetal::GetRenderSetting(TfToken const& key) const
{
    return  HdStRenderDelegate::GetRenderSetting(key);
}

HdStRenderDelegateMetal::~HdStRenderDelegateMetal()
{
    if (_mtlRenderPassDescriptor) {
        [_mtlRenderPassDescriptor release];
    }
}

bool
HdStRenderDelegateMetal::IsSupported()
{
    if (MtlfContextCaps::GetAPIVersion() >= MtlfContextCaps::APIVersion_Metal2_0)
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

    context->StartFrame();
    context->StartFrameForThread();

    if (_mtlRenderPassDescriptor == nil)
    {
        _mtlRenderPassDescriptor =
            [[MTLRenderPassDescriptor alloc] init];
    }

    // Set the render pass descriptor to use for the render encoders
    context->SetRenderPassDescriptor(_mtlRenderPassDescriptor);

    // hydra orients all geometry during topological processing so that
    // front faces have ccw winding. We disable culling because culling
    // is handled by fragment shader discard.
    if (!params.flipFrontFacing) {
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
        context->SetAlphaBlendingEnable(false);
    }
}

void HdStRenderDelegateMetal::FinalizeRender()
{
    MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();
    context->CleanupUnusedBuffers(false);

    context->EndFrameForThread();
    context->EndFrame();
}

PXR_NAMESPACE_CLOSE_SCOPE
