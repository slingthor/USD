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

float const sizeThreshold = 1.0f;
float const sizeThresholdSq = sizeThreshold * sizeThreshold;

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
    
    bool ShouldRejectBasedOnSize(vector_float4 const* points, matrix_float4x4 const &viewProjMatrix, vector_float2 const &dimensions)
    {
        vector_float4 projectedPoints[] =
        {
            matrix_multiply(viewProjMatrix, points[0]),
            matrix_multiply(viewProjMatrix, points[1])
        };
        
        vector_float2 screenSpace[2];
        
        float inv = 1.0f / projectedPoints[0][3];
        screenSpace[0] = projectedPoints[0].xy * inv;
        
        inv = 1.0f / projectedPoints[1][3];
        screenSpace[1] = projectedPoints[1].xy * inv;
        
        vector_float2 d = vector_abs(screenSpace[1] - screenSpace[0]);
        return (d.x < dimensions.x && d.y < dimensions.y);
    }
    
    bool ShouldRejectBasedOnSize(const GfVec3f& minVec, const GfVec3f& maxVec, matrix_float4x4 const &viewProjMatrix, vector_float2 const &dimensions)
    {
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
        
        vector_float2 d = vector_abs(screenSpace[1] - screenSpace[0]);
        return (d.x < dimensions.x && d.y < dimensions.y);
    }
    
    bool FrustumFullyContains(const OctreeNode* node, matrix_float4x4 const &viewProjMatrix)
    {
        vector_float4 const *points = node->points;
        
        int clipFlags;
        // point[0]
        vector_float4 clipPos = matrix_multiply(viewProjMatrix, *points++);
        clipFlags = ((clipPos.x < clipPos.z) << 3) | ((clipPos.x > -clipPos.z) << 2) |
                    ((clipPos.y < clipPos.z) << 1) | (clipPos.y  > -clipPos.z);
        if (clipFlags != 0xf)
            return false;
        
        // point[1]
        clipPos = matrix_multiply(viewProjMatrix, *points++);
        clipFlags = ((clipPos.x < clipPos.z) << 3) | ((clipPos.x > -clipPos.z) << 2) |
                    ((clipPos.y < clipPos.z) << 1) | (clipPos.y  > -clipPos.z);
        if (clipFlags != 0xf)
            return false;
        
        // point[2]
        clipPos = matrix_multiply(viewProjMatrix, *points++);
        clipFlags = ((clipPos.x < clipPos.z) << 3) | ((clipPos.x > -clipPos.z) << 2) |
                    ((clipPos.y < clipPos.z) << 1) | (clipPos.y  > -clipPos.z);
        if (clipFlags != 0xf)
            return false;
        
        // point[3]
        clipPos = matrix_multiply(viewProjMatrix, *points++);
        clipFlags = ((clipPos.x < clipPos.z) << 3) | ((clipPos.x > -clipPos.z) << 2) |
                    ((clipPos.y < clipPos.z) << 1) | (clipPos.y  > -clipPos.z);
        if (clipFlags != 0xf)
            return false;
        
        // point[4]
        clipPos = matrix_multiply(viewProjMatrix, *points++);
        clipFlags = ((clipPos.x < clipPos.z) << 3) | ((clipPos.x > -clipPos.z) << 2) |
                    ((clipPos.y < clipPos.z) << 1) | (clipPos.y  > -clipPos.z);
        if (clipFlags != 0xf)
            return false;
        
        // point[5]
        clipPos = matrix_multiply(viewProjMatrix, *points++);
        clipFlags = ((clipPos.x < clipPos.z) << 3) | ((clipPos.x > -clipPos.z) << 2) |
                    ((clipPos.y < clipPos.z) << 1) | (clipPos.y  > -clipPos.z);
        if (clipFlags != 0xf)
            return false;
        
        // point[6]
        clipPos = matrix_multiply(viewProjMatrix, *points++);
        clipFlags = ((clipPos.x < clipPos.z) << 3) | ((clipPos.x > -clipPos.z) << 2) |
                    ((clipPos.y < clipPos.z) << 1) | (clipPos.y  > -clipPos.z);
        if (clipFlags != 0xf)
            return false;
        
        // point[7]
        clipPos = matrix_multiply(viewProjMatrix, *points);
        clipFlags = ((clipPos.x < clipPos.z) << 3) | ((clipPos.x > -clipPos.z) << 2) |
                    ((clipPos.y < clipPos.z) << 1) | (clipPos.y  > -clipPos.z);
        if (clipFlags != 0xf)
            return false;

        return true;
    }
};

