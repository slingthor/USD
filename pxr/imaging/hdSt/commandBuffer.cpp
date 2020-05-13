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
#include "pxr/imaging/glf/glew.h"

#include "pxr/imaging/garch/contextCaps.h"
#include "pxr/imaging/garch/resourceFactory.h"

#include "pxr/imaging/mtlf/drawTarget.h"
#include "pxr/imaging/mtlf/mtlDevice.h"
#include "pxr/imaging/hgiMetal/hgi.h"

#include "pxr/imaging/hdSt/commandBuffer.h"
#include "pxr/imaging/hdSt/debugCodes.h"
#include "pxr/imaging/hdSt/geometricShader.h"
#include "pxr/imaging/hdSt/immediateDrawBatch.h"
#include "pxr/imaging/hdSt/indirectDrawBatch.h"
#include "pxr/imaging/hdSt/resourceFactory.h"
#include "pxr/imaging/hdSt/resourceRegistry.h"
#include "pxr/imaging/hdSt/materialParam.h"

#include "pxr/imaging/hd/bufferArrayRange.h"
#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/tokens.h"

#include "pxr/base/gf/matrix4f.h"
#include "pxr/base/gf/frustum.h"
#include "pxr/base/tf/diagnostic.h"
#include "pxr/base/tf/stl.h"
#include "pxr/base/work/dispatcher.h"

#include "pxr/base/work/loops.h"

#include <boost/functional/hash.hpp>

#include <tbb/enumerable_thread_specific.h>

#include <functional>
#include <unordered_map>

#include <sys/time.h>

#include <os/signpost.h>
#include <queue>
#include <stack>
#include <algorithm>

PXR_NAMESPACE_OPEN_SCOPE

static os_log_t cullingLog = os_log_create("hydra.metal", "Culling");

HdStCommandBuffer::HdStCommandBuffer()
    : _visibleSize(0)
    , _visChangeCount(0)
    , _batchVersion(0)
{
    /*NOTHING*/
}

HdStCommandBuffer::~HdStCommandBuffer()
{
}

static
HdSt_DrawBatchSharedPtr
_NewDrawBatch(HdStDrawItemInstance * drawItemInstance)
{
    GarchContextCaps const &caps = GarchResourceFactory::GetInstance()->GetContextCaps();
    HdSt_DrawBatchSharedPtr drawBatch;

    if (caps.multiDrawIndirectEnabled) {
        drawBatch = HdStResourceFactory::GetInstance()->NewIndirectDrawBatch(drawItemInstance);
    } else {
        drawBatch = HdSt_DrawBatchSharedPtr(
            new HdSt_ImmediateDrawBatch(drawItemInstance));
    }
    
    return drawBatch;
}

void
HdStCommandBuffer::PrepareDraw(
    HdStRenderPassStateSharedPtr const &renderPassState,
    HdStResourceRegistrySharedPtr const &resourceRegistry)
{
    HD_TRACE_FUNCTION();

    for (auto const& batch : _drawBatches) {
        batch->PrepareDraw(renderPassState, resourceRegistry);
    }
}

