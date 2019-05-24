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

#include "pxr/imaging/hdSt/commandBuffer.h"
#include "pxr/imaging/hdSt/geometricShader.h"
#include "pxr/imaging/hdSt/immediateDrawBatch.h"
#include "pxr/imaging/hdSt/indirectDrawBatch.h"
#include "pxr/imaging/hdSt/resourceFactory.h"
#include "pxr/imaging/hdSt/resourceRegistry.h"

#include "pxr/imaging/hd/bufferArrayRange.h"
#include "pxr/imaging/hd/debugCodes.h"
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

#include <sys/time.h>

#include <os/signpost.h>
#include <queue>

PXR_NAMESPACE_OPEN_SCOPE

#define USE_BVH_FOR_CULLING 1

static os_log_t cullingLog = os_log_create("hydra.metal", "Culling");

namespace SpatialHierarchy {
    const unsigned maxOctreeDepth = 64;
    const unsigned maxPerLevel = 100;
    
    namespace MissingFunctions {
        
        // TODO: this can be #DEFINED
        bool allLarger(const GfVec3f &lhs, const GfVec3f &rhs)
        {
            return (lhs.data()[0] >= rhs.data()[0]) && (lhs.data()[1] >= rhs.data()[1]) && (lhs.data()[2] >= rhs.data()[2]);
        }
        
        void LogBounds(GfBBox3f bounds)
        {
            NSLog(@"(%f, %f, %f) -- (%f, %f, %f)", bounds.GetRange().GetMin().data()[0], bounds.GetRange().GetMin().data()[1], bounds.GetRange().GetMin().data()[2], bounds.GetRange().GetMax().data()[0], bounds.GetRange().GetMax().data()[1], bounds.GetRange().GetMax().data()[2]);
        }
        
        bool IntersectsAllChildren(const OctreeNode* node, const GfRange3f &entity)
        {
            return (allLarger(entity.GetSize(), node->halfSize));
        }

        bool IntersectsAtLeast2Children(const OctreeNode* node, const GfRange3f &entity)
        {
            const GfVec3f &midPoint = node->aabb.GetMidpoint();
            const GfVec3f &entityMin = entity.GetMin();
            const GfVec3f &entityMax = entity.GetMax();

            return (((entityMin.data()[0] < midPoint.data()[0]) && (entityMax.data()[0] > midPoint.data()[0]))
                    || ((entityMin.data()[1] < midPoint.data()[1]) && (entityMax.data()[1] > midPoint.data()[1]))
                    || ((entityMin.data()[2] < midPoint.data()[2]) && (entityMax.data()[2] > midPoint.data()[2])));
        }

        Intersection SpatialRelation(const OctreeNode* node, const GfRange3f &entity)
        {
            const GfVec3f &entityMin = entity.GetMin();
            const GfVec3f &entityMax = entity.GetMax();
            
            if (allLarger(entityMin, node->minVec) && allLarger(node->maxVec, entityMax)) {
                return Intersection::Inside;
            }
            
            if (allLarger(node->maxVec, entityMin) && allLarger(entityMax, node->minVec)) {
                return Intersection::Intersects;
            }
            
            return Intersection::Outside;
        }
        
        bool ShouldRejectBasedOnSize(const GfVec3f& minVec, const GfVec3f& maxVec, matrix_float4x4 const &viewProjMatrix, vector_float2 const &dimensions)
        {
            float const threshold = 4.0f; // number of pixels in a dimension

            vector_float4 points[] =
            {
                matrix_multiply(viewProjMatrix, (vector_float4){minVec[0], minVec[1], minVec[2], 1}),
                matrix_multiply(viewProjMatrix, (vector_float4){maxVec[0], maxVec[1], maxVec[2], 1})
            };
            
            vector_float2 screenSpace[2];
            
            float inv = 1.0f / points[0][3];
            screenSpace[0] = points[0].xy * inv;
            
            inv = 1.0f / points[1][3];
            screenSpace[1] = points[1].xy * inv;
            
            vector_float2 d = vector_abs((screenSpace[1] - screenSpace[0]) * dimensions);
            return (d.x < threshold && d.y < threshold);
        }
        