DrawableItem::DrawableItem(HdStDrawItemInstance* itemInstance, GfRange3f const &aaBoundingBox, size_t instanceIndex, size_t totalInstancers)
: itemInstance(itemInstance)
, aabb(aaBoundingBox)
, isInstanced(true)
, instanceIdx(instanceIndex)
, numItemsInInstance(totalInstancers)
{
    halfSize = aabb.GetSize() * 0.5;
}

DrawableItem::DrawableItem(HdStDrawItemInstance* itemInstance, GfRange3f const &aaBoundingBox)
: DrawableItem(itemInstance, aaBoundingBox, 0, 1)
{
    isInstanced = false;
}

void DrawableItem::ProcessInstancesVisible()
{
    if (isInstanced) {
        if (instanceIdx)
            return;

        itemInstance->GetDrawItem()->BuildInstanceBuffer(itemInstance->GetCullResultVisibilityCache());
    }
    else {
        if (itemInstance->CullResultIsVisible())
            itemInstance->GetDrawItem()->SetNumVisible(1);
        else
            itemInstance->GetDrawItem()->SetNumVisible(0);
    }

    bool shouldBeVisible = itemInstance->GetDrawItem()->GetVisible() &&
        itemInstance->GetDrawItem()->GetNumVisible() > 0;
    if (itemInstance->IsVisible() != shouldBeVisible) {
      itemInstance->SetVisible(shouldBeVisible);
    }
}

GfRange3f DrawableItem::ConvertDrawablesToItems(std::vector<HdStDrawItemInstance> *drawables, std::vector<DrawableItem*> *items)
{
    GfRange3f boundingBox;
    
    for (size_t idx = 0; idx < drawables->size(); ++idx){
        HdStDrawItemInstance* drawable = &(*drawables)[idx];
        drawable->GetDrawItem()->CalculateCullingBounds();

        const std::vector<GfBBox3f>* instancedCullingBounds = drawable->GetDrawItem()->GetInstanceBounds();
        size_t const numItems = instancedCullingBounds->size();
        
        drawable->SetCullResultVisibilityCacheSize(numItems);
        
        if (numItems > 1) {
            // NOTE: create an item per instance
            for (size_t i = 0; i < numItems; ++i) {
                GfRange3f const &oobb = (*instancedCullingBounds)[i].GetRange();
                GfRange3f aabb;

                // We combine the min and max sparately because the range is not really AABB
                // The CalculateCullingBounds bakes the transform in, creating an OOBB.
                // This breakes GfRange3's internals sometimes, one being that IsEmpty() may return
                // true when it isn't?
                aabb.ExtendBy(oobb.GetMin());
                aabb.ExtendBy(oobb.GetMax());

                boundingBox.ExtendBy(aabb);
                items->push_back(new DrawableItem(drawable, aabb, i, numItems));
            }
        } else if (numItems == 1) {
            GfRange3f const &oobb = (*instancedCullingBounds)[0].GetRange();
            GfRange3f aabb;
            
            // We combine the min and max sparately because the range is not really AABB
            // The CalculateCullingBounds bakes the transform in, creating an OOBB.
            // This breakes GfRange3's internals sometimes, one being that IsEmpty() may return
            // true when it isn't?
            aabb.ExtendBy(oobb.GetMin());
            aabb.ExtendBy(oobb.GetMax());
            
            DrawableItem* drawableItem = new DrawableItem(drawable, aabb);
            
            boundingBox.ExtendBy(aabb);
            items->push_back(drawableItem);
        }
    }
    
    return boundingBox;
}

static int BVHCounterX = 0;

BVH::BVH()
: root(OctreeNode(0.f, 0.f, 0.f, 0.f, 0.f, 0.f))
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
    os_signpost_id_t bvhGenerate = os_signpost_id_generate(cullingLog);
    os_signpost_id_t bvhBake = os_signpost_id_generate(cullingLog);

    NSLog(@"Building BVH for %zu HdStDrawItemInstance(s), %i", drawables->size(), BVHCounter);
    if (drawables->size() <= 0) {
        return;
    }
    
    os_signpost_interval_begin(cullingLog, bvhGenerate, "BVH Generation");
    drawableItems.clear();
    
    uint64_t buildStart = ArchGetTickTime();
    
    GfRange3f bbox = DrawableItem::ConvertDrawablesToItems(drawables, &(this->drawableItems));

    root.ReInit(bbox);
    
    unsigned depth = 0;
    size_t drawableItemsCount = drawableItems.size();
    for (size_t idx = 0; idx < drawableItemsCount; ++idx)
    {
        unsigned currentDepth = root.Insert(drawableItems[idx], 0);
        depth = MAX(depth, currentDepth);
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
    bakedVisibility.resize(drawableItems.size());
    cullList.resize(drawableItems.size());

    size_t index = 0;
    root.WriteToList(index, &bakedDrawableItems, &bakedVisibility[0]);
}