void
HdStCommandBuffer::ExecuteDraw(
    HdStRenderPassStateSharedPtr const &renderPassState,
    HdStResourceRegistrySharedPtr const &resourceRegistry)
{
    HD_TRACE_FUNCTION();

    //
    // TBD: sort draw items
    //

    // Reset per-commandBuffer performance counters, updated by batch execution
    HD_PERF_COUNTER_SET(HdPerfTokens->drawCalls, 0);
    HD_PERF_COUNTER_SET(HdTokens->itemsDrawn, 0);

    //
    // draw batches
    //
    struct _Worker {
        static
        void draw(std::vector<HdSt_DrawBatchSharedPtr> * drawBatches,
                  HdStRenderPassStateSharedPtr const &renderPassState,
                  HdStResourceRegistrySharedPtr const &resourceRegistry,
                  MTLRenderPassDescriptor *rpd,
                  size_t begin, size_t end)
        {
            MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();
            context->StartFrameForThread();
            context->SetRenderPassDescriptor(rpd);
            
            for(size_t i = begin; i < end; i++) {
                HdSt_DrawBatchSharedPtr& batch = (*drawBatches)[i];
                batch->ExecuteDraw(renderPassState, resourceRegistry);
            }

            if (context->GeometryShadersActive()) {
                // Complete the GS command buffer if we have one
                context->CommitCommandBufferForThread(false, METALWORKQUEUE_GEOMETRY_SHADER);
            }

            if (context->GetWorkQueue(METALWORKQUEUE_DEFAULT).commandBuffer != nil) {
                context->CommitCommandBufferForThread(false);

                context->EndFrameForThread();
            }
        }
        
        struct VisibleBatch {
            HdSt_DrawBatch* batch;
            int             numVisible;
        };
        
        static
        void draw2(std::vector<VisibleBatch> *drawBatches,
                   HdStRenderPassStateSharedPtr const &renderPassState,
                   HdStResourceRegistrySharedPtr const &resourceRegistry,
                   MTLRenderPassDescriptor *rpd,
                   size_t begin, size_t end)
        {
            MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();
            context->StartFrameForThread();
            context->SetRenderPassDescriptor(rpd);
            
            for(size_t i = begin; i < end; i++) {
                HdSt_DrawBatch* batch = (*drawBatches)[i].batch;
                batch->ExecuteDraw(renderPassState, resourceRegistry);
            }
            
            if (context->GeometryShadersActive()) {
                // Complete the GS command buffer if we have one
                context->CommitCommandBufferForThread(false, METALWORKQUEUE_GEOMETRY_SHADER);
            }
            
            if (context->GetWorkQueue(METALWORKQUEUE_DEFAULT).commandBuffer != nil) {
                context->CommitCommandBufferForThread(false);
                
                context->EndFrameForThread();
            }
        }
        
        static
        void draw3(std::vector<std::vector<_Worker::VisibleBatch const*>> *drawBatches,
                   HdStRenderPassStateSharedPtr const &renderPassState,
                   HdStResourceRegistrySharedPtr const &resourceRegistry,
                   MTLRenderPassDescriptor *rpd,
                   size_t begin, size_t end)
        {
//            uint64_t timeStart = ArchGetTickTime();
//            int numItems = 0;

            @autoreleasepool{
                MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();
                context->StartFrameForThread();
                context->SetRenderPassDescriptor(rpd);

                if ((end - begin) != 1) {
                    TF_FATAL_CODING_ERROR("We're explicitly expecting one item ");
                }
                for(auto const& batchList : (*drawBatches)[begin]) {
                    HdSt_DrawBatch* batch = batchList->batch;
                    batch->ExecuteDraw(renderPassState, resourceRegistry);
    //                numItems += batchList->numVisible;
                }
                
                if (context->GeometryShadersActive()) {
                    // Complete the GS command buffer if we have one
                    context->CommitCommandBufferForThread(false, METALWORKQUEUE_GEOMETRY_SHADER);
                }
                
                if (context->GetWorkQueue(METALWORKQUEUE_DEFAULT).commandBuffer != nil) {
                    context->CommitCommandBufferForThread(false);
                    
                    context->EndFrameForThread();
                }
            }

//            uint64_t timeDiff = ArchGetTickTime() - timeStart;
//            NSLog(@"Thread time: %.2fms (%d items)",
//                  ArchTicksToNanoseconds(timeDiff) / 1000.0f / 1000.0f, numItems);
        }
    };

    MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();
    MTLRenderPassDescriptor *renderPassDescriptor = context->GetRenderPassDescriptor();
    
    bool mtBatchDrawing = true;

    // Create a new command buffer for each render pass to the current drawable
    if (renderPassDescriptor.colorAttachments[0].loadAction == MTLLoadActionClear) {
        id <MTLCommandBuffer> commandBuffer = context->GetHgi()->GetCommandBuffer();
        int frameNumber = context->GetCurrentFrame();
        [commandBuffer addScheduledHandler:^(id<MTLCommandBuffer> buffer)
         {
            context->GPUTimerStartTimer(frameNumber);
         }];
        [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> buffer)
        {
           context->GPUTimerEndTimer(frameNumber);
        }];

        int numAttachments = 1;
        if (context->GetDrawTarget()) {
            numAttachments = context->GetDrawTarget()->GetAttachments().size();
        }

        if (context->GetHgi()->BeginMtlf()) {
            renderPassDescriptor.depthAttachment.loadAction = MTLLoadActionLoad;
            renderPassDescriptor.stencilAttachment.loadAction = MTLLoadActionLoad;

            for (int i = 0; i < numAttachments; i++) {
                renderPassDescriptor.colorAttachments[i].loadAction = MTLLoadActionLoad;
            }
        }
        else {
            mtBatchDrawing = false;
        }
    }

    uint64_t timeStart = ArchGetTickTime();

    static os_log_t encodingLog = os_log_create("hydra.metal", "Drawing");
    os_signpost_id_t issueEncoding = os_signpost_id_generate(encodingLog);
    
    os_signpost_interval_begin(encodingLog, issueEncoding, "Encoding");
    if (mtBatchDrawing) {
        std::vector<_Worker::VisibleBatch> visibleBatches;
        visibleBatches.reserve(_drawBatches.size());
        for (auto const& batch : _drawBatches) {
            int numVisible = 0;
            for (auto &itemInstance : batch->_drawItemInstances) {
                if (itemInstance->IsVisible()) {
                    numVisible++;
                }
            }
            if (numVisible) {
                HdSt_DrawBatch &b = *batch;
                visibleBatches.push_back({&b, numVisible});
//                NSLog(@"Batch: %d of %lu", numVisible, batch->_drawItemInstances.size());
            }
        }
        
        // sort based on number of drawables
        std::sort(visibleBatches.begin(), visibleBatches.end(),
            [] (_Worker::VisibleBatch const& a, _Worker::VisibleBatch const& b)
                {
                    return a.numVisible > b.numVisible;
                });

//        NSLog(@"Culled from %lu batches to %lu", _drawBatches.size(), visibleBatches.size());
        
        unsigned const systemLimit = MAX(3, WorkGetConcurrencyLimit());
        
        // Limit the number of threads used to render with. Save two threads for the system
        unsigned const maxRenderThreads = MIN(MIN(systemLimit - 2, 6), visibleBatches.size());
        if (maxRenderThreads) {
            WorkSetConcurrencyLimit(maxRenderThreads);

            // Now distribute so that the number of draw instances is more evenly distributed across
            // all the threads
            std::vector<std::vector<_Worker::VisibleBatch const*>> renderOrderedBatches(maxRenderThreads);
            int index = 0;
            int step = 1;

            for (auto const& batch : visibleBatches) {
                renderOrderedBatches[index].push_back(&batch);
                index += step;
                if (index == -1) {
                    index++;
                    step = 1;
                }
                else if (index == maxRenderThreads) {
                    index--;
                    step = -1;
                }
            }

//            WorkParallelForN(_drawBatches.size(),
//                             std::bind(&_Worker::draw, &_drawBatches,
//                                       std::cref(renderPassState),
//                                       std::cref(resourceRegistry),
//                                       std::ref(renderPassDescriptor),
//                                       std::placeholders::_1,
//                                       std::placeholders::_2));
            
//            WorkParallelForN(visibleBatches.size(),
//                             std::bind(&_Worker::draw2, &visibleBatches,
//                                       std::cref(renderPassState),
//                                       std::cref(resourceRegistry),
//                                       std::ref(renderPassDescriptor),
//                                       std::placeholders::_1,
//                                       std::placeholders::_2));

            WorkParallelForN(maxRenderThreads,
                             std::bind(&_Worker::draw3, &renderOrderedBatches,
                                       std::cref(renderPassState),
                                       std::cref(resourceRegistry),
                                       std::ref(renderPassDescriptor),
                                       std::placeholders::_1,
                                       std::placeholders::_2));

            WorkSetConcurrencyLimit(systemLimit);
        }
    } else {
        _Worker::draw(&_drawBatches,
                      renderPassState,
                      resourceRegistry,
                      renderPassDescriptor,
                      0,
                      _drawBatches.size());
    }
    os_signpost_interval_end(encodingLog, issueEncoding, "Encoding");

