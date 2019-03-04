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

#include "pxr/pxr.h"
#include "pxr/base/arch/defines.h"

#if defined(ARCH_GFX_OPENGL)
#include <GL/glew.h>
#else // ARCH_GFX_OPENGL
// MTL_FIXME: These GL constants shouldn't be referenced by Hydra.
//            Create a Hydra enum to represent instead
#define GL_UNSIGNED_INT_2_10_10_10_REV  0x8368
#define GL_INT_2_10_10_10_REV           0x8D9F
#define GL_PRIMITIVES_GENERATED         0x8C87
#define GL_TIME_ELAPSED                 0x88BF
#endif // ARCH_GFX_OPENGL

#include <Metal/Metal.h>
#if defined(ARCH_OS_MACOS)
#import <Cocoa/Cocoa.h>
#else
#import <UIKit/UIKit.h>
#endif // ARCH_OS_MACOS

#include "pxr/imaging/mtlf/api.h"
#include "pxr/base/arch/threads.h"
#include "pxr/base/tf/token.h"

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>

#include <map>

PXR_NAMESPACE_OPEN_SCOPE

class MtlfGlInterop;

// Enable reuse of metal buffers - should increase perforamance but may also increase memory footprint
#define METAL_REUSE_BUFFERS

#if defined(METAL_REUSE_BUFFERS)
// How old a buffer must be before it can be reused - 0 should allow for most optimal memory footprint, higher values can be used for debugging issues
#define METAL_SAFE_BUFFER_REUSE_AGE 1
// How old a buffer can be before it's freed
#define METAL_MAX_BUFFER_AGE_IN_COMMAND_BUFFERS 20
#define METAL_MAX_BUFFER_AGE_IN_FRAMES 3

#if defined(ARCH_OS_IOS)
#define METAL_HIGH_MEMORY_THRESHOLD (1UL * 1024UL * 1024UL * 1024UL)
#else
#define METAL_HIGH_MEMORY_THRESHOLD (2UL * 1024UL * 1024UL * 1024UL)
#endif
#endif

// Enable stats gathering
#define METAL_ENABLE_STATS
#if defined(METAL_ENABLE_STATS)
#define METAL_INC_STAT(STAT) STAT++
#define METAL_INC_STAT_VAL(STAT, VAL) STAT+=VAL
#define METAL_MAX_STAT_VAL(ORIG, NEWVAL) ORIG = MAX(ORIG, NEWVAL)
#else
#define METAL_INC_STAT(STAT)
#define METAL_INC_STAT_VAL(STAT, VAL)
#define METAL_MAX_STAT_VAL(ORIG, NEWVAL)
#endif

#define METAL_NUM_GPU_FRAME_TIMES 5

#define METAL_GS_THREADGROUP_SIZE 32

#if defined(ARCH_OS_IOS)
#define METAL_FEATURESET_FOR_DISPATCHTHREADS MTLFeatureSet_iOS_GPUFamily4_v1
#else
#define METAL_FEATURESET_FOR_DISPATCHTHREADS MTLFeatureSet_macOS_GPUFamily1_v1
#endif

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
    
    kMSL_ProgramStage_NumStages = 3
};

enum MetalEncoderType {
    MTLENCODERTYPE_NONE,
    MTLENCODERTYPE_RENDER,
    MTLENCODERTYPE_COMPUTE,
    MTLENCODERTYPE_BLIT
};

enum MetalWorkQueueType {
    METALWORKQUEUE_INVALID          = -1,
    METALWORKQUEUE_DEFAULT          =  0,
    METALWORKQUEUE_GEOMETRY_SHADER  =  1,
    METALWORKQUEUE_RESOURCE         =  2,
    
    METALWORKQUEUE_MAX              =  3,       // This should always be last
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
    static MtlfMetalContextSharedPtr GetMetalContext() {
        if (!context)
            context = MtlfMetalContextSharedPtr(new MtlfMetalContext(nil, 256, 256));
        
        return context;
    }

    /// Returns whether this interface has been initialized.
    MTLF_API
    static bool IsInitialized();
    
    /// Returns a reset instance for the current Metal device.
    MTLF_API
    static void RecreateInstance(id<MTLDevice> device, int width, int height);

    MTLF_API
    void AllocateAttachments(int width, int height);
    
    /// Blit the current render target contents to the OpenGL FBO
    MTLF_API
    void BlitColorTargetToOpenGL();
    