void BVH::PerformCulling(matrix_float4x4 const &viewProjMatrix, vector_float2 const &dimensions)
{
    os_signpost_id_t bvhCulling = os_signpost_id_generate(cullingLog);
    os_signpost_id_t bvhCullingCull = os_signpost_id_generate(cullingLog);
    os_signpost_id_t bvhCullingFinal = os_signpost_id_generate(cullingLog);
    os_signpost_id_t bvhCullingBuildBuffer = os_signpost_id_generate(cullingLog);

    uint64_t cullStart = ArchGetTickTime();

    os_signpost_interval_begin(cullingLog, bvhCulling, "Culling: BVH");
    os_signpost_interval_begin(cullingLog, bvhCullingCull, "Culling: BVH -- Culllist");
    cullList.clear();
    root.PerformCulling(viewProjMatrix, dimensions, &bakedVisibility[0], cullList, false);
    os_signpost_interval_end(cullingLog, bvhCullingCull, "Culling: BVH -- Culllist");
    float cullListTimeMS = (ArchGetTickTime() - cullStart) / 1000.0f;
    
    static matrix_float4x4 const *_viewProjMatrix;
    static vector_float2 const *_dimensions;
    
    _viewProjMatrix = &viewProjMatrix;
    _dimensions = &dimensions;
    
    struct _Worker {
        static
        void processInstancesVisible(std::vector<DrawableItem*> *instancedDrawableItems, size_t begin, size_t end)
        {
            for (size_t idx = begin; idx < end; ++idx) {
                (*instancedDrawableItems)[idx]->ProcessInstancesVisible();
            }
        }
        
        static
        void processApplyContained(std::vector<CullListItem> *cullList, size_t begin, size_t end)
        {
            for (size_t idx = begin; idx < end; ++idx)
            {
                auto &cullItem = (*cullList)[idx];
                uint8_t *visibilityWritePtr = cullItem.visibilityWritePtr;

                for (auto &drawableItem : cullItem.node->drawables) {
                    GfBBox3f const &box = (*drawableItem->itemInstance->GetDrawItem()->GetInstanceBounds())[drawableItem->instanceIdx];

                    bool visible;
                    visible = !MissingFunctions::ShouldRejectBasedOnSize(box.GetRange().GetMin(), box.GetRange().GetMax(), *_viewProjMatrix, *_dimensions);
                    *visibilityWritePtr++ = visible;
                }
            }
        }
        
        static
        void processApplyFrustum(std::vector<CullListItem> *cullList, size_t begin, size_t end)
        {
            for (size_t idx = begin; idx < end; ++idx)
            {
                auto const& cullItem = (*cullList)[idx];
                uint8_t *visibilityWritePtr = cullItem.visibilityWritePtr;
                
                for (auto &drawableItem : cullItem.node->drawables) {
                    GfBBox3f const &box = (*drawableItem->itemInstance->GetDrawItem()->GetInstanceBounds())[drawableItem->instanceIdx];
                    
                    bool visible;
                    visible = GfFrustum::IntersectsViewVolumeFloat(box, *_viewProjMatrix, *_dimensions);
                    *visibilityWritePtr++ = visible;
                }
            }
        }

        static
        void processApplyInvisible(std::vector<CullListItem> *cullList, size_t begin, size_t end)
        {
            for (size_t idx = begin; idx < end; ++idx)
            {
                auto const& cullItem = (*cullList)[idx];
                uint8_t *visibilityWritePtr = cullItem.visibilityWritePtr;
                memset(visibilityWritePtr, 0, cullItem.node->totalItemCount);
            }
        }
    };
    
    unsigned grainApply = 50;
    unsigned grainBuild = 20;

    os_signpost_interval_begin(cullingLog, bvhCullingFinal, "Culling: BVH -- Apply");
    uint64_t cullApplyStart = ArchGetTickTime();

    WorkParallelForN(cullList.perItemContained.size(),
                     std::bind(&_Worker::processApplyContained, &cullList.perItemContained,
                               std::placeholders::_1,
                               std::placeholders::_2));
    WorkParallelForN(cullList.perItemFrustum.size(),
                     std::bind(&_Worker::processApplyFrustum, &cullList.perItemFrustum,
                               std::placeholders::_1,
                               std::placeholders::_2));
    WorkParallelForN(cullList.allItemInvisible.size(),
                     std::bind(&_Worker::processApplyInvisible, &cullList.allItemInvisible,
                               std::placeholders::_1,
                               std::placeholders::_2));

    os_signpost_interval_end(cullingLog, bvhCullingFinal, "Culling: BVH -- Apply");
    float cullApplyTimeMS = (ArchGetTickTime() - cullApplyStart) / 1000.0f;
    
    uint64_t cullBuildBufferTimeBegin = ArchGetTickTime();
    os_signpost_interval_begin(cullingLog, bvhCullingBuildBuffer, "Culling: BVH -- Build Buffer");
    WorkParallelForN(drawableItems.size(),
                     std::bind(&_Worker::processInstancesVisible, &drawableItems,
                               std::placeholders::_1,
                               std::placeholders::_2),
                     grainBuild);
    os_signpost_interval_end(cullingLog, bvhCullingBuildBuffer, "Culling: BVH -- Build Buffer");

    os_signpost_interval_end(cullingLog, bvhCulling, "Culling: BVH");

    uint64_t end = ArchGetTickTime();
    float cullBuildBufferTimeMS = (end - cullBuildBufferTimeBegin) / 1000.0f;
    lastCullTimeMS = (end - cullStart) / 1000.0f;
    
//    NSLog(@"CullList: %.2fms   Apply: %.2fms   BuildBuffer: %.2fms   Total: %.2fms", cullListTimeMS, cullApplyTimeMS, cullBuildBufferTimeMS, lastCullTimeMS);
}