//    uint64_t timeDiff = ArchGetTickTime() - timeStart;
//    static std::unordered_map<HdStCommandBuffer*, uint64_t> timings;
//    auto search = timings.find(this);
//    uint64_t fastest;
//    if(search == timings.end()) {
//        timings.insert(std::make_pair(this, timeDiff));
//        fastest = timeDiff;
//    }
//    else {
//        search->second = std::min(search->second, timeDiff);
//        fastest = search->second;
//    }
//    NSLog(@"HdStCommandBuffer::ExecuteDraw: %.2fms (%.2fms fastest)",
//          ArchTicksToNanoseconds(timeDiff) / 1000.0f / 1000.0f,
//          ArchTicksToNanoseconds(fastest) / 1000.0f / 1000.0f);

    HD_PERF_COUNTER_SET(HdPerfTokens->drawBatches, _drawBatches.size());
}

void
HdStCommandBuffer::SwapDrawItems(std::vector<HdStDrawItem const*>* items,
                               unsigned currentBatchVersion)
{
    _drawItems.swap(*items);
    _RebuildDrawBatches();
    _batchVersion = currentBatchVersion;
}

void
HdStCommandBuffer::RebuildDrawBatchesIfNeeded(unsigned currentBatchVersion)
{
    HD_TRACE_FUNCTION();

    bool deepValidation = (currentBatchVersion != _batchVersion);
    _batchVersion = currentBatchVersion;
    
    // Force rebuild of all batches for debugging purposes. This helps quickly
    // triage issues wherein the command buffer wasn't updated correctly.
    bool rebuildAllDrawBatches =
        TfDebug::IsEnabled(HDST_FORCE_DRAW_BATCH_REBUILD);

    if (ARCH_LIKELY(!rebuildAllDrawBatches)) {
        for (auto const& batch : _drawBatches) {
            // Validate checks if the batch is referring to up-to-date
            // buffer arrays (via a cheap version number hash check).
            // If deepValidation is set, we loop over the draw items to check
            // if they can be aggregated. If these checks fail, we need to
            // rebuild the batch.
            bool needToRebuildBatch = !batch->Validate(deepValidation);
            if (needToRebuildBatch) {
                // Attempt to rebuild the batch. If that fails, we use a big
                // hammer and rebuilt ALL batches.
                bool rebuildSuccess = batch->Rebuild();
                if (!rebuildSuccess) {
                    rebuildAllDrawBatches = true;
                    break;
                }
            }
        }
    }

    if (rebuildAllDrawBatches) {
        _RebuildDrawBatches();
    }   
}