        bool FrustumFullyContains(const OctreeNode* node, matrix_float4x4 const &viewProjMatrix, vector_float2 const &dimensions)
        {
            vector_float4 points[] =
            {
                {node->minVec[0], node->minVec[1], node->minVec[2], 1},
                {node->maxVec[0], node->maxVec[1], node->maxVec[2], 1},
                {node->minVec[0], node->minVec[1], node->maxVec[2], 1},
                {node->minVec[0], node->maxVec[1], node->minVec[2], 1},
                {node->minVec[0], node->maxVec[1], node->maxVec[2], 1},
                {node->maxVec[0], node->minVec[1], node->minVec[2], 1},
                {node->maxVec[0], node->minVec[1], node->maxVec[2], 1},
                {node->maxVec[0], node->maxVec[1], node->minVec[2], 1}
            };
            
            for (int i = 0; i < 8; ++i) {
                int clipFlags = 0;
                vector_float4 clipPos;
                
                clipPos = matrix_multiply(viewProjMatrix, points[i]);
                clipFlags |= ((clipPos.x < clipPos.z) << 3) |
                             ((clipPos.x > -clipPos.z) << 2) |
                             ((clipPos.y <  clipPos.z) << 1) |
                              (clipPos.y > -clipPos.z);

                if (clipFlags != 0xf) {
                    return false;
                }
            }
            
            return true;
        }
    };
    
    DrawableItem::DrawableItem(HdStDrawItemInstance* itemInstance)
    : item(itemInstance)
    , visible(false)
    , isInstanced(false)
    , instanceIdx(0)
    {
        aabb = itemInstance->GetDrawItem()->GetBounds().ComputeAlignedRange();
        halfSize = aabb.GetSize() * 0.5;
    }
    
    DrawableItem::DrawableItem(HdStDrawItemInstance* itemInstance, GfRange3f boundingBox, size_t instanceIndex)
    : item(itemInstance)
    , aabb(boundingBox)
    , visible(false)
    , isInstanced(true)
    , instanceIdx(instanceIndex)
    {
        halfSize = aabb.GetSize() * 0.5;
    }
    

    void DrawableItem::SetVisible(bool visible)
    {
        if (isInstanced) {
            // NOTE: this is where the instanced indices can be set - .instanceIdx has it stored.
            if (visible) {
                item->SetVisible(true);
            }
        } else {
            item->SetVisible(visible);
        }
    }
    
    GfRange3f DrawableItem::ConvertDrawablesToItems(std::vector<HdStDrawItemInstance> *drawables, std::vector<DrawableItem*> *items)
    {
        GfRange3f boundingBox;
        
        for (size_t idx=0; idx<drawables->size(); ++idx){
            HdStDrawItemInstance* drawable = &(*drawables)[idx];
            HdBufferArrayRangeSharedPtr const & instanceIndexRange = drawable->GetDrawItem()->GetInstanceIndexRange();
            if (instanceIndexRange) {
                drawable->GetDrawItem()->CalculateInstanceBounds();
                const std::vector<GfBBox3f>* instancedCullingBounds = drawable->GetDrawItem()->GetInstanceBounds();
#if 0
                // NOTE: create an item per instance
                for (size_t idx = 0; idx < instancedCullingBounds->size(); ++idx) {
                    GfRange3f bbox = (*instancedCullingBounds)[idx].ComputeAlignedRange();
                    boundingBox.ExtendBy(bbox);
                    items->push_back(new DrawableItem(drawable, bbox, idx));
                }
#else
                // NOTE: create an item for all instances ... that's equal to the 'naïve approach'
                GfRange3f itemBBox;
                for (size_t idx = 0; idx < instancedCullingBounds->size(); ++idx) {
                    GfRange3f bbox = (*instancedCullingBounds)[idx].ComputeAlignedRange();
                    itemBBox.ExtendBy(bbox);
                }
                items->push_back(new DrawableItem(drawable, itemBBox, 0));
                boundingBox.ExtendBy(itemBBox);
#endif
                
            } else {
                DrawableItem* drawableItem = new DrawableItem(drawable);
                boundingBox.ExtendBy(drawableItem->aabb);
                items->push_back(drawableItem);
            }
        }
        
        return boundingBox;
    }

