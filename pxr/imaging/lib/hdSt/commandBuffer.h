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
#ifndef HDST_COMMAND_BUFFER_H
#define HDST_COMMAND_BUFFER_H

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

namespace SpatialHierarchy {
    enum Intersection {
        Inside,
        Outside,
        Intersects
    };
    
    struct DrawableItem {
        DrawableItem(HdStDrawItemInstance* itemInstance);
        DrawableItem(HdStDrawItemInstance* itemInstance, GfRange3f boundingBox, size_t instanceIndex);
        
        void SetVisible(bool visible);
        
        static GfRange3f ConvertDrawablesToItems(std::vector<HdStDrawItemInstance> *drawables, std::vector<DrawableItem*> *items);
        
        HdStDrawItemInstance *item;
        GfRange3f aabb;
        GfVec3f halfSize;
        
        bool visible;
        bool isInstanced;
        size_t instanceIdx;
    };
    
    class OctreeNode {
    public:
        OctreeNode(float minX, float minY, float minZ, float maxX, float maxY, float maxZ, unsigned currentDepth);
        ~OctreeNode();
        
        void ReInit(GfRange3f const &boundingBox, std::vector<DrawableItem*> *drawables);
        
        std::list<OctreeNode*> PerformCulling(matrix_float4x4 const &viewProjMatrix, vector_float2 const &dimensions);
        unsigned Insert(DrawableItem* drawable);
        
        void LogStatus(bool recursive);
        
        GfRange3f aabb;
        GfVec3f minVec;
        GfVec3f maxVec;
        GfVec3f halfSize;
        
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
    };
}
class HdStDrawItem;
class HdStDrawItemInstance;

typedef boost::shared_ptr<class HdStResourceRegistry>
    HdStResourceRegistrySharedPtr;
typedef boost::shared_ptr<class HdStRenderPassState> HdStRenderPassStateSharedPtr;
typedef boost::shared_ptr<class HdSt_DrawBatch> HdSt_DrawBatchSharedPtr;
typedef std::vector<HdSt_DrawBatchSharedPtr> HdSt_DrawBatchSharedPtrVector;

/// \class HdStCommandBuffer
///
/// A buffer of commands (HdStDrawItem or HdComputeItem objects) to be executed.
///
/// The HdStCommandBuffer is responsible for accumulating draw items and sorting
/// them for correctness (e.g. alpha transparency) and efficiency (e.g. the
/// fewest number of GPU state changes).
///
class HdStCommandBuffer {
public:
    HDST_API
    HdStCommandBuffer();
    HDST_API
    ~HdStCommandBuffer();

    /// Prepare the command buffer for draw
    HDST_API
    void PrepareDraw(HdStRenderPassStateSharedPtr const &renderPassState,
                     HdStResourceRegistrySharedPtr const &resourceRegistry);

    /// Execute the command buffer
    HDST_API
    void ExecuteDraw(HdStRenderPassStateSharedPtr const &renderPassState,
                     HdStResourceRegistrySharedPtr const &resourceRegistry);

    /// Cull drawItemInstances based on passed in combined view and projection matrix
    HDST_API
    void FrustumCull(GfMatrix4d const &cullMatrix);

    /// Sync visibility state from RprimSharedState to DrawItemInstances.
    HDST_API
    void SyncDrawItemVisibility(unsigned visChangeCount);

    /// Destructively swaps the contents of \p items with the internal list of
    /// all draw items. Culling state is reset, with no items visible.
    HDST_API
    void SwapDrawItems(std::vector<HdStDrawItem const*>* items,
                       unsigned currentBatchVersion);

    /// Rebuild all draw batches if any underlying buffer array is invalidated.
    HDST_API
    void RebuildDrawBatchesIfNeeded(unsigned currentBatchVersion);

    /// Returns the total number of draw items, including culled items.
    size_t GetTotalSize() const { return _drawItems.size(); }

    /// Returns the number of draw items, excluding culled items.
    size_t GetVisibleSize() const { return _visibleSize; }

    /// Returns the number of culled draw items.
    size_t GetCulledSize() const { 
        return _drawItems.size() - _visibleSize; 
    }

    HDST_API
    void SetEnableTinyPrimCulling(bool tinyPrimCulling);

private:
    void _RebuildDrawBatches();

    std::vector<HdStDrawItem const*> _drawItems;
    std::vector<HdStDrawItemInstance> _drawItemInstances;
    HdSt_DrawBatchSharedPtrVector _drawBatches;
    size_t _visibleSize;
    unsigned _visChangeCount;
    unsigned _batchVersion;
    
    SpatialHierarchy::BVH bvh;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif //HDST_COMMAND_BUFFER_H