    MTLF_API
    void CopyDepthTextureToOpenGL();
    
    MTLF_API
    id<MTLBuffer> GetIndexBuffer() {
        return indexBuffer;
    }
    
    MTLF_API
    id<MTLBuffer> GetQuadIndexBuffer(MTLIndexType indexTypeMetal);
    
    MTLF_API
    id<MTLBuffer> GetPointIndexBuffer(MTLIndexType indexTypeMetal, int numIndicesNeeded, bool usingQuads);
     
    MTLF_API
    void CreateCommandBuffer(MetalWorkQueueType workQueueType = METALWORKQUEUE_DEFAULT);
    
    MTLF_API
    void LabelCommandBuffer(NSString *label, MetalWorkQueueType workQueueType = METALWORKQUEUE_DEFAULT);
    
    MTLF_API
    void CommitCommandBuffer(bool waituntilScheduled, bool waitUntilCompleted, MetalWorkQueueType workQueueType = METALWORKQUEUE_DEFAULT);
    
    MTLF_API
    void SetOutputPixelFormats(MTLPixelFormat pixelFormat, MTLPixelFormat depthFormat);
    
    MTLF_API
    void SetDrawTarget(MtlfDrawTarget *drawTarget);
    
    MTLF_API
    void SetShadingPrograms(id<MTLFunction> vertexFunction, id<MTLFunction> fragmentFunction, bool _enableMVA);
    
    MTLF_API
    void SetGSProgram(id<MTLFunction> computeFunction);
    
    MTLF_API
    void SetVertexAttribute(uint32_t index,
                            int size,
                            int type,
                            size_t stride,
                            uint32_t offset,
                            const TfToken& name);
    
    MTLF_API
    void SetUniform(const void* _data, uint32_t _dataSize, const TfToken& _name, uint32_t index, MSL_ProgramStage stage);
    
    MTLF_API
    void SetUniformBuffer(int index, id<MTLBuffer> buffer, const TfToken& name, MSL_ProgramStage stage, int offset = 0);
    
    MTLF_API
    void SetOldStyleUniformBuffer(
             int index,
             MSL_ProgramStage stage,
             int oldStyleUniformSize);

    MTLF_API
    void SetBuffer(int index, id<MTLBuffer> buffer, const TfToken& name);	//Implementation binds this as a vertex buffer!
    
    MTLF_API
    void SetIndexBuffer(id<MTLBuffer> buffer);

    MTLF_API
    void SetTexture(int index, id<MTLTexture> texture, const TfToken& name, MSL_ProgramStage stage);
    
    MTLF_API
    void SetSampler(int index, id<MTLSamplerState> sampler, const TfToken& name, MSL_ProgramStage stage);

    MTLF_API
    void SetFrontFaceWinding(MTLWinding winding);
    
    MTLF_API
    void SetCullMode(MTLCullMode cullMode);
	
    MTLF_API
    void SetPolygonFillMode(MTLTriangleFillMode fillMode);
    
    MTLF_API
    void SetRenderPassDescriptor(MTLRenderPassDescriptor *renderPassDescriptor);
    
    MTLF_API
    MTLRenderPassDescriptor* GetRenderPassDescriptor();
    
    MTLF_API
    void SetRenderEncoderState();

    MTLF_API
    void ClearRenderEncoderState();
    
    MTLF_API
    // Returns optimmum thread execution width for this kernel
    NSUInteger SetComputeEncoderState(id<MTLFunction>     computeFunction,
                                      unsigned int        bufferCount,
                                      unsigned long       immutableBufferMask,
                                      NSString           *label,
                                      MetalWorkQueueType  workQueueType = METALWORKQUEUE_DEFAULT);
    
    MTLF_API
    id<MTLBlitCommandEncoder>    GetBlitEncoder(MetalWorkQueueType workQueueType = METALWORKQUEUE_DEFAULT);

    MTLF_API
    id<MTLComputeCommandEncoder> GetComputeEncoder(MetalWorkQueueType workQueueType = METALWORKQUEUE_DEFAULT);

    MTLF_API
    int GetCurrentComputeThreadExecutionWidth(MetalWorkQueueType workQueueType = METALWORKQUEUE_DEFAULT);
    MTLF_API
    int GetMaxThreadsPerThreadgroup(MetalWorkQueueType workQueueType = METALWORKQUEUE_DEFAULT);