void
HdStCommandBuffer::_RebuildDrawBatches()
{
    HD_TRACE_FUNCTION();

    TF_DEBUG(HDST_DRAW_BATCH).Msg(
        "Rebuilding all draw batches for command buffer %p ...\n", (void*)this);

    _visibleSize = 0;

    _drawBatches.clear();
    _drawItemInstances.clear();
    _drawItemInstances.reserve(_drawItems.size());

    HD_PERF_COUNTER_INCR(HdPerfTokens->rebuildBatches);

    bool const bindlessTexture =
        GarchResourceFactory::GetInstance()->
            GetContextCaps().bindlessTextureEnabled;

    // Use a cheap bucketing strategy to reduce to number of comparison tests
    // required to figure out if a draw item can be batched.
    // We use a hash of the geometric shader, BAR version and (optionally)
    // material params as the key, and test (in the worst case) against each of 
    // the batches for the key.
    // Test against the previous draw item's hash and batch prior to looking up
    // the map.
    struct _PrevBatchHit {
        _PrevBatchHit() : key(0) {}
        void Update(size_t _key, HdSt_DrawBatchSharedPtr &_batch) {
            key = _key;
            batch = _batch;
        }
        size_t key;
        HdSt_DrawBatchSharedPtr batch;
    };
    _PrevBatchHit prevBatch;
    
    using _DrawBatchMap = 
        std::unordered_map<size_t, HdSt_DrawBatchSharedPtrVector>;
    _DrawBatchMap batchMap;

    for (size_t i = 0; i < _drawItems.size(); i++) {
        HdStDrawItem const * drawItem = _drawItems[i];

        if (!TF_VERIFY(drawItem->GetGeometricShader(), "%s",
                       drawItem->GetRprimID().GetText()) ||
            !TF_VERIFY(drawItem->GetMaterialShader(), "%s",
                       drawItem->GetRprimID().GetText())) {
            continue;
        }

        _drawItemInstances.push_back(HdStDrawItemInstance(drawItem));
        HdStDrawItemInstance* drawItemInstance = &_drawItemInstances.back();

        size_t key = drawItem->GetGeometricShader()->ComputeHash();
        boost::hash_combine(key, drawItem->GetBufferArraysHash());
        if (!bindlessTexture) {
            // Geometric, RenderPass and Lighting shaders should never break
            // batches, however materials can. We consider the textures
            // used by the material to be part of the batch key for that
            // reason.
            // Since textures can be animated and thus materials can be batched
            // at some times but not other times, we use the texture prim path
            // for the hash which does not vary over time.
            // 
            boost::hash_combine(
                key, drawItem->GetMaterialShader()->ComputeTextureSourceHash());
        }

        // Do a quick check to see if the draw item can be batched with the
        // previous draw item, before checking the batchMap.
        if (key == prevBatch.key && prevBatch.batch) {
            if (prevBatch.batch->Append(drawItemInstance)) {
                continue;
            }
        }

        _DrawBatchMap::iterator const batchIter = batchMap.find(key);
        bool const foundKey = batchIter != batchMap.end();
        bool batched = false;
        if (foundKey) {
            HdSt_DrawBatchSharedPtrVector &batches = batchIter->second;
            for (HdSt_DrawBatchSharedPtr &batch : batches) {
                if (batch->Append(drawItemInstance)) {
                    batched = true;
                    prevBatch.Update(key, batch);
                    break;
                }
            }
        }

        if (!batched) {
            HdSt_DrawBatchSharedPtr batch = _NewDrawBatch(drawItemInstance);
            _drawBatches.emplace_back(batch);
            prevBatch.Update(key, batch);

            if (foundKey) {
                HdSt_DrawBatchSharedPtrVector &batches = batchIter->second;
                batches.emplace_back(batch);
            } else {
                batchMap[key] = HdSt_DrawBatchSharedPtrVector({batch});
            }
        }
    }
    TF_DEBUG(HDST_DRAW_BATCH).Msg(
        "   %lu draw batches created for %lu draw items\n", _drawBatches.size(),
		_drawItems.size());
    bvh.BuildBVH(&_drawItemInstances);
}

