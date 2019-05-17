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

PXR_NAMESPACE_OPEN_SCOPE

#define USE_BVH_FOR_CULLING 1

#ifdef USE_BVH_FOR_CULLING
namespace SpatialHierarchy {
    enum Intersection {
        Inside,
        Outside,
        Intersects
    };
    
    const unsigned maxElementCountPerNode = 10;
    const unsigned maxOctreeDepth = 100;
    const float minOctreeLeafVolume = 0.f;
    
    struct DrawableItem {
        DrawableItem(HdStDrawItemInstance* itemInstance);
        void SetVisible(bool visible) const;

        HdStDrawItemInstance *item;
        GfRange3f aabb;
        GfVec3f halfSize;
    };
    
    class OctreeNode {
    public:
        OctreeNode(float minX, float minY, float minZ, float maxX, float maxY, float maxZ, unsigned currentDepth);
        ~OctreeNode();
        
        void ReInit(GfRange3f const &boundingBox);
        
        unsigned long PerformCulling(matrix_float4x4 const &viewProjMatrix, vector_float2 const &dimensions);
        unsigned long MarkSubtreeVisible(bool visible);
        unsigned Insert(const DrawableItem &drawable);

        void LogStatus(bool recursive);

        GfRange3f aabb;
        GfVec3f minVec;
        GfVec3f maxVec;
        GfVec3f halfSize;

    private:
        void subdivide();
        bool canSubdivide();
        unsigned insertStraight(const DrawableItem &drawable);

        std::vector<const DrawableItem> drawables;
        
        unsigned depth;
        bool isSplit;

        //OctreeNode* parent;
        OctreeNode* children[8] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
    };
    
    class BVH {
    public:
        BVH();
        void BuildBVH(const std::vector<HdStDrawItemInstance*> &drawables);
        unsigned long PerformCulling(matrix_float4x4 const &viewProjMatrix, vector_float2 const &dimensions);
        
        OctreeNode root;
        unsigned long totalItems;
        unsigned long visibleItems;
    };
    
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
        
        bool IntersectsAllChildren(const OctreeNode* node, const GfBBox3f &entity)
        {
            const GfVec3f sizeEntity = entity.GetRange().GetMax() - entity.GetRange().GetMin();
            
            // Better alternative: (calc center) of entity within node, check if (minVec is negative) && (maxVec is positive);
            // Or if needs touch >= 2 children: verify that at least one component is negative and the other positive (minVec, maxVec)
            
            if (allLarger(sizeEntity, node->halfSize)) {
                LogBounds(node->aabb);
                LogBounds(entity);
                return true;
            }
            return false;
        }
        