    static int BVHCounterX = 0;
    
    BVH::BVH()
    : root(OctreeNode(0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0))
    , buildTimeMS(0.f)
    , populated(false)
    {
        // nothing.
        BVHCounter = ++BVHCounterX;
        NSLog(@"BVH Created,%i", BVHCounter);
    }
    
    BVH::~BVH()
    {
        for (size_t idx = 0; idx < drawableItems.size(); ++idx) {
            delete drawableItems[idx];
        }
        NSLog(@"BVH dead,%i", BVHCounter);
    }
    
    void BVH::BuildBVH(std::vector<HdStDrawItemInstance> *drawables)
    {
        // NOTE: this is a hack to not have twice the same octree...
        if (BVHCounter > 2) {
            return;
        }
        os_signpost_id_t bvhGenerate = os_signpost_id_generate(cullingLog);
        
        NSLog(@"Building BVH for %zu HdStDrawItemInstance(s), %i", drawables->size(), BVHCounter);
        if (drawables->size() <= 0) {
            return;
        }
        
        os_signpost_interval_begin(cullingLog, bvhGenerate, "BVH Generation");
        drawableItems.clear();
        
        uint64_t buildStart = ArchGetTickTime();
        
        GfRange3f bbox = DrawableItem::ConvertDrawablesToItems(drawables, &(this->drawableItems));

        if (drawableItems.size() <= 0) {
            return;
        }

        root.ReInit(bbox, &drawableItems);
        
        unsigned depth = 0;
        for (size_t idx=0; idx < drawableItems.size(); ++idx)
        {
            depth = MAX(depth, root.Insert(drawableItems[idx]));
        }
        os_signpost_interval_end(cullingLog, bvhGenerate, "BVH Generation");

        buildTimeMS = (ArchGetTickTime() - buildStart) / 1000.0f;
        
        populated = true;
        
        NSLog(@"Building BVH done: MaxDepth=%u, %fms, %zu items", depth, buildTimeMS, drawableItems.size());
    }
    