OctreeNode::OctreeNode(float minX, float minY, float minZ, float maxX, float maxY, float maxZ)
: aabb(GfRange3f(GfVec3f(minX, minY, minZ), GfVec3f(maxX, maxY, maxZ)))
, minVec(minX, minY, minZ)
, maxVec(maxX, maxY, maxZ)
, halfSize((maxX - minX) * 0.5, (maxY - minY) * 0.5, (maxZ - minZ) * 0.5)
, index(0)
, indexEnd(0)
, itemCount(0)
, totalItemCount(0)
, lastIntersection(Unspecified)
, isSplit(false)
, numChildren(0)
{
    CalcPoints();
}

OctreeNode::~OctreeNode()
{
    if (isSplit) {
        for (size_t idx = 0; idx < numChildren; ++idx)
        {
            delete children[idx];
        }
    }
}

void OctreeNode::CalcPoints()
{
    points[0] = {minVec[0], minVec[1], minVec[2], 1};
    points[1] = {maxVec[0], maxVec[1], maxVec[2], 1};
    points[2] = {minVec[0], minVec[1], maxVec[2], 1};
    points[3] = {minVec[0], maxVec[1], minVec[2], 1};
    points[4] = {minVec[0], maxVec[1], maxVec[2], 1};
    points[5] = {maxVec[0], minVec[1], minVec[2], 1};
    points[6] = {maxVec[0], minVec[1], maxVec[2], 1};
    points[7] = {maxVec[0], maxVec[1], minVec[2], 1};
}

void OctreeNode::ReInit(GfRange3f const &boundingBox)
{
    aabb = GfRange3f(boundingBox);
    minVec = aabb.GetMin();
    maxVec = aabb.GetMax();
    halfSize = (maxVec - minVec) * 0.5;
    CalcPoints();
}

void OctreeNode::PerformCulling(matrix_float4x4 const &viewProjMatrix,
                                vector_float2 const &dimensions,
                                uint8_t *visibility,
                                CullList &cullList,
                                bool fullyContained)
{
    if (!fullyContained) {
        if (!GfFrustum::IntersectsViewVolumeFloat(aabb, viewProjMatrix, dimensions)) {
            if (totalItemCount) {
                if (lastIntersection != OutsideCull) {
                    cullList.allItemInvisible.push_back({this, visibility + index});
                }
            }
            lastIntersection = OutsideCull;
            return;
        }

        if (MissingFunctions::FrustumFullyContains(this, viewProjMatrix)) {
            fullyContained = true;
        }
    }

    if (fullyContained) {
        if (MissingFunctions::ShouldRejectBasedOnSize(points, viewProjMatrix, dimensions)) {
            if (totalItemCount) {
                if (lastIntersection != InsideCull) {
                    cullList.allItemInvisible.push_back({this, visibility + index});
                }
            }
            lastIntersection = InsideCull;
            return;
        }
    }

    if (itemCount > 0) {
        if (fullyContained)
            cullList.perItemContained.push_back({this, visibility + index});
        else
            cullList.perItemFrustum.push_back({this, visibility + index});
        lastIntersection = InsideTest;
    }
    
    if (isSplit) {
        for (int i = 0; i < numChildren; ++i) {
            children[i]->PerformCulling(viewProjMatrix, dimensions, visibility, cullList, fullyContained);
        }
    }
}

