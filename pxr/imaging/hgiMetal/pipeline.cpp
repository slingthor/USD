//
// Copyright 2020 Pixar
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
#include <vector>

#include "pxr/base/tf/diagnostic.h"

#include "pxr/imaging/hgiMetal/hgi.h"
#include "pxr/imaging/hgiMetal/conversions.h"
#include "pxr/imaging/hgiMetal/diagnostic.h"
#include "pxr/imaging/hgiMetal/pipeline.h"
#include "pxr/imaging/hgiMetal/resourceBindings.h"
#include "pxr/imaging/hgiMetal/shaderProgram.h"
#include "pxr/imaging/hgiMetal/shaderFunction.h"

PXR_NAMESPACE_OPEN_SCOPE

HgiMetalPipeline::HgiMetalPipeline(
    HgiMetal *hgi,
    HgiPipelineDesc const& desc)
    : HgiPipeline(desc)
    , _vertexDescriptor(nil)
    , _depthStencilState(nil)
    , _renderPipelineState(nil)
{
    _CreateVertexDescriptor();
    _CreateDepthStencilState(hgi->GetDevice());
    _CreateRenderPipelineState(hgi->GetDevice());
}

HgiMetalPipeline::~HgiMetalPipeline()
{
    if (_renderPipelineState) {
        [_renderPipelineState release];
    }
    if (_depthStencilState) {
        [_depthStencilState release];
    }
    if (_vertexDescriptor) {
        [_vertexDescriptor release];
    }
}

void
HgiMetalPipeline::_CreateVertexDescriptor()
{
    _vertexDescriptor = [[MTLVertexDescriptor alloc] init];

    int index = 0;
    for (HgiVertexBufferDesc const& vbo : _descriptor.vertexBuffers) {

        HgiVertexAttributeDescVector const& vas = vbo.vertexAttributes;
        
        _vertexDescriptor.layouts[index].stepFunction =
            MTLVertexStepFunctionPerVertex;
        _vertexDescriptor.layouts[index].stepRate = 1;
        _vertexDescriptor.layouts[index].stride = vbo.vertexStride;
        
        // Describe each vertex attribute in the vertex buffer
        for (size_t loc = 0; loc<vas.size(); loc++) {
            HgiVertexAttributeDesc const& va = vas[loc];

            uint32_t idx = va.shaderBindLocation;
            _vertexDescriptor.attributes[idx].format =
                HgiMetalConversions::GetVertexFormat(va.format);
            _vertexDescriptor.attributes[idx].bufferIndex = vbo.bindingIndex;
            _vertexDescriptor.attributes[idx].offset = va.offset;
        }
        index++;
    }
}

void
HgiMetalPipeline::_CreateRenderPipelineState(id<MTLDevice> device)
{
    MTLRenderPipelineDescriptor *renderPipelineStateDescriptor =
        [[MTLRenderPipelineDescriptor alloc] init];

    // Create a new render pipeline state object
    renderPipelineStateDescriptor.label = @(_descriptor.debugName.c_str());
    renderPipelineStateDescriptor.rasterSampleCount = 1;
    
    renderPipelineStateDescriptor.inputPrimitiveTopology =
        MTLPrimitiveTopologyClassUnspecified;
    
    renderPipelineStateDescriptor.rasterizationEnabled = NO;
    
    HgiMetalShaderProgram const *metalProgram =
        static_cast<HgiMetalShaderProgram*>(_descriptor.shaderProgram.Get());
    
    renderPipelineStateDescriptor.vertexFunction =
        metalProgram->GetVertexFunction();
    id<MTLFunction> fragmentFunction = metalProgram->GetFragmentFunction();
    if (fragmentFunction) {
        renderPipelineStateDescriptor.fragmentFunction = fragmentFunction;
        renderPipelineStateDescriptor.rasterizationEnabled = YES;
    }

    MTLPixelFormat depthFormat = MTLPixelFormatInvalid;
    renderPipelineStateDescriptor.depthAttachmentPixelFormat = depthFormat;

    renderPipelineStateDescriptor.colorAttachments[0].blendingEnabled = NO;
    
    if (_descriptor.multiSampleState.alphaToCoverageEnable) {
        renderPipelineStateDescriptor.alphaToCoverageEnabled = YES;
    }
    else {
        renderPipelineStateDescriptor.alphaToCoverageEnabled = NO;
    }

    MTLPixelFormat pixelFormat = MTLPixelFormatRGBA16Float;
    renderPipelineStateDescriptor.colorAttachments[0].pixelFormat = pixelFormat;

    renderPipelineStateDescriptor.vertexDescriptor = _vertexDescriptor;

    NSError *error = NULL;
    _renderPipelineState = [device
        newRenderPipelineStateWithDescriptor:renderPipelineStateDescriptor
        error:&error];
    [renderPipelineStateDescriptor release];
    
    if (!_renderPipelineState) {
        NSString *err = [error localizedDescription];
        TF_WARN("Failed to created pipeline state, error %s",
            [err UTF8String]);
        if (error) {
            [error release];
        }
        return;
    }
}

void
HgiMetalPipeline::_CreateDepthStencilState(id<MTLDevice> device)
{
    MTLDepthStencilDescriptor *depthStencilStateDescriptor =
        [[MTLDepthStencilDescriptor alloc] init];
    
    depthStencilStateDescriptor.label = @(_descriptor.debugName.c_str());

    if (_descriptor.depthState.depthWriteEnabled) {
        depthStencilStateDescriptor.depthWriteEnabled = YES;
    }
    else {
        depthStencilStateDescriptor.depthWriteEnabled = NO;
    }
    if (_descriptor.depthState.depthTestEnabled) {
        TF_CODING_ERROR("Missing implementation: set depth func");
    }
    else {
        depthStencilStateDescriptor.depthCompareFunction =
            MTLCompareFunctionNever;
    }
    
    if (_descriptor.depthState.stencilTestEnabled) {
        TF_CODING_ERROR("Missing implementation stencil mask enabled");
    } else {
        depthStencilStateDescriptor.backFaceStencil = nil;
        depthStencilStateDescriptor.frontFaceStencil = nil;
    }
    
    _depthStencilState = [device
        newDepthStencilStateWithDescriptor:depthStencilStateDescriptor];
    [depthStencilStateDescriptor release];

    TF_VERIFY(_depthStencilState,
        "Failed to created depth stencil state");
}

void
HgiMetalPipeline::BindPipeline(id<MTLRenderCommandEncoder> renderEncoder)
{
    [renderEncoder setRenderPipelineState:_renderPipelineState];

    //
    // Rasterization state
    //
    [renderEncoder setCullMode:HgiMetalConversions::GetCullMode(
        _descriptor.rasterizationState.cullMode)];
    [renderEncoder setTriangleFillMode:HgiMetalConversions::GetPolygonMode(
        _descriptor.rasterizationState.polygonMode)];
    [renderEncoder setFrontFacingWinding:HgiMetalConversions::GetWinding(
        _descriptor.rasterizationState.winding)];
    [renderEncoder setDepthStencilState:_depthStencilState];

    TF_VERIFY(_descriptor.rasterizationState.lineWidth == 1.0f,
        "Missing implementation buffers");
}

PXR_NAMESPACE_CLOSE_SCOPE
