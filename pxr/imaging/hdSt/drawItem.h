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
#ifndef PXR_IMAGING_HD_ST_DRAW_ITEM_H
#define PXR_IMAGING_HD_ST_DRAW_ITEM_H

#include "pxr/pxr.h"
#include "pxr/imaging/hdSt/api.h"
#include "pxr/imaging/hd/drawItem.h"

PXR_NAMESPACE_OPEN_SCOPE

using HdSt_GeometricShaderSharedPtr = std::shared_ptr<class HdSt_GeometricShader>;
using HdStShaderCodeSharedPtr = std::shared_ptr<class HdStShaderCode>;

class HdStDrawItem : public HdDrawItem
{
public:
    HF_MALLOC_TAG_NEW("new HdStDrawItem");

    HDST_API
    HdStDrawItem(HdRprimSharedData const *sharedData);

    HDST_API
    ~HdStDrawItem() override;

    void SetGeometricShader(HdSt_GeometricShaderSharedPtr const &shader) {
        _geometricShader = shader;
    }

    HdSt_GeometricShaderSharedPtr const &GetGeometricShader() const {
        return _geometricShader;
    }

    HdStShaderCodeSharedPtr const &GetMaterialShader() const {
        return _materialShader;
    }

    void SetMaterialShader(HdStShaderCodeSharedPtr const &shader) {
        _materialShader = shader;
    }
    
    int GetNumVisible() const {
        return _numVisible;
    }
    
    void SetNumVisible(int visibleCount) const {
        _numVisible = visibleCount;
    }
    
    const std::vector<GfBBox3f>* GetInstanceBounds() const {
        return &_instancedCullingBounds;
    }

    /// Tests the intersection with the view projection matrix.
    /// Returns true if this drawItem is in the frustum.
    HDST_API
    bool IntersectsViewVolume(matrix_float4x4 const &viewProjMatrix,
                              vector_float2 windowDimensions) const;
    
    HDST_API
    void CalculateCullingBounds(bool forceReCalculate=false) const;

    HDST_API
    int BuildInstanceBuffer(uint8_t** instanceVisibility) const;

protected:
    size_t _GetBufferArraysHash() const override;
    size_t _GetElementOffsetsHash() const override;

private:
    HdSt_GeometricShaderSharedPtr _geometricShader;
    HdStShaderCodeSharedPtr _materialShader;
    
    // CPU culling
    mutable std::vector<GfBBox3f> _instancedCullingBounds;
    mutable bool _instancedCullingBoundsCalculated = false;
    mutable int _numVisible = 1;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif //PXR_IMAGING_HD_ST_DRAW_ITEM_H