    MTLF_API
    id<MTLRenderCommandEncoder>  GetRenderEncoder(MetalWorkQueueType workQueueType = METALWORKQUEUE_DEFAULT);

    MTLF_API
    void ReleaseEncoder(bool endEncoding, MetalWorkQueueType workQueueType = METALWORKQUEUE_DEFAULT);
    
    MTLF_API
    void SetActiveWorkQueue(MetalWorkQueueType workQueueType) { currentWorkQueue = &workQueues[workQueueType]; currentWorkQueueType = workQueueType;};
    
    MTLF_API
    void EncodeWaitForEvent(MetalWorkQueueType waitingQueue, MetalWorkQueueType signalQueue, uint64_t eventValue = 0);
    
    MTLF_API
    void EncodeWaitForQueue(MetalWorkQueueType waitingQueue, MetalWorkQueueType signalQueue);
    
    //Signals the event, increments the event value afterwards.
    MTLF_API
    uint64_t EncodeSignalEvent(MetalWorkQueueType signalQueue);
    
    MTLF_API
    uint64_t GetEventValue(MetalWorkQueueType signalQueue) const { return workQueues[signalQueue].currentEventValue; }
	
    MTLF_API
    id<MTLBuffer> GetMetalBuffer(NSUInteger length, MTLResourceOptions options = MTLResourceStorageModeDefault, const void *pointer = NULL);
    
    MTLF_API
    void ReleaseMetalBuffer(id<MTLBuffer> buffer);

    // Gets space inside a buffer that has a lifetime of the current frame only - to be used for temporary data such as uniforms, the offset *must* be used
    // Resource options are MTLResourceStorageModeShared | MTLResourceStorageModeManaged | MTLResourceOptionCPUCacheModeDefault
    MTLF_API
    id<MTLBuffer> GetMetalBufferAllocation(NSUInteger length, const void *pointer, NSUInteger *offset);
    
    MTLF_API
    void StartFrame();
    
    MTLF_API
    void EndFrame();
    
    MTLF_API
    bool GeometryShadersActive() const { return workQueues[METALWORKQUEUE_GEOMETRY_SHADER].commandBuffer != nil; }
    
    //Returns the maximum number of _primitives_ to process per ComputeGS part.
    MTLF_API
    uint32_t GetMaxComputeGSPartSize(uint32_t numOutVertsPerInPrim, uint32_t numOutPrimsPerInPrim, uint32_t dataPerVert, uint32_t dataPerPrim) const;
    
    //Sets up all state for a new ComputeGS part, including any buffer allocations, syncs, encoders and commandbuffers.
    //Be careful when encoding commands around this as it may start a new encoder.
    MTLF_API
    void PrepareForComputeGSPart(uint32_t vertData, uint32_t primData, id<MTLBuffer>& dataBuffer, uint32_t& vertOffset, uint32_t& primOffset);
   
    MTLF_API
    unsigned long IncNumberPrimsDrawn(unsigned long numPrims, bool init) { numPrimsDrawn = init ? numPrims : (numPrimsDrawn += numPrims); return numPrimsDrawn; }
    
    MTLF_API
    bool IsTempPointWorkaroundActive() const { return tempPointsWorkaroundActive; }

    MTLF_API
    void SetTempPointWorkaround(bool activate) { tempPointsWorkaroundActive = activate; }
    
    MTLF_API
    void SetOSDEnabledThisFrame(bool OSDStatus) { OSDEnabledThisFrame = OSDStatus; }
    
    MTLF_API
    bool IsOSDEnabledThisFrame() const { return OSDEnabledThisFrame; }

    MTLF_API
    int64_t GetCurrentFrame() const { return frameCount; }

    MTLF_API
    float GetGPUTimeInMs();
    
    id<MTLDevice> device;
    id<MTLCommandQueue> commandQueue;
    id<MTLCommandQueue> commandQueueGS;
    id<MTLTexture> mtlColorTexture;
    id<MTLTexture> mtlDepthTexture;
    
    bool enableMultiQueue;
    bool enableMVA;
    bool enableComputeGS;
    
protected:
    MTLF_API
    MtlfMetalContext(id<MTLDevice> device, int width, int height);

