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

#include "pxr/imaging/mtlf/mtlDevice.h"
#include "pxr/imaging/mtlf/drawTarget.h"
#include "pxr/base/tf/getenv.h"

#include "pxr/imaging/hgiMetal/hgi.h"
#include "pxr/imaging/hgiMetal/capabilities.h"
#include "pxr/imaging/hgiMetal/graphicsCmds.h"
#include "pxr/imaging/hgi/texture.h"

#import <simd/simd.h>
#include <sys/time.h>

#define METAL_TESSELLATION_SUPPORT 0

enum {
    DIRTY_METALRENDERSTATE_OLD_STYLE_VERTEX_UNIFORM   = 1 << 0,
    DIRTY_METALRENDERSTATE_OLD_STYLE_FRAGMENT_UNIFORM = 1 << 1,
    DIRTY_METALRENDERSTATE_INDEX_BUFFER               = 1 << 2,
    DIRTY_METALRENDERSTATE_VERTEX_BUFFER              = 1 << 3,
    DIRTY_METALRENDERSTATE_SAMPLER                    = 1 << 4,
    DIRTY_METALRENDERSTATE_TEXTURE                    = 1 << 5,
    DIRTY_METALRENDERSTATE_DRAW_TARGET                = 1 << 6,
    DIRTY_METALRENDERSTATE_VERTEX_DESCRIPTOR          = 1 << 7,
    DIRTY_METALRENDERSTATE_CULLMODE_WINDINGORDER      = 1 << 8,
    DIRTY_METALRENDERSTATE_FILL_MODE                  = 1 << 9,

    DIRTY_METALRENDERSTATE_ALL                      = 0xFFFFFFFF
};

#define METAL_COMPUTEGS_ALLOW_ASYNCHRONOUS_COMPUTE        1

PXR_NAMESPACE_OPEN_SCOPE

MtlfMetalContextSharedPtr MtlfMetalContext::context = NULL;
std::mutex MtlfMetalContext::_commandBufferPoolMutex;
std::mutex MtlfMetalContext::_pipelineMutex;
std::mutex MtlfMetalContext::_bufferMutex;
thread_local MtlfMetalContext::ThreadState MtlfMetalContext::threadState;

void MtlfMetalContext::ThreadState::PrepareThread(MtlfMetalContext *_this) {
    if (!init)
    {
        gsDataOffset = 0;
        gsBufferIndex = 0;
        gsEncodedBatches = 0;
        gsCurrentBuffer = nil;
        gsHasOpenBatch = false;
        enableMVA = false;
        enableComputeGS = false;

        size_t const defaultBufferSize = 1024;
        
        for(int i = 0; i < kMSL_ProgramStage_NumStages; i++) {
            oldStyleUniformBufferSize[i] = 0;
            oldStyleUniformBufferAllocatedSize[i] = defaultBufferSize;
            oldStyleUniformBuffer[i] = new uint8_t[defaultBufferSize];
            memset(oldStyleUniformBuffer[i], 0x00, defaultBufferSize);
        }
    
        vertexDescriptor = nil;
        indexBuffer = nil;
        vertexPositionBuffer = nil;
        
        numVertexComponents = 0;
        
        currentWorkQueueType = METALWORKQUEUE_DEFAULT;
        currentWorkQueue     = &workQueueDefault;
        
        workQueueDefault.lastWaitEventValue                   = 0;
        workQueueGeometry.lastWaitEventValue                  = 0;

        currentEventValue                    = 1;
        highestExpectedEventValue            = 0;

        _this->ResetEncoders(METALWORKQUEUE_DEFAULT, true);
        _this->ResetEncoders(METALWORKQUEUE_GEOMETRY_SHADER, true);
        
        MTLResourceOptions resourceOptions = MTLResourceStorageModePrivate|MTLResourceCPUCacheModeDefaultCache;
        for(int i = 0; i < _this->gsMaxConcurrentBatches; i++)
            gsBuffers.push_back([_this->currentDevice newBufferWithLength:_this->gsMaxDataPerBatch options:resourceOptions]);
        remappedQuadIndexBuffer = nil;
        pointIndexBuffer = nil;

        init = true;
    }
    
}

MtlfMetalContext::ThreadState::~ThreadState() {
    for(int i = 0; i < kMSL_ProgramStage_NumStages; i++) {
        delete[] oldStyleUniformBuffer[i];
    }
//    for(int i = 0; i < gsBuffers.size(); i++)
//        [gsBuffers.at(i) release];
    gsBuffers.clear();
}

#if defined(ARCH_OS_MACOS)
// Called when the window is dragged to another display
void MtlfMetalContext::handleDisplayChange()
{
    NSLog(@"Detected display change - but not doing about it");
}

// Called when an eGPU is either removed or added
void MtlfMetalContext::handleGPUHotPlug(id<MTLDevice> device, MTLDeviceNotificationName notifier)
{
    // Device plugged in
    if (notifier == MTLDeviceWasAddedNotification) {
        NSLog(@"New Device was added");
    }
    // Device Removal Requested. Cleanup and switch to preferred device
    else if (notifier == MTLDeviceRemovalRequestedNotification) {
        NSLog(@"Device removal request was notified");
    }
    // additional handling of surprise removal
    else if (notifier == MTLDeviceWasRemovedNotification) {
        NSLog(@"Device was removed");
    }
}
#endif // ARCH_OS_MACOS

id<MTLDevice> MtlfMetalContext::GetMetalDevice(PREFERRED_GPU_TYPE preferredGPUType)
{
#if defined(ARCH_OS_MACOS)
    // Get a list of all devices and register an obsever for eGPU events
    id <NSObject> metalDeviceObserver = nil;
    
    // Get a list of all devices and register an obsever for eGPU events
    NSArray<id<MTLDevice>> *allDevices = MTLCopyAllDevicesWithObserver(&metalDeviceObserver,
                                                ^(id<MTLDevice> device, MTLDeviceNotificationName name) {
                                                    MtlfMetalContext::handleGPUHotPlug(device, name);
                                                });

    NSMutableArray<id<MTLDevice>> *_eGPUs          = [NSMutableArray array];
    NSMutableArray<id<MTLDevice>> *_integratedGPUs = [NSMutableArray array];
    NSMutableArray<id<MTLDevice>> *_discreteGPUs   = [NSMutableArray array];
    id<MTLDevice>                  _defaultDevice  = MTLCreateSystemDefaultDevice();
    NSArray *preferredDeviceList = _discreteGPUs;
    
    bool const multiGPUSuportEnabled = false && preferredGPUType == PREFER_DEFAULT_GPU;
    
    // Put the device into the appropriate device list
    for (id<MTLDevice>dev in allDevices) {
        bool multiDeviceRenderOption = false;
        if (dev.removable) {
        	[_eGPUs addObject:dev];
        }
        else if (dev.lowPower) {
        	[_integratedGPUs addObject:dev];
        }
        else {
        	[_discreteGPUs addObject:dev];
            multiDeviceRenderOption = multiGPUSuportEnabled &&
                [dev peerGroupID] != 0 &&
                [dev peerGroupID] == [_defaultDevice peerGroupID];
        }
    }

    switch (preferredGPUType) {
        case PREFER_DISPLAY_GPU:
            NSLog(@"Display device selection not supported yet, returning default GPU");
        case PREFER_DEFAULT_GPU:
            return _defaultDevice;
        case PREFER_EGPU:
            preferredDeviceList = _eGPUs;
            break;
       case PREFER_DISCRETE_GPU:
            preferredDeviceList = _discreteGPUs;
            break;
        case PREFER_INTEGRATED_GPU:
            preferredDeviceList = _integratedGPUs;
            break;
    }
    // If no device matching the requested one was found then get the default device
    if (preferredDeviceList.count != 0) {
        return preferredDeviceList.firstObject;
    }
    else {
        NSLog(@"Preferred device not found, returning default GPU");
        return _defaultDevice;
    }
#else
    return MTLCreateSystemDefaultDevice();
#endif
}

//
// MtlfMetalContext
//

MtlfMetalContext::MtlfMetalContext(HgiMetal *hgi_)
: hgi(hgi_)
{
    Init();
}

MtlfMetalContext::~MtlfMetalContext()
{
    Cleanup();
}

void MtlfMetalContext::Init()
{
    gsMaxConcurrentBatches = 0;
    gsMaxDataPerBatch = 0;
    points = TfToken("points");

    currentDevice = hgi->GetPrimaryDevice();

#if defined(METAL_ENABLE_STATS)
    NSLog(@"Selected %@ for Metal Device", currentDevice.name);
#endif

    gpus.commandQueue = hgi->GetQueue();
    [gpus.commandQueue retain];

#if defined(ARCH_OS_IOS)
    gsMaxDataPerBatch = 1024 * 1024 * 32;
    gsMaxConcurrentBatches = 2;
#else
    gsMaxDataPerBatch = 1024 * 1024 * 32;
    gsMaxConcurrentBatches = 4;
#endif

    workQueueResource.lastWaitEventValue                  = 0;

    triIndexBuffer = nil;

    ResetEncoders(METALWORKQUEUE_RESOURCE, true);

    memset(commandBuffers, 0x00, sizeof(commandBuffers));
    commandBuffersStackPos = 0;

    concurrentDispatchSupported = hgi->GetCapabilities().concurrentDispatchSupported;

    MTLTextureDescriptor* blackDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA16Float
                                                                                    width:1
                                                                                   height:1
                                                                                mipmapped:NO];
    blackDesc.usage = MTLTextureUsageShaderRead;
    blackDesc.resourceOptions = MTLResourceStorageModeDefault;
    blackDesc.arrayLength = 1;
    
    uint16_t zero[4] = {};

    blackDesc.textureType = MTLTextureType2D;
    gpus.blackTexture2D = [currentDevice newTextureWithDescriptor:blackDesc];
    
    blackDesc.textureType = MTLTextureType2DArray;
    gpus.blackTexture2DArray = [currentDevice newTextureWithDescriptor:blackDesc];
    [gpus.blackTexture2D replaceRegion:MTLRegionMake2D(0, 0, 1, 1)
                           mipmapLevel:0
                             withBytes:&zero
                           bytesPerRow:sizeof(zero)];
    
    [gpus.blackTexture2DArray replaceRegion:MTLRegionMake2D(0, 0, 1, 1)
                                mipmapLevel:0
                                      slice:0
                                  withBytes:&zero
                                bytesPerRow:sizeof(zero)
                              bytesPerImage:0];

    MTLSamplerDescriptor* samplerDescriptor = [[MTLSamplerDescriptor alloc] init];
    gpus.dummySampler = [currentDevice newSamplerStateWithDescriptor:samplerDescriptor];
    [samplerDescriptor release];

    windingOrder = MTLWindingCounterClockwise;
    cullMode = MTLCullModeNone;
    fillMode = MTLTriangleFillModeFill;
    
    blendState.blendEnable = false;
    blendState.alphaCoverageEnable = false;
    blendState.alphaToOneEnable = false;
    blendState.rgbBlendOp = MTLBlendOperationAdd;
    blendState.alphaBlendOp = MTLBlendOperationAdd;
    blendState.sourceColorFactor = MTLBlendFactorSourceAlpha;
    blendState.destColorFactor = MTLBlendFactorOneMinusSourceAlpha;
    blendState.sourceAlphaFactor = MTLBlendFactorSourceAlpha;
    blendState.destAlphaFactor = MTLBlendFactorOneMinusSourceAlpha;
    blendState.blendColor = GfVec4f(1.0f);
    blendState.writeMask = MTLColorWriteMaskAll;
    
    depthState.depthWriteEnable = true;
    depthState.depthCompareFunction = MTLCompareFunctionLessEqual;
    
    outputPixelFormat = MTLPixelFormatInvalid;
    outputDepthFormat = MTLPixelFormatInvalid;
    
