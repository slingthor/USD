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
#include "pxr/imaging/hgiMetal/hgi.h"
#include "pxr/imaging/hgiMetal/buffer.h"
#include "pxr/imaging/hgiMetal/conversions.h"
#include "pxr/imaging/hgiMetal/diagnostic.h"
#include "pxr/imaging/hgiMetal/pipeline.h"
#include "pxr/imaging/hgiMetal/resourceBindings.h"
#include "pxr/imaging/hgiMetal/shaderFunction.h"
#include "pxr/imaging/hgiMetal/shaderProgram.h"
#include "pxr/imaging/hgiMetal/texture.h"

#include "pxr/base/tf/getenv.h"
#include "pxr/base/tf/registryManager.h"
#include "pxr/base/tf/type.h"

#import <Cocoa/Cocoa.h>

PXR_NAMESPACE_OPEN_SCOPE

TF_REGISTRY_FUNCTION(TfType)
{
    TfType t = TfType::Define<HgiMetal, TfType::Bases<Hgi> >();
    t.SetFactory<HgiFactory<HgiMetal>>();
}

static int _GetAPIVersion()
{
    static NSOperatingSystemVersion minimumSupportedOSVersion13_0 = { .majorVersion = 10, .minorVersion = 13, .patchVersion = 0 };
    static NSOperatingSystemVersion minimumSupportedOSVersion14_0 = { .majorVersion = 10, .minorVersion = 14, .patchVersion = 0 };
    static NSOperatingSystemVersion minimumSupportedOSVersion15_0 = { .majorVersion = 10, .minorVersion = 15, .patchVersion = 0 };
    static bool sysVerGreaterOrEqualTo13_0 = [NSProcessInfo.processInfo isOperatingSystemAtLeastVersion:minimumSupportedOSVersion13_0];
    static bool sysVerGreaterOrEqualTo14_0 = [NSProcessInfo.processInfo isOperatingSystemAtLeastVersion:minimumSupportedOSVersion14_0];
    static bool sysVerGreaterOrEqualTo15_0 = [NSProcessInfo.processInfo isOperatingSystemAtLeastVersion:minimumSupportedOSVersion15_0];

    if (sysVerGreaterOrEqualTo15_0) {
        return APIVersion_Metal3_0;
    }
    else if (sysVerGreaterOrEqualTo13_0) {
        return APIVersion_Metal2_0;
    }
    
    return APIVersion_Metal1_0;
}

HgiMetal::HgiMetal(id<MTLDevice> device)
: _device(device)
, _apiVersion(_GetAPIVersion())
{
    if (!_device) {
        if( TfGetenvBool("USD_METAL_USE_INTEGRATED_GPU", false)) {
            _device = MTLCopyAllDevices()[1];
        }

        if (!_device) {
            _device = MTLCreateSystemDefaultDevice();
        }
    }
    
    static int const commandBufferPoolSize = 256;
    _commandQueue = [_device newCommandQueueWithMaxCommandBufferCount:
                     commandBufferPoolSize];

    _immediateCommandBuffer.reset(
        new HgiMetalImmediateCommandBuffer(_device, _commandQueue));

    HgiMetalSetupMetalDebug();
    
    _captureScopeFullFrame =
        [[MTLCaptureManager sharedCaptureManager]
            newCaptureScopeWithDevice:_device];
    _captureScopeFullFrame.label =
        [NSString stringWithFormat:@"Full Hydra Frame"];
    
    [[MTLCaptureManager sharedCaptureManager]
        setDefaultCaptureScope:_captureScopeFullFrame];
    
    static NSOperatingSystemVersion minimumSupportedOSVersion = { .majorVersion = 10, .minorVersion = 14, .patchVersion = 5 };
    static bool sysVerGreaterOrEqualTo10_14_5 = [NSProcessInfo.processInfo isOperatingSystemAtLeastVersion:minimumSupportedOSVersion];
    
    _concurrentDispatchSupported = sysVerGreaterOrEqualTo10_14_5;

}

HgiMetal::~HgiMetal()
{
    [_captureScopeFullFrame release];
    [_commandQueue release];
    [_device release];
}

HgiImmediateCommandBuffer&
HgiMetal::GetImmediateCommandBuffer()
{
    return *_immediateCommandBuffer;
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

void
HgiMetal::StartFrame()
{
    [_captureScopeFullFrame beginScope];
    
    _immediateCommandBuffer->StartFrame();
}

void
HgiMetal::EndFrame()
{
    [_captureScopeFullFrame endScope];
}

PXR_NAMESPACE_CLOSE_SCOPE
