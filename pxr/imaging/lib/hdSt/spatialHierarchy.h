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

PXR_NAMESPACE_OPEN_SCOPE

enum Intersection {
    Inside,
    Outside,
    Intersects
};

class OctreeNode;

struct Interval {
    size_t start;
    size_t end;
    bool visible;
    
    Interval(size_t start, size_t end, bool visible);// : start(start), end(end), visible(visible) {}
    Interval(OctreeNode* node, bool visible);// : Interval(node->index, node->indexEnd, visible) {}
    
    static
    bool compare(Interval &a, Interval &b) { return a.start < b.start; };
};

struct DrawableItem {
    DrawableItem(HdStDrawItemInstance* itemInstance, GfRange3f boundingBox);
    DrawableItem(HdStDrawItemInstance* itemInstance, GfRange3f boundingBox, size_t instanceIndex, size_t totalInstancers);
    
    void SetVisible(bool visible);
    void ProcessInstancesVisible();
    
    static GfRange3f ConvertDrawablesToItems(std::vector<HdStDrawItemInstance> *drawables, std::vector<DrawableItem*> *items);
    
    HdStDrawItemInstance *itemInstance;
    GfRange3f aabb;
    GfVec3f halfSize;
    
    bool visible;
    bool isInstanced;
    size_t instanceIdx;
    size_t numItemsInInstance;
};

class OctreeNode {
public:
    OctreeNode(float minX, float minY, float minZ, float maxX, float maxY, float maxZ, unsigned currentDepth);
    ~OctreeNode();
    
    void ReInit(GfRange3f const &boundingBox, std::vector<DrawableItem*> *drawables);
    
    std::list<Interval> PerformCulling(matrix_float4x4 const &viewProjMatrix, vector_float2 const &dimensions);
    unsigned Insert(DrawableItem* drawable);
    
    size_t CalcSubtreeItems();
    void WriteToList(size_t &idx, std::vector<DrawableItem*> *bakedDrawableItems);
    
    void LogStatus(bool recursive);
    
    GfRange3f aabb;
    GfVec3f minVec;
    GfVec3f maxVec;
    GfVec3f halfSize;
    
    size_t index;
    size_t indexEnd;
    size_t itemCount;
    size_t totalItemCount;
    
    NSString* name;
    
    std::list<DrawableItem*> drawables;
    std::list<DrawableItem*> drawablesTooLarge;
    bool isSplit;

    OctreeNode* children[8] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
private:
    void subdivide();
    bool canSubdivide();
    
    unsigned depth;
    
    unsigned InsertStraight(DrawableItem* drawable);
};

class BVH {
public:
    BVH();
    ~BVH();
    void BuildBVH(std::vector<HdStDrawItemInstance> *drawables);
    void PerformCulling(matrix_float4x4 const &viewProjMatrix, vector_float2 const &dimensions);
    
    OctreeNode root;
    std::vector<DrawableItem*> drawableItems;
    
    float buildTimeMS;
    float lastCullTimeMS;
    
    bool populated;
    int BVHCounter;
private:
    void Bake();
    
    std::vector<DrawableItem*> instancedDrawableItems;
    std::vector<DrawableItem*> bakedDrawableItems;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif //HDST_SPACIAL_HIERARCHY_H
