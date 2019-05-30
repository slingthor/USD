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
#include "pxr/imaging/hdSt/spatialHierarchy.h"

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
#include <stack>
#include <algorithm>

PXR_NAMESPACE_OPEN_SCOPE

static os_log_t cullingLog = os_log_create("hydra.metal", "Culling");


const unsigned maxOctreeDepth = 1024;
const unsigned maxPerLevel = 25;

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

Interval::Interval(size_t start, size_t end, bool visible)
: start(start), end(end), visible(visible) {
    // nothing
}

Interval::Interval(OctreeNode* node, bool visible)
: Interval(node->index, node->indexEnd, visible) {
    // nothing
}

DrawableItem::DrawableItem(HdStDrawItemInstance* itemInstance, GfRange3f boundingBox, size_t instanceIndex, size_t totalInstancers)
: itemInstance(itemInstance)
, aabb(boundingBox)
, visible(false)
, isInstanced(true)
, instanceIdx(instanceIndex)
, numItemsInInstance(totalInstancers)
{
    halfSize = aabb.GetSize() * 0.5;
}

DrawableItem::DrawableItem(HdStDrawItemInstance* itemInstance, GfRange3f boundingBox)
: DrawableItem(itemInstance, boundingBox, 0, 1)
{
    isInstanced = false;
}

void DrawableItem::SetVisible(bool visible)
{
    if (isInstanced) {
        itemInstance->GetDrawItem()->SetInstanceVisibility(instanceIdx, visible);
    } else {
        if (itemInstance->IsVisible() != visible) {
            itemInstance->SetVisible(visible);
        }
        itemInstance->GetDrawItem()->SetNumVisible(numItemsInInstance);
    }
}

