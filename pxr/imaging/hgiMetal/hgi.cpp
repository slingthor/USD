//
// Copyright 2019 Pixar
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
#include "pxr/base/arch/defines.h"

#include "pxr/imaging/hgiMetal/hgi.h"
#include "pxr/imaging/hgiMetal/buffer.h"
#include "pxr/imaging/hgiMetal/blitEncoder.h"
#include "pxr/imaging/hgiMetal/capabilities.h"
#include "pxr/imaging/hgiMetal/conversions.h"
#include "pxr/imaging/hgiMetal/diagnostic.h"
#include "pxr/imaging/hgiMetal/graphicsEncoder.h"
#include "pxr/imaging/hgiMetal/pipeline.h"
#include "pxr/imaging/hgiMetal/resourceBindings.h"
#include "pxr/imaging/hgiMetal/shaderFunction.h"
#include "pxr/imaging/hgiMetal/shaderProgram.h"
#include "pxr/imaging/hgiMetal/texture.h"

#include "pxr/base/tf/getenv.h"
#include "pxr/base/tf/registryManager.h"
#include "pxr/base/tf/type.h"

PXR_NAMESPACE_OPEN_SCOPE

TF_REGISTRY_FUNCTION(TfType)
{
    TfType t = TfType::Define<HgiMetal, TfType::Bases<Hgi> >();
    t.SetFactory<HgiFactory<HgiMetal>>();
}

static int _GetAPIVersion()
{
    if (@available(macOS 10.15, ios 13.0, *)) {
        return APIVersion_Metal3_0;
    }
    if (@available(macOS 10.13, ios 11.0, *)) {
        return APIVersion_Metal2_0;
    }
    
    return APIVersion_Metal1_0;
}

HgiMetal::HgiMetal(id<MTLDevice> device)
: _device(device)
, _frameDepth(0)
, _apiVersion(_GetAPIVersion())
, _useInterop(false)
, _workToFlush(false)
, _encoder(nil)
{
    if (!_device) {
#if defined(ARCH_OS_MACOS)
            if( TfGetenvBool("USD_METAL_USE_INTEGRATED_GPU", false)) {
                _device = MTLCopyAllDevices()[1];
            }
#endif
        if (!_device) {
            _device = MTLCreateSystemDefaultDevice();
        }
    }
    
    static int const commandBufferPoolSize = 256;
    _commandQueue = [_device newCommandQueueWithMaxCommandBufferCount:
                     commandBufferPoolSize];
    _commandBuffer = [_commandQueue commandBuffer];
    [_commandBuffer retain];

    _capabilities.reset(
        new HgiMetalCapabilities(device));

    HgiMetalSetupMetalDebug();
    
    _captureScopeFullFrame =
        [[MTLCaptureManager sharedCaptureManager]
            newCaptureScopeWithDevice:_device];
    _captureScopeFullFrame.label =
        [NSString stringWithFormat:@"Full Hydra Frame"];
    
    [[MTLCaptureManager sharedCaptureManager]
        setDefaultCaptureScope:_captureScopeFullFrame];
        
#if defined(PXR_OPENGL_SUPPORT_ENABLED)
    _useInterop = true;
#endif
}

HgiMetal::~HgiMetal()
{
    [_commandBuffer commit];
    [_commandBuffer release];
    [_captureScopeFullFrame release];
    [_commandQueue release];
}

HgiGraphicsEncoderUniquePtr
HgiMetal::CreateGraphicsEncoder(
    HgiGraphicsEncoderDesc const& desc)
{
    // XXX We should TF_CODING_ERROR here when there are no attachments, but
    // during the Hgi transition we allow it to render to global gl framebuffer.
    if (!desc.HasAttachments()) {
        // TF_CODING_ERROR("Graphics encoder desc has no attachments");
        return nullptr;
    }

    _workToFlush = true;
    HgiMetalGraphicsEncoder* encoder(
        new HgiMetalGraphicsEncoder(this, desc));

    // TEMP
    _encoder = encoder;
    
    return HgiGraphicsEncoderUniquePtr(encoder);
}

HgiBlitEncoderUniquePtr
HgiMetal::CreateBlitEncoder()
{
    _workToFlush = true;
    return HgiBlitEncoderUniquePtr(new HgiMetalBlitEncoder(this));
}

HgiTextureHandle
HgiMetal::CreateTexture(HgiTextureDesc const & desc)
{
    return HgiTextureHandle(new HgiMetalTexture(this, desc), GetUniqueId());
}