#if defined(METAL_ENABLE_STATS)
    resourceStats.commandBuffersCreated.store(0, std::memory_order_relaxed);
    resourceStats.commandBuffersCommitted.store(0, std::memory_order_relaxed);
    resourceStats.buffersCreated.store(0, std::memory_order_relaxed);
    resourceStats.buffersReused.store(0, std::memory_order_relaxed);
    resourceStats.bufferSearches.store(0, std::memory_order_relaxed);
    resourceStats.renderEncodersCreated.store(0, std::memory_order_relaxed);
    resourceStats.computeEncodersCreated.store(0, std::memory_order_relaxed);
    resourceStats.blitEncodersCreated.store(0, std::memory_order_relaxed);
    resourceStats.renderEncodersRequested.store(0, std::memory_order_relaxed);
    resourceStats.computeEncodersRequested.store(0, std::memory_order_relaxed);
    resourceStats.blitEncodersRequested.store(0, std::memory_order_relaxed);
    resourceStats.renderPipelineStates.store(0, std::memory_order_relaxed);
    resourceStats.depthStencilStates.store(0, std::memory_order_relaxed);
    resourceStats.computePipelineStates.store(0, std::memory_order_relaxed);
    resourceStats.peakBufferAllocation.store(0, std::memory_order_relaxed);
    resourceStats.GSBatchesStarted.store(0, std::memory_order_relaxed);
#endif
    currentBufferAllocation.store(0, std::memory_order_relaxed);
    
    frameCount = 0;
    lastCompletedFrame = -1;
    lastCompletedCommandBuffer = -1;
    committedCommandBufferCount.store(0, std::memory_order_relaxed);
}

void MtlfMetalContext::Cleanup()
{
    CleanupUnusedBuffers(true);
    bufferFreeList.clear();

    [gpus.blackTexture2D release];
    [gpus.blackTexture2DArray release];
    [gpus.dummySampler release];
    [gpus.commandQueue release];

#if defined(METAL_ENABLE_STATS)
    if(frameCount > 0) {
        NSLog(@"--- METAL Resource Stats (average per frame / total) ----");
        NSLog(@"Frame count:                %7lld", frameCount);
        NSLog(@"Command Buffers created:    %7llu / %7lu",
              resourceStats.commandBuffersCreated.load(std::memory_order_relaxed) / frameCount,
              resourceStats.commandBuffersCreated.load(std::memory_order_relaxed));
        NSLog(@"Command Buffers committed:  %7llu / %7lu",
              resourceStats.commandBuffersCommitted.load(std::memory_order_relaxed) / frameCount,
              resourceStats.commandBuffersCommitted.load(std::memory_order_relaxed));
        NSLog(@"Metal   Buffers created:    %7llu / %7lu",
              resourceStats.buffersCreated.load(std::memory_order_relaxed) / frameCount,
              resourceStats.buffersCreated.load(std::memory_order_relaxed));
        NSLog(@"Metal   Buffers reused:     %7llu / %7lu",
              resourceStats.buffersReused.load(std::memory_order_relaxed) / frameCount,
              resourceStats.buffersReused.load(std::memory_order_relaxed));
        int32_t buffersCreated = resourceStats.buffersCreated.load(std::memory_order_relaxed);
        int32_t buffersReused = resourceStats.buffersReused.load(std::memory_order_relaxed);
        if (buffersCreated + buffersReused) {
            NSLog(@"Metal   Av buf search depth:%7lu"       ,
                  resourceStats.bufferSearches.load(std::memory_order_relaxed) /
                  (buffersCreated + buffersReused));
        }
        NSLog(@"Render  Encoders requested: %7llu / %7lu",
              resourceStats.renderEncodersRequested.load(std::memory_order_relaxed) / frameCount,
              resourceStats.renderEncodersRequested.load(std::memory_order_relaxed));
        NSLog(@"Render  Encoders created:   %7llu / %7lu",
              resourceStats.renderEncodersCreated.load(std::memory_order_relaxed) / frameCount,
              resourceStats.renderEncodersCreated.load(std::memory_order_relaxed));
        NSLog(@"Render  Pipeline States:    %7llu / %7lu",
              resourceStats.renderPipelineStates.load(std::memory_order_relaxed) / frameCount,
              resourceStats.renderPipelineStates.load(std::memory_order_relaxed));
        NSLog(@"Depth   Stencil  States:    %7llu / %7lu",
              resourceStats.depthStencilStates.load(std::memory_order_relaxed) / frameCount,
              resourceStats.depthStencilStates.load(std::memory_order_relaxed));
        NSLog(@"Compute Encoders requested: %7llu / %7lu",
              resourceStats.computeEncodersRequested.load(std::memory_order_relaxed) / frameCount,
              resourceStats.computeEncodersRequested.load(std::memory_order_relaxed));
        NSLog(@"Compute Encoders created:   %7llu / %7lu",
              resourceStats.computeEncodersCreated.load(std::memory_order_relaxed) / frameCount,
              resourceStats.computeEncodersCreated.load(std::memory_order_relaxed));
        NSLog(@"Compute Pipeline States:    %7llu / %7lu",
              resourceStats.computePipelineStates.load(std::memory_order_relaxed) / frameCount,
              resourceStats.computePipelineStates.load(std::memory_order_relaxed));
        NSLog(@"Blit    Encoders requested: %7llu / %7lu",
              resourceStats.blitEncodersRequested.load(std::memory_order_relaxed) / frameCount,
              resourceStats.blitEncodersRequested.load(std::memory_order_relaxed));
        NSLog(@"Blit    Encoders created:   %7llu / %7lu",
              resourceStats.blitEncodersCreated.load(std::memory_order_relaxed) / frameCount,
              resourceStats.blitEncodersCreated.load(std::memory_order_relaxed));
        NSLog(@"GS Batches started:         %7llu / %7lu",
              resourceStats.GSBatchesStarted.load(std::memory_order_relaxed) / frameCount,
              resourceStats.GSBatchesStarted.load(std::memory_order_relaxed));
        NSLog(@"Peak    Buffer Allocation:  %7luMbs",
              resourceStats.peakBufferAllocation.load(std::memory_order_relaxed) / (1024 * 1024));
    }
#endif
}

bool
MtlfMetalContext::IsInitialized()
{
    return true;
}

id<MTLBuffer>
MtlfMetalContext::GetQuadIndexBuffer(MTLIndexType indexTypeMetal) {
    // Each 4 vertices will require 6 remapped one
    uint32_t remappedIndexBufferSize = (threadState.indexBuffer.length / 4) * 6;

    // Since remapping is expensive check if the buffer we created this from originally has changed  - MTL_FIXME - these checks are not robust
    if (threadState.remappedQuadIndexBuffer) {
        if ((threadState.remappedQuadIndexBufferSource != threadState.indexBuffer) ||
            (threadState.remappedQuadIndexBuffer.length != remappedIndexBufferSize)) {
            [threadState.remappedQuadIndexBuffer release];
            threadState.remappedQuadIndexBuffer = nil;
        }
    }
    // Remap the quad indices into two sets of triangle indices
    if (!threadState.remappedQuadIndexBuffer) {
        if (indexTypeMetal != MTLIndexTypeUInt32) {
            TF_FATAL_CODING_ERROR("Only 32 bit indices currently supported for quads");
        }
        NSLog(@"Recreating quad remapped index buffer");
        
        threadState.remappedQuadIndexBufferSource = threadState.indexBuffer;
        threadState.remappedQuadIndexBuffer =
            [currentDevice newBufferWithLength:remappedIndexBufferSize options:MTLResourceStorageModeDefault];
        
        uint32_t *srcData =  (uint32_t *)threadState.indexBuffer.contents;
        uint32_t *destData = (uint32_t *)threadState.remappedQuadIndexBuffer.contents;
        for (int i = 0; i < (threadState.indexBuffer.length / 4) ; i+=4)
        {
            destData[0] = srcData[0];
            destData[1] = srcData[1];
            destData[2] = srcData[2];
            destData[3] = srcData[0];
            destData[4] = srcData[2];
            destData[5] = srcData[3];
            srcData  += 4;
            destData += 6;
        }
#if defined(ARCH_OS_MACOS)
        [threadState.remappedQuadIndexBuffer didModifyRange:(NSMakeRange(0, threadState.remappedQuadIndexBuffer.length))];
#endif
    }
    return threadState.remappedQuadIndexBuffer;
}

id<MTLBuffer>
MtlfMetalContext::GetTriListIndexBuffer(MTLIndexType indexTypeMetal, uint32_t numTriangles) {
    uint32_t numIndices = numTriangles * 3;
    uint32_t indexBufferSize = numIndices * sizeof(uint32_t);
    
    if (triIndexBuffer) {
        if ((triIndexBuffer.length < indexBufferSize)) {
            [triIndexBuffer release];
            triIndexBuffer = nil;
        }
    }
    // Remap the quad indices into two sets of triangle indices
    if (!triIndexBuffer) {
        if (indexTypeMetal != MTLIndexTypeUInt32) {
            TF_FATAL_CODING_ERROR("Only 32 bit indices currently supported");
        }
        NSLog(@"Recreating triangle list index buffer");
        
        triIndexBuffer = [currentDevice newBufferWithLength:indexBufferSize
                                                    options:MTLResourceStorageModeDefault];
        
        uint32_t *destData = (uint32_t *)[triIndexBuffer contents];
        for (int i = 0; i < numIndices; i++)
        {
            *destData++ = i;
        }
#if defined(ARCH_OS_MACOS)
        [triIndexBuffer didModifyRange:(NSMakeRange(0, triIndexBuffer.length))];
#endif
    }
    return triIndexBuffer;
}

id<MTLBuffer>
MtlfMetalContext::GetPointIndexBuffer(MTLIndexType indexTypeMetal, int numIndicesNeeded, bool usingQuads) {
    // infer the size from a 3 component vector of floats
    uint32_t pointBufferSize = numIndicesNeeded * sizeof(int);
    
    // Since remapping is expensive check if the buffer we created this from originally has changed  - MTL_FIXME - these checks are not robust
    if (threadState.pointIndexBuffer) {
        if ((threadState.pointIndexBuffer.length < pointBufferSize)) {
            [threadState.pointIndexBuffer release];
            threadState.pointIndexBuffer = nil;
        }
    }
    // Remap the quad indices into two sets of triangle indices
    if (!threadState.pointIndexBuffer) {
        if (indexTypeMetal != MTLIndexTypeUInt32) {
            TF_FATAL_CODING_ERROR("Only 32 bit indices currently supported for quads");
        }
        NSLog(@"Recreating quad remapped index buffer");
        
        threadState.pointIndexBuffer =
            [currentDevice newBufferWithLength:pointBufferSize
                                       options:MTLResourceStorageModeDefault];
        
        uint32_t *destData = (uint32_t *)threadState.pointIndexBuffer.contents;
        uint32_t arraySize = numIndicesNeeded;
        
        if (usingQuads) {
            for (int i = 0; i < arraySize; i+=6) {
                int base = i;
                *destData++ = base + 0;
                *destData++ = base + 1;
                *destData++ = base + 2;
                *destData++ = base + 1;
                *destData++ = base + 2;
                *destData++ = base + 3;
            }
        }
        else {
            for (int i = 0; i < arraySize; i+=3) {
                int base = i;
                *destData++ = base + 0;
                *destData++ = base + 1;
                *destData++ = base + 2;
            }
        }
#if defined(ARCH_OS_MACOS)
        [threadState.pointIndexBuffer didModifyRange:(NSMakeRange(0, threadState.pointIndexBuffer.length))];
#endif
    }
    return threadState.pointIndexBuffer;
}

void MtlfMetalContext::CheckNewStateGather()
{
}

