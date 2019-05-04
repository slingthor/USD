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

#if defined(ARCH_GFX_OPENGL)
#include "pxr/imaging/mtlf/glInterop.h"
#endif

#import <simd/simd.h>

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

MtlfMetalContext *MtlfMetalContext::context = NULL;
std::mutex MtlfMetalContext::_commandBufferPoolMutex;
std::mutex MtlfMetalContext::_pipelineMutex;
std::mutex MtlfMetalContext::_bufferMutex;
thread_local MtlfMetalContext::ThreadState MtlfMetalContext::threadState(MtlfMetalContext::GetMetalContext());

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
    NSArray<id<MTLDevice>> *_deviceList = MTLCopyAllDevicesWithObserver(&metalDeviceObserver,
                                                ^(id<MTLDevice> device, MTLDeviceNotificationName name) {
                                                    MtlfMetalContext::handleGPUHotPlug(device, name);
                                                });
    NSMutableArray<id<MTLDevice>> *_eGPUs          = [NSMutableArray array];
    NSMutableArray<id<MTLDevice>> *_integratedGPUs = [NSMutableArray array];
    NSMutableArray<id<MTLDevice>> *_discreteGPUs   = [NSMutableArray array];
    id<MTLDevice>                  _defaultDevice  = MTLCreateSystemDefaultDevice();
    NSArray *preferredDeviceList = _discreteGPUs;
    
    if (preferredGPUType == PREFER_DEFAULT_GPU) {
        return _defaultDevice;
    }
    
    // Put the device into the appropriate device list
    for (id<MTLDevice>dev in _deviceList) {
        if (dev.removable)
        	[_eGPUs addObject:dev];
        else if (dev.lowPower)
        	[_integratedGPUs addObject:dev];
        else
        	[_discreteGPUs addObject:dev];
    }
    
    switch (preferredGPUType) {
        case PREFER_DISPLAY_GPU:
            NSLog(@"Display device selection not supported yet, returning default GPU");
        case PREFER_DEFAULT_GPU:
            break;
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

MtlfMetalContext::MtlfMetalContext(id<MTLDevice> _device, int width, int height)
: gsMaxConcurrentBatches(0)
, gsMaxDataPerBatch(0)
, points("points")
#if defined(ARCH_GFX_OPENGL)
, glInterop(NULL)
#endif
{
    if (_device == nil) {
        //device = MtlfMetalContext::GetMetalDevice(PREFER_INTEGRATED_GPU);
        //device = MtlfMetalContext::GetMetalDevice(PREFER_DISCRETE_GPU);
        device = MtlfMetalContext::GetMetalDevice(PREFER_DEFAULT_GPU);
    }
    else
        device = _device;
    
    captureScope = [[MTLCaptureManager sharedCaptureManager] newCaptureScopeWithDevice:device];
    [[MTLCaptureManager sharedCaptureManager] setDefaultCaptureScope:captureScope];

    NSLog(@"Selected %@ for Metal Device", device.name);

    enableMultiQueue = true;

    // Create a new command queue
    commandQueue = [device newCommandQueueWithMaxCommandBufferCount:commandBufferPoolSize];
    if(enableMultiQueue) {
//        NSLog(@"Device %@ supports Metal 2, enabling multi-queue codepath.", device.name);
        commandQueueGS = [device newCommandQueueWithMaxCommandBufferCount:commandBufferPoolSize];
        gsMaxDataPerBatch = 1024 * 1024 * 32;
        gsMaxConcurrentBatches = 3;
    }
    else {
//        NSLog(@"Device %@ does not support Metal 2, using fallback path, performance may be sub-optimal.", device.name);
        gsMaxDataPerBatch = 1024 * 1024 * 32;
        gsMaxConcurrentBatches = 2;
    }
    
    ResetEncoders(METALWORKQUEUE_GEOMETRY_SHADER, true);
    ResetEncoders(METALWORKQUEUE_RESOURCE, true);
    
    memset(commandBuffers, 0x00, sizeof(commandBuffers));
    memset(commandBuffersGS, 0x00, sizeof(commandBuffersGS));

#if defined(ARCH_OS_IOS)
    #define SYSTEM_VERSION_GREATER_THAN_OR_EQUAL_TO(v) ([[[UIDevice currentDevice] systemVersion] compare:v options:NSNumericSearch] != NSOrderedAscending)

    static bool sysVerGreaterThanOrEqualTo12_0 = SYSTEM_VERSION_GREATER_THAN_OR_EQUAL_TO(@"12.0");
    concurrentDispatchSupported = sysVerGreaterThanOrEqualTo12_0;
#if defined(METAL_EVENTS_API_PRESENT)
    eventsAvailable = sysVerGreaterThanOrEqualTo12_0;
#endif

#else // ARCH_OS_IOS
    static NSOperatingSystemVersion minimumSupportedOSVersion = { .majorVersion = 10, .minorVersion = 14, .patchVersion = 5 };
    static bool sysVerGreaterOrEqualTo10_14_5 = [NSProcessInfo.processInfo isOperatingSystemAtLeastVersion:minimumSupportedOSVersion];

    concurrentDispatchSupported = sysVerGreaterOrEqualTo10_14_5;
#if defined(METAL_EVENTS_API_PRESENT)
    eventsAvailable = sysVerGreaterOrEqualTo10_14_5;
#endif
    
#endif // ARCH_OS_IOS

    MTLDepthStencilDescriptor *depthStateDesc = [[MTLDepthStencilDescriptor alloc] init];
    depthStateDesc.depthWriteEnabled = YES;
    depthStateDesc.depthCompareFunction = MTLCompareFunctionLessEqual;
    depthState = [device newDepthStencilStateWithDescriptor:depthStateDesc];

    windingOrder = MTLWindingClockwise;
    cullMode = MTLCullModeBack;
    fillMode = MTLTriangleFillModeFill;
    
    AllocateAttachments(width, height);
    
    drawTarget = NULL;
    mtlColorTexture = nil;
    mtlDepthTexture = nil;
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
    resourceStats.computePipelineStates.store(0, std::memory_order_relaxed);
    resourceStats.currentBufferAllocation.store(0, std::memory_order_relaxed);
    resourceStats.peakBufferAllocation.store(0, std::memory_order_relaxed);
    resourceStats.GSBatchesStarted.store(0, std::memory_order_relaxed);
#endif
    
    frameCount = 0;
    lastCompletedFrame = -1;
    lastCompletedCommandBuffer = -1;
    committedCommandBufferCount.store(0, std::memory_order_relaxed);
}

MtlfMetalContext::~MtlfMetalContext()
{
    [[MTLCaptureManager sharedCaptureManager] setDefaultCaptureScope:nil];
    [captureScope release];
    captureScope = nil;
    
#if defined(ARCH_GFX_OPENGL)
    if (glInterop) {
        delete glInterop;
        glInterop = NULL;
    }
#endif

    CleanupUnusedBuffers(true);
    bufferFreeList.clear();

    [commandQueue release];
    if(enableMultiQueue)
        [commandQueueGS release];
   
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
        NSLog(@"Metal   Av buf search depth:%7lu"       ,
              resourceStats.bufferSearches.load(std::memory_order_relaxed) /
              (resourceStats.buffersCreated.load(std::memory_order_relaxed) +
               resourceStats.buffersReused.load(std::memory_order_relaxed)));
        NSLog(@"Render  Encoders requested: %7llu / %7lu",
              resourceStats.renderEncodersRequested.load(std::memory_order_relaxed) / frameCount,
              resourceStats.renderEncodersRequested.load(std::memory_order_relaxed));
        NSLog(@"Render  Encoders created:   %7llu / %7lu",
              resourceStats.renderEncodersCreated.load(std::memory_order_relaxed) / frameCount,
              resourceStats.renderEncodersCreated.load(std::memory_order_relaxed));
        NSLog(@"Render  Pipeline States:    %7llu / %7lu",
              resourceStats.renderPipelineStates.load(std::memory_order_relaxed) / frameCount,
              resourceStats.renderPipelineStates.load(std::memory_order_relaxed));
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

void MtlfMetalContext::RecreateInstance(id<MTLDevice> device, int width, int height)
{
    delete context;
    context = new MtlfMetalContext(device, width, height);
}

void MtlfMetalContext::AllocateAttachments(int width, int height)
{
#if defined(ARCH_GFX_OPENGL)
    if (glInterop) {
        glInterop->AllocateAttachments(width, height);

        mtlColorTexture = glInterop->mtlColorTexture;
        mtlDepthTexture = glInterop->mtlDepthTexture;
    }
#endif
}

bool
MtlfMetalContext::IsInitialized()
{
    if (!context)
        context = new MtlfMetalContext(nil, 256, 256);

    return context->device != nil;
}

void
MtlfMetalContext::BlitColorTargetToOpenGL()
{
#if defined(ARCH_GFX_OPENGL)
    if (glInterop) {
        glInterop->BlitColorTargetToOpenGL();
    }
    else {
        TF_FATAL_CODING_ERROR("Gl interop is disabled, must call InitGLInterop"
                              " before rendering");
    }
#else
    TF_FATAL_CODING_ERROR("Gl interop is disabled, because OpenGL is disabled");
#endif
}

void
MtlfMetalContext::CopyDepthTextureToOpenGL()
{
#if defined(ARCH_GFX_OPENGL)
    if (glInterop) {
        id <MTLComputeCommandEncoder> computeEncoder = GetComputeEncoder();
        computeEncoder.label = @"Depth buffer copy";

        glInterop->CopyDepthTextureToOpenGL(computeEncoder);

        ReleaseEncoder(true);
    }
    else {
        TF_FATAL_CODING_ERROR("Gl interop is disabled, must call InitGLInterop"
                              " before rendering");
    }
#else
    TF_FATAL_CODING_ERROR("Gl interop is disabled, because OpenGL is disabled");
#endif
}

void MtlfMetalContext::InitGLInterop() {
#if defined(ARCH_GFX_OPENGL)
    if (!glInterop) {
        glInterop = new MtlfGlInterop(device);
    }
#endif
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
        threadState.remappedQuadIndexBuffer = [device newBufferWithLength:remappedIndexBufferSize  options:MTLResourceStorageModeDefault];
        
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
        
        threadState.pointIndexBuffer = [device newBufferWithLength:pointBufferSize options:MTLResourceStorageModeDefault];
        
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

void MtlfMetalContext::CreateCommandBuffer(MetalWorkQueueType workQueueType) {
    MetalWorkQueue *wq = &GetWorkQueue(workQueueType);
    
    //NSLog(@"Creating command buffer %d", (int)workQueueType);

    if (wq->commandBuffer == nil) {
        std::lock_guard<std::mutex> lock(_commandBufferPoolMutex);

        if (enableMultiQueue && workQueueType == METALWORKQUEUE_GEOMETRY_SHADER) {
            if (commandBuffersGSStackPos > 0) {
                wq->commandBuffer = commandBuffersGS[--commandBuffersGSStackPos];
            }
            else {
                wq->commandBuffer = [context->commandQueueGS commandBuffer];
                [wq->commandBuffer retain];
            }
        }
        else {
            if (commandBuffersStackPos > 0) {
                wq->commandBuffer = commandBuffers[--commandBuffersStackPos];
            }
            else {
                wq->commandBuffer = [context->commandQueue commandBuffer];
                [wq->commandBuffer retain];
            }
        }

#if defined(METAL_EVENTS_API_PRESENT)
        if (eventsAvailable) {
            wq->event = [device newEvent];
        }
#endif
    }
    // We'll reuse an existing buffer silently if it's empty, otherwise emit warning
    else if (wq->encoderHasWork) {
        TF_CODING_WARNING("Command buffer already exists");
    }
    METAL_INC_STAT(resourceStats.commandBuffersCreated);
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

    eventValue = (eventValue != 0) ? eventValue : signal_wq->currentEventValue;
    
    // We should only wait for the event if we haven't already encoded a wait for an equal or larger value.
    if(eventValue > wait_wq->lastWaitEventValue) {
        // Update the signalling queue's highest expected value to make sure the wait completes.
        if(eventValue > signal_wq->highestExpectedEventValue)
            signal_wq->highestExpectedEventValue = eventValue;
#if defined(METAL_EVENTS_API_PRESENT)
        // Make this command buffer wait for the event to be resolved
        if (eventsAvailable) {
            [wait_wq->commandBuffer encodeWaitForEvent:signal_wq->event value:eventValue];
            wait_wq->lastWaitEventValue = eventValue;
        }
#endif
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
#if defined(METAL_EVENTS_API_PRESENT)
    if (eventsAvailable) {
        // Generate event
        [wq->commandBuffer encodeSignalEvent:wq->event value:wq->currentEventValue];
    }
#endif
    return wq->currentEventValue++;
}

MTLRenderPassDescriptor* MtlfMetalContext::GetRenderPassDescriptor()
{
    MetalWorkQueue *wq = &GetWorkQueue(METALWORKQUEUE_DEFAULT);
    return (wq == nil) ? nil : wq->currentRenderPassDescriptor;
}

void MtlfMetalContext::SetFrontFaceWinding(MTLWinding _windingOrder)
{
    windingOrder = _windingOrder;
    //threadState.dirtyRenderState |= DIRTY_METALRENDERSTATE_CULLMODE_WINDINGORDER;
}

void MtlfMetalContext::SetCullMode(MTLCullMode _cullMode)
{
    cullMode = _cullMode;
    //threadState.dirtyRenderState |= DIRTY_METALRENDERSTATE_CULLMODE_WINDINGORDER;
}

void MtlfMetalContext::SetPolygonFillMode(MTLTriangleFillMode _fillMode)
{
    fillMode = _fillMode;
    //threadState.dirtyRenderState |= DIRTY_METALRENDERSTATE_FILL_MODE;
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
        id<MTLBuffer> buffer,
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

void MtlfMetalContext::SetBuffer(int index, id<MTLBuffer> buffer, const TfToken& name)
{
    BufferBinding *bufferInfo = new BufferBinding{index, buffer, name, kMSL_ProgramStage_Vertex, 0, true};
    threadState.boundBuffers.push_back(bufferInfo);

    if (name == points) {
        threadState.vertexPositionBuffer = buffer;
    }
    
    threadState.dirtyRenderState |= DIRTY_METALRENDERSTATE_VERTEX_BUFFER;
}

void MtlfMetalContext::SetIndexBuffer(id<MTLBuffer> buffer)
{
    threadState.indexBuffer = buffer;
    //threadState.remappedQuadIndexBuffer = nil;
    threadState.dirtyRenderState |= DIRTY_METALRENDERSTATE_INDEX_BUFFER;
}

void MtlfMetalContext::SetSampler(int index, id<MTLSamplerState> sampler, const TfToken& name, MSL_ProgramStage stage)
{
    threadState.samplers.push_back({index, sampler, name, stage});
    threadState.dirtyRenderState |= DIRTY_METALRENDERSTATE_SAMPLER;
}

void MtlfMetalContext::SetTexture(int index, id<MTLTexture> texture, const TfToken& name, MSL_ProgramStage stage)
{
    threadState.textures.push_back({index, texture, name, stage});
    threadState.dirtyRenderState |= DIRTY_METALRENDERSTATE_TEXTURE;
}

void MtlfMetalContext::SetDrawTarget(MtlfDrawTarget *dt)
{
    drawTarget = dt;
    threadState.dirtyRenderState |= DIRTY_METALRENDERSTATE_DRAW_TARGET;
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
        TF_FATAL_CODING_ERROR("Not valid to call SetPipelineState() without an active render encoder");
    }
    
    if (!threadState.enableMVA) {
        if (threadState.dirtyRenderState & DIRTY_METALRENDERSTATE_VERTEX_DESCRIPTOR/* || renderPipelineStateDescriptor.vertexDescriptor == NULL*/) {
            // Update vertex descriptor hash
            wq->currentVertexDescriptorHash = HashVertexDescriptor();
        }
    }
    
    if (threadState.dirtyRenderState & DIRTY_METALRENDERSTATE_DRAW_TARGET) {
        size_t hashVal = 0;

        if (drawTarget) {
            auto& attachments = drawTarget->GetAttachments();
            for(auto it : attachments) {
                MtlfDrawTarget::MtlfAttachment* attachment = ((MtlfDrawTarget::MtlfAttachment*)&(*it.second));
                MTLPixelFormat depthFormat = [attachment->GetTextureName() pixelFormat];

                if(attachment->GetFormat() == GL_DEPTH_COMPONENT || attachment->GetFormat() == GL_DEPTH_STENCIL) {
                    boost::hash_combine(hashVal, depthFormat);
                    if(attachment->GetFormat() == GL_DEPTH_STENCIL) {
                        boost::hash_combine(hashVal, depthFormat); // again
                    }
                }
                else {
                    id<MTLTexture> texture = attachment->GetTextureName();
                    MTLPixelFormat pixelFormat = [texture pixelFormat];
                    int idx = attachment->GetAttach();
                    
                    boost::hash_combine(hashVal, pixelFormat);
                }
            }
        }
        else {
            if (mtlColorTexture != nil) {
                boost::hash_combine(hashVal, mtlColorTexture.pixelFormat);
            }
            else {
                boost::hash_combine(hashVal, outputPixelFormat);
            }
            
            if (mtlDepthTexture != nil) {
                boost::hash_combine(hashVal, mtlDepthTexture.pixelFormat);
            }
            else {
                boost::hash_combine(hashVal, outputDepthFormat);
            }
        }
        [wq->currentRenderEncoder setDepthStencilState:depthState];
        // Update colour attachments hash
        wq->currentColourAttachmentsHash = hashVal;
    }
    
    // Always call this because currently we're not tracking changes to its state
    size_t hashVal = 0;
    
    boost::hash_combine(hashVal, threadState.renderVertexFunction);
    boost::hash_combine(hashVal, threadState.renderFragmentFunction);
    boost::hash_combine(hashVal, wq->currentVertexDescriptorHash);
    boost::hash_combine(hashVal, wq->currentColourAttachmentsHash);
    
    // If this matches the current pipeline state then we should already have the correct pipeline bound
    if (hashVal == wq->currentRenderPipelineDescriptorHash && wq->currentRenderPipelineState != nil) {
        return;
    }
    wq->currentRenderPipelineDescriptorHash = hashVal;
    
    std::lock_guard<std::mutex> lock(_pipelineMutex);
    auto pipelineStateIt = renderPipelineStateMap.find(wq->currentRenderPipelineDescriptorHash);
    
    id<MTLRenderPipelineState> pipelineState;

    if (pipelineStateIt != renderPipelineStateMap.end()) {
        pipelineState = pipelineStateIt->second;
    }
    else
    {
        MTLRenderPipelineDescriptor *renderPipelineStateDescriptor = [[MTLRenderPipelineDescriptor alloc] init];

        if (!threadState.enableMVA) {
            if (threadState.dirtyRenderState & DIRTY_METALRENDERSTATE_VERTEX_DESCRIPTOR || renderPipelineStateDescriptor.vertexDescriptor == NULL) {
                // This assignment can be expensive as the vertexdescriptor will be copied (due to interface property)
                renderPipelineStateDescriptor.vertexDescriptor = threadState.vertexDescriptor;
                // Update vertex descriptor hash
                wq->currentVertexDescriptorHash = HashVertexDescriptor();
            }
        }
        
        threadState.dirtyRenderState &= ~DIRTY_METALRENDERSTATE_VERTEX_DESCRIPTOR;

        // Create a new render pipeline state object
        renderPipelineStateDescriptor.label = @"SetRenderEncoderState";
        renderPipelineStateDescriptor.sampleCount = 1;
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
            
            if (drawTarget) {
                auto& attachments = drawTarget->GetAttachments();
                for(auto it : attachments) {
                    MtlfDrawTarget::MtlfAttachment* attachment = ((MtlfDrawTarget::MtlfAttachment*)&(*it.second));
                    MTLPixelFormat depthFormat = [attachment->GetTextureName() pixelFormat];
                    
                    if(attachment->GetFormat() == GL_DEPTH_COMPONENT || attachment->GetFormat() == GL_DEPTH_STENCIL) {
                        renderPipelineStateDescriptor.depthAttachmentPixelFormat = depthFormat;
                        if(attachment->GetFormat() == GL_DEPTH_STENCIL) {
                            renderPipelineStateDescriptor.stencilAttachmentPixelFormat = depthFormat; //Do not use the stencil pixel format (X32_S8)
                        }
                    }
                    else {
                        id<MTLTexture> texture = attachment->GetTextureName();
                        MTLPixelFormat pixelFormat = [texture pixelFormat];
                        int idx = attachment->GetAttach();
                        
                        renderPipelineStateDescriptor.colorAttachments[idx].blendingEnabled = NO;
                        renderPipelineStateDescriptor.colorAttachments[idx].pixelFormat = pixelFormat;
                    }
                }
            }
            else {
                //METAL TODO: Why does this need to be hardcoded? There is no matching drawTarget? Can we get this info from somewhere?
                renderPipelineStateDescriptor.colorAttachments[0].blendingEnabled = YES;
                renderPipelineStateDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
                renderPipelineStateDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
                renderPipelineStateDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
                renderPipelineStateDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
                
                if (mtlColorTexture != nil) {
                    renderPipelineStateDescriptor.colorAttachments[0].pixelFormat = mtlColorTexture.pixelFormat;
                }
                else {
                    renderPipelineStateDescriptor.colorAttachments[0].pixelFormat = outputPixelFormat;
                }
                
                if (mtlDepthTexture != nil) {
                    renderPipelineStateDescriptor.depthAttachmentPixelFormat = mtlDepthTexture.pixelFormat;
                }
                else {
                    renderPipelineStateDescriptor.depthAttachmentPixelFormat = outputDepthFormat;
                    renderPipelineStateDescriptor.stencilAttachmentPixelFormat = outputDepthFormat;
                }
            }
        }

        NSError *error = NULL;
        pipelineState = [device newRenderPipelineStateWithDescriptor:renderPipelineStateDescriptor error:&error];
        if (!pipelineState) {
            NSLog(@"Failed to created pipeline state, error %@", error);
            return;
        }
        
        [renderPipelineStateDescriptor release];
        
        renderPipelineStateMap.emplace(wq->currentRenderPipelineDescriptorHash, pipelineState);
        METAL_INC_STAT(resourceStats.renderPipelineStates);
    }
  
    if (pipelineState != wq->currentRenderPipelineState)
    {
        [wq->currentRenderEncoder setRenderPipelineState:pipelineState];
        wq->currentRenderPipelineState = pipelineState;
    }
}

void MtlfMetalContext::SetRenderEncoderState()
{
    threadState.dirtyRenderState |= DIRTY_METALRENDERSTATE_OLD_STYLE_VERTEX_UNIFORM;
    threadState.dirtyRenderState |= 0xffffffff;

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
    
    if (threadState.dirtyRenderState & DIRTY_METALRENDERSTATE_CULLMODE_WINDINGORDER) {
        [wq->currentRenderEncoder setFrontFacingWinding:windingOrder];
        [wq->currentRenderEncoder setCullMode:cullMode];
        threadState.dirtyRenderState &= ~DIRTY_METALRENDERSTATE_CULLMODE_WINDINGORDER;
    }

    if (threadState.dirtyRenderState & DIRTY_METALRENDERSTATE_FILL_MODE) {
        [wq->currentRenderEncoder setTriangleFillMode:fillMode];
        threadState.dirtyRenderState &= ~DIRTY_METALRENDERSTATE_FILL_MODE;
    }

    // Any buffers modified
    if (threadState.dirtyRenderState & DIRTY_METALRENDERSTATE_VERTEX_BUFFER) {
        
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
    
    if (threadState.dirtyRenderState & DIRTY_METALRENDERSTATE_OLD_STYLE_VERTEX_UNIFORM) {
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
    if (threadState.dirtyRenderState & DIRTY_METALRENDERSTATE_OLD_STYLE_FRAGMENT_UNIFORM) {
        [wq->currentRenderEncoder setFragmentBytes:threadState.oldStyleUniformBuffer[kMSL_ProgramStage_Fragment]
                                            length:threadState.oldStyleUniformBufferSize[kMSL_ProgramStage_Fragment]
                                           atIndex:threadState.oldStyleUniformBufferIndex[kMSL_ProgramStage_Fragment]];
        threadState.dirtyRenderState &= ~DIRTY_METALRENDERSTATE_OLD_STYLE_FRAGMENT_UNIFORM;
    }

    if (threadState.dirtyRenderState & DIRTY_METALRENDERSTATE_TEXTURE) {
        for(auto texture : threadState.textures) {
            if(texture.stage == kMSL_ProgramStage_Vertex) {
                if(threadState.enableComputeGS) {
                    [computeEncoder setTexture:texture.texture atIndex:texture.index];
                }
                [wq->currentRenderEncoder setVertexTexture:texture.texture atIndex:texture.index];
            }
            else if(texture.stage == kMSL_ProgramStage_Fragment)
                [wq->currentRenderEncoder setFragmentTexture:texture.texture atIndex:texture.index];
            //else
            //    TF_FATAL_CODING_ERROR("Not implemented!"); //Compute case
        }
        threadState.dirtyRenderState &= ~DIRTY_METALRENDERSTATE_TEXTURE;
    }
    if (threadState.dirtyRenderState & DIRTY_METALRENDERSTATE_SAMPLER) {
        for(auto sampler : threadState.samplers) {
            if(sampler.stage == kMSL_ProgramStage_Vertex) {
                if(threadState.enableComputeGS) {
                    [computeEncoder setSamplerState:sampler.sampler atIndex:sampler.index];
                }
                [wq->currentRenderEncoder setVertexSamplerState:sampler.sampler atIndex:sampler.index];
            }
            else if(sampler.stage == kMSL_ProgramStage_Fragment)
                [wq->currentRenderEncoder setFragmentSamplerState:sampler.sampler atIndex:sampler.index];
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

void MtlfMetalContext::ClearRenderEncoderState()
{
    MetalWorkQueue *wq = threadState.currentWorkQueue;
    
    // Release owned resources
    [threadState.vertexDescriptor               release];
    threadState.vertexDescriptor               = nil;
    
    wq->currentRenderPipelineDescriptorHash  = 0;
    wq->currentRenderPipelineState           = nil;
    
    // clear referenced resources
    threadState.indexBuffer          = nil;
    threadState.vertexPositionBuffer = nil;
    threadState.numVertexComponents  = 0;
    threadState.dirtyRenderState     = DIRTY_METALRENDERSTATE_ALL;
    
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
    std::lock_guard<std::mutex> lock(_pipelineMutex);
    auto computePipelineStateIt = computePipelineStateMap.find(wq->currentComputePipelineDescriptorHash);

    if (computePipelineStateIt != computePipelineStateMap.end()) {
        // Retrieve pre generated state
        computePipelineState = computePipelineStateIt->second;
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
        computePipelineState = [device newComputePipelineStateWithDescriptor:computePipelineStateDescriptor options:MTLPipelineOptionNone reflection:reflData error:&error];
        [computePipelineStateDescriptor release];

        if (!computePipelineState) {
            NSLog(@"Failed to create compute pipeline state, error %@", error);
            return 0;
        }
        computePipelineStateMap.emplace(wq->currentComputePipelineDescriptorHash, computePipelineState);
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
        if(wq->highestExpectedEventValue != endOfQueueEventValue && wq->highestExpectedEventValue >= wq->currentEventValue) {
            TF_FATAL_CODING_ERROR("There is a WaitForEvent which is never going to get Signalled!");
		}
#if defined(METAL_EVENTS_API_PRESENT)
        if(wq->event != nil) {
            [wq->event release];
		}
#endif
        if(threadState.gsHasOpenBatch)
            TF_FATAL_CODING_ERROR("A Compute Geometry Shader batch is left open!");
    }
   
    wq->commandBuffer         = nil;
#if defined(METAL_EVENTS_API_PRESENT)
    wq->event                 = nil;
#endif
    
    wq->encoderInUse             = false;
    wq->encoderEnded             = false;
    wq->encoderHasWork           = false;
    wq->currentEncoderType       = MTLENCODERTYPE_NONE;
    wq->currentBlitEncoder       = nil;
    wq->currentRenderEncoder     = nil;
    wq->currentComputeEncoder    = nil;
    
    wq->currentEventValue                    = 1;
    wq->highestExpectedEventValue            = 0;
    wq->lastWaitEventValue                   = 0;
    wq->currentVertexDescriptorHash          = 0;
    wq->currentColourAttachmentsHash         = 0;
    wq->currentRenderPipelineDescriptorHash  = 0;
    wq->currentRenderPipelineState           = nil;
    wq->currentComputePipelineDescriptorHash = 0;
    wq->currentComputePipelineState          = nil;
}

void MtlfMetalContext::CommitCommandBufferForThread(bool waituntilScheduled, bool waitUntilCompleted, MetalWorkQueueType workQueueType)
{
    MetalWorkQueue *wq = &GetWorkQueue(workQueueType);
    
    //NSLog(@"Committing command buffer %d %@", (int)workQueueType, wq->commandBuffer.label);
    
    if (waituntilScheduled && waitUntilCompleted) {
        TF_FATAL_CODING_ERROR("Just pick one please!");
    }
    
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
                wq->commandBuffer.label = @"stacked";
                if (workQueueType == METALWORKQUEUE_GEOMETRY_SHADER)
                    commandBuffersGS[commandBuffersGSStackPos++] = wq->commandBuffer;
                else
                    commandBuffers[commandBuffersStackPos++] = wq->commandBuffer;
                wq->commandBuffer = nil;
            }
            ResetEncoders(workQueueType);
            return;
        }
    }
    
    _gsEncodeSync(false);
    
    if(wq->generatesEndOfQueueEvent) {
        wq->currentEventValue = endOfQueueEventValue;
#if defined(METAL_EVENTS_API_PRESENT)
        if (eventsAvailable) {
            [wq->commandBuffer encodeSignalEvent:wq->event value:wq->currentEventValue];
        }
#endif
        wq->generatesEndOfQueueEvent = false;
    }
    
    [wq->commandBuffer commit];
    
    if (waitUntilCompleted) {
        [wq->commandBuffer waitUntilCompleted];
    }
    else if (waituntilScheduled && wq->encoderHasWork) {
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
    
    wq->currentRenderPassDescriptor = renderPassDescriptor;
    isRenderPassDescriptorPatched = false;
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
        NSLog(@"Creating a command buffer on demand, try and avoid this!");
        CreateCommandBuffer(workQueueType);
        LabelCommandBuffer(@"Default label - fix!");
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
            _PatchRenderPassDescriptor();

            // Since the encoder is new we'll need to emit all the state again
            threadState.dirtyRenderState     = DIRTY_METALRENDERSTATE_ALL;
            for(auto buffer : threadState.boundBuffers) { buffer->modified = true; }
            METAL_INC_STAT(resourceStats.renderEncodersCreated);
            break;
        }
        case MTLENCODERTYPE_COMPUTE: {
#if defined(METAL_EVENTS_API_PRESENT)
            if (concurrentDispatchSupported && eventsAvailable) {
                wq->currentComputeEncoder = [wq->commandBuffer computeCommandEncoderWithDispatchType:MTLDispatchTypeConcurrent];
            }
            else
#endif
            {
                wq->currentComputeEncoder = [wq->commandBuffer computeCommandEncoder];
            }
            
            threadState.dirtyRenderState     = DIRTY_METALRENDERSTATE_ALL;
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
    id<MTLBuffer> buffer = nil;

    MTLStorageMode  storageMode  =  MTLStorageMode((options & MTLResourceStorageModeMask)  >> MTLResourceStorageModeShift);
    MTLCPUCacheMode cpuCacheMode = MTLCPUCacheMode((options & MTLResourceCPUCacheModeMask) >> MTLResourceCPUCacheModeShift);

    std::lock_guard<std::mutex> lock(_bufferMutex);
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
            break;
        }
    }
    
    if (buffer) {
        // Copy over data
        if (pointer) {
            memcpy(buffer.contents, pointer, length);
#if defined(ARCH_OS_MACOS)
            [buffer didModifyRange:(NSMakeRange(0, length))];
#endif
        }
        
        METAL_INC_STAT(resourceStats.buffersReused);
    }
    else {
        //NSLog(@"Creating buffer of length %lu (%lu)", length, frameCount);
        if (pointer) {
            buffer  =  [device newBufferWithBytes:pointer length:length options:options];
        } else {
            buffer  =  [device newBufferWithLength:length options:options];
        }
        METAL_INC_STAT(resourceStats.buffersCreated);
        METAL_INC_STAT_VAL(resourceStats.currentBufferAllocation, length);
        METAL_MAX_STAT_VAL(resourceStats.peakBufferAllocation, resourceStats.currentBufferAllocation);
    }
    return buffer;
}

void MtlfMetalContext::ReleaseMetalBuffer(id<MTLBuffer> buffer)
{
    MetalBufferListEntry bufferEntry;
    bufferEntry.buffer = buffer;
    bufferEntry.releasedOnFrame = frameCount;
    bufferEntry.releasedOnCommandBuffer = committedCommandBufferCount.load(std::memory_order_relaxed);
    
    std::lock_guard<std::mutex> lock(_bufferMutex);
    bufferFreeList.push_back(bufferEntry);
    //NSLog(@"Adding buffer to free list of length %lu (%lu)", buffer.length, frameCount);
}

void MtlfMetalContext::CleanupUnusedBuffers(bool forceClean)
{
    std::lock_guard<std::mutex> lock(_bufferMutex);
    // Release all buffers that have not been recently reused
    for (auto entry = bufferFreeList.begin(); entry != bufferFreeList.end();) {
        MetalBufferListEntry bufferEntry = *entry;
        id<MTLBuffer>  buffer = bufferEntry.buffer;
        
        // Criteria for non forced releasing buffers:
        // a) Older than x number of frames
        // b) Older than y number of command buffers
        // c) Memory threshold higher than z
        bool bReleaseBuffer = (frameCount > (bufferEntry.releasedOnFrame + METAL_MAX_BUFFER_AGE_IN_FRAMES)  ||
                               lastCompletedCommandBuffer > (bufferEntry.releasedOnCommandBuffer +  METAL_MAX_BUFFER_AGE_IN_COMMAND_BUFFERS) ||
                               resourceStats.currentBufferAllocation.load(std::memory_order_relaxed) > METAL_HIGH_MEMORY_THRESHOLD ||
                               forceClean);
                               
        if (bReleaseBuffer) {
            //NSLog(@"Releasing buffer of length %lu (%lu) (%lu outstanding)", buffer.length, frameCount, bufferFreeList.size());
            METAL_INC_STAT_VAL(resourceStats.currentBufferAllocation, -buffer.length);
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
    threadState.gsFirstBatch = true;
}

void MtlfMetalContext::StartFrame() {
    numPrimsDrawn = 0;
    GPUTImerResetTimer(frameCount);
    
    [captureScope beginScope];
}

void MtlfMetalContext::EndFrameForThread() {
    threadState.currentWorkQueueType = METALWORKQUEUE_DEFAULT;
    threadState.currentWorkQueue     = &GetWorkQueue(threadState.currentWorkQueueType);
    
    //Reset the Compute GS intermediate buffer offset
    _gsResetBuffers();
}

void MtlfMetalContext::EndFrame() {
    GPUTimerFinish(frameCount);
    
    //NSLog(@"Time: %3.3f (%lu)", GetGPUTimeInMs(), frameCount);
    
    frameCount++;
    
    // Reset it here as OSD may get invoked before StartFrame() is called.
    OSDEnabledThisFrame = false;
    isRenderPassDescriptorPatched = false;
    
    [captureScope endScope];
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
    const uint32_t alignment = 16;
    vertData = ((vertData + (alignment - 1)) / alignment) * alignment;
    primData = ((primData + (alignment - 1)) / alignment) * alignment;
    
    bool useNextBuffer = (threadState.gsDataOffset + vertData + primData) > gsMaxDataPerBatch;
    bool startingNewBatch = useNextBuffer || (threadState.gsBufferIndex == 0 && threadState.gsDataOffset == 0);
    if(useNextBuffer)
        _gsAdvanceBuffer();
    dataBuffer = threadState.gsCurrentBuffer;
    vertOffset = threadState.gsDataOffset;
    threadState.gsDataOffset += vertData;
    primOffset = threadState.gsDataOffset;
    threadState.gsDataOffset += primData;

    //If we are using a new buffer we've started a new batch. That means some synching/committing may need to happen before we can continue.
    if(startingNewBatch) {
        METAL_INC_STAT(resourceStats.GSBatchesStarted);
        if(enableMultiQueue) {
            _gsEncodeSync(true);
        }
        else {
            //When not using multiple queues we rely on the order in the commandbuffer combined with a fence for synching.
            MetalWorkQueue* wq_def = &GetWorkQueue(METALWORKQUEUE_DEFAULT);
            MetalWorkQueue* wq_gs = &GetWorkQueue(METALWORKQUEUE_GEOMETRY_SHADER);
            
            if(wq_def->encoderInUse || wq_gs->encoderInUse)
                TF_FATAL_CODING_ERROR("Default and Geometry Shader encoder must not be active before calling PrepareForComputeGSPart!");

            //Commit the geometry shader queue ahead of the rendering queue. But only if there has been work before it.
            if(wq_gs->encoderHasWork) {
                CommitCommandBufferForThread(false, false, METALWORKQUEUE_GEOMETRY_SHADER);
                
                CreateCommandBuffer(METALWORKQUEUE_GEOMETRY_SHADER);
            }
            if(wq_def->encoderHasWork) {
                CommitCommandBufferForThread(false, false, METALWORKQUEUE_DEFAULT);
                
                CreateCommandBuffer(METALWORKQUEUE_DEFAULT);
                
                //Patch the descriptor to prevent clearing attachments we just rendered to.
                _PatchRenderPassDescriptor();
            }
        }
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
        EncodeSignalEvent(METALWORKQUEUE_GEOMETRY_SHADER);
        
        if(doOpenBatch) {
            uint64_t value_gs = GetEventValue(METALWORKQUEUE_GEOMETRY_SHADER);
            uint64_t offset = gsMaxConcurrentBatches;
            if(value_gs > offset) {
                EncodeWaitForEvent(METALWORKQUEUE_GEOMETRY_SHADER, METALWORKQUEUE_DEFAULT, value_gs - offset);
                EncodeSignalEvent(METALWORKQUEUE_DEFAULT);
            }
        }
        
        //By using events we triggered ending of the encoders. Need to patch up the render descriptor to prevent previous results from being wiped.
        _PatchRenderPassDescriptor();
        threadState.gsHasOpenBatch = false;
    }
    
    if(doOpenBatch) {
        EncodeWaitForEvent(METALWORKQUEUE_DEFAULT, METALWORKQUEUE_GEOMETRY_SHADER);
        threadState.gsHasOpenBatch = true;
    }
}

void MtlfMetalContext::_PatchRenderPassDescriptor() {
    if (isRenderPassDescriptorPatched)
        return;
    
    MTLRenderPassDescriptor* rpd = context->GetRenderPassDescriptor();
    if (rpd.depthAttachment != nil)
        rpd.depthAttachment.loadAction = MTLLoadActionLoad;
    if (rpd.stencilAttachment != nil)
        rpd.stencilAttachment.loadAction = MTLLoadActionLoad;
    for(int i = 0; i < 8; i++) {
        if(rpd.colorAttachments[i] != nil)
            rpd.colorAttachments[i].loadAction = MTLLoadActionLoad;
    }
    context->SetRenderPassDescriptor(rpd);
    
    isRenderPassDescriptorPatched = true;
}

void  MtlfMetalContext::GPUTImerResetTimer(unsigned long frameNumber) {
    GPUFrameTime *timer = &gpuFrameTimes[frameNumber % METAL_NUM_GPU_FRAME_TIMES];
    
    timer->startingFrame        = frameNumber;
    timer->timingEventsIssued   = 0;
    timer->timingEventsReceived = 0;
    timer->timingCompleted      = false;
}


// Starts the GPU frame timer, only the first call per frame will start the timer
void MtlfMetalContext::GPUTimerStartTimer(unsigned long frameNumber)
{
    GPUFrameTime *timer = &gpuFrameTimes[frameNumber % METAL_NUM_GPU_FRAME_TIMES];
    // Just start the timer on the first call
    if (!timer->timingEventsIssued) {
        gettimeofday(&timer->frameStartTime, 0);
    }
    timer->timingEventsIssued++;
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
    if (timer->timingCompleted && timer->timingEventsIssued == timer->timingEventsReceived) {
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
            timer->timingEventsIssued == timer->timingEventsReceived) {
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

