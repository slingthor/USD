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

#include <queue>
#include <stack>
#include <algorithm>

PXR_NAMESPACE_OPEN_SCOPE

float const sizeThreshold = 1.0f;
float const sizeThresholdSq = sizeThreshold * sizeThreshold;

namespace MissingFunctions {
    
    // TODO: this can be #DEFINED
    bool allLarger(const GfVec3f &lhs, const GfVec3f &rhs)
    {
        return (lhs.data()[0] > rhs.data()[0]) && (lhs.data()[1] > rhs.data()[1]) && (lhs.data()[2] > rhs.data()[2]);
    }
    
    bool firstAllSmallerThanSecond(const GfVec3f &lhs, const GfVec3f &rhs)
    {
        return (lhs.data()[0] < rhs.data()[0]) && (lhs.data()[1] < rhs.data()[1]) && (lhs.data()[2] < rhs.data()[2]);
    }
    
    bool firstAllLargerThanSecond(const GfVec3f &lhs, const GfVec3f &rhs)
    {
        return (lhs.data()[0] > rhs.data()[0]) && (lhs.data()[1] > rhs.data()[1]) && (lhs.data()[2] > rhs.data()[2]);
    }
    
    void LogBounds(GfBBox3f bounds)
    {
        NSLog(@"(%f, %f, %f) -- (%f, %f, %f)", bounds.GetRange().GetMin().data()[0], bounds.GetRange().GetMin().data()[1], bounds.GetRange().GetMin().data()[2], bounds.GetRange().GetMax().data()[0], bounds.GetRange().GetMax().data()[1], bounds.GetRange().GetMax().data()[2]);
    }
    