void DrawableItem::ProcessInstancesVisible()
{
    if (isInstanced) {
        itemInstance->GetDrawItem()->SetNumVisible(numItemsInInstance);
        itemInstance->GetDrawItem()->BuildInstanceBuffer();

        bool shouldBeVisible = itemInstance->GetDrawItem()->AnyInstanceVisible();
        if (itemInstance->IsVisible() != shouldBeVisible) {
            itemInstance->SetVisible(shouldBeVisible);
        }
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
#if 1
            // NOTE: create an item per instance
            for (size_t idx = 0; idx < instancedCullingBounds->size(); ++idx) {
                GfRange3f bbox = (*instancedCullingBounds)[idx].ComputeAlignedRange();
                boundingBox.ExtendBy(bbox);
                items->push_back(new DrawableItem(drawable, bbox, idx, instancedCullingBounds->size()));
            }
#else
            // NOTE: create an item for all instances ... that's equal to the 'naïve approach'
            GfRange3f itemBBox;
            for (size_t idx = 0; idx < instancedCullingBounds->size(); ++idx) {
                GfRange3f bbox = (*instancedCullingBounds)[idx].ComputeAlignedRange();
                itemBBox.ExtendBy(bbox);
            }
            DrawableItem* item = new DrawableItem(drawable, itemBBox);
            item->numItemsInInstance = instancedCullingBounds->size();
            items->push_back(item);
            boundingBox.ExtendBy(itemBBox);
#endif
            
        } else {
            GfRange3f bbox = drawable->GetDrawItem()->GetBounds().ComputeAlignedRange();
            DrawableItem* drawableItem = new DrawableItem(drawable, bbox);
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
    os_signpost_id_t bvhBake = os_signpost_id_generate(cullingLog);

    NSLog(@"Building BVH for %zu HdStDrawItemInstance(s), %i", drawables->size(), BVHCounter);
    if (drawables->size() <= 0) {
        return;
    }
    
    os_signpost_interval_begin(cullingLog, bvhGenerate, "BVH Generation");
    drawableItems.clear();
    instancedDrawableItems.clear();
    
    uint64_t buildStart = ArchGetTickTime();
    
    GfRange3f bbox = DrawableItem::ConvertDrawablesToItems(drawables, &(this->drawableItems));

    if (drawableItems.size() <= 0) {
        return;
    }

    root.ReInit(bbox, &drawableItems);
    root.name = @"0";
    
    unsigned depth = 0;
    size_t drawableItemsCount = drawableItems.size();
    for (size_t idx=0; idx < drawableItemsCount; ++idx)
    {
        unsigned currentDepth = root.Insert(drawableItems[idx]);
        depth = MAX(depth, currentDepth);
        
        if (drawableItems[idx]->isInstanced && drawableItems[idx]->instanceIdx == 0) {
            instancedDrawableItems.push_back(drawableItems[idx]);
        }
    }
    os_signpost_interval_end(cullingLog, bvhGenerate, "BVH Generation");
    
    os_signpost_interval_begin(cullingLog, bvhBake, "BVH Bake");
    Bake();
    os_signpost_interval_end(cullingLog, bvhBake, "BVH Bake");

    buildTimeMS = (ArchGetTickTime() - buildStart) / 1000.0f;
    
    populated = true;
    
    NSLog(@"Building BVH done: MaxDepth=%u, %fms, %zu items", depth, buildTimeMS, drawableItems.size());
}

void BVH::Bake()
{
    root.CalcSubtreeItems();

    bakedDrawableItems.resize(drawableItems.size());
    
    size_t index = 0;
    root.WriteToList(index, &bakedDrawableItems);
}

void BVH::PerformCulling(matrix_float4x4 const &viewProjMatrix, vector_float2 const &dimensions)
{
    // NOTE: this is a hack to not have twice the same octree...
    if (BVHCounter > 2) {
        return;
    }

    os_signpost_id_t bvhCulling = os_signpost_id_generate(cullingLog);
    os_signpost_id_t bvhCullingCull = os_signpost_id_generate(cullingLog);
    os_signpost_id_t bvhCullingFinal = os_signpost_id_generate(cullingLog);
    os_signpost_id_t bvhCullingBuildBuffer = os_signpost_id_generate(cullingLog);

    uint64_t cullStart = ArchGetTickTime();

    os_signpost_interval_begin(cullingLog, bvhCulling, "Culling: BVH");
    os_signpost_interval_begin(cullingLog, bvhCullingCull, "Culling: BVH -- Culllist");
    std::list<Interval> visibleSubtreesList = root.PerformCulling(viewProjMatrix, dimensions);
    os_signpost_interval_end(cullingLog, bvhCullingCull, "Culling: BVH -- Culllist");

    os_signpost_interval_begin(cullingLog, bvhCullingFinal, "Culling: BVH -- Apply");

   static std::vector<DrawableItem*>* octreeHeap = &this->bakedDrawableItems;
    octreeHeap = &this->bakedDrawableItems;
    
    struct _Worker {
        static
        void setAnyInstanceInvisible(std::vector<DrawableItem*> *instancedDrawableItems, size_t begin, size_t end)
        {
            for (size_t idx=begin; idx < end; ++idx) {
                (*instancedDrawableItems)[idx]->itemInstance->GetDrawItem()->SetAnyInstanceVisible(false);
            }
        }
        
        static
        void setIntervals(std::vector<Interval> *intervals, size_t begin, size_t end)
        {
            for(size_t idx = begin; idx < end; ++idx) {
                Interval &interval = (*intervals)[idx];
                for (size_t diIdx = interval.start; diIdx < interval.end; ++diIdx) {
                    assert((*octreeHeap)[diIdx] != NULL);
                    (*octreeHeap)[diIdx]->SetVisible(interval.visible);
                }
            }
        }
        
        static
        void processInstancesVisible(std::vector<DrawableItem*> *instancedDrawableItems, size_t begin, size_t end)
        {
            for (size_t idx=begin; idx < end; ++idx) {
                (*instancedDrawableItems)[idx]->ProcessInstancesVisible();
            }
        }
    };

    std::vector<Interval> intervals {
        std::make_move_iterator(std::begin(visibleSubtreesList)),
        std::make_move_iterator(std::end(visibleSubtreesList))
    };
    std::sort(intervals.begin(), intervals.end(), Interval::compare);
    
    size_t minInterval = 0;
    size_t count = intervals.size();
    for (size_t idx=0; idx < count; ++idx) {
        Interval &interval = intervals[idx];
        if (interval.start > minInterval) {
            intervals.push_back(Interval(minInterval, interval.start, false));
        }
        minInterval = intervals[idx].end;
    }
    if (minInterval < bakedDrawableItems.size()) {
        intervals.push_back(Interval(minInterval, bakedDrawableItems.size(), false));
    }
    
    unsigned instacedGrain = 100;
    WorkParallelForN(instancedDrawableItems.size(),
                     std::bind(&_Worker::setAnyInstanceInvisible, &instancedDrawableItems,
                               std::placeholders::_1,
                               std::placeholders::_2),
                     instacedGrain);
    
    WorkParallelForN(intervals.size(),
                     std::bind(&_Worker::setIntervals, &intervals,
                               std::placeholders::_1,
                               std::placeholders::_2));

    os_signpost_interval_begin(cullingLog, bvhCullingBuildBuffer, "Culling: BVH -- Build Buffer");
    WorkParallelForN(instancedDrawableItems.size(),
                     std::bind(&_Worker::processInstancesVisible, &instancedDrawableItems,
                               std::placeholders::_1,
                               std::placeholders::_2),
                     instacedGrain);
    os_signpost_interval_end(cullingLog, bvhCullingBuildBuffer, "Culling: BVH -- Build Buffer");

    os_signpost_interval_end(cullingLog, bvhCullingFinal, "Culling: BVH -- Apply");

    os_signpost_interval_end(cullingLog, bvhCulling, "Culling: BVH");

    lastCullTimeMS = (ArchGetTickTime() - cullStart) / 1000.0f;
}

OctreeNode::OctreeNode(float minX, float minY, float minZ, float maxX, float maxY, float maxZ, unsigned currentDepth)
: aabb(GfRange3f(GfVec3f(minX, minY, minZ), GfVec3f(maxX, maxY, maxZ)))
, minVec(minX, minY, minZ)
, maxVec(maxX, maxY, maxZ)
, halfSize((maxX - minX) * 0.5, (maxY - minY) * 0.5, (maxZ - minZ) * 0.5)
, index(0)
, indexEnd(0)
, itemCount(0)
, totalItemCount(0)
, isSplit(false)
, depth(currentDepth)
{
    // do nothing
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

std::list<Interval> OctreeNode::PerformCulling(matrix_float4x4 const &viewProjMatrix, vector_float2 const &dimensions)
{
    std::list<Interval> result;
    if (!GfFrustum::IntersectsViewVolumeFloat(aabb, viewProjMatrix, dimensions)) {
        return result;
    }
    
    if (MissingFunctions::FrustumFullyContains(this, viewProjMatrix, dimensions)) {
        if (totalItemCount > 0) {
            result.push_back(Interval(this, true));
        }
        return result;
    }

    if (itemCount > 0) {
        result.push_back(Interval(index, index + itemCount, true));
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
    
    children[0]->name = [name stringByAppendingString:@"-0"];
    children[1]->name = [name stringByAppendingString:@"-1"];
    children[2]->name = [name stringByAppendingString:@"-2"];
    children[3]->name = [name stringByAppendingString:@"-3"];
    children[4]->name = [name stringByAppendingString:@"-4"];
    children[5]->name = [name stringByAppendingString:@"-5"];
    children[6]->name = [name stringByAppendingString:@"-6"];
    children[7]->name = [name stringByAppendingString:@"-7"];
    
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
        return depth;
    }
    
    if (drawables.size() + drawablesTooLarge.size() < maxPerLevel) {
        drawables.push_back(drawable);
        return depth;
    }
    
    if (drawablesTooLarge.size() > maxPerLevel) {
        // move the drawables to either 'drawablesTooLarge' or to children
        std::list<DrawableItem*> lst(drawables.begin(), drawables.end());
        
        drawables.clear();
        for (auto drawable : lst) {
            InsertStraight(drawable);
        }
    }

    return InsertStraight(drawable);
}

unsigned OctreeNode::InsertStraight(DrawableItem* drawable) {
    if (!MissingFunctions::IntersectsAllChildren(this, drawable->aabb)) {
        if (!isSplit) {
            subdivide();
        }
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

size_t OctreeNode::CalcSubtreeItems() {
    itemCount = drawables.size() + drawablesTooLarge.size();
    
    size_t res = itemCount;

    if (isSplit) {
        for (size_t idx = 0; idx < 8; ++idx) {
            res += children[idx]->CalcSubtreeItems();
        }
    }
    
    totalItemCount = res;

    return res;
};

void OctreeNode::WriteToList(size_t &pos, std::vector<DrawableItem*> *bakedDrawableItems) {
    index = pos;
    
    for(std::list<DrawableItem*>::const_iterator it = drawablesTooLarge.begin();
        it != drawablesTooLarge.end(); ++it)
    {
        (*bakedDrawableItems)[pos++] = *it;
    }
    
    for(std::list<DrawableItem*>::const_iterator it = drawables.begin();
        it != drawables.end(); ++it)
    {
        (*bakedDrawableItems)[pos++] = *it;
    }

    if (isSplit) {
        for (size_t idx = 0; idx < 8; ++idx) {
            children[idx]->WriteToList(pos, bakedDrawableItems);
        }
    }
    
    indexEnd = pos;
}

PXR_NAMESPACE_CLOSE_SCOPE