void
HdStCommandBuffer::SyncDrawItemVisibility(unsigned visChangeCount)
{
    HD_TRACE_FUNCTION();

    if (_visChangeCount == visChangeCount) {
        // There were no changes to visibility since the last time sync was
        // called, no need to re-sync now. Note that visChangeCount starts at
        // 0 in the class and starts at 1 in the change tracker, which ensures a
        // sync after contruction.
        return;
    }
    static os_log_t visSyncLog = os_log_create("hydra.metal", "VisibilitySync");
    os_signpost_id_t visSync = os_signpost_id_generate(visSyncLog);
    
    os_signpost_interval_begin(visSyncLog, visSync, "Visibility Sync");

    _visibleSize = 0;
    int const N = 10000;
    tbb::enumerable_thread_specific<size_t> visCounts;

    WorkParallelForN(_drawItemInstances.size()/N+1,
      [&visCounts, this, N](size_t start, size_t end) {
        TRACE_SCOPE("SetVis");
        start *= N;
        end = std::min(end*N, _drawItemInstances.size());
        size_t& count = visCounts.local();
        for (size_t i = start; i < end; ++i) {
            HdStDrawItem const* item = _drawItemInstances[i].GetDrawItem();

            bool visible = item->GetVisible();
            // DrawItemInstance->SetVisible is not only an inline function but
            // also internally calling virtual HdDrawBatch
            // DrawItemInstanceChanged.  shortcut by looking IsVisible(), which
            // is inline, if it's not actually changing.

            // however, if this is an instancing prim and visible, it always has
            // to be called since instanceCount may changes over time.
            if ((_drawItemInstances[i].IsVisible() != visible) || 
                (visible && item->HasInstancer())) {
                _drawItemInstances[i].SetVisible(visible);
            }
            if (visible) {
                ++count;
            }
        }
    });

    for (size_t i : visCounts) {
        _visibleSize += i;
    }
    os_signpost_interval_end(visSyncLog, visSync, "Visibility Sync");

    // Mark visible state as clean;
    _visChangeCount = visChangeCount;
}