    void BVH::PerformCulling(matrix_float4x4 const &viewProjMatrix, vector_float2 const &dimensions)
    {
        // NOTE: this is a hack to not have twice the same octree...
        if (BVHCounter > 2) {
            return;
        }

        os_signpost_id_t bvhCulling = os_signpost_id_generate(cullingLog);
        os_signpost_id_t bvhCullingInit = os_signpost_id_generate(cullingLog);
        os_signpost_id_t bvhCullingCull = os_signpost_id_generate(cullingLog);
        os_signpost_id_t bvhCullingFinal = os_signpost_id_generate(cullingLog);

        uint64_t cullStart = ArchGetTickTime();

        /* Algo
         1) Init: set all items to invisible (multithreaded)
         2) cull:
            - collect all fully visible subtrees
            - set all items to visible
         3) finalize: set all fully visible subtrees to visible (multithreaded)
         */
        
        os_signpost_interval_begin(cullingLog, bvhCulling, "Culling: BVH");
        os_signpost_interval_begin(cullingLog, bvhCullingInit, "Culling: BVH -- Init");
        struct _WorkerInvisible {
            static
            void setInVisible(const std::vector<DrawableItem*> &items, size_t begin, size_t end)
            {
                for(size_t idx = begin; idx < end; ++idx) {
                    items[idx]->item->SetVisible(false);
                }
            }
        };

        int grainSize = MAX(drawableItems.size()/WorkGetConcurrencyLimit(), 8);
        WorkParallelForN(drawableItems.size(),
                         std::bind(&_WorkerInvisible::setInVisible, drawableItems,
                                   std::placeholders::_1,
                                   std::placeholders::_2),
                         grainSize);
        os_signpost_interval_end(cullingLog, bvhCullingInit, "Culling: BVH -- Init");

        os_signpost_interval_begin(cullingLog, bvhCullingCull, "Culling: BVH -- Cull");
        std::list<OctreeNode*> visibleSubtreesList = root.PerformCulling(viewProjMatrix, dimensions);
        os_signpost_interval_end(cullingLog, bvhCullingCull, "Culling: BVH -- Cull");

        os_signpost_interval_begin(cullingLog, bvhCullingFinal, "Culling: BVH -- Final");
        struct _Worker {
            static
            void setVisible(std::vector<OctreeNode*> *nodes, size_t begin, size_t end)
            {
                for(size_t idx = begin; idx < end; ++idx) {
                    for (auto &drawable : (*nodes)[idx]->drawables) {
                        drawable->SetVisible(true);
                    }
                    for (auto &drawable : (*nodes)[idx]->drawablesTooLarge) {
                        drawable->SetVisible(true);
                    }
                    if ((*nodes)[idx]->isSplit) {
                        std::vector<OctreeNode*> children(std::begin((*nodes)[idx]->children), std::end((*nodes)[idx]->children));
                        setVisible(&children, 0, 8);
                    }
                }
            }
        };

        std::vector<OctreeNode*> visibleSubtreesVec{
            std::make_move_iterator(std::begin(visibleSubtreesList)),
            std::make_move_iterator(std::end(visibleSubtreesList))
        };

        WorkParallelForN(visibleSubtreesVec.size(),
                         std::bind(&_Worker::setVisible, &visibleSubtreesVec,
                                   std::placeholders::_1,
                                   std::placeholders::_2),
                         1);
        os_signpost_interval_end(cullingLog, bvhCullingFinal, "Culling: BVH -- Final");

        os_signpost_interval_end(cullingLog, bvhCulling, "Culling: BVH");

        lastCullTimeMS = (ArchGetTickTime() - cullStart) / 1000.0f;
    }
    
    OctreeNode::OctreeNode(float minX, float minY, float minZ, float maxX, float maxY, float maxZ, unsigned currentDepth)
    : aabb(GfRange3f(GfVec3f(minX, minY, minZ), GfVec3f(maxX, maxY, maxZ)))
    , minVec(minX, minY, minZ)
    , maxVec(maxX, maxY, maxZ)
    , halfSize((maxX - minX) * 0.5, (maxY - minY) * 0.5, (maxZ - minZ) * 0.5)
    , isSplit(false)
    , depth(currentDepth)
    {
    }
    
    OctreeNode::~OctreeNode()
    {
        if (isSplit) {
            for (size_t idx=0; idx < 8; ++idx)
            {
                delete children[idx];
            }
        }
    }
    
    void OctreeNode::ReInit(GfRange3f const &boundingBox, std::vector<DrawableItem*> *drawables)
    {
        aabb = GfRange3f(boundingBox);
        minVec = aabb.GetMin();
        maxVec = aabb.GetMax();
        halfSize = (maxVec - minVec) * 0.5;
    }
    
    std::list<OctreeNode*> OctreeNode::PerformCulling(matrix_float4x4 const &viewProjMatrix, vector_float2 const &dimensions)
    {
        std::list<OctreeNode*> result;
        if (!GfFrustum::IntersectsViewVolumeFloat(aabb, viewProjMatrix, dimensions)) {
            return result;
        }
        
        if (MissingFunctions::FrustumFullyContains(this, viewProjMatrix, dimensions)) {
            result.push_back(this);
            return result;
        }

        for (auto &drawable : drawables) {
            // TODO: could test here...
            drawable->SetVisible(true);
        }
        for (auto &drawable : drawablesTooLarge) {
            // TODO: could test here...
            drawable->SetVisible(true);
        }

        if (isSplit) {
            for (int i=0; i < 8; ++i) {
                result.splice(result.end(), children[i]->PerformCulling(viewProjMatrix, dimensions));
            }
        }
        
        return result;
    }