void MtlfMetalContext::CreateCommandBuffer(MetalWorkQueueType workQueueType, bool forceFromDevice) {
    MetalWorkQueue *wq = &GetWorkQueue(workQueueType);
    
    //NSLog(@"Creating command buffer %d", (int)workQueueType);
    if (wq->commandBuffer == nil) {
        std::lock_guard<std::mutex> lock(_commandBufferPoolMutex);

        forceFromDevice |= [[MTLCaptureManager sharedCaptureManager] isCapturing];
        if (commandBuffersStackPos > 0 && !forceFromDevice) {
            wq->commandBuffer = commandBuffers[--commandBuffersStackPos];
        }
        else {
            wq->commandBuffer = [gpus.commandQueue commandBuffer];
            [wq->commandBuffer retain];
        }
        if (workQueueType == METALWORKQUEUE_DEFAULT) {
            int frameNumber = GetCurrentFrame();
            GPUTimerEventExpected(frameNumber);
        }
    }
    // We'll reuse an existing buffer silently if it's empty, otherwise emit warning
    else if (wq->encoderHasWork) {
        TF_CODING_WARNING("Command buffer already exists");
    }
    METAL_INC_STAT(resourceStats.commandBuffersCreated);
}

MTLF_API
id<MTLCommandBuffer> MtlfMetalContext::GetCommandBuffer(MetalWorkQueueType workQueueType) {
    MetalWorkQueue *wq = &GetWorkQueue(workQueueType);

    if (wq->commandBuffer == nil) {
        CreateCommandBuffer(workQueueType);
    }
    else {
        if (wq->encoderInUse) {
            TF_FATAL_CODING_ERROR("Not valid to get a command buffer if an encoder is still in use");
        }
        
        // If the last used encoder wasn't ended then we need to end it now
        if (wq->encoderHasWork && !wq->encoderEnded) {
            wq->encoderInUse = true;
            ReleaseEncoder(true, workQueueType);
        }
    }
 
    return wq->commandBuffer;
}


void MtlfMetalContext::LabelCommandBuffer(NSString *label, MetalWorkQueueType workQueueType)
{
    MetalWorkQueue *wq = &GetWorkQueue(workQueueType);
    
    if (wq->commandBuffer == nil) {
        TF_FATAL_CODING_ERROR("No command buffer to label");
    }
    wq->commandBuffer.label = label;
}

void MtlfMetalContext::EncodeWaitForEvent(MetalWorkQueueType waitQueue, MetalWorkQueueType signalQueue, uint64_t eventValue)
{
    MetalWorkQueue *wait_wq   = &GetWorkQueue(waitQueue);
    MetalWorkQueue *signal_wq = &GetWorkQueue(signalQueue);
    
    // Check both work queues have been set up 
    if (!wait_wq->commandBuffer || !signal_wq->commandBuffer) {
        TF_FATAL_CODING_ERROR("One of the work queue has no command buffer associated with it");
    }
    
    if (wait_wq->encoderHasWork && wait_wq->encoderInUse) {
        TF_FATAL_CODING_ERROR("Can't set an event dependency if encoder is still in use");
    }
    // If the last used encoder wasn't ended then we need to end it now
    if (wait_wq->currentEncoderType != MTLENCODERTYPE_NONE && !wait_wq->encoderEnded) {
        wait_wq->encoderInUse = true;
        ReleaseEncoder(true, waitQueue);
    }

    eventValue = (eventValue != 0) ? eventValue : threadState.currentEventValue;
    
    // We should only wait for the event if we haven't already encoded a wait for an equal or larger value.
    if(eventValue > wait_wq->lastWaitEventValue) {
        // Update the signalling queue's highest expected value to make sure the wait completes.
        if(eventValue > threadState.highestExpectedEventValue)
            threadState.highestExpectedEventValue = eventValue;
    }
}

void MtlfMetalContext::EncodeWaitForQueue(MetalWorkQueueType waitQueue, MetalWorkQueueType signalQueue)
{
    MetalWorkQueue *signal_wq = &GetWorkQueue(signalQueue);
    signal_wq->generatesEndOfQueueEvent = true;

    EncodeWaitForEvent(waitQueue, signalQueue, endOfQueueEventValue);
}

uint64_t MtlfMetalContext::EncodeSignalEvent(MetalWorkQueueType signalQueue)
{
    MetalWorkQueue *wq = &GetWorkQueue(signalQueue);

    if (!wq->commandBuffer) {
        TF_FATAL_CODING_ERROR("Signal work queue has no command buffer associated with it");
    }
    
    if (wq->encoderHasWork) {
        if (wq->encoderInUse) {
            TF_FATAL_CODING_ERROR("Can't generate an event if encoder is still in use");
        }
        // If the last used encoder wasn't ended then we need to end it now
        if (!wq->encoderEnded) {
            wq->encoderInUse = true;
            ReleaseEncoder(true, signalQueue);
        }
    }
    return threadState.currentEventValue++;
}

MTLRenderPassDescriptor* MtlfMetalContext::GetRenderPassDescriptor()
{
    MetalWorkQueue *wq = &GetWorkQueue(METALWORKQUEUE_DEFAULT);
    return (wq == nil) ? nil : wq->currentRenderPassDescriptor;
}

void MtlfMetalContext::SetFrontFaceWinding(MTLWinding _windingOrder)
{
    windingOrder = _windingOrder;
//        threadState.dirtyRenderState |= DIRTY_METALRENDERSTATE_CULLMODE_WINDINGORDER;
}

void MtlfMetalContext::SetCullMode(MTLCullMode _cullMode)
{
    cullMode = _cullMode;
//      threadState.dirtyRenderState |= DIRTY_METALRENDERSTATE_CULLMODE_WINDINGORDER;
}

void MtlfMetalContext::SetPolygonFillMode(MTLTriangleFillMode _fillMode)
{
    fillMode = _fillMode;
//        threadState.dirtyRenderState |= DIRTY_METALRENDERSTATE_FILL_MODE;
}

void MtlfMetalContext::SetAlphaBlendingEnable(bool _blendEnable)
{
    blendState.blendEnable = _blendEnable;
}

void MtlfMetalContext::SetBlendOps(MTLBlendOperation _rgbBlendOp, MTLBlendOperation _alphaBlendOp)
{
    blendState.rgbBlendOp = _rgbBlendOp;
    blendState.alphaBlendOp = _alphaBlendOp;
}

void MtlfMetalContext::SetBlendFactors(MTLBlendFactor _sourceColorFactor, MTLBlendFactor _destColorFactor,
                                       MTLBlendFactor _sourceAlphaFactor, MTLBlendFactor _destAlphaFactor)
{
    blendState.sourceColorFactor = _sourceColorFactor;
    blendState.destColorFactor = _destColorFactor;
    blendState.sourceAlphaFactor = _sourceAlphaFactor;
    blendState.destAlphaFactor = _destAlphaFactor;
}

void MtlfMetalContext::SetBlendColor(GfVec4f const &blendColor)
{
    blendState.blendColor = blendColor;
}

void MtlfMetalContext::SetColorWriteMask(MTLColorWriteMask mask)
{
    blendState.writeMask = mask;
}

void MtlfMetalContext::SetDepthWriteEnable(bool depthWriteEnable)
{
    depthState.depthWriteEnable = depthWriteEnable;
}

void MtlfMetalContext::SetDepthComparisonFunction(MTLCompareFunction comparisonFn)
{
    depthState.depthCompareFunction = comparisonFn;
}

void MtlfMetalContext::SetAlphaCoverageEnable(bool alphaCoverageEnable, bool alphaToOneEnable)
{
    blendState.alphaCoverageEnable = alphaCoverageEnable;
    blendState.alphaToOneEnable = alphaToOneEnable;
}

void MtlfMetalContext::SetShadingPrograms(id<MTLFunction> vertexFunction, id<MTLFunction> fragmentFunction, bool _enableMVA)
{
    CheckNewStateGather();
    
    threadState.renderVertexFunction     = vertexFunction;
    threadState.renderFragmentFunction   = fragmentFunction;
    threadState.enableMVA                = _enableMVA;
    // Assume there is no GS associated with these shaders, they must be linked by calling SetGSPrograms after this
    threadState.renderComputeGSFunction  = nil;
    threadState.enableComputeGS          = false;
}

void MtlfMetalContext::SetGSProgram(id<MTLFunction> computeFunction)
{
    if (!computeFunction || !threadState.renderVertexFunction) {
         TF_FATAL_CODING_ERROR("Compute and Vertex functions must be set when using a Compute Geometry Shader!");
    }
    if(!threadState.enableMVA)
    {
        TF_FATAL_CODING_ERROR("Manual Vertex Assembly must be enabled when using a Compute Geometry Shader!");
    }
    threadState.renderComputeGSFunction = computeFunction;
    threadState.enableComputeGS = true;
 }

void MtlfMetalContext::SetVertexAttribute(uint32_t index,
                                          int size,
                                          int type,
                                          size_t stride,
                                          uint32_t offset,
                                          const TfToken& name)
{
    if (threadState.enableMVA)  //Setting vertex attributes means nothing when Manual Vertex Assembly is enabled.
        return;

    if (!threadState.vertexDescriptor)
    {
        threadState.vertexDescriptor = [[MTLVertexDescriptor alloc] init];

        threadState.vertexDescriptor.layouts[0].stepFunction = MTLVertexStepFunctionConstant;
        threadState.vertexDescriptor.layouts[0].stepRate = 0;
        threadState.vertexDescriptor.layouts[0].stride = stride;
        threadState.vertexDescriptor.attributes[0].format = MTLVertexFormatUInt;
        threadState.numVertexComponents = 1;
    }

    threadState.vertexDescriptor.attributes[index].bufferIndex = index;
    threadState.vertexDescriptor.attributes[index].offset = offset;
    threadState.vertexDescriptor.layouts[index].stepFunction = MTLVertexStepFunctionPerVertex;
    threadState.vertexDescriptor.layouts[index].stepRate = 1;
    threadState.vertexDescriptor.layouts[index].stride = stride;
    
    switch (type) {
        case GL_INT:
            threadState.vertexDescriptor.attributes[index].format = MTLVertexFormat(MTLVertexFormatInt + (size - 1));
            break;
        case GL_UNSIGNED_INT:
            threadState.vertexDescriptor.attributes[index].format = MTLVertexFormat(MTLVertexFormatUInt + (size - 1));
            break;
        case GL_FLOAT:
            threadState.vertexDescriptor.attributes[index].format = MTLVertexFormat(MTLVertexFormatFloat + (size - 1));
            break;
        case GL_INT_2_10_10_10_REV:
            threadState.vertexDescriptor.attributes[index].format = MTLVertexFormatInt1010102Normalized;
            break;
        default:
            TF_CODING_ERROR("Unsupported data type");
            break;
    }
    
    if (index + 1 > threadState.numVertexComponents) {
        threadState.numVertexComponents = index + 1;
    }
    
    threadState.dirtyRenderState |= DIRTY_METALRENDERSTATE_VERTEX_DESCRIPTOR;
}

// I think this can be removed didn't seem to make too much difference to speeds
void copyUniform(uint8_t *dest, uint8_t *src, uint32_t size)
{
    switch (size) {
        case 4: {
            *(uint32_t*)dest = *(uint32_t*)src;
            break; }
        case 8: {
            *(uint64_t*)dest = *(uint64_t*)src;
            break; }
        case 12: {
            *(((uint32_t*)dest) + 0) = *(((uint32_t*)src) + 0);
            *(((uint32_t*)dest) + 1) = *(((uint32_t*)src) + 1);
            *(((uint32_t*)dest) + 2) = *(((uint32_t*)src) + 2);
            break; }
        case 16: {
            *(((uint64_t*)dest) + 0) = *(((uint64_t*)src) + 0);
            *(((uint64_t*)dest) + 1) = *(((uint64_t*)src) + 1);
            break; }
        default:
            memcpy(dest, src, size);
    }
}