        Intersection SpatialRelation(const OctreeNode* node, const GfBBox3f &entity)
        {
            const GfVec3f &entityMin = entity.GetRange().GetMin();
            const GfVec3f &entityMax = entity.GetRange().GetMax();
            
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
    : item(itemInstance),
      aabb(itemInstance->GetDrawItem()->GetBounds().GetRange()),
      halfSize(aabb.GetSize() * 0.5)
    {
    }
    
    void DrawableItem::SetVisible(bool visible) const
    {
        item->SetVisible(visible);
    }

    BVH::BVH()
    : root(OctreeNode(0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0))
    {
        // nothing.
    }
    
    void BVH::BuildBVH(const std::vector<HdStDrawItemInstance*> &drawables)
    {
        NSLog(@"Building BVH for %zu items", drawables.size());
        if (drawables.size() <= 0) {
            return;
        }
        
        totalItems = drawables.size();
        
        // calculate the max size
        GfBBox3f bbox = drawables[0]->GetDrawItem()->GetBounds();
        for (auto drawable: drawables) {
            bbox = bbox.Combine(bbox, drawable->GetDrawItem()->GetBounds());
        }

        root.ReInit(bbox.GetRange());
        
        unsigned depth = 0;
        for (size_t idx=0; idx < drawables.size(); ++idx)
        {
            depth = MAX(depth, root.Insert(drawables[idx]));
        }
        
        //root.LogStatus(true);
        
        NSLog(@"Building BVH done. MaxDepth=%u", depth);
    }
    
    unsigned long BVH::PerformCulling(matrix_float4x4 const &viewProjMatrix, vector_float2 const &dimensions)
    {
        visibleItems = root.PerformCulling(viewProjMatrix, dimensions);
        
        return visibleItems;
    }
    
    OctreeNode::OctreeNode(float minX, float minY, float minZ, float maxX, float maxY, float maxZ, unsigned currentDepth)
    : aabb(GfRange3f(GfVec3f(minX, minY, minZ), GfVec3f(maxX, maxY, maxZ))),
      minVec(minX, minY, minZ),
      maxVec(maxX, maxY, maxZ),
      halfSize((maxX - minX) * 0.5, (maxY - minY) * 0.5, (maxZ - minZ) * 0.5),
      depth(currentDepth),
      isSplit(false)
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
    
    void OctreeNode::ReInit(GfRange3f const &boundingBox)
    {
        MissingFunctions::LogBounds(boundingBox);
        aabb = GfRange3f(boundingBox);
        minVec = aabb.GetMin();
        maxVec = aabb.GetMax();
        halfSize = (maxVec - minVec) * 0.5;
    }
    
    unsigned long OctreeNode::PerformCulling(matrix_float4x4 const &viewProjMatrix, vector_float2 const &dimensions)
    {
        if (MissingFunctions::ShouldRejectBasedOnSize(minVec, maxVec, viewProjMatrix, dimensions)) {
            MarkSubtreeVisible(false);
            return 0;
        }
        
        if (!GfFrustum::IntersectsViewVolumeFloat(aabb, viewProjMatrix, dimensions)) {
            MarkSubtreeVisible(false);
            return 0;
        }
        
        if (MissingFunctions::FrustumFullyContains(this, viewProjMatrix, dimensions)) {
            return MarkSubtreeVisible(true);
        }

        unsigned long visibleCount = drawables.size();
        
        for (auto &drawable : drawables) {
            drawable.SetVisible(true);
        }
        
        if (isSplit) {
            for (auto const &item : children) {
                visibleCount += item->PerformCulling(viewProjMatrix, dimensions);
            }
        }
        
        return visibleCount;
    }
    
    unsigned long OctreeNode::MarkSubtreeVisible(bool visible)
    {
        unsigned long visibleCount = drawables.size();
        
        for (auto &drawable : drawables) {
            drawable.SetVisible(visible);
        }
        
        if (isSplit) {
            for (auto const &item : children) {
                visibleCount += item->MarkSubtreeVisible(visible);
            }
        }
        
        return visibleCount;
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
        return (depth < maxOctreeDepth);// && (aabb.GetVolume() > minOctreeLeafVolume);
    }
    
    unsigned OctreeNode::Insert(const DrawableItem &drawable)
    {
        if (!canSubdivide()) {
//            NSLog(@"Adding: @ %u", depth);
            drawables.push_back(drawable);
            return depth;
        }
        
        if (!isSplit) {
//            NSLog(@"Subdivide @ %u", depth);
            subdivide();
        }
        
        return insertStraight(drawable);
    }
    
    unsigned OctreeNode::insertStraight(const DrawableItem &drawable)
    {
        int intersectsCount = 0;
        
        // TODO: ideally this is a bit vector.
        bool intersects[8] = {false, false, false, false, false, false, false, false};
        //bool foundContainer = false;
        
        if (MissingFunctions::IntersectsAllChildren(this, drawable.aabb)) {
            intersectsCount = 8;
        }
        
        if (intersectsCount <= 0) {
            for (int idx = 0; idx < 8; ++idx) {
                Intersection intersection = MissingFunctions::SpatialRelation(children[idx], drawable.aabb);
                if (Intersection::Inside == intersection) {
    //                NSLog(@"Found containing Container: @ %u::%i", depth, idx);
                    return children[idx]->Insert(drawable);
                }
                intersects[idx] = (Intersection::Intersects == intersection);
                intersectsCount += intersects[idx] ? 1 : 0;
            }
        }
        
        if (intersectsCount >= 8) {
//                NSLog(@"Intersects all 8! @ %u", depth);
            MissingFunctions::IntersectsAllChildren(this, drawable.aabb);
            drawables.push_back(drawable);
            return depth;
        } else {
            /* NOTE: this stores all objects that intersect with more than 1 item at the root
                    ==> fewer tests
                    ==> unique node/item
                    ==> possibly more overdraw
            */
            drawables.push_back(drawable);
            return depth;
            // Test: if an object is only at one tree level, it might be faster to process
//                for (int idx = 0; idx < 8; ++idx) {
//                    if (intersects[idx]) {
////                        NSLog(@"Found intersecting Container: @ %u::%i", depth, idx);
//                        children[idx]->Insert(drawable);
//                        return;
//                    }
//                }
        }
    }

}

#endif

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
#ifdef DEBUG
                context->LabelCommandBuffer(@"Geometry Shaders", METALWORKQUEUE_GEOMETRY_SHADER);
#endif
                //[context->GetWorkQueue(METALWORKQUEUE_GEOMETRY_SHADER).commandBuffer enqueue];
            }

            context->StartFrameForThread();
            
            // Create a new command buffer for each render pass to the current drawable
            context->CreateCommandBuffer(METALWORKQUEUE_DEFAULT);
#ifdef DEBUG
            context->LabelCommandBuffer(@"HdEngine::RenderWorker", METALWORKQUEUE_DEFAULT);
#endif
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
#ifdef DEBUG
        commandBuffer.label = @"Clear";
#endif
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
    static bool didBVH = false;
    static SpatialHierarchy::BVH bvh = SpatialHierarchy::BVH();

    if (!didBVH)
    {
        std::vector<HdStDrawItemInstance*> drawItemInstances;
        drawItemInstances.reserve(_drawItemInstances.size());
        for (size_t idx=0; idx < _drawItemInstances.size(); ++idx) {
            drawItemInstances.push_back(&_drawItemInstances[idx]);
        }
        bvh.BuildBVH(drawItemInstances);
        
        didBVH = true;
    }
#endif
    uint64_t timeStart = ArchGetTickTime();

#ifdef USE_BVH_FOR_CULLING
    bvh.PerformCulling(simdViewProjMatrix, dimensions);
    NSLog(@"Visible: %lu, total: %lu", bvh.visibleItems, bvh.totalItems);
#else
    if (!mtCullingDisabled) {
        WorkParallelForN(_drawItemInstances.size(),
                         std::bind(&_Worker::cull, &_drawItemInstances,
                                   std::cref(simdViewProjMatrix),
                                   std::placeholders::_1,
                                   std::placeholders::_2));
    } else {
        _Worker::cull(&_drawItemInstances,
                      simdViewProjMatrix,
                      0,
                      _drawItemInstances.size());
    }
#endif
    
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