    void OctreeNode::LogStatus(bool recursive)
    {
        NSLog(@"Lvl %u, MustKeep: %zu", depth, drawables.size());
        if (recursive) {
            if (isSplit) {
                for (int idx=0; idx < 8; ++idx) {
                    children[idx]->LogStatus(recursive);
                }
            }
        }
    }
    
    void OctreeNode::subdivide()
    {
        const GfVec3f &localMin = aabb.GetMin();
        const GfVec3f &localMax = aabb.GetMax();
        const GfVec3f midPoint = localMin + (localMax - localMin) / 2.f;

        children[0] = new OctreeNode(localMin.data()[0], localMin.data()[1], localMin.data()[2], midPoint.data()[0], midPoint.data()[1], midPoint.data()[2], depth + 1);
        children[1] = new OctreeNode(midPoint.data()[0], localMin.data()[1], localMin.data()[2], localMax.data()[0], midPoint.data()[1], midPoint.data()[2], depth + 1);
        children[2] = new OctreeNode(localMin.data()[0], midPoint.data()[1], localMin.data()[2], midPoint.data()[0], localMax.data()[1], midPoint.data()[2], depth + 1);
        children[3] = new OctreeNode(localMin.data()[0], localMin.data()[1], midPoint.data()[2], midPoint.data()[0], midPoint.data()[1], localMax.data()[2], depth + 1);
        
        children[4] = new OctreeNode(midPoint.data()[0], midPoint.data()[1], localMin.data()[2], localMax.data()[0], localMax.data()[1], midPoint.data()[2], depth + 1);
        children[5] = new OctreeNode(midPoint.data()[0], localMin.data()[1], midPoint.data()[2], localMax.data()[0], midPoint.data()[1], localMax.data()[2], depth + 1);
        children[6] = new OctreeNode(localMin.data()[0], midPoint.data()[1], midPoint.data()[2], midPoint.data()[0], localMax.data()[1], localMax.data()[2], depth + 1);
        children[7] = new OctreeNode(midPoint.data()[0], midPoint.data()[1], midPoint.data()[2], localMax.data()[0], localMax.data()[1], localMax.data()[2], depth + 1);
        
        isSplit = true;
    }
    
    bool OctreeNode::canSubdivide()
    {
        return (depth < maxOctreeDepth);
    }
    
    unsigned OctreeNode::Insert(DrawableItem* drawable)
    {
        if (!canSubdivide()){
            drawablesTooLarge.push_back(drawable);
            
            if (drawablesTooLarge.size() > maxPerLevel) {
                // move the drawables to either 'drawablesTooLarge' or to children
                std::list<DrawableItem*> lst(drawables.begin(), drawables.end());
                drawables.clear();
                for (auto drawable : lst) {
                    InsertStraight(drawable);
                }
            }
            return depth;
        }
        
        if (drawables.size() + drawablesTooLarge.size() < maxPerLevel) {
            drawables.push_back(drawable);
            return depth;
        }
        
        if (!isSplit) {
            subdivide();
        }
        
        return InsertStraight(drawable);
    }
    