    MTLF_API
    void CheckNewStateGather();

   
    struct BufferBinding {
        int              index;
        id<MTLBuffer>    buffer;
        TfToken          name;
        MSL_ProgramStage stage;
        int              offset;
        bool             modified;
        uint8_t          *contents;
    };
    std::vector<BufferBinding*> boundBuffers;
	
    size_t oldStyleUniformBufferSize[kMSL_ProgramStage_NumStages];
    size_t oldStyleUniformBufferAllocatedSize[kMSL_ProgramStage_NumStages];
    uint8_t *oldStyleUniformBuffer[kMSL_ProgramStage_NumStages];
    uint32_t oldStyleUniformBufferIndex[kMSL_ProgramStage_NumStages];
    
    struct TextureBinding { int index; id<MTLTexture> texture; TfToken name; MSL_ProgramStage stage; };
    std::vector<TextureBinding> textures;
	struct SamplerBinding { int index; id<MTLSamplerState> sampler; TfToken name; MSL_ProgramStage stage; };
    std::vector<SamplerBinding> samplers;
    
    id<MTLBuffer> indexBuffer;
    id<MTLBuffer> vertexPositionBuffer;
    id<MTLBuffer> remappedQuadIndexBuffer;
    id<MTLBuffer> remappedQuadIndexBufferSource;
    id<MTLBuffer> pointIndexBuffer;
    
    MtlfDrawTarget *drawTarget;

private:
    const static uint64_t endOfQueueEventValue = 0xFFFFFFFFFFFFFFFF;

    id<MTLDepthStencilState> depthState;
    id<MTLComputePipelineState> computePipelineState;
    
    // These are used when rendering from within a native Metal application, and
    // are set by the application
    MTLPixelFormat outputPixelFormat;
    MTLPixelFormat outputDepthFormat;
    
    enum PREFERRED_GPU_TYPE {
        PREFER_DEFAULT_GPU,
        PREFER_INTEGRATED_GPU,
        PREFER_DISCRETE_GPU,
        PREFER_EGPU,
        PREFER_DISPLAY_GPU,
    };
    
    // State for tracking dependencies between work queues
#if defined(METAL_EVENTS_API_PRESENT)
    id<MTLEvent> queueSyncEvent;
    bool         eventsAvailable;
#endif
    uint32_t queueSyncEventCounter;
    MetalWorkQueueType outstandingDependency;
    
    id<MTLDevice> GetMetalDevice(PREFERRED_GPU_TYPE preferredGPUType);
#if defined(ARCH_OS_MACOS)
    void handleDisplayChange();
    void handleGPUHotPlug(id<MTLDevice> device, MTLDeviceNotificationName notifier);
#endif

    static MtlfMetalContextSharedPtr context;
    
    struct MetalWorkQueue {
        id<MTLCommandBuffer>         commandBuffer;
#if defined(METAL_EVENTS_API_PRESENT)
        id<MTLEvent>                 event;
#endif
 
        MetalEncoderType             currentEncoderType;
        id<MTLBlitCommandEncoder>    currentBlitEncoder;
        id<MTLRenderCommandEncoder>  currentRenderEncoder;
        id<MTLComputeCommandEncoder> currentComputeEncoder;
        MTLRenderPassDescriptor      *currentRenderPassDescriptor;
        bool encoderInUse;
        bool encoderEnded;
        bool encoderHasWork;
        bool generatesEndOfQueueEvent;
        
        uint64_t currentEventValue;
        uint64_t highestExpectedEventValue;
        uint64_t lastWaitEventValue;
        size_t currentVertexDescriptorHash;
        size_t currentColourAttachmentsHash;
        size_t currentRenderPipelineDescriptorHash;
        size_t currentComputePipelineDescriptorHash;
        id<MTLRenderPipelineState>  currentRenderPipelineState;
        id<MTLComputePipelineState> currentComputePipelineState;
        NSUInteger                  currentComputeThreadExecutionWidth;
    };
    
    MetalWorkQueue      workQueues[METALWORKQUEUE_MAX];
    MetalWorkQueue      *currentWorkQueue;
    MetalWorkQueueType  currentWorkQueueType;
    
    // Internal encoder functions
    void SetCurrentEncoder(MetalEncoderType encoderType, MetalWorkQueueType workQueueType);
    void ResetEncoders(MetalWorkQueueType workQueueType, bool isInitializing = false);