void MtlfMetalContext::SetUniform(
        const void* _data,
        uint32_t _dataSize,
        const TfToken& _name,
        uint32_t _index,
        MSL_ProgramStage _stage)
{
    if (!_dataSize) {
        return;
    }

    uint8_t* bufferContents  = threadState.oldStyleUniformBuffer[_stage];

    uint32_t uniformEnd = (_index + _dataSize);
    copyUniform(bufferContents + _index, (uint8_t*)_data, _dataSize);
}

void MtlfMetalContext::SetOldStyleUniformBuffer(
    int index,
    MSL_ProgramStage stage,
    int oldStyleUniformSize)
{
    if(stage == 0)
        TF_FATAL_CODING_ERROR("Not allowed!");
    
    threadState.oldStyleUniformBufferSize[stage] = oldStyleUniformSize;

    if (oldStyleUniformSize > threadState.oldStyleUniformBufferAllocatedSize[stage]) {
        threadState.oldStyleUniformBufferAllocatedSize[stage] = oldStyleUniformSize;

        uint8_t *newBuffer = new uint8_t[oldStyleUniformSize];
        memcpy(newBuffer, threadState.oldStyleUniformBuffer[stage], oldStyleUniformSize);
        delete[] threadState.oldStyleUniformBuffer[stage];
        threadState.oldStyleUniformBuffer[stage] = newBuffer;
    }

    threadState.oldStyleUniformBufferIndex[stage] = index;
    
    if(stage == kMSL_ProgramStage_Vertex) {
        threadState.dirtyRenderState |= DIRTY_METALRENDERSTATE_OLD_STYLE_VERTEX_UNIFORM;
    }
    if(stage == kMSL_ProgramStage_Fragment) {
        threadState.dirtyRenderState |= DIRTY_METALRENDERSTATE_OLD_STYLE_FRAGMENT_UNIFORM;
    }
}

void MtlfMetalContext::SetUniformBuffer(
        int index,
        id<MTLBuffer> const buffer,
        const TfToken& name,
        MSL_ProgramStage stage,
        int offset)
{
    if(stage == 0)
        TF_FATAL_CODING_ERROR("Not allowed!");
    
    // Allocate a binding for this buffer
    BufferBinding *bufferInfo = new BufferBinding{
        index, buffer, name, stage, offset, true,
        (uint8_t *)(buffer.contents)};

    threadState.boundBuffers.push_back(bufferInfo);
}

void MtlfMetalContext::SetVertexBuffer(int index, id<MTLBuffer> const buffer, const TfToken& name)
{
    BufferBinding *bufferInfo = new BufferBinding{index, buffer, name, kMSL_ProgramStage_Vertex, 0, true};
    threadState.boundBuffers.push_back(bufferInfo);

    if (name == points) {
        threadState.vertexPositionBuffer = buffer;
    }
    
    threadState.dirtyRenderState |= DIRTY_METALRENDERSTATE_VERTEX_BUFFER;
}

void MtlfMetalContext::SetFragmentBuffer(int index, id<MTLBuffer> const buffer, const TfToken& name)
{
    BufferBinding *bufferInfo = new BufferBinding{index, buffer, name, kMSL_ProgramStage_Fragment, 0, true};
    threadState.boundBuffers.push_back(bufferInfo);

    if (name == points) {
        threadState.vertexPositionBuffer = buffer;
    }
    
    threadState.dirtyRenderState |= DIRTY_METALRENDERSTATE_VERTEX_BUFFER;
}

void MtlfMetalContext::SetIndexBuffer(id<MTLBuffer> const buffer)
{
    threadState.indexBuffer = buffer;
    //threadState.remappedQuadIndexBuffer = nil;
    threadState.dirtyRenderState |= DIRTY_METALRENDERSTATE_INDEX_BUFFER;
}

void MtlfMetalContext::SetSampler(int index, id<MTLSamplerState> const sampler, const TfToken& name, MSL_ProgramStage stage)
{
    threadState.samplers.push_back({index, sampler, name, stage});
    threadState.dirtyRenderState |= DIRTY_METALRENDERSTATE_SAMPLER;
}

void MtlfMetalContext::SetTexture(int index, id<MTLTexture> const texture, const TfToken& name, MSL_ProgramStage stage, bool arrayTexture)
{
    threadState.textures.push_back({index, texture, name, stage, arrayTexture});
    threadState.dirtyRenderState |= DIRTY_METALRENDERSTATE_TEXTURE;
}

size_t MtlfMetalContext::HashVertexDescriptor()
{
    size_t hashVal = 0;
    for (int i = 0; i < threadState.numVertexComponents; i++) {
        boost::hash_combine(hashVal, threadState.vertexDescriptor.layouts[i].stepFunction);
        boost::hash_combine(hashVal, threadState.vertexDescriptor.layouts[i].stepRate);
        boost::hash_combine(hashVal, threadState.vertexDescriptor.layouts[i].stride);
        boost::hash_combine(hashVal, threadState.vertexDescriptor.attributes[i].bufferIndex);
        boost::hash_combine(hashVal, threadState.vertexDescriptor.attributes[i].offset);
        boost::hash_combine(hashVal, threadState.vertexDescriptor.attributes[i].format);
    }
    return hashVal;
}

void MtlfMetalContext::SetRenderPipelineState()
{
    MetalWorkQueue *wq = threadState.currentWorkQueue;
    
    if (wq->currentEncoderType != MTLENCODERTYPE_RENDER || !wq->encoderInUse || !wq->currentRenderEncoder) {
        TF_FATAL_CODING_ERROR("Not valid to call SetRenderPipelineState() without an active render encoder");
    }
    
    if (!threadState.enableMVA) {
        if (threadState.dirtyRenderState & DIRTY_METALRENDERSTATE_VERTEX_DESCRIPTOR/* || renderPipelineStateDescriptor.vertexDescriptor == NULL*/) {
            // Update vertex descriptor hash
            wq->currentVertexDescriptorHash = HashVertexDescriptor();
        }
    }
    
    if (threadState.dirtyRenderState & DIRTY_METALRENDERSTATE_DRAW_TARGET) {
        size_t hashVal = 0;

        boost::hash_combine(hashVal, outputPixelFormat);
        boost::hash_combine(hashVal, outputDepthFormat);

        // Update colour attachments hash
        wq->currentColourAttachmentsHash = hashVal;
    }
    
    int sampleCount = 1;
    sampleCount = hgi->_sampleCount;
    
    // Always call this because currently we're not tracking changes to its state
    size_t hashVal = 0;
    
    boost::hash_combine(hashVal, currentDevice);
    boost::hash_combine(hashVal, threadState.renderVertexFunction);
    boost::hash_combine(hashVal, threadState.renderFragmentFunction);
    boost::hash_combine(hashVal, wq->currentVertexDescriptorHash);
    boost::hash_combine(hashVal, wq->currentColourAttachmentsHash);
    boost::hash_combine(hashVal, blendState.blendEnable);
    boost::hash_combine(hashVal, blendState.alphaCoverageEnable);
    boost::hash_combine(hashVal, blendState.alphaToOneEnable);
    boost::hash_combine(hashVal, blendState.rgbBlendOp);
    boost::hash_combine(hashVal, blendState.alphaBlendOp);
    boost::hash_combine(hashVal, blendState.sourceColorFactor);
    boost::hash_combine(hashVal, blendState.sourceAlphaFactor);
    boost::hash_combine(hashVal, blendState.destColorFactor);
    boost::hash_combine(hashVal, blendState.destAlphaFactor);
    boost::hash_combine(hashVal, blendState.writeMask);
    boost::hash_combine(hashVal, sampleCount);

    // If this matches the current pipeline state then we should already have the correct pipeline bound
    if (hashVal == wq->currentRenderPipelineDescriptorHash && wq->currentRenderPipelineState != nil) {
        return;
    }
    wq->currentRenderPipelineDescriptorHash = hashVal;
    
    _pipelineMutex.lock();
    auto pipelineStateIt = renderPipelineStateMap.find(wq->currentRenderPipelineDescriptorHash);
    
    id<MTLRenderPipelineState> pipelineState;

    if (pipelineStateIt != renderPipelineStateMap.end()) {
        pipelineState = pipelineStateIt->second;
        _pipelineMutex.unlock();
    }
    else
    {
        MTLRenderPipelineDescriptor *renderPipelineStateDescriptor = [[MTLRenderPipelineDescriptor alloc] init];

        if (!threadState.enableMVA) {
            if (threadState.dirtyRenderState & DIRTY_METALRENDERSTATE_VERTEX_DESCRIPTOR ||
                renderPipelineStateDescriptor.vertexDescriptor == NULL) {
                // This assignment can be expensive as the vertexdescriptor will be copied (due to interface property)
                renderPipelineStateDescriptor.vertexDescriptor = threadState.vertexDescriptor;
                // Update vertex descriptor hash
                wq->currentVertexDescriptorHash = HashVertexDescriptor();
            }
        }
        
        threadState.dirtyRenderState &= ~DIRTY_METALRENDERSTATE_VERTEX_DESCRIPTOR;

        // Create a new render pipeline state object
        renderPipelineStateDescriptor.label = @"SetRenderEncoderState";
        renderPipelineStateDescriptor.rasterSampleCount = sampleCount;
        
        renderPipelineStateDescriptor.inputPrimitiveTopology = MTLPrimitiveTopologyClassUnspecified;
        
        renderPipelineStateDescriptor.vertexFunction   = threadState.renderVertexFunction;
        renderPipelineStateDescriptor.fragmentFunction = threadState.renderFragmentFunction;
        
        if (threadState.renderFragmentFunction == nil) {
            renderPipelineStateDescriptor.rasterizationEnabled = false;
        }
        else {
            renderPipelineStateDescriptor.rasterizationEnabled = true;
        }
        
#if METAL_TESSELLATION_SUPPORT
        renderPipelineStateDescriptor.maxTessellationFactor             = 1;
        renderPipelineStateDescriptor.tessellationFactorScaleEnabled    = NO;
        renderPipelineStateDescriptor.tessellationFactorFormat          = MTLTessellationFactorFormatHalf;
        renderPipelineStateDescriptor.tessellationControlPointIndexType = MTLTessellationControlPointIndexTypeNone;
        renderPipelineStateDescriptor.tessellationFactorStepFunction    = MTLTessellationFactorStepFunctionConstant;
        renderPipelineStateDescriptor.tessellationOutputWindingOrder    = MTLWindingCounterClockwise;
        renderPipelineStateDescriptor.tessellationPartitionMode         = MTLTessellationPartitionModePow2;
#endif
    
        if (threadState.dirtyRenderState & DIRTY_METALRENDERSTATE_DRAW_TARGET) {
            threadState.dirtyRenderState &= ~DIRTY_METALRENDERSTATE_DRAW_TARGET;

            if (blendState.alphaCoverageEnable) {
                renderPipelineStateDescriptor.alphaToCoverageEnabled = YES;
            }
            else {
                renderPipelineStateDescriptor.alphaToCoverageEnabled = NO;
            }

            if (blendState.alphaToOneEnable) {
                renderPipelineStateDescriptor.alphaToOneEnabled = YES;
            } else {
                renderPipelineStateDescriptor.alphaToOneEnabled = NO;
            }

            for (int i = 0; i < METAL_MAX_COLOR_ATTACHMENTS; i++) {
                if (!wq->currentRenderPassDescriptor.colorAttachments[i].texture) {
                    break;
                }
                
                if (blendState.blendEnable) {
                    renderPipelineStateDescriptor.colorAttachments[i].blendingEnabled = YES;
                }
                else {
                    renderPipelineStateDescriptor.colorAttachments[i].blendingEnabled = NO;
                }

                renderPipelineStateDescriptor.colorAttachments[i].writeMask = blendState.writeMask;
                renderPipelineStateDescriptor.colorAttachments[i].rgbBlendOperation = blendState.rgbBlendOp;
                renderPipelineStateDescriptor.colorAttachments[i].alphaBlendOperation = blendState.alphaBlendOp;

                renderPipelineStateDescriptor.colorAttachments[i].sourceRGBBlendFactor = blendState.sourceColorFactor;
                renderPipelineStateDescriptor.colorAttachments[i].sourceAlphaBlendFactor = blendState.sourceAlphaFactor;
                renderPipelineStateDescriptor.colorAttachments[i].destinationRGBBlendFactor = blendState.destColorFactor;
                renderPipelineStateDescriptor.colorAttachments[i].destinationAlphaBlendFactor = blendState.destAlphaFactor;
                
                renderPipelineStateDescriptor.colorAttachments[i].pixelFormat = outputPixelFormat;
            }
            
            renderPipelineStateDescriptor.depthAttachmentPixelFormat = outputDepthFormat;
        }

        NSError *error = NULL;
        pipelineState = [currentDevice newRenderPipelineStateWithDescriptor:renderPipelineStateDescriptor error:&error];
        [renderPipelineStateDescriptor release];

        if (!pipelineState) {
            _pipelineMutex.unlock();
            NSLog(@"Failed to created pipeline state, error %@", error);
            if (error) {
                [error release];
            }
            return;
        }
        
        renderPipelineStateMap.emplace(wq->currentRenderPipelineDescriptorHash, pipelineState);
        _pipelineMutex.unlock();

        METAL_INC_STAT(resourceStats.renderPipelineStates);
    }

    if (pipelineState != wq->currentRenderPipelineState)
    {
        [wq->currentRenderEncoder setRenderPipelineState:pipelineState];
        wq->currentRenderPipelineState = pipelineState;
    }
}

