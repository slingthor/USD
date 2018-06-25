#line 1 "/Volumes/Data/USDMetal/pxr/imaging/lib/mtlf/mtlDevice.h"
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
#ifndef MTLF_METALCONTEXT_H
#define MTLF_METALCONTEXT_H

#include "pxr/imaging/glf/glew.h"

#include <Metal/Metal.h>
#import <Cocoa/Cocoa.h>

#include "pxr/pxr.h"
#include "pxr/base/tf/token.h"
#include "pxr/imaging/mtlf/api.h"
#include "pxr/base/arch/threads.h"
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>

#include <map>

PXR_NAMESPACE_OPEN_SCOPE

class MtlfDrawTarget;
typedef boost::shared_ptr<class MtlfMetalContext> MtlfMetalContextSharedPtr;

/// \class MtlfMetalContext
///
/// Provides window system independent access to Metal devices.
///

enum MSL_ProgramStage
{
    kMSL_ProgramStage_Vertex   = (1 << 0),
    kMSL_ProgramStage_Fragment = (1 << 1),
    kMSL_ProgramStage_Compute  = (1 << 2),
};

class MtlfMetalContext : public boost::noncopyable {
public:
    typedef struct {
        float position[2];
        float uv[2];
    } Vertex;

    MTLF_API
    virtual ~MtlfMetalContext();

    /// Returns an instance for the current Metal device.
    MTLF_API
    static MtlfMetalContextSharedPtr GetMetalContext();

    /// Returns whether this interface has been initialized.
    MTLF_API
    static bool IsInitialized();
    
    MTLF_API
    id<MTLBuffer> GetIndexBuffer() {
        return indexBuffer;
    }
    
    MTLF_API
    id<MTLCommandBuffer> CreateCommandBuffer();
    
    MTLF_API
    id<MTLRenderCommandEncoder> CreateRenderEncoder(MTLRenderPassDescriptor *renderPassDescriptor);
    
    MTLF_API
    void SetDrawTarget(MtlfDrawTarget *drawTarget);
    
    MTLF_API
    void SetShadingPrograms(id<MTLFunction> vertexFunction, id<MTLFunction> fragmentFunction);
    
    MTLF_API
    void SetVertexAttribute(uint32_t index,
                            int size,
                            int type,
                            size_t stride,
                            uint32_t offset,
                            const TfToken& name);
    
    MTLF_API
    void SetUniform(const void* _data, uint32 _dataSize, const TfToken& _name, uint32 index, MSL_ProgramStage stage);
    
    MTLF_API
    void SetUniformBuffer(int index, id<MTLBuffer> buffer, const TfToken& name, MSL_ProgramStage stage, int offset = 0, bool oldStyleBacker = false);
    
    MTLF_API
    void SetBuffer(int index, id<MTLBuffer> buffer, const TfToken& name);	//Implementation binds this as a vertex buffer!
    
    MTLF_API
    void SetIndexBuffer(id<MTLBuffer> buffer);

    MTLF_API
    void SetTexture(int index, id<MTLTexture> texture, const TfToken& name, MSL_ProgramStage stage);
    
    MTLF_API
    void SetSampler(int index, id<MTLSamplerState> sampler, const TfToken& name, MSL_ProgramStage stage);

    MTLF_API
    id<MTLRenderPipelineState> GetPipelineState(MTLRenderPipelineDescriptor *pipelineStateDescriptor);

    MTLF_API
    void BakeState();

    MTLF_API
    void ClearState();

    id<MTLDevice> device;
    id<MTLCommandQueue> commandQueue;
    id<MTLCommandBuffer> commandBuffer;
    id<MTLRenderCommandEncoder> renderEncoder;

    id<MTLLibrary> defaultLibrary;
    id<MTLRenderPipelineState> pipelineState;
    id<MTLDepthStencilState> depthState;
    id<MTLTexture> mtlTexture;

    uint32_t glShaderProgram;
    uint32_t glTexture;
    uint32_t glVAO;
    uint32_t glVBO;

protected:
    MTLF_API
    MtlfMetalContext();

    MTLF_API
    void CheckNewStateGather();

    MTLRenderPipelineDescriptor *pipelineStateDescriptor;
    MTLVertexDescriptor *vertexDescriptor;
    uint32_t numVertexComponents;
    uint32_t numColourAttachments;

    struct OldStyleUniformData
    {
        void alloc(const void* _data, uint32 _size)
        {
            dataSize = _size;
            data = malloc(dataSize);
            memcpy(data, _data, dataSize);
        }
        void release()
        {
            free(data);
            data = 0;
        }
        
        uint32           index;
        void*            data;
        uint32           dataSize;
        TfToken          name;
        MSL_ProgramStage stage;
    };
    std::vector<OldStyleUniformData> oldStyleUniforms;
    id<MTLBuffer> vtxUniformBackingBuffer;
    id<MTLBuffer> fragUniformBackingBuffer;

    struct VertexBufferBinding { int idx; id<MTLBuffer> buffer; TfToken name; };
    std::vector<VertexBufferBinding> vertexBuffers;
    struct UniformBufferBinding { int idx; id<MTLBuffer> buffer; TfToken name; MSL_ProgramStage stage; int offset; };
    std::vector<UniformBufferBinding> uniformBuffers;
	struct TextureBinding { int idx; id<MTLTexture> texture; TfToken name; MSL_ProgramStage stage; };
    std::vector<TextureBinding> textures;
	struct SamplerBinding { int idx; id<MTLSamplerState> sampler; TfToken name; MSL_ProgramStage stage; };
    std::vector<SamplerBinding> samplers;
    id<MTLBuffer> indexBuffer;
    
    MtlfDrawTarget *drawTarget;

private:
    enum PREFERRED_GPU_TYPE {
        PREFER_DEFAULT_GPU,
        PREFER_INTEGRATED_GPU,
        PREFER_DISCRETE_GPU,
        PREFER_EGPU,
        PREFER_DISPLAY_GPU,
    };
    
    id<MTLDevice> GetMetalDevice(PREFERRED_GPU_TYPE preferredGPUType);
    void handleDisplayChange();
    void handleGPUHotPlug(id<MTLDevice> device, MTLDeviceNotificationName notifier);
    
    static MtlfMetalContextSharedPtr context;
    
    CVOpenGLTextureCacheRef cvglTextureCache = nil;
    CVMetalTextureCacheRef cvmtlTextureCache = nil;
    
    id<MTLRenderPipelineState> currentPipelineState;
    boost::unordered_map<size_t, id<MTLRenderPipelineState>> pipelineStateMap;
    uint32 dirtyState;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif  // MTLF_METALCONTEXT_H