void OctreeNode::subdivide()
{
    const GfVec3f &localMin = aabb.GetMin();
    const GfVec3f &localMax = aabb.GetMax();
    const GfVec3f midPoint = localMin + (localMax - localMin) / 2.f;

    children[0] = new OctreeNode(localMin.data()[0], localMin.data()[1], localMin.data()[2], midPoint.data()[0], midPoint.data()[1], midPoint.data()[2]);
    children[1] = new OctreeNode(midPoint.data()[0], localMin.data()[1], localMin.data()[2], localMax.data()[0], midPoint.data()[1], midPoint.data()[2]);
    children[2] = new OctreeNode(localMin.data()[0], midPoint.data()[1], localMin.data()[2], midPoint.data()[0], localMax.data()[1], midPoint.data()[2]);
    children[3] = new OctreeNode(localMin.data()[0], localMin.data()[1], midPoint.data()[2], midPoint.data()[0], midPoint.data()[1], localMax.data()[2]);
    
    children[4] = new OctreeNode(midPoint.data()[0], midPoint.data()[1], localMin.data()[2], localMax.data()[0], localMax.data()[1], midPoint.data()[2]);
    children[5] = new OctreeNode(midPoint.data()[0], localMin.data()[1], midPoint.data()[2], localMax.data()[0], midPoint.data()[1], localMax.data()[2]);
    children[6] = new OctreeNode(localMin.data()[0], midPoint.data()[1], midPoint.data()[2], midPoint.data()[0], localMax.data()[1], localMax.data()[2]);
    children[7] = new OctreeNode(midPoint.data()[0], midPoint.data()[1], midPoint.data()[2], localMax.data()[0], localMax.data()[1], localMax.data()[2]);
    
    numChildren = 8;

    isSplit = true;
}

unsigned OctreeNode::Insert(DrawableItem* drawable, unsigned currentDepth) {
    if (!MissingFunctions::IntersectsAllChildren(this, drawable->aabb)) {
        if (!isSplit) {
            if ((maxVec - minVec).GetLengthSq() > sizeThresholdSq) {
                subdivide();
            }
        }
        for (int idx = 0; idx < numChildren; ++idx) {
            Intersection intersection = MissingFunctions::SpatialRelation(children[idx], drawable->aabb);
            if (Intersection::Inside == intersection) {
                return children[idx]->Insert(drawable, currentDepth + 1);
            }
        }
    }
    
    drawables.push_back(drawable);

    return currentDepth;
}

size_t OctreeNode::CalcSubtreeItems() {
    itemCount = drawables.size();
    
    size_t res = itemCount;
    
    GfRange3f bbox;
    for(auto drawItem : drawables)
    {
        bbox.ExtendBy(drawItem->aabb);
    }

    if (isSplit) {
        for (int idx = 0; idx < numChildren; ++idx) {
            int subItems = children[idx]->CalcSubtreeItems();
            if (!subItems) {
                // empty node - remove from list
                delete children[idx];
                numChildren--;
                if (idx == 7)
                    children[idx--] = NULL;
                else {
                    children[idx--] = children[numChildren];
                    children[numChildren] = NULL;
                }
            }
            else {
                bbox.ExtendBy(children[idx]->aabb);
            }
            res += subItems;
        }
        if (numChildren == 0) {
            isSplit = false;
        }
    }
    
    ReInit(bbox);
    
    totalItemCount = res;

    return res;
};

void OctreeNode::WriteToList(size_t &pos,
                             std::vector<DrawableItem*> *bakedDrawableItems,
                             uint8_t *bakedVisibility) {
    index = pos;
    
    for(auto drawItem : drawables)
    {
        drawItem->itemInstance->SetCullResultVisibilityCache(bakedVisibility + pos, drawItem->instanceIdx);
        (*bakedDrawableItems)[pos++] = drawItem;
    }

    if (isSplit) {
        for (size_t idx = 0; idx < numChildren; ++idx) {
            children[idx]->WriteToList(pos, bakedDrawableItems, bakedVisibility);
        }
    }
    
    indexEnd = pos;
}

PXR_NAMESPACE_CLOSE_SCOPE

