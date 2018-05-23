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

#include "pxr/pxr.h"
#include "pxr/imaging/mtlf/api.h"
#include "pxr/base/arch/threads.h"
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>

#include <map>

PXR_NAMESPACE_OPEN_SCOPE


typedef boost::shared_ptr<class MtlfMetalContext> MtlfMetalContextSharedPtr;

/// \class MtlfMetalContext
///
/// Provides window system independent access to Metal devices.
///
class MtlfMetalContext : public boost::noncopyable {
public:
    MTLF_API
    virtual ~MtlfMetalContext();

    /// Returns an instance for the current Metal device.
    MTLF_API
    static MtlfMetalContextSharedPtr GetMetalContext();

    /// Returns whether this interface has been initialized.
    MTLF_API
    static bool IsInitialized();
    
    void SetShadingPrograms(id<MTLFunction> vertexFunction, id<MTLFunction> fragmentFunction);
    void SetVertexAttribute(uint32_t index,
                            int size,
                            int type,
                            size_t stride,
                            uint32_t offset);
    void SetBuffer(int index, id<MTLBuffer> buffer);
    void SetIndexBuffer(id<MTLBuffer> buffer);

    void SetTexture(int index, id<MTLTexture> texture);
    void SetSampler(int index, id<MTLSamplerState> sampler);

    void BakeState();

    id<MTLDevice> device;
    id<MTLCommandQueue> commandQueue;
    id<MTLCommandBuffer> commandBuffer;

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
    id<MTLRenderCommandEncoder> renderEncoder;
    
    std::map<int, id<MTLBuffer>> vertexBuffers;
    std::map<int, id<MTLTexture>> textures;
    std::map<int, id<MTLSamplerState>> samplers;
    id<MTLBuffer> indexBuffer;

private:
    static MtlfMetalContextSharedPtr context;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif  // MTLF_METALCONTEXT_H