void MtlfMetalContext::SetDepthStencilState()
{
    MetalWorkQueue *wq = threadState.currentWorkQueue;
    
    if (wq->currentEncoderType != MTLENCODERTYPE_RENDER || !wq->encoderInUse || !wq->currentRenderEncoder) {
        TF_FATAL_CODING_ERROR("Not valid to call SetRenderPipelineState() without an active render encoder");
    }
    
    size_t hashVal = 0;
    
    boost::hash_combine(hashVal, currentDevice);
    boost::hash_combine(hashVal, depthState.depthWriteEnable);
    boost::hash_combine(hashVal, depthState.depthCompareFunction);
    
    if (hashVal == wq->currentDepthStencilDescriptorHash && wq->currentDepthStencilState != nil)
    {
        return;
    }
    
    wq->currentDepthStencilDescriptorHash = hashVal;
    
    _pipelineMutex.lock();
    auto depthStencilStateIt = depthStencilStateMap.find(wq->currentDepthStencilDescriptorHash);
    
    id<MTLDepthStencilState> depthStencilState;
    
    if (depthStencilStateIt != depthStencilStateMap.end()) {
        depthStencilState = depthStencilStateIt->second;
        _pipelineMutex.unlock();
    }
    else
    {
        MTLDepthStencilDescriptor * depthStencilStateDescriptor = [[MTLDepthStencilDescriptor alloc] init];
        depthStencilStateDescriptor.label = @"SetDepthStencilState";

        depthStencilStateDescriptor.depthWriteEnabled = depthState.depthWriteEnable;
        depthStencilStateDescriptor.depthCompareFunction = depthState.depthCompareFunction;
        
        depthStencilState = [currentDevice newDepthStencilStateWithDescriptor:depthStencilStateDescriptor];
        [depthStencilStateDescriptor release];

        if (!depthStencilState) {
            _pipelineMutex.unlock();
            NSLog(@"Failed to created depth stencil state");
            return;
        }
        
        depthStencilStateMap.emplace(wq->currentDepthStencilDescriptorHash, depthStencilState);
        _pipelineMutex.unlock();
        
        METAL_INC_STAT(resourceStats.depthStencilStates);
    }
    
    if (depthStencilState != wq->currentDepthStencilState)
    {
        [wq->currentRenderEncoder setDepthStencilState:depthStencilState];
        wq->currentDepthStencilState = depthStencilState;
    }
}


void MtlfMetalContext::SetRenderEncoderState()
{
    uint32_t dirtyRenderState = threadState.dirtyRenderState;

    dirtyRenderState |= DIRTY_METALRENDERSTATE_OLD_STYLE_VERTEX_UNIFORM;
//    threadState.dirtyRenderState |= 0xffffffff;

    MetalWorkQueue *wq = threadState.currentWorkQueue;
    MetalWorkQueue *gswq = &GetWorkQueue(METALWORKQUEUE_GEOMETRY_SHADER);
    id <MTLComputeCommandEncoder> computeEncoder;
    
    // Default is all buffers writable
    unsigned long immutableBufferMask = 0;
    
    // Get a compute encoder on the Geometry Shader work queue
    if(threadState.enableComputeGS) {
        MetalWorkQueueType oldWorkQueueType = threadState.currentWorkQueueType;
        computeEncoder = GetComputeEncoder(METALWORKQUEUE_GEOMETRY_SHADER);
        threadState.currentWorkQueueType = oldWorkQueueType;
        threadState.currentWorkQueue     = &GetWorkQueue(threadState.currentWorkQueueType);
    }
    
    if (wq->currentEncoderType != MTLENCODERTYPE_RENDER || !wq->encoderInUse || !wq->currentRenderEncoder) {
        TF_FATAL_CODING_ERROR("Not valid to call SetRenderEncoderState() without an active render encoder");
    }
    
    // Create and set a new pipelinestate if required
    SetRenderPipelineState();
    SetDepthStencilState();
    
    if (dirtyRenderState & DIRTY_METALRENDERSTATE_CULLMODE_WINDINGORDER) {
        [wq->currentRenderEncoder setFrontFacingWinding:windingOrder];
        [wq->currentRenderEncoder setCullMode:cullMode];
        threadState.dirtyRenderState &= ~DIRTY_METALRENDERSTATE_CULLMODE_WINDINGORDER;
    }

    if (dirtyRenderState & DIRTY_METALRENDERSTATE_FILL_MODE) {
        [wq->currentRenderEncoder setTriangleFillMode:fillMode];
        threadState.dirtyRenderState &= ~DIRTY_METALRENDERSTATE_FILL_MODE;
    }

    // Any buffers modified
    if (dirtyRenderState & DIRTY_METALRENDERSTATE_VERTEX_BUFFER) {
        
        for(auto buffer : threadState.boundBuffers)
        {
            // Only output if this buffer was modified
            if (buffer->modified) {
                if(buffer->stage == kMSL_ProgramStage_Vertex){
                    if(threadState.enableComputeGS) {
                        [computeEncoder setBuffer:buffer->buffer offset:buffer->offset atIndex:buffer->index];

                        // Remove writable status
                        immutableBufferMask |= (1 << buffer->index);
                    }
                    [wq->currentRenderEncoder setVertexBuffer:buffer->buffer offset:buffer->offset atIndex:buffer->index];
                }
                else if(buffer->stage == kMSL_ProgramStage_Fragment) {
                    [wq->currentRenderEncoder setFragmentBuffer:buffer->buffer offset:buffer->offset atIndex:buffer->index];
                }
                else{
                    if(threadState.enableComputeGS) {
                        [computeEncoder setBuffer:buffer->buffer offset:buffer->offset atIndex:buffer->index];

                        // Remove writable status
                        immutableBufferMask |= (1 << buffer->index);
                    }
                    else
                        TF_FATAL_CODING_ERROR("Compute Geometry Shader should be enabled when modifying Compute buffers!");
                }
                buffer->modified = false;
            }
        }
        
        threadState.dirtyRenderState &= ~DIRTY_METALRENDERSTATE_VERTEX_BUFFER;
    }
    
    if (dirtyRenderState & DIRTY_METALRENDERSTATE_OLD_STYLE_VERTEX_UNIFORM) {
        uint32_t index = threadState.oldStyleUniformBufferIndex[kMSL_ProgramStage_Vertex];
        if(threadState.enableComputeGS) {
            [computeEncoder setBytes:threadState.oldStyleUniformBuffer[kMSL_ProgramStage_Vertex]
                              length:threadState.oldStyleUniformBufferSize[kMSL_ProgramStage_Vertex]
                             atIndex:index];

            // Remove writable status
            immutableBufferMask |= (1 << index);
        }
        [wq->currentRenderEncoder setVertexBytes:threadState.oldStyleUniformBuffer[kMSL_ProgramStage_Vertex]
                                          length:threadState.oldStyleUniformBufferSize[kMSL_ProgramStage_Vertex]
                                         atIndex:index];
        threadState.dirtyRenderState &= ~DIRTY_METALRENDERSTATE_OLD_STYLE_VERTEX_UNIFORM;
    }
    if (dirtyRenderState & DIRTY_METALRENDERSTATE_OLD_STYLE_FRAGMENT_UNIFORM) {
        [wq->currentRenderEncoder setFragmentBytes:threadState.oldStyleUniformBuffer[kMSL_ProgramStage_Fragment]
                                            length:threadState.oldStyleUniformBufferSize[kMSL_ProgramStage_Fragment]
                                           atIndex:threadState.oldStyleUniformBufferIndex[kMSL_ProgramStage_Fragment]];
        threadState.dirtyRenderState &= ~DIRTY_METALRENDERSTATE_OLD_STYLE_FRAGMENT_UNIFORM;
    }

    if (dirtyRenderState & DIRTY_METALRENDERSTATE_TEXTURE) {
        for(auto texture : threadState.textures) {
            id<MTLTexture> t = texture.texture;
            if (t == nil) {
                if (texture.array)
                    t = gpus.blackTexture2DArray;
                else
                    t = gpus.blackTexture2D;
            }
            if(texture.stage == kMSL_ProgramStage_Vertex) {
                if(threadState.enableComputeGS) {
                    [computeEncoder setTexture:t atIndex:texture.index];
                }
                [wq->currentRenderEncoder setVertexTexture:t atIndex:texture.index];
            }
            else if(texture.stage == kMSL_ProgramStage_Fragment)
                [wq->currentRenderEncoder setFragmentTexture:t atIndex:texture.index];
            //else
            //    TF_FATAL_CODING_ERROR("Not implemented!"); //Compute case
        }
        threadState.dirtyRenderState &= ~DIRTY_METALRENDERSTATE_TEXTURE;
    }
    if (dirtyRenderState & DIRTY_METALRENDERSTATE_SAMPLER) {
        for(auto sampler : threadState.samplers) {
            id<MTLSamplerState> s = sampler.sampler;
            if (s == nil) {
                s = gpus.dummySampler;
            }
            if(sampler.stage == kMSL_ProgramStage_Vertex) {
                if(threadState.enableComputeGS) {
                    [computeEncoder setSamplerState:s atIndex:sampler.index];
                }
                [wq->currentRenderEncoder setVertexSamplerState:s atIndex:sampler.index];
            }
            else if(sampler.stage == kMSL_ProgramStage_Fragment)
                [wq->currentRenderEncoder setFragmentSamplerState:s atIndex:sampler.index];
            //else
            //    TF_FATAL_CODING_ERROR("Not implemented!"); //Compute case
        }
        threadState.dirtyRenderState &= ~DIRTY_METALRENDERSTATE_SAMPLER;
    }
    
    if(threadState.enableComputeGS) {
        SetComputeEncoderState(threadState.renderComputeGSFunction, threadState.boundBuffers.size(),
                               immutableBufferMask, @"GS Compute phase", METALWORKQUEUE_GEOMETRY_SHADER);

        // Release the geometry shader encoder
        ReleaseEncoder(false, METALWORKQUEUE_GEOMETRY_SHADER);
    }
}