    unsigned OctreeNode::InsertStraight(DrawableItem* drawable) {
        if (!MissingFunctions::IntersectsAllChildren(this, drawable->aabb)) {
            for (int idx = 0; idx < 8; ++idx) {
                Intersection intersection = MissingFunctions::SpatialRelation(children[idx], drawable->aabb);
                if (Intersection::Inside == intersection) {
                    return children[idx]->Insert(drawable);
                }
            }
        }
        
        drawablesTooLarge.push_back(drawable);
        
        return depth;
    }
}

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
            bool foundSomethingVisible = false;
            
            for(size_t i = begin; i < end; i++) {
                HdSt_DrawBatchSharedPtr& batch = (*drawBatches)[i];
                
                for (auto const& instance : batch->_drawItemInstances) {
                    if (instance->IsVisible()) {
                        foundSomethingVisible = true;
                        break;
                    }
                }
            }
            if (!foundSomethingVisible) {
                return;
            }
            
            MtlfMetalContext *context = MtlfMetalContext::GetMetalContext();

            if (!context->GeometryShadersActive()) {
                context->CreateCommandBuffer(METALWORKQUEUE_GEOMETRY_SHADER);
                if (TF_DEV_BUILD) {
                    context->LabelCommandBuffer(@"Geometry Shaders", METALWORKQUEUE_GEOMETRY_SHADER);
                }
                //[context->GetWorkQueue(METALWORKQUEUE_GEOMETRY_SHADER).commandBuffer enqueue];
            }

            context->StartFrameForThread();
            
            // Create a new command buffer for each render pass to the current drawable
            context->CreateCommandBuffer(METALWORKQUEUE_DEFAULT);
            if (TF_DEV_BUILD) {
                context->LabelCommandBuffer(@"HdEngine::RenderWorker", METALWORKQUEUE_DEFAULT);
            }
            context->SetRenderPassDescriptor(rpd);
            
            for(size_t i = begin; i < end; i++) {
                HdSt_DrawBatchSharedPtr& batch = (*drawBatches)[i];
                batch->ExecuteDraw(renderPassState, resourceRegistry);
            }

            if (context->GeometryShadersActive()) {
                // Complete the GS command buffer if we have one
                context->CommitCommandBufferForThread(false, false, METALWORKQUEUE_GEOMETRY_SHADER);
            }

            if (context->GetWorkQueue(METALWORKQUEUE_DEFAULT).commandBuffer != nil) {
                context->CommitCommandBufferForThread(false, false);

                context->EndFrameForThread();
            }
        }
    };
    
    bool setAlpha = false;

    MtlfMetalContext *context = MtlfMetalContext::GetMetalContext();
    MTLRenderPassDescriptor *renderPassDescriptor = context->GetRenderPassDescriptor();
    
    // Create a new command buffer for each render pass to the current drawable
    if (renderPassDescriptor.colorAttachments[0].loadAction == MTLLoadActionClear) {
        id <MTLCommandBuffer> commandBuffer = [context->commandQueue commandBuffer];
        if (TF_DEV_BUILD) {
            commandBuffer.label = @"Clear";
        }
        id <MTLRenderCommandEncoder> renderEncoder =
            [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
        [renderEncoder endEncoding];
        [commandBuffer commit];
     
        int numAttachments = 1;
        
        if (context->GetDrawTarget()) {
            numAttachments = context->GetDrawTarget()->GetAttachments().size();
        }
        else
            setAlpha = true;

        renderPassDescriptor.depthAttachment.loadAction = MTLLoadActionLoad;

        for (int i = 0; i < numAttachments; i++) {
            renderPassDescriptor.colorAttachments[i].loadAction = MTLLoadActionLoad;
        }
    }

    uint64_t timeStart = ArchGetTickTime();
    static bool mtBatchDrawing = _drawBatches.size() >= 10;

    static os_log_t encodingLog = os_log_create("hydra.metal", "Drawing");
    os_signpost_id_t issueEncoding = os_signpost_id_generate(encodingLog);
    
    os_signpost_interval_begin(encodingLog, issueEncoding, "Encoding");
    if (mtBatchDrawing) {
        unsigned const systemLimit = WorkGetConcurrencyLimit();
        
        // Limit the number of threads used to render with
        unsigned const maxRenderThreads = MIN(systemLimit, 12);
        WorkSetConcurrencyLimit(maxRenderThreads);

        unsigned const maxRenderCommandBufferCount = maxRenderThreads * 2;
        unsigned grainSize = MAX(_drawBatches.size() / maxRenderCommandBufferCount, 1);

        WorkParallelForN(_drawBatches.size(),
                         std::bind(&_Worker::draw, &_drawBatches,
                                   std::cref(renderPassState),
                                   std::cref(resourceRegistry),
                                   std::ref(renderPassDescriptor),
                                   std::placeholders::_1,
                                   std::placeholders::_2),
                         grainSize);

        WorkSetConcurrencyLimit(systemLimit);
    } else {
        _Worker::draw(&_drawBatches,
                      renderPassState,
                      resourceRegistry,
                      renderPassDescriptor,
                      0,
                      _drawBatches.size());
    }
    os_signpost_interval_end(encodingLog, issueEncoding, "Encoding");

    if (setAlpha) {
        context->SetAlphaBlendingEnable(true);
//        renderPassDescriptor.colorAttachments[0].blendingEnabled = YES;
//        renderPipelineStateDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorOne;
//        renderPipelineStateDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
//        renderPipelineStateDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOne;
//        renderPipelineStateDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOne;
    }
    
    uint64_t timeDiff = ArchGetTickTime() - timeStart;
    static uint64_t fastestTime = 0xffffffffffffffff;

//    fastestTime = std::min(fastestTime, timeDiff);
//    NSLog(@"HdStCommandBuffer::ExecuteDraw: %.2fms (%.2fms fastest)",
//          ArchTicksToNanoseconds(timeDiff) / 1000.0f / 1000.0f,
//          ArchTicksToNanoseconds(fastestTime) / 1000.0f / 1000.0f);

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
    
    bool deepValidation
        = (currentBatchVersion != _batchVersion);

    for (auto const& batch : _drawBatches) {
        if (!batch->Validate(deepValidation) && !batch->Rebuild()) {
            TRACE_SCOPE("Invalid Batches");
            _RebuildDrawBatches();
            _batchVersion = currentBatchVersion;
            return;
        }
    }
    _batchVersion = currentBatchVersion;
}

