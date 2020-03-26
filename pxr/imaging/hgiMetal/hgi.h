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
#ifndef HGIMETAL_HGIMETAL_H
#define HGIMETAL_HGIMETAL_H

#include "pxr/pxr.h"
#include "pxr/imaging/hgiMetal/api.h"
#include "pxr/imaging/hgiMetal/immediateCommandBuffer.h"
#include "pxr/imaging/hgi/hgi.h"
#include "pxr/imaging/hgi/tokens.h"

#import <Metal/Metal.h>

PXR_NAMESPACE_OPEN_SCOPE

class HgiMetalCapabilities;

enum {
    APIVersion_Metal1_0 = 0,
    APIVersion_Metal2_0,
    APIVersion_Metal3_0
};

/// \class HgiMetal
///
/// Metal implementation of the Hydra Graphics Interface.
///
class HgiMetal final : public Hgi
{
public:
    HGIMETAL_API
    HgiMetal(id<MTLDevice> device = nil);

    HGIMETAL_API
    ~HgiMetal();

    //
    // Command Buffers
    //

    HGIMETAL_API
    HgiImmediateCommandBuffer& GetImmediateCommandBuffer() override;

    //
    // Resources
    //

    HGIMETAL_API
    HgiTextureHandle CreateTexture(HgiTextureDesc const & desc) override;

    HGIMETAL_API
    void DestroyTexture(HgiTextureHandle* texHandle) override;

    HGIMETAL_API
    HgiBufferHandle CreateBuffer(HgiBufferDesc const & desc) override;

    HGIMETAL_API
    void DestroyBuffer(HgiBufferHandle* texHandle) override;

    HGIMETAL_API
    HgiShaderFunctionHandle CreateShaderFunction(
        HgiShaderFunctionDesc const& desc) override;

    HGIMETAL_API
    void DestroyShaderFunction(
        HgiShaderFunctionHandle* shaderFunctionHandle) override;

    HGIMETAL_API
    HgiShaderProgramHandle CreateShaderProgram(
        HgiShaderProgramDesc const& desc) override;

    HGIMETAL_API
    void DestroyShaderProgram(
        HgiShaderProgramHandle* shaderProgramHandle) override;

    HGIMETAL_API
    HgiResourceBindingsHandle CreateResourceBindings(
        HgiResourceBindingsDesc const& desc) override;

    HGIMETAL_API
    void DestroyResourceBindings(HgiResourceBindingsHandle* resHandle) override;

    HGIMETAL_API
    HgiPipelineHandle CreatePipeline(
        HgiPipelineDesc const& pipeDesc) override;

    HGIMETAL_API
    void DestroyPipeline(HgiPipelineHandle* pipeHandle) override;

    HGIMETAL_API
    TfToken const& GetAPIName() const override;
    
    HGIMETAL_API
    void StartFrame() override;

    HGIMETAL_API
    void EndFrame() override;

    //
    // HgiMetal specific
    //
    
    HGIMETAL_API
    id<MTLDevice> GetDevice() const {
        return _device;
    }
    
    HGIMETAL_API
    id<MTLCommandQueue> GetQueue() const {
        return _commandQueue;
    }
    
    HGIMETAL_API
    int GetAPIVersion() const {
        return _apiVersion;
    }
    
    HGIMETAL_API
    bool GetConcurrentDispatch() const {
        return _concurrentDispatchSupported;
    }
    
    HGIMETAL_API
    void SetNeedsInterop(bool useInterop) {
        _useInterop = useInterop;
    }
    
    HGIMETAL_API
    bool GetNeedsInterop() const {
        return _useInterop;
    }
    
    HGIMETAL_API
    HgiMetalCapabilities const & GetCapabilities() const {
        return *_capabilities;
    }
    
private:
    HgiMetal & operator=(const HgiMetal&) = delete;
    HgiMetal(const HgiMetal&) = delete;

    id<MTLDevice> _device;
    id<MTLCommandQueue> _commandQueue;
    id<MTLCaptureScope> _captureScopeFullFrame;
    int _frameDepth;

    int _apiVersion;
    bool _concurrentDispatchSupported;
    bool _useInterop;

    std::unique_ptr<HgiMetalImmediateCommandBuffer> _immediateCommandBuffer;
    std::unique_ptr<HgiMetalCapabilities> _capabilities;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif
