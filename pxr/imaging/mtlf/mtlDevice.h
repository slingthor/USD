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

#if defined(PXR_OPENGL_SUPPORT_ENABLED)
#include <GL/glew.h>
#else // PXR_OPENGL_SUPPORT_ENABLED
// MTL_FIXME: These GL constants shouldn't be referenced by Hydra.
//            Create a Hydra enum to represent instead
#define GL_UNSIGNED_INT_2_10_10_10_REV  0x8368
#define GL_INT_2_10_10_10_REV           0x8D9F
#define GL_PRIMITIVES_GENERATED         0x8C87
#define GL_TIME_ELAPSED                 0x88BF
#endif // PXR_OPENGL_SUPPORT_ENABLED

#import <Metal/Metal.h>

#include "pxr/imaging/mtlf/api.h"
#include "pxr/imaging/garch/texture.h"
#include "pxr/base/arch/threads.h"
#include "pxr/base/gf/vec4f.h"
#include "pxr/base/tf/token.h"

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>

#include <map>

PXR_NAMESPACE_OPEN_SCOPE

class HgiMetal;

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

// Enable stats gathering
#if !defined(NDEBUG)
#define METAL_ENABLE_STATS
#endif
#if defined(METAL_ENABLE_STATS)
#define METAL_INC_STAT(STAT) STAT++
#define METAL_INC_STAT_VAL(STAT, VAL) STAT.fetch_add(VAL, std::memory_order_relaxed)
#define METAL_MAX_STAT_VAL(ORIG, NEWVAL) ORIG = MAX(ORIG.load(std::memory_order_relaxed), NEWVAL.load(std::memory_order_relaxed))
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
typedef std::shared_ptr<class MtlfMetalContext> MtlfMetalContextSharedPtr;

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

struct MetalWorkQueue {
    id<MTLCommandBuffer>         commandBuffer;
    
    MetalEncoderType             currentEncoderType;
    id<MTLBlitCommandEncoder>    currentBlitEncoder;
    id<MTLRenderCommandEncoder>  currentRenderEncoder;
    id<MTLComputeCommandEncoder> currentComputeEncoder;
    MTLRenderPassDescriptor      *currentRenderPassDescriptor;
    bool encoderInUse;
    bool encoderEnded;
    bool encoderHasWork;
    bool generatesEndOfQueueEvent;
    
    uint64_t lastWaitEventValue;
    size_t currentVertexDescriptorHash;
    size_t currentColourAttachmentsHash;
    size_t currentRenderPipelineDescriptorHash;
    size_t currentComputePipelineDescriptorHash;
    size_t currentDepthStencilDescriptorHash;
    id<MTLRenderPipelineState>  currentRenderPipelineState;
    id<MTLComputePipelineState> currentComputePipelineState;
    id<MTLDepthStencilState>    currentDepthStencilState;
    NSUInteger                  currentComputeThreadExecutionWidth;
};

class MtlfMetalContext : public boost::noncopyable {
public:
    void PrepareBufferFlush();
    bool _FlushCachingStarted = false;
    
    void QueueBufferFlush(id<MTLBuffer> const &buffer, uint64_t start, uint64_t end);
    
    void FlushBuffers();
    
    MTLF_API
    virtual ~MtlfMetalContext();

    MTLF_API
    static MtlfMetalContextSharedPtr CreateMetalContext(HgiMetal *hgi) {
        context = MtlfMetalContextSharedPtr(new MtlfMetalContext(hgi));

        return context;
    }

    /// Returns an instance for the current Metal device.
    MTLF_API
    static MtlfMetalContextSharedPtr GetMetalContext() {
        return context;
    }

    /// Returns whether this interface has been initialized.
    MTLF_API
    static bool IsInitialized();

    MTLF_API
    id<MTLBuffer> GetIndexBuffer() const {
        return threadState.indexBuffer;
    }
    
    MTLF_API
    id<MTLBuffer> GetQuadIndexBuffer(MTLIndexType indexTypeMetal);
    
    MTLF_API
    id<MTLBuffer> GetPointIndexBuffer(MTLIndexType indexTypeMetal, int numIndicesNeeded, bool usingQuads);

    MTLF_API
    id<MTLBuffer> GetTriListIndexBuffer(MTLIndexType indexTypeMetal, uint32_t numTriangles);

    MTLF_API
    void CreateCommandBuffer(MetalWorkQueueType workQueueType = METALWORKQUEUE_DEFAULT, bool forceFromDevice = false);
    
    // Function intended to allow code outside of the normal Hydra/Metal stack to put its work into a common command buffer
    // there must be no active encoders when this function is called and no active encoders left when the calling code finishes with it.
    MTLF_API
    id<MTLCommandBuffer> GetCommandBuffer(MetalWorkQueueType workQueueType = METALWORKQUEUE_DEFAULT);
    
    MTLF_API
    void LabelCommandBuffer(NSString *label, MetalWorkQueueType workQueueType = METALWORKQUEUE_DEFAULT);
    
    MTLF_API
    void CommitCommandBufferForThread(bool waituntilScheduled, MetalWorkQueueType workQueueType = METALWORKQUEUE_DEFAULT);

    MTLF_API
    void CleanupUnusedBuffers(bool forceClean);
    
    MTLF_API
    void SetOutputPixelFormats(MTLPixelFormat pixelFormat, MTLPixelFormat depthFormat);
    
    MTLF_API
    void SetDrawTarget(MtlfDrawTarget *drawTarget);
    
    MTLF_API
    MtlfDrawTarget *GetDrawTarget() const {
        return drawTarget;
    }
    
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
    void SetUniformBuffer(int index, id<MTLBuffer> const buffer, const TfToken& name, MSL_ProgramStage stage, int offset = 0);
    
    MTLF_API
    void SetOldStyleUniformBuffer(
             int index,
             MSL_ProgramStage stage,
             int oldStyleUniformSize);

    MTLF_API
    void SetBuffer(int index, id<MTLBuffer> const buffer, const TfToken& name);	//Implementation binds this as a vertex buffer!
    
    MTLF_API
    void SetIndexBuffer(id<MTLBuffer> const buffer);

    MTLF_API
    void SetTexture(int index, id<MTLTexture> const texture, const TfToken& name, MSL_ProgramStage stage, bool arrayTexture = false);
    
    MTLF_API
    void SetSampler(int index, id<MTLSamplerState> const sampler, const TfToken& name, MSL_ProgramStage stage);

    MTLF_API
    void SetFrontFaceWinding(MTLWinding winding);
    
    MTLF_API
    void SetCullMode(MTLCullMode cullMode);
	
    MTLF_API
    void SetPolygonFillMode(MTLTriangleFillMode fillMode);

    MTLF_API
    void SetAlphaBlendingEnable(bool blendEnable);
    
    MTLF_API
    void SetBlendOps(MTLBlendOperation rgbBlendOp, MTLBlendOperation alphaBlendOp);

    MTLF_API
    void SetBlendFactors(MTLBlendFactor sourceColorFactor, MTLBlendFactor destColorFactor,
                         MTLBlendFactor sourceAlphaFactor, MTLBlendFactor destAlphaFactor);

    MTLF_API
    void SetBlendColor(GfVec4f const &blendColor);

    MTLF_API
    void SetDepthWriteEnable(bool depthWriteEnable);
    
    MTLF_API
    void SetDepthComparisonFunction(MTLCompareFunction comparisonFn);
    
    MTLF_API
    void SetAlphaCoverageEnable(bool alphaCoverageEnable);

    MTLF_API
    void SetRenderPassDescriptor(MTLRenderPassDescriptor *renderPassDescriptor);
    
    MTLF_API
    MTLRenderPassDescriptor* GetRenderPassDescriptor();
    
    MTLF_API
    void SetRenderEncoderState();
    
    MTLF_API
    void SetComputeEncoderState(id<MTLComputeCommandEncoder> computeEncoder);

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
    id<MTLComputePipelineState> GetComputeEncoderState(
                                     id<MTLFunction>     computeFunction,
                                     unsigned int        bufferCount,
                                     unsigned int        textureCount,
                                     unsigned long       immutableBufferMask,
                                     NSString            *label);
    
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
    void SetActiveWorkQueue(MetalWorkQueueType workQueueType) {
        threadState.currentWorkQueue = &GetWorkQueue(workQueueType);
        threadState.currentWorkQueueType = workQueueType;
    };
    
    MTLF_API
    void EncodeWaitForEvent(MetalWorkQueueType waitingQueue, MetalWorkQueueType signalQueue, uint64_t eventValue = 0);
    
    MTLF_API
    void EncodeWaitForQueue(MetalWorkQueueType waitingQueue, MetalWorkQueueType signalQueue);
    
    //Signals the event, increments the event value afterwards.
    MTLF_API
    uint64_t EncodeSignalEvent(MetalWorkQueueType signalQueue);
    
    MTLF_API
    uint64_t GetEventValue() { return threadState.currentEventValue; }
	
    MTLF_API
    id<MTLBuffer> GetMetalBuffer(NSUInteger length, MTLResourceOptions options = MTLResourceStorageModeDefault, const void *pointer = NULL);
    
    MTLF_API
    void ReleaseMetalBuffer(id<MTLBuffer> const buffer);

    MTLF_API
    void StartFrame();

    MTLF_API
    void StartFrameForThread();

    MTLF_API
    void EndFrame();
    
    MTLF_API
    void EndFrameForThread();

    MTLF_API
    bool GeometryShadersActive() const {
        return threadState.workQueueGeometry.commandBuffer != nil;
    }
    
    //Returns the maximum number of _primitives_ to process per ComputeGS part.
    MTLF_API
    uint32_t GetMaxComputeGSPartSize(uint32_t numOutVertsPerInPrim, uint32_t numOutPrimsPerInPrim, uint32_t dataPerVert, uint32_t dataPerPrim) const;
    
    //Sets up all state for a new ComputeGS part, including any buffer allocations, syncs, encoders and commandbuffers.
    //Be careful when encoding commands around this as it may start a new encoder.
    MTLF_API
    void PrepareForComputeGSPart(uint32_t vertData, uint32_t primData, id<MTLBuffer>& dataBuffer, uint32_t& vertOffset, uint32_t& primOffset);
   
    MTLF_API
    unsigned long IncNumberPrimsDrawn(unsigned long numPrims, bool init)
    {
        if (init) {
            numPrimsDrawn.store(numPrims);
            return numPrims;
        }

        return numPrimsDrawn.fetch_add(numPrims) + numPrims;
    }
    
    MTLF_API
    bool IsTempPointWorkaroundActive() const { return threadState.tempPointsWorkaroundActive; }

    MTLF_API
    void SetTempPointWorkaround(bool activate) { threadState.tempPointsWorkaroundActive = activate; }
    
    MTLF_API
    int64_t GetCurrentFrame() const { return frameCount; }

    MTLF_API
    float GetGPUTimeInMs();
    
    void BeginCaptureSubset(int gpuIndex);
    void EndCaptureSubset(int gpuIndex);

    id<MTLDevice> currentDevice;
    
    HgiMetal* GetHgi() {
        return hgi;
    }

    struct GPUInstance {
        id<MTLCommandQueue> commandQueue;

        // Dummy black texture for missing textures
        id<MTLTexture> blackTexture2D;
        // Dummy black texture for missing textures
        id<MTLTexture> blackTexture2DArray;
        // Dummy sampler for missing textures
        id<MTLSamplerState> dummySampler;
    };
    
    GPUInstance gpus;

    MTLF_API MetalWorkQueue& GetWorkQueue(MetalWorkQueueType workQueueType) {
        if (workQueueType == METALWORKQUEUE_DEFAULT)
            return threadState.workQueueDefault;
        if (workQueueType == METALWORKQUEUE_GEOMETRY_SHADER)
            return threadState.workQueueGeometry;
        return workQueueResource;
    }

    static MtlfMetalContextSharedPtr context;
    
    void  GPUTimerStartTimer(unsigned long frameNumber);
    void  GPUTimerEventExpected(unsigned long frameNumber);
    void  GPUTimerUnexpectEvent(unsigned long frameNumber);
    void  GPUTimerEndTimer(unsigned long frameNumber);

protected:

    MTLF_API
    MtlfMetalContext(HgiMetal* hgi);
    
    void Init();
    void Cleanup();

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
    
    struct TextureBinding { int index; id<MTLTexture> texture; TfToken name; MSL_ProgramStage stage; bool array; };
    struct SamplerBinding { int index; id<MTLSamplerState> sampler; TfToken name; MSL_ProgramStage stage; };
    
    struct ThreadState {

        ThreadState(){}
        
        ~ThreadState();

        void PrepareThread(MtlfMetalContext *_this);
        
        bool init = false;
        
        std::vector<BufferBinding*> boundBuffers;

        size_t oldStyleUniformBufferSize[kMSL_ProgramStage_NumStages];
        size_t oldStyleUniformBufferAllocatedSize[kMSL_ProgramStage_NumStages];
        uint8_t *oldStyleUniformBuffer[kMSL_ProgramStage_NumStages];
        uint32_t oldStyleUniformBufferIndex[kMSL_ProgramStage_NumStages];

        std::vector<TextureBinding> textures;
        std::vector<SamplerBinding> samplers;

        id<MTLBuffer> indexBuffer;
        id<MTLBuffer> vertexPositionBuffer;
        
        id<MTLComputePipelineState> computePipelineState;

        uint64_t currentEventValue;
        uint64_t highestExpectedEventValue;

        MetalWorkQueue      *currentWorkQueue;
        MetalWorkQueueType  currentWorkQueueType;
        
        MetalWorkQueue      workQueueGeometry;
        MetalWorkQueue      workQueueDefault;

        // Pipeline state
        MTLVertexDescriptor          *vertexDescriptor;
        uint32_t numVertexComponents;
        id<MTLFunction> renderVertexFunction;
        id<MTLFunction> renderFragmentFunction;
        id<MTLFunction> renderComputeGSFunction;
        
        uint32_t dirtyRenderState;
        
        //Geometry Shader Related
        int                        gsDataOffset;
        int                        gsBufferIndex;
        int                        gsEncodedBatches;
        id<MTLBuffer>              gsCurrentBuffer;
        std::vector<id<MTLBuffer>> gsBuffers;
        bool                       gsHasOpenBatch;
        
        bool tempPointsWorkaroundActive = false;
        
        id<MTLBuffer> remappedQuadIndexBuffer;
        id<MTLBuffer> remappedQuadIndexBufferSource;
        id<MTLBuffer> pointIndexBuffer;
        
        bool enableMVA;
        bool enableComputeGS;
    };

    static thread_local ThreadState threadState;
    
    static std::mutex _commandBufferPoolMutex;
    static int const commandBufferPoolSize = 256;
    id<MTLCommandBuffer> commandBuffers[commandBufferPoolSize];
    int commandBuffersStackPos;

    static std::mutex _pipelineMutex;
    boost::unordered_map<size_t, id<MTLRenderPipelineState>>  renderPipelineStateMap;
    boost::unordered_map<size_t, id<MTLDepthStencilState>>  depthStencilStateMap;
    boost::unordered_map<size_t, id<MTLComputePipelineState>> computePipelineStateMap;

    static std::mutex _bufferMutex;

    MetalWorkQueue             workQueueResource;

    int                        gsMaxConcurrentBatches;
    int                        gsMaxDataPerBatch;

    // Internal state which gets applied to the render encoder
    MTLWinding windingOrder;
    MTLCullMode cullMode;
    MTLTriangleFillMode fillMode;
    
    struct BlendState {
        bool blendEnable;
        bool alphaCoverageEnable;
        MTLBlendOperation rgbBlendOp;
        MTLBlendOperation alphaBlendOp;
        MTLBlendFactor sourceColorFactor;
        MTLBlendFactor destColorFactor;
        MTLBlendFactor sourceAlphaFactor;
        MTLBlendFactor destAlphaFactor;
        GfVec4f blendColor;
        size_t hashValue;
    } blendState;

    struct DepthState {
        bool depthWriteEnable;
        MTLCompareFunction depthCompareFunction;
    } depthState;
    
    MtlfDrawTarget *drawTarget;

private:
    const static uint64_t endOfQueueEventValue = 0xFFFFFFFFFFFFFFFF;
    
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
    uint32_t queueSyncEventCounter;
    MetalWorkQueueType outstandingDependency;
    
    id<MTLDevice> GetMetalDevice(PREFERRED_GPU_TYPE preferredGPUType);
#if defined(ARCH_OS_MACOS)
    void handleDisplayChange();
    void handleGPUHotPlug(id<MTLDevice> device, MTLDeviceNotificationName notifier);
#endif
    
    // Internal encoder functions
    void SetCurrentEncoder(MetalEncoderType encoderType, MetalWorkQueueType workQueueType);
    void ResetEncoders(MetalWorkQueueType workQueueType, bool isInitializing = false);

    // Pipeline state functions
    void SetRenderPipelineState();
    void SetDepthStencilState();

    size_t HashVertexDescriptor();
    
    bool concurrentDispatchSupported;
    
    void _gsAdvanceBuffer();
    void _gsResetBuffers();
    void _gsEncodeSync(bool doOpenBatch);

    struct MetalBufferListEntry {
        id<MTLBuffer> buffer;
        unsigned int releasedOnFrame;
        unsigned int releasedOnCommandBuffer;
    };
    std::vector<MetalBufferListEntry> bufferFreeList;
    
    struct MetalBufferFlushListEntry {
        MetalBufferFlushListEntry(uint64_t _start, uint64_t _end) {
            start = _start;
            end = _end;
        }
        uint64_t start;
        uint64_t end;
    };
    boost::unordered_map<id<MTLBuffer> const, MetalBufferFlushListEntry> modifiedBuffers;
    
    id<MTLBuffer> triIndexBuffer;

    int64_t frameCount;
    
    int64_t lastCompletedFrame;
    std::atomic_int64_t committedCommandBufferCount;
    int64_t lastCompletedCommandBuffer;
    
    TfToken points;

#if defined(METAL_ENABLE_STATS)
    struct ResourceStats {
        std::atomic_ulong commandBuffersCreated;
        std::atomic_ulong commandBuffersCommitted;
        std::atomic_ulong buffersCreated;
        std::atomic_ulong buffersReused;
        std::atomic_ulong bufferSearches;
        std::atomic_ulong peakBufferAllocation;
        std::atomic_ulong renderEncodersCreated;
        std::atomic_ulong computeEncodersCreated;
        std::atomic_ulong blitEncodersCreated;
        std::atomic_ulong renderEncodersRequested;
        std::atomic_ulong computeEncodersRequested;
        std::atomic_ulong blitEncodersRequested;
        std::atomic_ulong renderPipelineStates;
        std::atomic_ulong depthStencilStates;
        std::atomic_ulong computePipelineStates;
        std::atomic_ulong GSBatchesStarted;
    } resourceStats;
#endif
    std::atomic_ulong currentBufferAllocation;

    struct GPUFrameTime {
        unsigned long  startingFrame;
        struct timeval frameStartTime;
        struct timeval frameEndTime;
        unsigned int   timingEventsExpected;
        unsigned int   timingEventsReceived;
        bool timingCompleted;
        
    } gpuFrameTimes[METAL_NUM_GPU_FRAME_TIMES];
    
    float lastGPUFrameTime;
    
    void  GPUTimerFinish(unsigned long frameNumber);
    void  GPUTimerResetTimer(unsigned long frameNumber);
    
    std::atomic_ulong numPrimsDrawn;
    
    id<MTLCaptureScope> captureScopeSubset;
    HgiMetal* hgi;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif  // MTLF_METALCONTEXT_H