void
HdStCommandBuffer::_RebuildDrawBatches()
{
    HD_TRACE_FUNCTION();

    _visibleSize = 0;

    _drawBatches.clear();
    _drawItemInstances.clear();
    _drawItemInstances.reserve(_drawItems.size());

    HD_PERF_COUNTER_INCR(HdPerfTokens->rebuildBatches);

    bool bindlessTexture = GarchResourceFactory::GetInstance()->GetContextCaps()
                                               .bindlessTextureEnabled;

    // XXX: Temporary sorting by shader.
    std::map<size_t, HdSt_DrawBatchSharedPtr> batchMap;

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
            // batches, however materials can. We consider the material 
            // parameters to be part of the batch key here for that reason.
            boost::hash_combine(key, HdMaterialParam::ComputeHash(
                            drawItem->GetMaterialShader()->GetParams()));
        }

        TF_DEBUG(HD_DRAW_BATCH).Msg("%lu (%lu)\n", 
                key, 
                drawItem->GetBufferArraysHash());
                //, drawItem->GetRprimID().GetText());

        HdSt_DrawBatchSharedPtr batch;
        TfMapLookup(batchMap, key, &batch);
        if (!batch || !batch->Append(drawItemInstance)) {
            batch = _NewDrawBatch(drawItemInstance);
            _drawBatches.push_back(batch);
            batchMap[key] = batch;
        }
    }
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
HdStCommandBuffer::FrustumCull(GfMatrix4d const &viewProjMatrix)
{
    HD_TRACE_FUNCTION();
    
    const bool
    mtCullingDisabled = TfDebug::IsEnabled(HD_DISABLE_MULTITHREADED_CULLING);

    primCount.store(0);
    
    static vector_float2 dimensions = {1.f, 1.f};
    dimensions.x = float(MtlfMetalContext::GetMetalContext()->mtlColorTexture.width);
    dimensions.y = float(MtlfMetalContext::GetMetalContext()->mtlColorTexture.height);

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
        
        static
        void cullDispatch(WorkDispatcher &dispatcher,
                HdStDrawItemInstance &itemInstance,
                GfMatrix4f const &viewProjMatrix,
                float const width, float const height)
        {
            HdStDrawItem const* item = itemInstance.GetDrawItem();
            if (item->GetVisible()) {
                item->IntersectsViewVolume(dispatcher,
                                           itemInstance.cullResult,
                                           viewProjMatrix,
                                           width,
                                           height);
            }
        }
    };

    
    GfMatrix4f viewProjMatrixf(viewProjMatrix);
    matrix_float4x4 simdViewProjMatrix = matrix_from_columns((vector_float4){viewProjMatrixf[0][0], viewProjMatrixf[0][1], viewProjMatrixf[0][2], viewProjMatrixf[0][3]},
                                                             (vector_float4){viewProjMatrixf[1][0], viewProjMatrixf[1][1], viewProjMatrixf[1][2], viewProjMatrixf[1][3]},
                                                             (vector_float4){viewProjMatrixf[2][0], viewProjMatrixf[2][1], viewProjMatrixf[2][2], viewProjMatrixf[2][3]},
                                                             (vector_float4){viewProjMatrixf[3][0], viewProjMatrixf[3][1], viewProjMatrixf[3][2], viewProjMatrixf[3][3]});