    bool IntersectsAllChildren(const OctreeNode* node, const GfRange3f &entity)
    {
        const GfVec3f &midPoint = node->aabb.GetMidpoint();
        
        return firstAllSmallerThanSecond(entity.GetMin(), midPoint)
            && firstAllLargerThanSecond(entity.GetMax(), midPoint);
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
    
    bool ShouldRejectBasedOnSize(CullStateCache &cache, matrix_float4x4 const &viewProjMatrix, vector_float2 const &dimensions)
    {
        if (cache.suggestedTestType == CullStateCache::TestSphere) {
            vector_float4 points[] =
            {
                matrix_multiply(viewProjMatrix, cache.points[0]),
                matrix_multiply(viewProjMatrix, cache.points[1]),
            };
            
            vector_float4 screenSpace[2];
            vector_float4 inv = vector_fast_recip((vector_float4){points[0][3], points[1][3], points[2][3], points[3][3]});
            screenSpace[0].xy = points[0].xy * inv.x;
            screenSpace[1].xy = points[1].xy * inv.y;
            screenSpace[0].zw = points[2].xy * inv.z;
            screenSpace[1].zw = points[3].xy * inv.w;
            
            vector_float4 d = vector_abs(screenSpace[1] - screenSpace[0]);
            return (d.x < dimensions.x && d.y < dimensions.y) &&
                   (d.z < dimensions.x && d.w < dimensions.y);
            return true;
        }

        vector_float4 points[] =
        {
            matrix_multiply(viewProjMatrix, cache.points[0]),
            matrix_multiply(viewProjMatrix, cache.points[1]),
            matrix_multiply(viewProjMatrix, cache.points[2]),
            matrix_multiply(viewProjMatrix, cache.points[3])
        };

        vector_float4 screenSpace[2];
        vector_float4 inv = vector_fast_recip((vector_float4){points[0][3], points[1][3], points[2][3], points[3][3]});
        screenSpace[0].xy = points[0].xy * inv.x;
        screenSpace[1].xy = points[1].xy * inv.y;
        screenSpace[0].zw = points[2].xy * inv.z;
        screenSpace[1].zw = points[3].xy * inv.w;
        
        vector_float4 d = vector_abs(screenSpace[1] - screenSpace[0]);
        return (d.x < dimensions.x && d.y < dimensions.y) &&
               (d.z < dimensions.x && d.w < dimensions.y);
    }
    
    bool ShouldRejectBasedOnSize(const GfVec3f& minVec, const GfVec3f& maxVec, matrix_float4x4 const &viewProjMatrix, vector_float2 const &dimensions)
    {
        vector_float4 points[] =
        {
            matrix_multiply(viewProjMatrix, (vector_float4){minVec[0], minVec[1], minVec[2], 1}),
            matrix_multiply(viewProjMatrix, (vector_float4){maxVec[0], maxVec[1], maxVec[2], 1}),
            matrix_multiply(viewProjMatrix, (vector_float4){minVec[0], maxVec[1], minVec[2], 1}),
            matrix_multiply(viewProjMatrix, (vector_float4){maxVec[0], minVec[1], maxVec[2], 1})
        };
        
        vector_float4 screenSpace[2];
        vector_float4 inv = vector_fast_recip((vector_float4){points[0][3], points[1][3], points[2][3], points[3][3]});
        screenSpace[0].xy = points[0].xy * inv.x;
        screenSpace[1].xy = points[1].xy * inv.y;
        screenSpace[0].zw = points[2].xy * inv.z;
        screenSpace[1].zw = points[3].xy * inv.w;

        vector_float4 d = vector_abs(screenSpace[1] - screenSpace[0]);
        return (d.x < dimensions.x && d.y < dimensions.y) &&
               (d.z < dimensions.x && d.w < dimensions.y);
    }
    
    bool FrustumFullyContains(CullStateCache &cache,
                              vector_float4 const *clipPlanes)
    {
        if (cache.suggestedTestType == CullStateCache::TestSphere) {
            vector_float3 mid = cache.mid.xyz;
            float radius = cache.radius;

            for (int p = 0; p < 5; p++, clipPlanes++) {
                float value = vector_dot(clipPlanes->xyz, mid) + clipPlanes->w - radius;
                if (value < 0) {
                    return false;
                }
            }
            return true;
        }

        vector_float4 const *points = cache.points;
        
        float value;
        for (int p = 0; p < 5; p++) {
            int hits = 0;
            value = vector_dot(clipPlanes[p].xyz, points[0].xyz) + clipPlanes[p].w;
            if (value < 0)
                return false;
            value = vector_dot(clipPlanes[p].xyz, points[1].xyz) + clipPlanes[p].w;
            if (value < 0)
                return false;
            value = vector_dot(clipPlanes[p].xyz, points[2].xyz) + clipPlanes[p].w;
            if (value < 0)
                return false;
            value = vector_dot(clipPlanes[p].xyz, points[3].xyz) + clipPlanes[p].w;
            if (value < 0)
                return false;
            value = vector_dot(clipPlanes[p].xyz, points[4].xyz) + clipPlanes[p].w;
            if (value < 0)
                return false;
            value = vector_dot(clipPlanes[p].xyz, points[5].xyz) + clipPlanes[p].w;
            if (value < 0)
                return false;
            value = vector_dot(clipPlanes[p].xyz, points[6].xyz) + clipPlanes[p].w;
            if (value < 0)
                return false;
            value = vector_dot(clipPlanes[p].xyz, points[7].xyz) + clipPlanes[p].w;
            if (value < 0)
                return false;
        }

        return true;
    }
    
    bool IntersectsFrustum(CullStateCache &cache,
                           vector_float4 const *clipPlanes)
    {
        int planeHint = cache.lastCullPlane;

        if (cache.suggestedTestType == CullStateCache::TestSphere) {
            vector_float3 mid = cache.mid.xyz;
            float radius = cache.radius;
            
            if (planeHint >= 0) {
                float value = vector_dot(clipPlanes[planeHint].xyz, mid) + clipPlanes[planeHint].w + radius;
                if (value < 0)
                    return false;
            }
            
            for (int p = 0; p < 5; p++, clipPlanes++) {
                if (p == planeHint)
                    continue;

                float value = vector_dot(clipPlanes->xyz, mid) + clipPlanes->w + radius;
                if (value < 0) {
                    cache.lastCullPlane = p;
                    return false;
                }
            }
            cache.lastCullPlane = -1;
            return true;
        }

        vector_float4 const *points = cache.points;
        
        // Test the plane we hit last time we discarded this object first
        if (planeHint >= 0) {
            float value;
            do {
                value = vector_dot(clipPlanes[planeHint].xyz, points[0].xyz) + clipPlanes[planeHint].w;
                if (value > 0)
                    break;
                value = vector_dot(clipPlanes[planeHint].xyz, points[1].xyz) + clipPlanes[planeHint].w;
                if (value > 0)
                    break;
                value = vector_dot(clipPlanes[planeHint].xyz, points[2].xyz) + clipPlanes[planeHint].w;
                if (value > 0)
                    break;
                value = vector_dot(clipPlanes[planeHint].xyz, points[3].xyz) + clipPlanes[planeHint].w;
                if (value > 0)
                    break;
                value = vector_dot(clipPlanes[planeHint].xyz, points[4].xyz) + clipPlanes[planeHint].w;
                if (value > 0)
                    break;
                value = vector_dot(clipPlanes[planeHint].xyz, points[5].xyz) + clipPlanes[planeHint].w;
                if (value > 0)
                    break;
                value = vector_dot(clipPlanes[planeHint].xyz, points[6].xyz) + clipPlanes[planeHint].w;
                if (value > 0)
                    break;
                value = vector_dot(clipPlanes[planeHint].xyz, points[7].xyz) + clipPlanes[planeHint].w;
                if (value > 0)
                    break;
                return false;
            } while(false);
        }

        // Don't test near - the left/right/top/bottom converge just behind it anyway
        float value;
        for (int p = 0; p < 5; p++, clipPlanes++) {
            if (p == planeHint)
                continue;

            value = vector_dot(clipPlanes->xyz, points[0].xyz) + clipPlanes->w;
            if (value > 0)
                continue;
            value = vector_dot(clipPlanes->xyz, points[1].xyz) + clipPlanes->w;
            if (value > 0)
                continue;
            value = vector_dot(clipPlanes->xyz, points[2].xyz) + clipPlanes->w;
            if (value > 0)
                continue;
            value = vector_dot(clipPlanes->xyz, points[3].xyz) + clipPlanes->w;
            if (value > 0)
                continue;
            value = vector_dot(clipPlanes->xyz, points[4].xyz) + clipPlanes->w;
            if (value > 0)
                continue;
            value = vector_dot(clipPlanes->xyz, points[5].xyz) + clipPlanes->w;
            if (value > 0)
                continue;
            value = vector_dot(clipPlanes->xyz, points[6].xyz) + clipPlanes->w;
            if (value > 0)
                continue;
            value = vector_dot(clipPlanes->xyz, points[7].xyz) + clipPlanes->w;
            if (value > 0)
                continue;
            planeHint = p;
            return false;
        }
        cache.lastCullPlane = -1;

        return true;
    }
};

GfRange3f ConvertDrawablesToItems(std::vector<HdStDrawItemInstance> *drawables,
                                  std::vector<DrawableItem*> *items,
                                  std::vector<DrawableItem*> *visibilityOwners,
                                  std::vector<DrawableAnimatedItem> *animatedDrawables,
                                  size_t &bakedAnimatedVisibilityItemCount)
{
    GfRange3f boundingBox;
    
    bakedAnimatedVisibilityItemCount = 0;
    
    for (size_t idx = 0; idx < drawables->size(); ++idx){
        HdStDrawItemInstance* drawable = &(*drawables)[idx];
        drawable->GetDrawItem()->CalculateCullingBounds(true);
        const std::vector<GfBBox3f>* instancedCullingBounds = drawable->GetDrawItem()->GetInstanceBounds();
        size_t const numItems = instancedCullingBounds->size();

        drawable->SetCullResultVisibilityCacheSize(numItems);
        
        if(drawable->GetDrawItem()->GetAnimated()) {
            if (numItems > 0) {
                animatedDrawables->emplace_back(drawable, bakedAnimatedVisibilityItemCount);
                bakedAnimatedVisibilityItemCount += numItems;
            }
            continue;
        }

        if (numItems > 1) {
            // NOTE: create an item per instance
            for (size_t i = 0; i < numItems; ++i) {
                GfBBox3f const &oobb = (*instancedCullingBounds)[i];
                GfRange3f const &ooRange = oobb.GetRange();
                GfRange3f aabb;

                // We combine the min and max sparately because the range is not really AABB
                // The CalculateCullingBounds bakes the transform in, creating an OOBB.
                // This breakes GfRange3's internals sometimes, one being that IsEmpty() may return
                // true when it isn't.
                aabb.ExtendBy(ooRange.GetMin());
                aabb.ExtendBy(ooRange.GetMax());

                if (aabb.GetMax()[0] != FLT_MAX) {
                    boundingBox.ExtendBy(aabb);
                    DrawableItem *newItem = new DrawableItem(drawable, aabb, oobb, i, numItems);
                    items->push_back(newItem);
                    if (i == 0) {
                        visibilityOwners->push_back(newItem);
                    }
                }
            }
        } else if (numItems == 1) {
            GfBBox3f const &oobb = (*instancedCullingBounds)[0];
            GfRange3f const &ooRange = oobb.GetRange();
            GfRange3f aabb;
            
            // We combine the min and max sparately because the range is not really AABB
            // The CalculateCullingBounds bakes the transform in, creating an OOBB.
            // This breakes GfRange3's internals sometimes, one being that IsEmpty() may return
            // true when it isn't?
            aabb.ExtendBy(ooRange.GetMin());
            aabb.ExtendBy(ooRange.GetMax());

            if (aabb.GetMax()[0] != FLT_MAX) {
                boundingBox.ExtendBy(aabb);
                DrawableItem* drawableItem = new DrawableItem(drawable, aabb, oobb);
                
                items->push_back(drawableItem);
                visibilityOwners->push_back(drawableItem);
            }
        }
    }
    
    return boundingBox;
}

DrawableItem::DrawableItem(HdStDrawItemInstance* itemInstance,
                           GfRange3f const &aaBoundingBox,
                           GfBBox3f const &cullingBoundingBox,
                           size_t instanceIndex,
                           size_t totalInstancers)
: itemInstance(itemInstance)
, aabb(aaBoundingBox)
, cullingBBox(cullingBoundingBox)
, cullCache(cullingBoundingBox.GetRange().GetMin(), cullingBoundingBox.GetRange().GetMax())
, instanceIdx(instanceIndex)
, numItemsInInstance(totalInstancers)
, isInstanced(true)
{
    // Nothing
}

DrawableItem::DrawableItem(HdStDrawItemInstance* itemInstance,
                           GfRange3f const &aaBoundingBox,
                           GfBBox3f const &cullingBoundingBox)
: DrawableItem(itemInstance, aaBoundingBox, cullingBoundingBox, 0, 1)
{
    isInstanced = false;
}

void DrawableItem::ProcessInstancesVisible()
{
    int numVisible;
    if (isInstanced) {
        numVisible = itemInstance->GetDrawItem()->BuildInstanceBuffer(itemInstance->GetCullResultVisibilityCache());
    }
    else {
        if (itemInstance->CullResultIsVisible())
            numVisible = 1;
        else
            numVisible = 0;
        itemInstance->GetDrawItem()->SetNumVisible(numVisible);
    }

    bool shouldBeVisible = itemInstance->GetDrawItem()->GetVisible() && numVisible;
    if (itemInstance->IsVisible() != shouldBeVisible) {
        itemInstance->SetVisible(shouldBeVisible);
    }
}

static int BVHCounterX = 0;

BVH::BVH()
: root(NULL)
, buildTimeMS(0.f)
, populated(false)
{
    // nothing.
    BVHCounter = ++BVHCounterX;
//    NSLog(@"BVH Created,%i", BVHCounter);
}

BVH::~BVH()
{
    delete root;
    root = NULL;

    for (size_t idx = 0; idx < drawableItems.size(); ++idx) {
        delete drawableItems[idx];
    }
//    NSLog(@"BVH dead,%i", BVHCounter);
}

os_log_t BVH::cullingLog(void) {
    static const os_log_t _cullingLog = os_log_create("hydra.metal", "Culling");
    return _cullingLog;
}

void BVH::BuildBVH(std::vector<HdStDrawItemInstance> *drawables)
{

    if (root) {
        delete root;
        root = NULL;
    }

    populated = false;

//    NSLog(@"Building BVH for %zu HdStDrawItemInstance(s), %i", drawables->size(), BVHCounter);
    if (drawables->size() <= 0) {
        return;
    }

    for (size_t idx = 0; idx < drawableItems.size(); ++idx) {
        delete drawableItems[idx];
    }
    drawableItems.clear();
    drawableVisibilityOwners.clear();
    animatedDrawables.clear();
    bakedAnimatedVisibility.clear();
    bakedAnimatedVisibilityItemCount = 0;
    
    uint64_t buildStart = ArchGetTickTime();
    
    GfRange3f bbox = ConvertDrawablesToItems(drawables,
                                             &(this->drawableItems),
                                             &drawableVisibilityOwners,
                                             &animatedDrawables,
                                             bakedAnimatedVisibilityItemCount);
    bakedAnimatedVisibility.resize(bakedAnimatedVisibilityItemCount);
    populated = true;
    root = new OctreeNode(0.f, 0.f, 0.f, 0.f, 0.f, 0.f);
    root->ReInit(bbox);
    unsigned depth = 0;
    size_t drawableItemsCount = drawableItems.size();
    for (size_t idx = 0; idx < drawableItemsCount; ++idx)
    {
        unsigned currentDepth = root->Insert(drawableItems[idx], 0);
        depth = MAX(depth, currentDepth);
    }
    Bake();
    buildTimeMS = (ArchGetTickTime() - buildStart) / 1000.0f;
        
    //    NSLog(@"Building BVH done: MaxDepth=%u, %fms, %zu items", depth, buildTimeMS, drawableItems.size());
}

void BVH::Bake()
{
    root->CalcSubtreeItems();

    bakedDrawableItems.resize(drawableItems.size());
    bakedVisibility.resize(drawableItems.size());
    cullList.resize(drawableItems.size());

    size_t index = 0;
    root->WriteToList(index, &bakedDrawableItems, &bakedVisibility[0]);
}

void BVH::PerformCulling(matrix_float4x4 const &viewProjMatrix,
                         vector_float2 const &dimensions)
{
    uint64_t cullStart = ArchGetTickTime();

    vector_float4 clipPlanes[6] = {
        // Right clip plane
        (vector_float4){viewProjMatrix.columns[0][3] - viewProjMatrix.columns[0][0],
                        viewProjMatrix.columns[1][3] - viewProjMatrix.columns[1][0],
                        viewProjMatrix.columns[2][3] - viewProjMatrix.columns[2][0],
                        viewProjMatrix.columns[3][3] - viewProjMatrix.columns[3][0]},
        // Left clip plane
        (vector_float4){viewProjMatrix.columns[0][3] + viewProjMatrix.columns[0][0],
                        viewProjMatrix.columns[1][3] + viewProjMatrix.columns[1][0],
                        viewProjMatrix.columns[2][3] + viewProjMatrix.columns[2][0],
                        viewProjMatrix.columns[3][3] + viewProjMatrix.columns[3][0]},
        // Bottom clip plane
        (vector_float4){viewProjMatrix.columns[0][3] + viewProjMatrix.columns[0][1],
                        viewProjMatrix.columns[1][3] + viewProjMatrix.columns[1][1],
                        viewProjMatrix.columns[2][3] + viewProjMatrix.columns[2][1],
                        viewProjMatrix.columns[3][3] + viewProjMatrix.columns[3][1]},
        // Top clip plane
        (vector_float4){viewProjMatrix.columns[0][3] - viewProjMatrix.columns[0][1],
                        viewProjMatrix.columns[1][3] - viewProjMatrix.columns[1][1],
                        viewProjMatrix.columns[2][3] - viewProjMatrix.columns[2][1],
                        viewProjMatrix.columns[3][3] - viewProjMatrix.columns[3][1]},
        // Far clip plane
        (vector_float4){viewProjMatrix.columns[0][3] - viewProjMatrix.columns[0][2],
                        viewProjMatrix.columns[1][3] - viewProjMatrix.columns[1][2],
                        viewProjMatrix.columns[2][3] - viewProjMatrix.columns[2][2],
                        viewProjMatrix.columns[3][3] - viewProjMatrix.columns[3][2]},
        // Near clipping plane
        (vector_float4){viewProjMatrix.columns[0][3] + viewProjMatrix.columns[0][2],
                        viewProjMatrix.columns[1][3] + viewProjMatrix.columns[1][2],
                        viewProjMatrix.columns[2][3] + viewProjMatrix.columns[2][2],
                        viewProjMatrix.columns[3][3] + viewProjMatrix.columns[3][2]}
    };
    
    for (int i = 0; i < 6; i++)
    {
        vector_float4 t = clipPlanes[i] * clipPlanes[i];
        float inv = vector_precise_rsqrt(vector_reduce_add(t.xyz));
        clipPlanes[i] = clipPlanes[i] * inv;
    }

    cullList.clear();
    root->PerformCulling(viewProjMatrix, clipPlanes, dimensions, &bakedVisibility[0], cullList, false);
    float cullListTimeMS = (ArchGetTickTime() - cullStart) / 1000.0f;
    
    static matrix_float4x4 const *_viewProjMatrix;
    static vector_float2 const *_dimensions;
    static vector_float4 const *_clipPlanes;
    
    _viewProjMatrix = &viewProjMatrix;
    _dimensions = &dimensions;
    _clipPlanes = clipPlanes;
    
    struct _Worker {
        static
        void processAnimatedNodes(std::pair<std::vector<DrawableAnimatedItem>*, uint8_t*> *animatedProcessParam, size_t begin, size_t end)
        {
            if (!animatedProcessParam) {
                return;
            }
            std::vector<DrawableAnimatedItem> *animatedDrawables = animatedProcessParam->first;
            if(animatedDrawables->empty()) {
                return;
            }
            
            uint8_t* visibility = animatedProcessParam->second;
            //we only need to get the location of the first visibility index - the loop follows sequentially like it was
            //structured afterwards
            visibility += animatedDrawables->front().instanceIdx;
            for (size_t idx = begin; idx < end; ++idx) {
                const auto& animatedDrawable = (*animatedDrawables)[idx].itemInstance;
                const auto& drawItem = animatedDrawable->GetDrawItem();
                drawItem->CalculateCullingBounds(true);
                const std::vector<GfBBox3f>* instancedCullingBounds = drawItem->GetInstanceBounds();
                size_t const numItems = instancedCullingBounds->size();
                //iterate all instances and set their cull result visibility
                for (size_t i = 0; i < numItems; ++i) {
                    GfBBox3f const &oobb = (*instancedCullingBounds)[i];
                    GfRange3f const &ooRange = oobb.GetRange();
                    CullStateCache cullCache{ooRange.GetMin(), ooRange.GetMax()};
                    *visibility = MissingFunctions::IntersectsFrustum(cullCache, _clipPlanes);
                    animatedDrawable->SetCullResultVisibilityCache(visibility, i);
                    visibility++;
                }
                //write the visibility for the item
                int numVisible;
                const bool isInstanced = numItems > 1;
                if (isInstanced) {
                    numVisible = drawItem->BuildInstanceBuffer(animatedDrawable->GetCullResultVisibilityCache());
                }
                else {
                    numVisible = animatedDrawable->CullResultIsVisible();
                    drawItem->SetNumVisible(numVisible);
                }

                bool shouldBeVisible = drawItem->GetVisible() && numVisible;
                //take into account the authored visibility
                if (animatedDrawable->IsVisible() != shouldBeVisible) {
                    animatedDrawable->SetVisible(shouldBeVisible);
                }
            }
        }
        
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
                int numItems = cullItem.node->drawables.size();
                DrawableItem const * const *drawableItem = &cullItem.node->drawables[0];
                uint8_t *visibilityWritePtr = cullItem.visibilityWritePtr;

                while (numItems--) {
                    GfRange3f const &range = (*drawableItem)->cullingBBox.GetRange();

                    bool visible = !MissingFunctions::ShouldRejectBasedOnSize(
                                        range.GetMin(), range.GetMax(), *_viewProjMatrix, *_dimensions);
                    *visibilityWritePtr++ = visible;
                    drawableItem++;
                }
            }
        }
        