void
HgiMetal::DestroyTexture(HgiTextureHandle* texHandle)
{
    DestroyObject(texHandle);
}

HgiBufferHandle
HgiMetal::CreateBuffer(HgiBufferDesc const & desc)
{
    return HgiBufferHandle(new HgiMetalBuffer(this, desc), GetUniqueId());
}

void
HgiMetal::DestroyBuffer(HgiBufferHandle* bufHandle)
{
    DestroyObject(bufHandle);
}

HgiShaderFunctionHandle
HgiMetal::CreateShaderFunction(HgiShaderFunctionDesc const& desc)
{
    return HgiShaderFunctionHandle(
        new HgiMetalShaderFunction(this, desc), GetUniqueId());
}

void
HgiMetal::DestroyShaderFunction(HgiShaderFunctionHandle* shaderFunctionHandle)
{
    DestroyObject(shaderFunctionHandle);
}

HgiShaderProgramHandle
HgiMetal::CreateShaderProgram(HgiShaderProgramDesc const& desc)
{
    return HgiShaderProgramHandle(
        new HgiMetalShaderProgram(desc), GetUniqueId());
}

void
HgiMetal::DestroyShaderProgram(HgiShaderProgramHandle* shaderProgramHandle)
{
    DestroyObject(shaderProgramHandle);
}


HgiResourceBindingsHandle
HgiMetal::CreateResourceBindings(HgiResourceBindingsDesc const& desc)
{
    return HgiResourceBindingsHandle(
        new HgiMetalResourceBindings(desc), GetUniqueId());
}

void
HgiMetal::DestroyResourceBindings(HgiResourceBindingsHandle* resHandle)
{
    DestroyObject(resHandle);
}

HgiPipelineHandle
HgiMetal::CreatePipeline(HgiPipelineDesc const& desc)
{
    return HgiPipelineHandle(new HgiMetalPipeline(this, desc), GetUniqueId());
}

void
HgiMetal::DestroyPipeline(HgiPipelineHandle* pipeHandle)
{
    DestroyObject(pipeHandle);
}

TfToken const&
HgiMetal::GetAPIName() const {
    return HgiTokens->Metal;
}

void
HgiMetal::StartFrame()
{
    _encoder = nil;

    if (_frameDepth++ == 0) {
        if (@available(macos 10.15, ios 13.0, *)) {
#if defined(ARCH_OS_IOS) || \
 (defined(__MAC_10_15) && __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_10_15)
            MTLCaptureDescriptor *desc = [[MTLCaptureDescriptor alloc] init];
            desc.captureObject = _captureScopeFullFrame;
            desc.destination = MTLCaptureDestinationDeveloperTools;
//            [[MTLCaptureManager sharedCaptureManager]
//                startCaptureWithDescriptor:desc error:nil];
#endif
        }

        [_captureScopeFullFrame beginScope];

        if ([[MTLCaptureManager sharedCaptureManager] isCapturing]) {
            // We need to grab a new command buffer otherwise the previous one
            // (if it was allocated at the end of the last frame) won't appear in
            // this frame's capture, and it will confuse us!
            CommitCommandBuffer(CommitCommandBuffer_NoWait, true);
        }
    }
}

void
HgiMetal::EndFrame()
{
    if (--_frameDepth == 0) {
        [_captureScopeFullFrame endScope];
//        [[MTLCaptureManager sharedCaptureManager] stopCapture];
    }
}

void
HgiMetal::CommitCommandBuffer(CommitCommandBufferWaitType waitType,
                              bool forceNewBuffer)
{
    if (!_workToFlush && !forceNewBuffer) {
        return;
    }

    [_commandBuffer commit];
    if (waitType == CommitCommandBuffer_WaitUntilScheduled) {
        [_commandBuffer waitUntilScheduled];
    }
    else if (waitType == CommitCommandBuffer_WaitUntilCompleted) {
        [_commandBuffer waitUntilCompleted];
    }
    [_commandBuffer release];

    _commandBuffer = [_commandQueue commandBuffer];
    [_commandBuffer retain];
    
    _workToFlush = false;
    _encoder = nil;
}

bool
HgiMetal::BeginMtlf()
{
    // SOOOO TEMP and specialised!
    if (_encoder) {
        _encoder->Commit();
        return true;
    }
    
    return false;
}

PXR_NAMESPACE_CLOSE_SCOPE