void MtlfMetalContext::SetComputeEncoderState(id<MTLComputeCommandEncoder> computeEncoder)
{
    // Any buffers modified
    for(auto buffer : threadState.boundBuffers)
    {
        // Only output if this buffer was modified
//        if (buffer->modified) {
        [computeEncoder setBuffer:buffer->buffer offset:buffer->offset atIndex:buffer->index];

        // Remove writable status
//        immutableBufferMask |= (1 << buffer->index);
    }

    for(auto texture : threadState.textures) {
        [computeEncoder setTexture:texture.texture atIndex:texture.index];
        //else
        //    TF_FATAL_CODING_ERROR("Not implemented!"); //Compute case
    }

    for(auto sampler : threadState.samplers) {
        [computeEncoder setSamplerState:sampler.sampler atIndex:sampler.index];
        //else
        //    TF_FATAL_CODING_ERROR("Not implemented!"); //Compute case
    }
}

void MtlfMetalContext::ClearRenderEncoderState()
{
    MetalWorkQueue *wq = threadState.currentWorkQueue;
    
    // Release owned resources
    [threadState.vertexDescriptor               release];
    threadState.vertexDescriptor               = nil;
    
    wq->currentDepthStencilDescriptorHash = 0;
    wq->currentDepthStencilState = nil;
    
    wq->currentRenderPipelineDescriptorHash  = 0;
    wq->currentRenderPipelineState           = nil;
    
    // clear referenced resources
    threadState.indexBuffer = nil;
    threadState.vertexPositionBuffer = nil;
    threadState.numVertexComponents  = 0;
    threadState.dirtyRenderState = 0xffffffff;
    
    // Free all state associated with the buffers
    for(auto buffer : threadState.boundBuffers) {
        delete buffer;
    }
    threadState.boundBuffers.clear();
    threadState.textures.clear();
    threadState.samplers.clear();
}

// Using this function instead of setting the pipeline state directly allows caching
NSUInteger MtlfMetalContext::SetComputeEncoderState(id<MTLFunction>     computeFunction,
                                                    unsigned int        bufferCount,
                                                    unsigned long       immutableBufferMask,
                                                    NSString            *label,
                                                    MetalWorkQueueType  workQueueType)
{
    MetalWorkQueue *wq = &GetWorkQueue(workQueueType);
    
    id<MTLComputePipelineState> computePipelineState;
    
    if (wq->currentComputeEncoder == nil || wq->currentEncoderType != MTLENCODERTYPE_COMPUTE
        || !wq->encoderInUse) {
        TF_FATAL_CODING_ERROR("Compute encoder must be set and active to set the pipeline state");
    }

    size_t hashVal = 0;
    boost::hash_combine(hashVal, currentDevice);
    boost::hash_combine(hashVal, bufferCount);
    boost::hash_combine(hashVal, computeFunction);
    boost::hash_combine(hashVal, immutableBufferMask);

    // If this matches the currently bound pipeline state (assuming one is bound) then carry on using it
    if (wq->currentComputePipelineState != nil && hashVal == wq->currentComputePipelineDescriptorHash) {
        return wq->currentComputeThreadExecutionWidth;
    }

    // Update the hash
    wq->currentComputePipelineDescriptorHash = hashVal;
    
    // Search map to see if we've created a pipeline state object for this already
    _pipelineMutex.lock();
    auto computePipelineStateIt = computePipelineStateMap.find(wq->currentComputePipelineDescriptorHash);

    if (computePipelineStateIt != computePipelineStateMap.end()) {
        // Retrieve pre generated state
        computePipelineState = computePipelineStateIt->second;
        _pipelineMutex.unlock();
    }
    else
    {
        NSError *error = NULL;
        MTLAutoreleasedComputePipelineReflection* reflData = 0;
        
        MTLComputePipelineDescriptor *computePipelineStateDescriptor = [[MTLComputePipelineDescriptor alloc] init];
        
        [computePipelineStateDescriptor reset];
        computePipelineStateDescriptor.computeFunction = computeFunction;
        computePipelineStateDescriptor.label = label;
        
        // Setup buffer mutability
        int i = 0;
        while (immutableBufferMask) {
            if (immutableBufferMask & 0x1) {
                computePipelineStateDescriptor.buffers[i].mutability = MTLMutabilityImmutable;
            }
            immutableBufferMask >>= 1;
            i++;
        }
        
        // Create a new Compute pipeline state object
        computePipelineState = [currentDevice newComputePipelineStateWithDescriptor:computePipelineStateDescriptor
                                                                            options:MTLPipelineOptionNone
                                                                         reflection:reflData error:&error];
        [computePipelineStateDescriptor release];

        if (!computePipelineState) {
            _pipelineMutex.unlock();
            NSLog(@"Failed to create compute pipeline state, error %@", error);
            if (error) {
                [error release];
            }
            return 0;
        }
        computePipelineStateMap.emplace(wq->currentComputePipelineDescriptorHash, computePipelineState);
        _pipelineMutex.unlock();
        METAL_INC_STAT(resourceStats.computePipelineStates);
    }
    
    if (computePipelineState != wq->currentComputePipelineState)
    {
        [wq->currentComputeEncoder setComputePipelineState:computePipelineState];
        wq->currentComputePipelineState = computePipelineState;
        wq->currentComputeThreadExecutionWidth = [wq->currentComputePipelineState threadExecutionWidth];
    }
    
    return wq->currentComputeThreadExecutionWidth;
}

// Using this function instead of setting the pipeline state directly allows caching
id<MTLComputePipelineState> MtlfMetalContext::GetComputeEncoderState(
    id<MTLFunction>     computeFunction,
    unsigned int        bufferCount,
    unsigned int        textureCount,
    unsigned long       immutableBufferMask,
    NSString            *label)
{
    id<MTLComputePipelineState> computePipelineState;
    
    size_t hashVal = 0;
    boost::hash_combine(hashVal, currentDevice);
    boost::hash_combine(hashVal, bufferCount);
    boost::hash_combine(hashVal, textureCount);
    boost::hash_combine(hashVal, computeFunction);
    boost::hash_combine(hashVal, immutableBufferMask);
    
    // Search map to see if we've created a pipeline state object for this already
    _pipelineMutex.lock();
    auto computePipelineStateIt = computePipelineStateMap.find(hashVal);

    if (computePipelineStateIt != computePipelineStateMap.end()) {
        // Retrieve pre generated state
        computePipelineState = computePipelineStateIt->second;
        _pipelineMutex.unlock();
    }
    else
    {
        NSError *error = NULL;
        MTLAutoreleasedComputePipelineReflection* reflData = 0;
        
        MTLComputePipelineDescriptor *computePipelineStateDescriptor = [[MTLComputePipelineDescriptor alloc] init];
        
        [computePipelineStateDescriptor reset];
        computePipelineStateDescriptor.computeFunction = computeFunction;
        computePipelineStateDescriptor.label = label;
        
        // Setup buffer mutability
        int i = 0;
        while (immutableBufferMask) {
            if (immutableBufferMask & 0x1) {
                computePipelineStateDescriptor.buffers[i].mutability = MTLMutabilityImmutable;
            }
            immutableBufferMask >>= 1;
            i++;
        }
        
        // Create a new Compute pipeline state object
        computePipelineState = [currentDevice newComputePipelineStateWithDescriptor:computePipelineStateDescriptor
                                                                            options:MTLPipelineOptionNone
                                                                         reflection:reflData error:&error];
        [computePipelineStateDescriptor release];
        
        if (!computePipelineState) {
            _pipelineMutex.unlock();
            NSLog(@"Failed to create compute pipeline state, error %@", error);
            if (error) {
                [error release];
            }
            return 0;
        }
        computePipelineStateMap.emplace(hashVal, computePipelineState);
        _pipelineMutex.unlock();
        METAL_INC_STAT(resourceStats.computePipelineStates);
    }
    
    return computePipelineState;
}

int MtlfMetalContext::GetCurrentComputeThreadExecutionWidth(MetalWorkQueueType workQueueType)
{
    MetalWorkQueue const* const wq = &GetWorkQueue(workQueueType);
    
    return wq->currentComputeThreadExecutionWidth;
}

int MtlfMetalContext::GetMaxThreadsPerThreadgroup(MetalWorkQueueType workQueueType)
{
    MetalWorkQueue const* const wq = &GetWorkQueue(workQueueType);
    
    return [wq->currentComputePipelineState maxTotalThreadsPerThreadgroup];
}

void MtlfMetalContext::ResetEncoders(MetalWorkQueueType workQueueType, bool isInitializing)
{
    MetalWorkQueue *wq = &GetWorkQueue(workQueueType);

    if(!isInitializing) {
        if(threadState.highestExpectedEventValue != endOfQueueEventValue && threadState.highestExpectedEventValue >= threadState.currentEventValue) {
            TF_FATAL_CODING_ERROR("There is a WaitForEvent which is never going to get Signalled!");
		}
        if(threadState.gsHasOpenBatch)
            TF_FATAL_CODING_ERROR("A Compute Geometry Shader batch is left open!");
    }

    wq->commandBuffer         = nil;
    
    wq->encoderInUse             = false;
    wq->encoderEnded             = false;
    wq->encoderHasWork           = false;
    wq->currentEncoderType       = MTLENCODERTYPE_NONE;
    wq->currentBlitEncoder       = nil;
    wq->currentRenderEncoder     = nil;
    wq->currentComputeEncoder    = nil;
    wq->generatesEndOfQueueEvent = false;
    
    wq->currentVertexDescriptorHash          = 0;
    wq->currentColourAttachmentsHash         = 0;
    wq->currentRenderPipelineDescriptorHash  = 0;
    wq->currentRenderPipelineState           = nil;
    wq->currentDepthStencilDescriptorHash    = 0;
    wq->currentDepthStencilState             = nil;
    wq->currentComputePipelineDescriptorHash = 0;
    wq->currentComputePipelineState          = nil;
}

void MtlfMetalContext::CommitCommandBufferForThread(bool waituntilScheduled, MetalWorkQueueType workQueueType)
{
    MetalWorkQueue *wq = &GetWorkQueue(workQueueType);
    
    //NSLog(@"Committing command buffer %d %@", (int)workQueueType, wq->commandBuffer.label);
        
    // Check if there was any work to submit on this queue
    if (wq->commandBuffer == nil) {
        TF_FATAL_CODING_ERROR("Can't commit command buffer if it was never created");
    }
    
    // Check that we actually encoded something to commit..
    if (wq->encoderHasWork) {
        if (wq->encoderInUse) {
            TF_FATAL_CODING_ERROR("Can't commit command buffer if encoder is still in use");
        }
        
        // If the last used encoder wasn't ended then we need to end it now
        if (!wq->encoderEnded) {
            wq->encoderInUse = true;
            ReleaseEncoder(true, workQueueType);
        }
        //NSLog(@"Committing command buffer: %@",  wq->commandBuffer.label);
    }
    else {
        /*
         There may be cases where we speculatively create a command buffer (geometry shaders) but dont
         actually use it. In this case we've nothing to commit so we'll return but not destroy the buffer
         so we don't have the overhead of recreating it every time. If it's required to generate an event
         we have to kick it regardless
         */
        if (!wq->generatesEndOfQueueEvent) {
            //NSLog(@"No work in this command buffer: %@", wq->commandBuffer.label);
            {
                std::lock_guard<std::mutex> lock(_commandBufferPoolMutex);
                
                commandBuffers[commandBuffersStackPos++] = wq->commandBuffer;
                wq->commandBuffer = nil;
            }
            if (workQueueType == METALWORKQUEUE_DEFAULT) {
                int frameNumber = GetCurrentFrame();
                GPUTimerUnexpectEvent(frameNumber);
            }
            ResetEncoders(workQueueType);
            return;
        }
    }
    
    _gsEncodeSync(false);
    
    if(wq->generatesEndOfQueueEvent) {
        TF_FATAL_CODING_ERROR("TODO: This needs updating to work with persistent event objects. Can't just use a large value.");
        threadState.currentEventValue = endOfQueueEventValue;
        wq->generatesEndOfQueueEvent = false;
    }
    
    if (workQueueType == METALWORKQUEUE_DEFAULT) {
        int frameNumber = GetCurrentFrame();
        [wq->commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> buffer)
        {
           GPUTimerEndTimer(frameNumber);
        }];
    }
    [wq->commandBuffer commit];

    if (waituntilScheduled && wq->encoderHasWork) {
        [wq->commandBuffer waitUntilScheduled];
    }
    [wq->commandBuffer release];
    
    ResetEncoders(workQueueType);
    committedCommandBufferCount++;
    METAL_INC_STAT(resourceStats.commandBuffersCommitted);
}