    // Pipeline state functions
    void SetRenderPipelineState();
    size_t HashVertexDescriptor();
    size_t HashColourAttachments(uint32_t numColourAttachments);
    size_t HashPipelineDescriptor();
    size_t HashComputePipelineDescriptor(unsigned int bufferCount);
    
    // Pipeline state
    MTLRenderPipelineDescriptor  *renderPipelineStateDescriptor;
    MTLComputePipelineDescriptor *computePipelineStateDescriptor;
    MTLVertexDescriptor          *vertexDescriptor;
    uint32_t numVertexComponents;
    boost::unordered_map<size_t, id<MTLRenderPipelineState>>  renderPipelineStateMap;
    boost::unordered_map<size_t, id<MTLComputePipelineState>> computePipelineStateMap;
    id<MTLFunction> renderVertexFunction;
    id<MTLFunction> renderFragmentFunction;
    id<MTLFunction> renderComputeGSFunction;
 
    bool concurrentDispatchSupported;
    
    // Internal state which gets applied to the render encoder
    MTLWinding windingOrder;
    MTLCullMode cullMode;
    MTLTriangleFillMode fillMode;
    uint32_t dirtyRenderState;
    
    //Geometry Shader Related
    int                        gsDataOffset;
    int                        gsBufferIndex;
    int                        gsMaxConcurrentBatches;
    int                        gsMaxDataPerBatch;
    id<MTLBuffer>              gsCurrentBuffer;
    std::vector<id<MTLBuffer>> gsBuffers;
    id<MTLFence>               gsFence;
    bool                       gsHasOpenBatch;
    bool                       gsFirstBatch;
    bool                       gsSyncRequired;
    bool                       isRenderPassDescriptorPatched;

    void _gsAdvanceBuffer();
    void _gsResetBuffers();
    void _gsEncodeSync(bool doOpenBatch);
    void _PatchRenderPassDescriptor();
    
    void CleanupUnusedBuffers(bool forceClean);
#if defined(METAL_REUSE_BUFFERS)
    struct MetalBufferListEntry {
        id<MTLBuffer> buffer;
        unsigned int releasedOnFrame;
        unsigned int releasedOnCommandBuffer;
    };
    std::vector<MetalBufferListEntry> bufferFreeList;
#endif

    // Metal buffer for per frame data
    id<MTLBuffer> perFrameBuffer;
    NSUInteger    perFrameBufferSize;
    NSUInteger    perFrameBufferOffset;
    NSUInteger    perFrameBufferAlignment;
    
    int64_t frameCount;
    int64_t lastCompletedFrame;
    int64_t committedCommandBufferCount;
    int64_t lastCompletedCommandBuffer;

#if defined(METAL_ENABLE_STATS)
    struct ResourceStats {
        unsigned long commandBuffersCreated;
        unsigned long commandBuffersCommitted;
        unsigned long buffersCreated;
        unsigned long buffersReused;
        unsigned long bufferSearches;
        unsigned long currentBufferAllocation;
        unsigned long peakBufferAllocation;
        unsigned long renderEncodersCreated;
        unsigned long computeEncodersCreated;
        unsigned long blitEncodersCreated;
        unsigned long renderEncodersRequested;
        unsigned long computeEncodersRequested;
        unsigned long blitEncodersRequested;
        unsigned long renderPipelineStates;
        unsigned long computePipelineStates;
        unsigned long GSBatchesStarted;
    } resourceStats;
#endif
    
    struct GPUFrameTime {
        unsigned long  startingFrame;
        struct timeval frameStartTime;
        struct timeval frameEndTime;
        unsigned int   timingEventsIssued;
        unsigned int   timingEventsReceived;
        bool timingCompleted;
        
    } gpuFrameTimes[METAL_NUM_GPU_FRAME_TIMES];
    
    float lastGPUFrameTime;
    
    void  GPUTimerStartTimer(unsigned long frameNumber);
    void  GPUTimerEndTimer(unsigned long frameNumber);
    void  GPUTimerFinish(unsigned long frameNumber);
    void  GPUTImerResetTimer(unsigned long frameNumber);
    
    unsigned long numPrimsDrawn;
    
    bool tempPointsWorkaroundActive = false;
    bool OSDEnabledThisFrame = false;
    
#if defined(ARCH_GFX_OPENGL)
    MtlfGlInterop *glInterop;
#endif
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif  // MTLF_METALCONTEXT_H