        static
        void processApplyFrustum(std::vector<CullListItem> *cullList, size_t begin, size_t end)
        {
            for (size_t idx = begin; idx < end; ++idx)
            {
                auto const& cullItem = (*cullList)[idx];
                int numItems = cullItem.node->drawables.size();
                DrawableItem const * const *drawableItem = &cullItem.node->drawables[0];
                uint8_t *visibilityWritePtr = cullItem.visibilityWritePtr;
                
                while (numItems--) {
                    GfRange3f const &range = (*drawableItem)->cullingBBox.GetRange();
                    
                    bool visible = !MissingFunctions::ShouldRejectBasedOnSize(
                                        range.GetMin(), range.GetMax(), *_viewProjMatrix, *_dimensions);

                    if (visible) {
                        visible = MissingFunctions::IntersectsFrustum((*drawableItem)->cullCache, _clipPlanes);
                    }

                    *visibilityWritePtr++ = visible;
                    drawableItem++;
                }
            }
        }

        static
        void processApplyInvisible(std::vector<CullListItem> *cullList, size_t begin, size_t end)
        {
            for (size_t idx = begin; idx < end; ++idx)
            {
                auto const& cullItem = (*cullList)[idx];
                memset(cullItem.visibilityWritePtr, 0, cullItem.node->totalItemCount);
            }
        }
    };
    
    unsigned grainApply = 1;
    unsigned grainBuild = 2;

    uint64_t cullApplyStart = ArchGetTickTime();
    if(!bakedAnimatedVisibility.empty()) {
        uint8_t* visibilityIndex;
        std::pair<std::vector<DrawableAnimatedItem>*, uint8_t*> animatedProcessParam;
        animatedProcessParam.first = &animatedDrawables;
        animatedProcessParam.second = &(bakedAnimatedVisibility[0]);
        WorkParallelForN(animatedDrawables.size(),
                         std::bind(&_Worker::processAnimatedNodes, &animatedProcessParam,
                                   std::placeholders::_1,
                                   std::placeholders::_2),
                         grainApply * 500);
    }
    
    WorkParallelForN(cullList.perItemContained.size(),
                     std::bind(&_Worker::processApplyContained, &cullList.perItemContained,
                               std::placeholders::_1,
                               std::placeholders::_2),
                     grainApply);
    WorkParallelForN(cullList.perItemFrustum.size(),
                     std::bind(&_Worker::processApplyFrustum, &cullList.perItemFrustum,
                               std::placeholders::_1,
                               std::placeholders::_2),
                     grainApply);
    WorkParallelForN(cullList.allItemInvisible.size(),
                     std::bind(&_Worker::processApplyInvisible, &cullList.allItemInvisible,
                               std::placeholders::_1,
                               std::placeholders::_2),
                     grainApply * 10);

    float cullApplyTimeMS = (ArchGetTickTime() - cullApplyStart) / 1000.0f;
    
    uint64_t cullBuildBufferTimeBegin = ArchGetTickTime();
    
    WorkParallelForN(drawableVisibilityOwners.size(),
                std::bind(&_Worker::processInstancesVisible, &drawableVisibilityOwners,
                                   std::placeholders::_1,
                                   std::placeholders::_2));

    uint64_t end = ArchGetTickTime();
    float cullBuildBufferTimeMS = (end - cullBuildBufferTimeBegin) / 1000.0f;
    lastCullTimeMS = (end - cullStart) / 1000.0f;
}