void MtlfMetalContext::SetOutputPixelFormats(MTLPixelFormat pixelFormat, MTLPixelFormat depthFormat)
{
    outputPixelFormat = pixelFormat;
    outputDepthFormat = depthFormat;
}

void MtlfMetalContext::SetRenderPassDescriptor(MTLRenderPassDescriptor *renderPassDescriptor)
{
    if (!threadState.currentWorkQueue)
        return;
    
    MetalWorkQueue *wq = threadState.currentWorkQueue;
    
    // Could relax this check to only include render encoders but probably not worthwhile
    if (wq->encoderInUse) {
        TF_FATAL_CODING_ERROR("Dont set a new renderpass descriptor whilst an encoder is active");
    }
    
    // Release the current render encoder if it's active to force a new encoder to be created with the new descriptor
    if (wq->currentEncoderType == MTLENCODERTYPE_RENDER) {
        // Mark encoder in use so we can do release+end
        wq->encoderInUse = true;
        ReleaseEncoder(true);
    }
    
    threadState.dirtyRenderState |= DIRTY_METALRENDERSTATE_DRAW_TARGET;
    wq->currentRenderPassDescriptor = renderPassDescriptor;
}

void MtlfMetalContext::DirtyDrawTargets()
{
    threadState.dirtyRenderState |= DIRTY_METALRENDERSTATE_DRAW_TARGET;
}

void MtlfMetalContext::ReleaseEncoder(bool endEncoding, MetalWorkQueueType workQueueType)
{
    MetalWorkQueue *wq = &GetWorkQueue(workQueueType);
    
    if (!wq->encoderInUse) {
        TF_FATAL_CODING_ERROR("No encoder to release");
    }
    if (!wq->commandBuffer) {
        TF_FATAL_CODING_ERROR("Shouldn't be able to get here without having a command buffer created");
    }
   
    if (endEncoding) {
        switch (wq->currentEncoderType) {
            case MTLENCODERTYPE_RENDER:
            {
                [wq->currentRenderEncoder endEncoding];
                wq->currentRenderEncoder      = nil;
                wq->currentRenderPipelineState = nil;
                wq->currentDepthStencilState = nil;
                break;
            }
            case MTLENCODERTYPE_COMPUTE:
            {
                [wq->currentComputeEncoder endEncoding];
                wq->currentComputePipelineState = nil;
                wq->currentComputeEncoder       = nil;
                break;
            }
            case MTLENCODERTYPE_BLIT:
            {
                [wq->currentBlitEncoder endEncoding];
                wq->currentBlitEncoder = nil;
                break;
            }
            case MTLENCODERTYPE_NONE:
            default:
            {
                TF_FATAL_CODING_ERROR("Unsupported encoder type to flush");
                break;
            }
        }
        wq->currentEncoderType = MTLENCODERTYPE_NONE;
        wq->encoderEnded       = true;
    }
    wq->encoderInUse       = false;
}

void MtlfMetalContext::SetCurrentEncoder(MetalEncoderType encoderType, MetalWorkQueueType workQueueType)
{
    MetalWorkQueue *wq = &GetWorkQueue(workQueueType);
    
    if (wq->encoderInUse) {
        TF_FATAL_CODING_ERROR("Need to release the current encoder before getting a new one");
    }
    if (!wq->commandBuffer) {
        CreateCommandBuffer(workQueueType);
        //LabelCommandBuffer(@"Default label - fix!");
        //TF_FATAL_CODING_ERROR("Shouldn't be able to get here without having a command buffer created");
    }

    // Check if we have a cuurently active encoder
    if (wq->currentEncoderType != MTLENCODERTYPE_NONE) {
        // If the encoder types match then we can carry on using the current encoder otherwise we need to create a new one
        if(wq->currentEncoderType == encoderType) {
            // Mark that this curent encoder is in use and return
            wq->encoderInUse = true;
            return;
        }
        // If we're switching encoder types then we should end the current one
        else if(wq->currentEncoderType != encoderType && !wq->encoderEnded) {
            // Mark encoder in use so we can do release+end
            wq->encoderInUse = true;
            ReleaseEncoder(true);
        }
    }

    // Create a new encoder
    switch (encoderType) {
        case MTLENCODERTYPE_RENDER: {
            // Check we have a valid render pass descriptor
            if (!wq->currentRenderPassDescriptor) {
                TF_FATAL_CODING_ERROR("Can ony pass null renderPassDescriptor if the render encoder is currently active");
            }
            wq->currentRenderEncoder = [wq->commandBuffer renderCommandEncoderWithDescriptor: wq->currentRenderPassDescriptor];
            double w = 0.0;
            double h = 0.0;
            if (wq->currentRenderPassDescriptor.colorAttachments[0].texture) {
                w = wq->currentRenderPassDescriptor.colorAttachments[0].texture.width;
                h = wq->currentRenderPassDescriptor.colorAttachments[0].texture.height;
            } else if (wq->currentRenderPassDescriptor.depthAttachment) {
                w = wq->currentRenderPassDescriptor.depthAttachment.texture.width;
                h = wq->currentRenderPassDescriptor.depthAttachment.texture.height;
            }
            [wq->currentRenderEncoder setViewport:(MTLViewport){0, h, w, -h, 0.0, 1.0}];

            // Since the encoder is new we'll need to emit all the state again
            threadState.dirtyRenderState = 0xffffffff;
            for(auto buffer : threadState.boundBuffers) { buffer->modified = true; }
            METAL_INC_STAT(resourceStats.renderEncodersCreated);
            break;
        }
        case MTLENCODERTYPE_COMPUTE: {
#if defined(METAL_EVENTS_API_PRESENT)
            if (concurrentDispatchSupported) {
                wq->currentComputeEncoder = [wq->commandBuffer computeCommandEncoderWithDispatchType:MTLDispatchTypeConcurrent];
            }
            else
#endif
            {
                wq->currentComputeEncoder = [wq->commandBuffer computeCommandEncoder];
            }
            
            threadState.dirtyRenderState = 0xffffffff;
            for(auto buffer : threadState.boundBuffers) { buffer->modified = true; }
            METAL_INC_STAT(resourceStats.computeEncodersCreated);
            break;
        }
        case MTLENCODERTYPE_BLIT: {
            wq->currentBlitEncoder = [wq->commandBuffer blitCommandEncoder];
            METAL_INC_STAT(resourceStats.blitEncodersCreated);
            break;
        }
        case MTLENCODERTYPE_NONE:
        default: {
            TF_FATAL_CODING_ERROR("Invalid encoder type!");
        }
    }
    // Mark that this curent encoder is in use
    wq->currentEncoderType = encoderType;
    wq->encoderInUse       = true;
    wq->encoderEnded       = false;
    wq->encoderHasWork     = true;
    
    threadState.currentWorkQueueType = workQueueType;
    threadState.currentWorkQueue     = &GetWorkQueue(threadState.currentWorkQueueType);
}

id<MTLBlitCommandEncoder> MtlfMetalContext::GetBlitEncoder(MetalWorkQueueType workQueueType)
{
    MetalWorkQueue *wq = &GetWorkQueue(workQueueType);
    SetCurrentEncoder(MTLENCODERTYPE_BLIT, workQueueType);
    METAL_INC_STAT(resourceStats.blitEncodersRequested);
    return wq->currentBlitEncoder;
}

id<MTLComputeCommandEncoder> MtlfMetalContext::GetComputeEncoder(MetalWorkQueueType workQueueType)
{
    MetalWorkQueue *wq = &GetWorkQueue(workQueueType);
    SetCurrentEncoder(MTLENCODERTYPE_COMPUTE, workQueueType);
    METAL_INC_STAT(resourceStats.computeEncodersRequested);
    return wq->currentComputeEncoder;
    
}

// If a renderpass descriptor is provided a new render encoder will be created otherwise we'll use the current one
id<MTLRenderCommandEncoder>  MtlfMetalContext::GetRenderEncoder(MetalWorkQueueType workQueueType)
{
    MetalWorkQueue *wq = &GetWorkQueue(workQueueType);
    SetCurrentEncoder(MTLENCODERTYPE_RENDER, workQueueType);
    METAL_INC_STAT(resourceStats.renderEncodersRequested);
    return threadState.currentWorkQueue->currentRenderEncoder;
}

id<MTLBuffer> MtlfMetalContext::GetMetalBuffer(NSUInteger length, MTLResourceOptions options, const void *pointer)
{
    id<MTLBuffer> buffer;

    MTLStorageMode  storageMode  =  MTLStorageMode((options & MTLResourceStorageModeMask)  >> MTLResourceStorageModeShift);
    MTLCPUCacheMode cpuCacheMode = MTLCPUCacheMode((options & MTLResourceCPUCacheModeMask) >> MTLResourceCPUCacheModeShift);

    _bufferMutex.lock();
    for (auto entry = bufferFreeList.begin(); entry != bufferFreeList.end(); entry++) {
        MetalBufferListEntry bufferEntry = *entry;

        METAL_INC_STAT(resourceStats.bufferSearches);
        // Check if buffer matches size and storage mode and is old enough to reuse
        if (bufferEntry.buffer.length == length              &&
            storageMode   == bufferEntry.buffer.storageMode  &&
            cpuCacheMode  == bufferEntry.buffer.cpuCacheMode &&
            lastCompletedCommandBuffer >= (bufferEntry.releasedOnCommandBuffer + METAL_SAFE_BUFFER_REUSE_AGE)) {
            buffer = bufferEntry.buffer;
            bufferFreeList.erase(entry);
            _bufferMutex.unlock();
            
            // Copy over data
            if (pointer) {
                memcpy(buffer.contents, pointer, length);
#if defined(ARCH_OS_MACOS)
                [buffer didModifyRange:(NSMakeRange(0, length))];
#endif
            }

            METAL_INC_STAT(resourceStats.buffersReused);
            return buffer;
        }
    }
    _bufferMutex.unlock();
    
    //NSLog(@"Creating buffer of length %lu (%lu)", length, frameCount);
    if (pointer) {
        buffer  = [currentDevice newBufferWithBytes:pointer length:length options:options];
    } else {
        buffer  = [currentDevice newBufferWithLength:length options:options];
    }
    METAL_INC_STAT(resourceStats.buffersCreated);
    currentBufferAllocation.fetch_add(length, std::memory_order_relaxed);
    METAL_MAX_STAT_VAL(resourceStats.peakBufferAllocation, currentBufferAllocation);

    return buffer;
}

void MtlfMetalContext::ReleaseMetalBuffer(id<MTLBuffer> const buffer)
{
    if (!this) {
        [buffer release];
        return;
    }
    MetalBufferListEntry bufferEntry;
    bufferEntry.buffer = buffer;
    bufferEntry.releasedOnFrame = frameCount;
    bufferEntry.releasedOnCommandBuffer = committedCommandBufferCount.load(std::memory_order_relaxed);
    
    std::lock_guard<std::mutex> lock(_bufferMutex);
    bufferFreeList.push_back(bufferEntry);
    
    auto const &it = modifiedBuffers.find(buffer);
    if (it != modifiedBuffers.end()) {
        modifiedBuffers.erase(it);
    }
    //NSLog(@"Adding buffer to free list of length %lu (%lu)", buffer.length, frameCount);
}

void MtlfMetalContext::PrepareBufferFlush()
{
    _FlushCachingStarted = true;
}