#ifdef USE_BVH_FOR_CULLING
    if (!bvh.populated)
    {
        bvh.BuildBVH(&_drawItemInstances);
        //bvh.root.LogStatus(true);
    }
#endif
    uint64_t timeStart = ArchGetTickTime();

    // NOTE: here's the switch between the methods.
//#ifndef USE_BVH_FOR_CULLING
    os_signpost_id_t naiveCulling = os_signpost_id_generate(cullingLog);
    os_signpost_interval_begin(cullingLog, naiveCulling, "Culling: Naïve");
//    if (!mtCullingDisabled) {
//        WorkParallelForN(_drawItemInstances.size(),
//                         std::bind(&_Worker::cull, &_drawItemInstances,
//                                   std::cref(simdViewProjMatrix),
//                                   std::placeholders::_1,
//                                   std::placeholders::_2));
//    } else {
//        _Worker::cull(&_drawItemInstances,
//                      simdViewProjMatrix,
//                      0,
//                      _drawItemInstances.size());
//    }
    os_signpost_interval_end(cullingLog, naiveCulling, "Culling: Naïve");
//#else
    bvh.PerformCulling(simdViewProjMatrix, dimensions);
//#endif
    
    
    MtlfMetalContext::GetMetalContext()->FlushBuffers();

    uint64_t timeDiff = ArchGetTickTime() - timeStart;
    
    static uint64_t fastestTime = 0xffffffffffffffff;

//    fastestTime = std::min(fastestTime, timeDiff);
//    NSLog(@"HdStCommandBuffer::FrustumCull: %.2fms (%.2fms fastest) : %lu items",
//          ArchTicksToNanoseconds(timeDiff) / 1000.0f / 1000.0f,
//          ArchTicksToNanoseconds(fastestTime) / 1000.0f / 1000.0f, _drawItemInstances.size());

    if (primCount.load()) {
        NSLog(@"Scene prims: %lu", primCount.load());
    }
    
    _visibleSize = 0;
    for (auto const& instance : _drawItemInstances) {
        if (instance.IsVisible()) {
            ++_visibleSize;
        }
    }
}

void
HdStCommandBuffer::SetEnableTinyPrimCulling(bool tinyPrimCulling)
{
    for(auto const& batch : _drawBatches) {
        batch->SetEnableTinyPrimCulling(tinyPrimCulling);
    }
}

PXR_NAMESPACE_CLOSE_SCOPE