static std::atomic_ulong primCount;
void
HdStCommandBuffer::FrustumCull(
    GfMatrix4d const &viewProjMatrix,
    float renderTargetWidth,
    float renderTargetHeight)
{
    HD_TRACE_FUNCTION();
    
    const bool
    skipCull = false;
    
    if (skipCull) {
        return;
    }
    
    const bool
    mtCullingDisabled = TfDebug::IsEnabled(HDST_DISABLE_MULTITHREADED_CULLING);

    primCount.store(0);
    
    static vector_float2 dimensions;

    // Temp workaround for selection rendertargets being small, and small object
    // culling resulting in object selection not working
    if (renderTargetWidth <= 256 && renderTargetHeight <= 256) {
        renderTargetWidth = 2048;
        renderTargetHeight = 2048;
    }
    dimensions.x = 4.0f / renderTargetWidth;
    dimensions.y = 4.0f / renderTargetHeight;
    
    MtlfMetalContext::GetMetalContext()->PrepareBufferFlush();
    
    struct _Worker {
        static
        void cull(std::vector<HdStDrawItemInstance> * drawItemInstances,
                matrix_float4x4 const &viewProjMatrix,
                size_t begin, size_t end) 
        {
    
            // Count primitives for marketing purposes!
            if (false)
            {
                int numIndicesPerPrimitive = 3;
                
                for(size_t i = begin; i < end; i++) {
                    HdStDrawItemInstance& itemInstance = (*drawItemInstances)[i];
                    HdStDrawItem const* item = itemInstance.GetDrawItem();
                    
                    item->CountPrimitives(primCount, numIndicesPerPrimitive);
                }
            }
            
            for(size_t i = begin; i < end; i++) {
                HdStDrawItemInstance& itemInstance = (*drawItemInstances)[i];
                HdStDrawItem const* item = itemInstance.GetDrawItem();
                bool visible = item->GetVisible() &&
                    item->IntersectsViewVolume(viewProjMatrix, dimensions);
                if ((itemInstance.IsVisible() != visible) || 
                    (visible && item->HasInstancer())) {
                    itemInstance.SetVisible(visible);
                }
            }
        }
    };

    
    GfMatrix4f viewProjMatrixf(viewProjMatrix);
    matrix_float4x4 simdViewProjMatrix =
        matrix_from_columns((vector_float4){viewProjMatrixf[0][0], viewProjMatrixf[0][1], viewProjMatrixf[0][2], viewProjMatrixf[0][3]},
                            (vector_float4){viewProjMatrixf[1][0], viewProjMatrixf[1][1], viewProjMatrixf[1][2], viewProjMatrixf[1][3]},
                            (vector_float4){viewProjMatrixf[2][0], viewProjMatrixf[2][1], viewProjMatrixf[2][2], viewProjMatrixf[2][3]},
                            (vector_float4){viewProjMatrixf[3][0], viewProjMatrixf[3][1], viewProjMatrixf[3][2], viewProjMatrixf[3][3]});


    if (!bvh.populated)
    {
        bvh.BuildBVH(&_drawItemInstances);
    }

    uint64_t timeStart = ArchGetTickTime();


    bvh.PerformCulling(simdViewProjMatrix, dimensions);
    
    MtlfMetalContext::GetMetalContext()->FlushBuffers();

    uint64_t timeDiff = ArchGetTickTime() - timeStart;
    
    static uint64_t fastestTime = 0xffffffffffffffff;

//    fastestTime = std::min(fastestTime, timeDiff);
//    NSLog(@"HdStCommandBuffer::FrustumCull: %.2fms (%.2fms fastest)",
//          ArchTicksToNanoseconds(timeDiff) / 1000.0f / 1000.0f,
//          ArchTicksToNanoseconds(fastestTime) / 1000.0f / 1000.0f);

    if (primCount.load()) {
        NSLog(@"Scene prims: %lu", primCount.load());
    }
    
    _visibleSize = 0;
//    for (auto const& instance : _drawItemInstances) {
//        if (instance.IsVisible()) {
//            ++_visibleSize;
//        }
//    }
}

void
HdStCommandBuffer::SetEnableTinyPrimCulling(bool tinyPrimCulling)
{
    for(auto const& batch : _drawBatches) {
        batch->SetEnableTinyPrimCulling(tinyPrimCulling);
    }
}

PXR_NAMESPACE_CLOSE_SCOPE