void MtlfMetalContext::FlushBuffers() {
#if defined(ARCH_OS_MACOS)
    for(auto &buffer: modifiedBuffers) {
        id<MTLBuffer> const &b = buffer.first;
        MetalBufferFlushListEntry const &e = buffer.second;
        [buffer.first didModifyRange:NSMakeRange(e.start, e.end - e.start)];
    }
    modifiedBuffers.clear();
#endif
    _FlushCachingStarted = false;
}

void MtlfMetalContext::QueueBufferFlush(
    id<MTLBuffer> const &buffer, uint64_t start, uint64_t end) {
#if defined(ARCH_OS_MACOS)
    if ([buffer storageMode] != MTLStorageModeManaged) {
        return;
    }

    if (!_FlushCachingStarted) {
        [buffer didModifyRange:NSMakeRange(start, end - start)];
        return;
    }
    
    static std::mutex _mutex;
    std::lock_guard<std::mutex> lock(_mutex);
    
    auto const &it = modifiedBuffers.find(buffer);
    if (it != modifiedBuffers.end()) {
        auto &bufferEntry = it->second;
        if (start == bufferEntry.end) {
            bufferEntry.end = end;
        }
        else {
            [buffer didModifyRange:NSMakeRange(
                bufferEntry.start, bufferEntry.end - bufferEntry.start)];
            bufferEntry.start = start;
            bufferEntry.end = end;
        }
    }
    else
    {
        modifiedBuffers.emplace(buffer, MetalBufferFlushListEntry(start, end));
    }
#endif
}

void MtlfMetalContext::CleanupUnusedBuffers(bool forceClean)
{
    std::lock_guard<std::mutex> lock(_bufferMutex);
    // Release all buffers that have not been recently reused
    for (auto entry = bufferFreeList.begin(); entry != bufferFreeList.end();) {
        MetalBufferListEntry bufferEntry = *entry;
        
        // Criteria for non forced releasing buffers:
        // a) Older than x number of frames
        // b) Older than y number of command buffers
        // c) Memory threshold higher than z
        bool bReleaseBuffer = (frameCount > (bufferEntry.releasedOnFrame + METAL_MAX_BUFFER_AGE_IN_FRAMES)  ||
                               lastCompletedCommandBuffer > (bufferEntry.releasedOnCommandBuffer +  METAL_MAX_BUFFER_AGE_IN_COMMAND_BUFFERS) ||
                               currentBufferAllocation.load(std::memory_order_relaxed) > METAL_HIGH_MEMORY_THRESHOLD ||
                               forceClean);
                               
        if (bReleaseBuffer) {
            id<MTLBuffer> buffer = bufferEntry.buffer;
            //NSLog(@"Releasing buffer of length %lu (%lu) (%lu outstanding)", buffer.length, frameCount, bufferFreeList.size());
            currentBufferAllocation.fetch_add(-buffer.length, std::memory_order_relaxed);
            [buffer release];
            entry = bufferFreeList.erase(entry);
        }
        else {
            entry++;
        }
    }
    
    if (forceClean && (bufferFreeList.size() != 0)) {
        TF_FATAL_CODING_ERROR("Failed to release all Metal buffers");
    }
}

void MtlfMetalContext::StartFrameForThread() {
    threadState.PrepareThread(this);
    threadState.gsDataOffset = 0;
    threadState.gsCurrentBuffer = 0;
    threadState.gsEncodedBatches = 0;

    threadState.gsBufferIndex = 0;
    threadState.gsCurrentBuffer = threadState.gsBuffers.at(threadState.gsBufferIndex);
}

void MtlfMetalContext::StartFrame() {
    numPrimsDrawn.store(0);
    GPUTimerResetTimer(frameCount);
}

void MtlfMetalContext::EndFrameForThread() {
    threadState.currentWorkQueueType = METALWORKQUEUE_DEFAULT;
    threadState.currentWorkQueue     = &GetWorkQueue(threadState.currentWorkQueueType);
    
    //Reset the Compute GS intermediate buffer offset
//    _gsResetBuffers();
}

void MtlfMetalContext::EndFrame() {
    GPUTimerFinish(frameCount);
    
    //NSLog(@"Time: %3.3f (%lu)", GetGPUTimeInMs(), frameCount);
    
    frameCount++;
}

void MtlfMetalContext::BeginCaptureSubset(int gpuIndex)
{
    [captureScopeSubset beginScope];
}

void MtlfMetalContext::EndCaptureSubset(int gpuIndex)
{
    [captureScopeSubset endScope];
}

void MtlfMetalContext::_gsAdvanceBuffer() {
    threadState.gsBufferIndex = (threadState.gsBufferIndex + 1) % gsMaxConcurrentBatches;
    threadState.gsCurrentBuffer = threadState.gsBuffers.at(threadState.gsBufferIndex);

    threadState.gsDataOffset = 0;
}

void MtlfMetalContext::_gsResetBuffers() {
    threadState.gsBufferIndex = 0;
    threadState.gsCurrentBuffer = threadState.gsBuffers.at(threadState.gsBufferIndex);
    threadState.gsDataOffset = 0;
}

uint32_t MtlfMetalContext::GetMaxComputeGSPartSize(
        uint32_t numOutVertsPerInPrim,
        uint32_t numOutPrimsPerInPrim,
        uint32_t dataPerVert,
        uint32_t dataPerPrim) const
{
    const uint32_t maxAlignmentOffset = 15; //Reserve some space for a possible alignment taking up some.
    uint32_t sizePerPrimitive = numOutVertsPerInPrim * dataPerVert + numOutPrimsPerInPrim * dataPerPrim;
    return (gsMaxDataPerBatch - maxAlignmentOffset * 2) / sizePerPrimitive;
}

void MtlfMetalContext::PrepareForComputeGSPart(
       uint32_t vertData,
       uint32_t primData,
       id<MTLBuffer>& dataBuffer,
       uint32_t& vertOffset,
       uint32_t& primOffset)
{

    //Pad data to 16byte boundaries
    const uint32_t alignmentMask = (16 - 1);
    vertData = (vertData + alignmentMask) & ~alignmentMask;
    primData = (primData + alignmentMask) & ~alignmentMask;
    
    bool useNextBuffer = (threadState.gsDataOffset + vertData + primData) > gsMaxDataPerBatch;
    bool startingNewBatch = useNextBuffer || !threadState.gsHasOpenBatch;
    if(useNextBuffer) {
        _gsAdvanceBuffer();
    }
    dataBuffer = threadState.gsCurrentBuffer;
    vertOffset = threadState.gsDataOffset;
    threadState.gsDataOffset += vertData;
    primOffset = threadState.gsDataOffset;
    threadState.gsDataOffset += primData;

    //If we are using a new buffer we've started a new batch. That means some synching/committing may need to happen before we can continue.
    if(startingNewBatch) {
        METAL_INC_STAT(resourceStats.GSBatchesStarted);
        _gsEncodeSync(true);
    }
}

void MtlfMetalContext::_gsEncodeSync(bool doOpenBatch) {
    //Using multiple queues means the synching happens using events as the queuews are executed in parallel.
    
    //There are cases where we don't have a command buffer yet to push the wait onto.
    MetalWorkQueue *wait_wq   = &GetWorkQueue(METALWORKQUEUE_DEFAULT);
    if(wait_wq->commandBuffer == nil)
        CreateCommandBuffer(METALWORKQUEUE_DEFAULT);
    
    // Close the current batch if there is one open
    if(threadState.gsHasOpenBatch) {
        if(doOpenBatch) {
            
            threadState.gsEncodedBatches++;
            if (threadState.gsEncodedBatches == gsMaxConcurrentBatches) {
                [GetWorkQueue(METALWORKQUEUE_GEOMETRY_SHADER).commandBuffer enqueue];
                CommitCommandBufferForThread(false, METALWORKQUEUE_GEOMETRY_SHADER);
                
                [GetWorkQueue(METALWORKQUEUE_DEFAULT).commandBuffer enqueue];
                CommitCommandBufferForThread(false, METALWORKQUEUE_DEFAULT);

                CreateCommandBuffer(METALWORKQUEUE_GEOMETRY_SHADER);
                CreateCommandBuffer(METALWORKQUEUE_DEFAULT);
                
                threadState.gsEncodedBatches = 0;
            }
        }
        threadState.gsHasOpenBatch = false;
    }

    if(doOpenBatch) {
        threadState.gsHasOpenBatch = true;
    }
}

void  MtlfMetalContext::GPUTimerResetTimer(unsigned long frameNumber) {
    GPUFrameTime *timer = &gpuFrameTimes[frameNumber % METAL_NUM_GPU_FRAME_TIMES];
    
    timer->startingFrame        = frameNumber;
    timer->timingEventsExpected = 0;
    timer->timingEventsReceived = 0;
    timer->timingCompleted      = false;
}


// Starts the GPU frame timer, only the first call per frame will start the timer
void MtlfMetalContext::GPUTimerStartTimer(unsigned long frameNumber)
{
    GPUFrameTime *timer = &gpuFrameTimes[frameNumber % METAL_NUM_GPU_FRAME_TIMES];
    // Just start the timer on the first call
    gettimeofday(&timer->frameStartTime, 0);
    timer->timingEventsExpected++;
}

void MtlfMetalContext::GPUTimerEventExpected(unsigned long frameNumber)
{
    GPUFrameTime *timer = &gpuFrameTimes[frameNumber % METAL_NUM_GPU_FRAME_TIMES];
    timer->timingEventsExpected++;
}

void MtlfMetalContext::GPUTimerUnexpectEvent(unsigned long frameNumber)
{
    GPUFrameTime *timer = &gpuFrameTimes[frameNumber % METAL_NUM_GPU_FRAME_TIMES];
    timer->timingEventsExpected--;
}

// Records a GPU end of frame timer, if multiple are received only the last is recorded
void MtlfMetalContext::GPUTimerEndTimer(unsigned long frameNumber)
{
    GPUFrameTime *timer = &gpuFrameTimes[frameNumber % METAL_NUM_GPU_FRAME_TIMES];
    gettimeofday(&timer->frameEndTime, 0);
    timer->timingEventsReceived++;
    
    // If this is the last end timer call from the current frame then update the last completed frame value.
    // Note there is potentially a race condition here that means if this command buffer completes before EndOfFrame marks
    // the timer as complete we won't update. But this would only result in less efficient resource resusage.
    // We update again in the GPUGetTime call so it will get set eventually
    if (timer->timingCompleted && timer->timingEventsExpected == timer->timingEventsReceived) {
        lastCompletedFrame = frameNumber;
    }
}

// Indicates that a timer has finished receiving all of the events to be issued
void  MtlfMetalContext::GPUTimerFinish(unsigned long frameNumber) {
    GPUFrameTime *timer = &gpuFrameTimes[frameNumber % METAL_NUM_GPU_FRAME_TIMES];
    timer->timingCompleted = true;
}

// Returns the most recently valid frame time
float MtlfMetalContext::GetGPUTimeInMs() {
    GPUFrameTime *validTimer = NULL;
    unsigned long highestFrameNumber = 0;
    
    for (int i = 0; i < METAL_NUM_GPU_FRAME_TIMES; i++) {
        GPUFrameTime *timer = &gpuFrameTimes[i];
        // To be a valid time it must have received all timing events back and have it's frame marked as finished
        if (timer->startingFrame >= highestFrameNumber &&
            timer->timingCompleted                   &&
            timer->timingEventsExpected == timer->timingEventsReceived &&
            timer->timingEventsExpected > 0) {
            validTimer = timer;
            highestFrameNumber = timer->startingFrame;
        }
    }
    if (!validTimer) {
        return 0.0f;
    }
    // Store this in the context as it can be used for tracking resource usage
    lastCompletedFrame = highestFrameNumber;
    
    struct timeval diff;
    timersub(&validTimer->frameEndTime, &validTimer->frameStartTime, &diff);
    return (float) ((diff.tv_sec + diff.tv_usec) / 1000.0f);
}


PXR_NAMESPACE_CLOSE_SCOPE

