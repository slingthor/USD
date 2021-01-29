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
#ifndef HDST_SPACIAL_HIERARCHY_H
#define HDST_SPACIAL_HIERARCHY_H

#include "pxr/pxr.h"
#include "pxr/imaging/hdSt/api.h"
#include "pxr/imaging/hd/version.h"
#include "pxr/imaging/hdSt/drawItemInstance.h"

#include "pxr/base/gf/vec2f.h"
#include "pxr/base/gf/matrix4f.h"
#include "pxr/base/gf/matrix4d.h"

#include <boost/shared_ptr.hpp>

#include <vector>
#include <os/signpost.h>

PXR_NAMESPACE_OPEN_SCOPE

enum Intersection {
    Inside,
    Outside,
    Intersects
};

enum NodeCullState {
    Unspecified = -1,
    OutsideCull,
    InsideCull,
    InsideTest
};

struct DrawableItem;
class OctreeNode;

struct CullListItem {
    OctreeNode*     node;
    uint8_t*        visibilityWritePtr;
};

struct CullList {
    std::vector<CullListItem> perItemContained;
    std::vector<CullListItem> perItemFrustum;
    std::vector<CullListItem> allItemInvisible;
    
    void clear() {
        perItemContained.clear();
        perItemFrustum.clear();
        allItemInvisible.clear();
    }
    
    void resize(size_t size) {
        perItemContained.resize(size);
        perItemFrustum.resize(size);
        allItemInvisible.resize(size);
    }
};

struct CullStateCache {
    enum CullTestType {
        TestSphere,
        TestBox,
    };

    CullStateCache(GfVec3f const &minVec, GfVec3f const &maxVec) {
        points[0] = {minVec[0], minVec[1], minVec[2], 1};
        points[1] = {maxVec[0], maxVec[1], maxVec[2], 1};
        points[2] = {minVec[0], maxVec[1], minVec[2], 1};
        points[3] = {maxVec[0], minVec[1], maxVec[2], 1};
        points[4] = {minVec[0], minVec[1], maxVec[2], 1};
        points[5] = {minVec[0], maxVec[1], maxVec[2], 1};
        points[6] = {maxVec[0], minVec[1], minVec[2], 1};
        points[7] = {maxVec[0], maxVec[1], minVec[2], 1};
        GfVec3f m((minVec + maxVec) * 0.5f);
        mid = {m[0], m[1], m[2], 1};
        lastCullPlane = -1;

        vector_float3 size = (points[1].xyz - points[0].xyz) * 0.5f;
        radius = vector_length(size);

        vector_float3 t = vector_abs(vector_normalize(size));
        static float chooseBoxThreshold = vector_normalize((vector_float3){1, 1, 1}).x * 0.75f;
        if (t.x < chooseBoxThreshold ||
            t.y < chooseBoxThreshold ||
            t.z < chooseBoxThreshold) {
            suggestedTestType = TestBox;
        }
        else {
            suggestedTestType = TestSphere;
        }
        
        // Pending further sphere optimisation
        suggestedTestType = TestBox;
    }
    
    vector_float4   points[8];
    vector_float4   mid;
    float           radius;
    int             lastCullPlane;
    int             suggestedTestType;
};

struct DrawableItem {
    DrawableItem(HdStDrawItemInstance* itemInstance,
                 GfRange3f const &aaBoundingBox,
                 GfBBox3f const &cullingBBox);
    DrawableItem(HdStDrawItemInstance* itemInstance,
                 GfRange3f const &aaBoundingBox,
                 GfBBox3f const &cullingBBox,
                 size_t instanceIndex,
                 size_t totalInstancers);
    
    void ProcessInstancesVisible();
    
    static GfRange3f ConvertDrawablesToItems(std::vector<HdStDrawItemInstance> *drawables,
                                             std::vector<DrawableItem*> *items,
                                             std::vector<DrawableItem*> *visibilityOwners);
    
    HdStDrawItemInstance    *itemInstance;
    GfRange3f               aabb;
    GfBBox3f const&         cullingBBox;
    mutable CullStateCache  cullCache;

    size_t instanceIdx;
    size_t numItemsInInstance;

    bool isInstanced;
};

class OctreeNode {
public:
    OctreeNode(float minX, float minY, float minZ, float maxX, float maxY, float maxZ);
    ~OctreeNode();
    
    void CalcPoints();
    void ReInit(GfRange3f const &boundingBox);
    
    void PerformCulling(matrix_float4x4 const &viewProjMatrix,
                        vector_float4 const *clipPlanes,
                        vector_float2 const &dimensions,
                        uint8_t *visibility,
                        CullList &cullList,
                        bool fullyContained);

    unsigned Insert(DrawableItem* drawable, unsigned currentDepth);
    
    size_t CalcSubtreeItems();
    void WriteToList(size_t &idx, std::vector<DrawableItem*> *bakedDrawableItems, uint8_t *bakedVisibility);
    
    GfRange3f aabb;
    GfVec3f minVec;
    GfVec3f maxVec;
    GfVec3f halfSize;
    mutable CullStateCache  cullCache;
    
    size_t index;
    size_t indexEnd;
    size_t itemCount;
    size_t totalItemCount;
    
    NodeCullState lastIntersection;
    std::vector<DrawableItem*> drawables;
    
    bool isSplit;

    int numChildren;
    OctreeNode* children[8] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

private:
    void subdivide();
};

class BVH {
public:
    BVH();
    ~BVH();
    void BuildBVH(std::vector<HdStDrawItemInstance> *drawables);
    void PerformCulling(matrix_float4x4 const &viewProjMatrix,
                        vector_float2 const &dimensions);
    
    OctreeNode *root;
    std::vector<DrawableItem*> drawableItems;
    std::vector<DrawableItem*> drawableVisibilityOwners;
    
    float buildTimeMS;
    float lastCullTimeMS;
    
    bool populated;
    int BVHCounter;
private:
    void Bake();
    os_log_t cullingLog();

    std::vector<DrawableItem*> bakedDrawableItems;
    std::vector<uint8_t> bakedVisibility;
    CullList cullList;
    bool visibilityDirty;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif //HDST_SPACIAL_HIERARCHY_H