OctreeNode::OctreeNode(float minX, float minY, float minZ, float maxX, float maxY, float maxZ)
: aabb(GfRange3f(GfVec3f(minX, minY, minZ), GfVec3f(maxX, maxY, maxZ)))
, minVec(minX, minY, minZ)
, maxVec(maxX, maxY, maxZ)
, halfSize((maxX - minX) * 0.5, (maxY - minY) * 0.5, (maxZ - minZ) * 0.5)
, cullCache(minVec, maxVec)
, index(0)
, indexEnd(0)
, itemCount(0)
, totalItemCount(0)
, lastIntersection(Unspecified)
, isSplit(false)
, numChildren(0)
{
    // Nothing
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

void OctreeNode::ReInit(GfRange3f const &boundingBox)
{
    aabb = GfRange3f(boundingBox);
    minVec = aabb.GetMin();
    maxVec = aabb.GetMax();
    halfSize = (maxVec - minVec) * 0.5;
    cullCache = CullStateCache(minVec, maxVec);
}

void OctreeNode::PerformCulling(matrix_float4x4 const &viewProjMatrix,
                                vector_float4 const *clipPlanes,
                                vector_float2 const &dimensions,
                                uint8_t *visibility,
                                CullList &cullList,
                                bool fullyContained)
{
    if (!fullyContained) {
        if (!MissingFunctions::IntersectsFrustum(cullCache, clipPlanes) ||
            MissingFunctions::ShouldRejectBasedOnSize(cullCache, viewProjMatrix, dimensions)) {
            if (totalItemCount) {
                if (lastIntersection != OutsideCull) {
                    cullList.allItemInvisible.push_back({this, visibility + index});
                }
            }
            lastIntersection = OutsideCull;
            return;
        }

        if (MissingFunctions::FrustumFullyContains(cullCache, clipPlanes)) {
            fullyContained = true;
        }
    }

    if (fullyContained) {
        if (MissingFunctions::ShouldRejectBasedOnSize(cullCache, viewProjMatrix, dimensions)) {
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
            children[i]->PerformCulling(viewProjMatrix, clipPlanes, dimensions, visibility, cullList, fullyContained);
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
