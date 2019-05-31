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
#include "pxr/imaging/hd/drawItem.h"
#include "pxr/imaging/hd/bufferArrayRange.h"

#include "pxr/base/gf/frustum.h"
#include "pxr/base/gf/matrix4f.h"
#include "pxr/base/gf/matrix4d.h"

#include <boost/functional/hash.hpp>
#include <iostream>

PXR_NAMESPACE_OPEN_SCOPE


HdDrawItem::HdDrawItem(HdRprimSharedData const *sharedData)
    : _sharedData(sharedData)
{
    HF_MALLOC_TAG_FUNCTION();
}

HdDrawItem::~HdDrawItem()
{
    /*NOTHING*/
}

size_t
HdDrawItem::GetBufferArraysHash() const
{
    size_t hash = 0;
    boost::hash_combine(hash,
                        GetTopologyRange() ?
                        GetTopologyRange()->GetVersion() : 0);
    boost::hash_combine(hash,
                        GetConstantPrimvarRange() ?
                        GetConstantPrimvarRange()->GetVersion() : 0);
    boost::hash_combine(hash,
                        GetVertexPrimvarRange() ?
                        GetVertexPrimvarRange()->GetVersion() : 0);
    boost::hash_combine(hash,
                        GetElementPrimvarRange() ?
                        GetElementPrimvarRange()->GetVersion() : 0);
    boost::hash_combine(hash,
                        GetTopologyVisibilityRange() ?
                        GetTopologyVisibilityRange()->GetVersion() : 0);
    int instancerNumLevels = GetInstancePrimvarNumLevels();
    for (int i = 0; i < instancerNumLevels; ++i) {
        boost::hash_combine(hash,
                            GetInstancePrimvarRange(i) ?
                            GetInstancePrimvarRange(i)->GetVersion(): 0);
    }
    boost::hash_combine(hash,
                        GetInstanceIndexRange() ?
                        GetInstanceIndexRange()->GetVersion(): 0);
    return hash;
}

void
HdDrawItem::CountPrimitives(std::atomic_ulong &primCount, int numIndicesPerPrimitive) const
{
    
    HdBufferArrayRangeSharedPtr const & indexBar =
        GetTopologyRange();
    int indexCount = indexBar ? indexBar->GetNumElements() : 0;

    HdBufferArrayRangeSharedPtr const & instanceIndexBar =
        GetInstanceIndexRange();

    int instancerNumLevels = instanceIndexBar ? GetInstancePrimvarNumLevels() : 0;
    int instanceIndexWidth = instancerNumLevels + 1;

    int instanceCount = instanceIndexBar
        ? instanceIndexBar->GetNumElements() / instanceIndexWidth : 1;
    
    primCount.fetch_add(indexCount * instanceCount);
}

static GfBBox3f BakeBoundsTransform(GfBBox3f const& bounds)
{
    GfVec3f const &localMin = bounds.GetRange().GetMin();
    GfVec3f const &localMax = bounds.GetRange().GetMax();
    GfVec4f worldMin = GfVec4f(localMin[0], localMin[1], localMin[2], 1);
    GfVec4f worldMax = GfVec4f(localMax[0], localMax[1], localMax[2], 1);
    GfMatrix4f const &matrix = bounds.GetMatrix();
    
    // Transform min/max bbox local space points into clip space
    worldMin = worldMin * matrix;
    worldMax = worldMax * matrix;
    
    static GfMatrix4f identity(1.0f);
    
    return GfBBox3f(
            GfRange3f(
              GfVec3f(worldMin[0], worldMin[1], worldMin[2]),
              GfVec3f(worldMax[0], worldMax[1], worldMax[2])),
            identity);
}

bool
HdDrawItem::IntersectsViewVolume(matrix_float4x4 const &viewProjMatrix,
                                 vector_float2 windowDimensions) const
{
    HdBufferArrayRangeSharedPtr const & instanceIndexRange = GetInstanceIndexRange();
    if (instanceIndexRange) {
        int instancerNumLevels = GetInstancePrimvarNumLevels();
        int instanceIndexWidth = instancerNumLevels + 1;
        int numInstances = instanceIndexRange->GetNumElements() / instanceIndexWidth;
        
        if (instancerNumLevels == 1) {
            int instanceOffset = instanceIndexRange->GetOffset();

            HdBufferResourceSharedPtr const & instanceIndexRes = instanceIndexRange->GetResource(HdTokens->instanceIndices);
            
            uint8_t *instanceIndexBuffer = const_cast<uint8_t*>(instanceIndexRes->GetBufferContents());
            uint32_t *instanceBuffer = reinterpret_cast<uint32_t*>(instanceIndexBuffer) + instanceOffset;
            
            if (!_instancedCullingBoundsCalculated) {
                _instancedCullingBoundsCalculated = true;

                HdBufferArrayRangeSharedPtr const & primvar = GetConstantPrimvarRange();
                HdBufferResourceSharedPtr const & transformRes = primvar->GetResource(HdTokens->transform);
                HdBufferResourceSharedPtr const & instancerTransformRes = primvar->GetResource(HdTokens->instancerTransform);
                HdBufferArrayRangeSharedPtr const & instanceBar = GetInstancePrimvarRange(0);

                HdBufferResourceSharedPtr const & instanceTransformRes = instanceBar->GetResource(HdTokens->instanceTransform);
                HdBufferResourceSharedPtr const & translateRes = instanceBar->GetResource(HdTokens->translate);
                HdBufferResourceSharedPtr const & rotateRes = instanceBar->GetResource(HdTokens->rotate);
                HdBufferResourceSharedPtr const & scaleRes = instanceBar->GetResource(HdTokens->scale);

                // Item transform
                size_t stride = transformRes->GetStride();
                uint8_t const* rawBuffer = transformRes->GetBufferContents();
                GfMatrix4f const *itemTransform =
                    (GfMatrix4f const*)&rawBuffer[stride * primvar->GetIndex() + transformRes->GetOffset()];

                // Instancer transform
                stride = instancerTransformRes->GetStride();
                rawBuffer = instancerTransformRes->GetBufferContents();
                GfMatrix4f const *instancerTransform =
                    (GfMatrix4f const*)&rawBuffer[stride * primvar->GetIndex() + instancerTransformRes->GetOffset()];
                GfMatrix4f m;

                int instanceDrawingCoord = instanceBar->GetOffset();

                for (int i = 0; i < numInstances; i++) {
                    
                    int instanceIndex = instanceBuffer[i * instanceIndexWidth + 1] + instanceDrawingCoord;
                    
                    // instance coordinates

                    if (instanceTransformRes) {
                        // Instance transform
                        stride = instanceTransformRes->GetStride();
                        rawBuffer = instanceTransformRes->GetBufferContents();
                        GfMatrix4f const *instanceTransform = (GfMatrix4f const*)&rawBuffer[stride * instanceIndex];
                        m = *instanceTransform;
                    }
                    else {
                        m.SetIdentity();
                    }
                    
                    GfVec3f translate(0), scale(1);
                    GfQuaternion rotate(GfQuaternion::GetIdentity());
                    
                    if (scaleRes) {
                        stride = scaleRes->GetStride();
                        rawBuffer = scaleRes->GetBufferContents();
                        scale = *(GfVec3f const*)&rawBuffer[stride * instanceIndex];
                    }
                    
                    if (rotateRes) {
                        stride = rotateRes->GetStride();
                        rawBuffer = rotateRes->GetBufferContents();
                        float const* const floatArray = (float const*)&rawBuffer[stride * instanceIndex];
                        rotate = GfQuaternion(floatArray[0], GfVec3d(floatArray[1], floatArray[2], floatArray[3]));
                    }
                    
                    if (translateRes) {
                        stride = translateRes->GetStride();
                        rawBuffer = translateRes->GetBufferContents();
                        translate = *(GfVec3f const*)&rawBuffer[stride * instanceIndex];
                    }
                    
                    GfMatrix4f mtxScale, mtxRotate, mtxTranslate;
                    mtxScale.SetScale(scale);
                    mtxRotate.SetRotate(rotate);
                    mtxTranslate.SetTranslate(translate);
                    
                    m = (*itemTransform) * (m * mtxScale * mtxRotate * mtxTranslate * (*instancerTransform));

                    GfBBox3f box(GetBounds().GetRange(), m);
                    _instancedCullingBounds.push_back(BakeBoundsTransform(box));
                }
            }
            
            static bool perInstanceCulling = false;

            if (!perInstanceCulling) {
                HdDrawItem* _this = const_cast<HdDrawItem*>(this);
                _this->numVisible = _instancedCullingBounds.size();

                for(auto& bounds : _instancedCullingBounds) {
                    if (GfFrustum::IntersectsViewVolumeFloat(bounds, viewProjMatrix, windowDimensions))
                        return true;
                }
                return false;
            }

            bool result = false;
            HdBufferResourceSharedPtr const & culledInstanceIndexRes = instanceIndexRange->GetResource(HdTokens->culledInstanceIndices);

            uint8_t *culledInstanceIndexBuffer = const_cast<uint8_t*>(culledInstanceIndexRes->GetBufferContents());
            uint32_t *culledInstanceBuffer = reinterpret_cast<uint32_t*>(culledInstanceIndexBuffer) + instanceOffset;
            
            bool modified = false;
            int numVisible = 0;
            int numItems = _instancedCullingBounds.size();
            for(int i = 0; i < numItems; i++) {
                int instanceIndex = instanceBuffer[i * instanceIndexWidth];
                auto const & bounds = _instancedCullingBounds[i];

                if (GfFrustum::IntersectsViewVolumeFloat(bounds, viewProjMatrix, windowDimensions)) {
                    result = true;

                    if (*culledInstanceBuffer != instanceIndex) {
                        modified = true;
                        *culledInstanceBuffer++ = instanceIndex;
                        for(int j = 1; j < instanceIndexWidth; j++)
                            *culledInstanceBuffer++ = instanceBuffer[i * instanceIndexWidth + j];
                    }
                    else {
                        culledInstanceBuffer+=instanceIndexWidth;
                    }
                    numVisible++;
                }
            }

            if (modified) {
#if defined(ARCH_OS_MACOS)
                MtlfMetalContext::MtlfMultiBuffer h = culledInstanceIndexRes->GetId();
                id<MTLBuffer> metalBuffer = h.forCurrentGPU();

                uint32_t start = instanceOffset * sizeof(uint32_t);
                uint32_t length = numVisible * sizeof(uint32_t) * instanceIndexWidth;
                MtlfMetalContext::GetMetalContext()->QueueBufferFlush(metalBuffer, start, start + length);
#endif
            }

            HdDrawItem* _this = const_cast<HdDrawItem*>(this);
            _this->numVisible = numVisible;
            
            return result;
        }
        // We don't process multiple levels of instancer yet
        return true;
    }
    else {
        if (!_instancedCullingBoundsCalculated) {
            _instancedCullingBoundsCalculated = true;
            _instancedCullingBounds.push_back(BakeBoundsTransform(GetBounds()));
        }
        if( GfFrustum::IntersectsViewVolumeFloat(_instancedCullingBounds.front(), viewProjMatrix, windowDimensions)) {
            return true;
        }
        return false;
    }
}

void
HdDrawItem::CalculateCullingBounds() const
{
    if (_instancedCullingBoundsCalculated) {
        return;
    }
    
    HdBufferArrayRangeSharedPtr const & instanceIndexRange = GetInstanceIndexRange();
    if (instanceIndexRange) {
        int instancerNumLevels = GetInstancePrimvarNumLevels();
        int instanceIndexWidth = instancerNumLevels + 1;
        int numInstances = instanceIndexRange->GetNumElements() / instanceIndexWidth;
        
        if (instancerNumLevels == 1) {
            int instanceOffset = instanceIndexRange->GetOffset();
            
            HdBufferResourceSharedPtr const & instanceIndexRes = instanceIndexRange->GetResource(HdTokens->instanceIndices);
            
            uint8_t *instanceIndexBuffer = const_cast<uint8_t*>(instanceIndexRes->GetBufferContents());
            uint32_t *instanceBuffer = reinterpret_cast<uint32_t*>(instanceIndexBuffer) + instanceOffset;
            
            _instancedCullingBoundsCalculated = true;
            
            HdBufferArrayRangeSharedPtr const & primvar = GetConstantPrimvarRange();
            HdBufferResourceSharedPtr const & transformRes = primvar->GetResource(HdTokens->transform);
            HdBufferResourceSharedPtr const & instancerTransformRes = primvar->GetResource(HdTokens->instancerTransform);
            HdBufferArrayRangeSharedPtr const & instanceBar = GetInstancePrimvarRange(0);
            
            HdBufferResourceSharedPtr const & instanceTransformRes = instanceBar->GetResource(HdTokens->instanceTransform);
            HdBufferResourceSharedPtr const & translateRes = instanceBar->GetResource(HdTokens->translate);
            HdBufferResourceSharedPtr const & rotateRes = instanceBar->GetResource(HdTokens->rotate);
            HdBufferResourceSharedPtr const & scaleRes = instanceBar->GetResource(HdTokens->scale);
            
            // Item transform
            size_t stride = transformRes->GetStride();
            uint8_t const* rawBuffer = transformRes->GetBufferContents();
            GfMatrix4f const *itemTransform =
            (GfMatrix4f const*)&rawBuffer[stride * primvar->GetIndex() + transformRes->GetOffset()];
            
            // Instancer transform
            stride = instancerTransformRes->GetStride();
            rawBuffer = instancerTransformRes->GetBufferContents();
            GfMatrix4f const *instancerTransform =
            (GfMatrix4f const*)&rawBuffer[stride * primvar->GetIndex() + instancerTransformRes->GetOffset()];
            GfMatrix4f m;
            
            int instanceDrawingCoord = instanceBar->GetOffset();
            
            _instancedCullingBounds.clear();
                
            for (int i = 0; i < numInstances; i++) {
                
                int instanceIndex = instanceBuffer[i * instanceIndexWidth + 1] + instanceDrawingCoord;
                
                // instance coordinates
                
                if (instanceTransformRes) {
                    // Instance transform
                    stride = instanceTransformRes->GetStride();
                    rawBuffer = instanceTransformRes->GetBufferContents();
                    GfMatrix4f const *instanceTransform = (GfMatrix4f const*)&rawBuffer[stride * instanceIndex];
                    m = *instanceTransform;
                }
                else {
                    m.SetIdentity();
                }
                
                GfVec3f translate(0), scale(1);
                GfQuaternion rotate(GfQuaternion::GetIdentity());
                
                if (scaleRes) {
                    stride = scaleRes->GetStride();
                    rawBuffer = scaleRes->GetBufferContents();
                    scale = *(GfVec3f const*)&rawBuffer[stride * instanceIndex];
                }
                
                if (rotateRes) {
                    stride = rotateRes->GetStride();
                    rawBuffer = rotateRes->GetBufferContents();
                    float const* const floatArray = (float const*)&rawBuffer[stride * instanceIndex];
                    rotate = GfQuaternion(floatArray[0], GfVec3d(floatArray[1], floatArray[2], floatArray[3]));
                }
                
                if (translateRes) {
                    stride = translateRes->GetStride();
                    rawBuffer = translateRes->GetBufferContents();
                    translate = *(GfVec3f const*)&rawBuffer[stride * instanceIndex];
                }
                
                GfMatrix4f mtxScale, mtxRotate, mtxTranslate;
                mtxScale.SetScale(scale);
                mtxRotate.SetRotate(rotate);
                mtxTranslate.SetTranslate(translate);
                
                m = (*itemTransform) * (m * mtxScale * mtxRotate * mtxTranslate * (*instancerTransform));
                
                GfBBox3f box(GetBounds().GetRange(), m);
                _instancedCullingBounds.push_back(BakeBoundsTransform(box));
            }
        }
        else {
            TF_CODING_WARNING("Only expected to find one instance level, found %d with %d instances", instancerNumLevels, numInstances);
            _instancedCullingBounds.push_back(BakeBoundsTransform(GetBounds()));
        }
    }
    else {
        _instancedCullingBounds.push_back(BakeBoundsTransform(GetBounds()));
    }
    
    _instancedCullingBoundsCalculated = true;
}

void HdDrawItem::BuildInstanceBuffer(uint8_t** instanceVisibility) const
{
    int numItems = _instancedCullingBounds.size();
    int i;
    for(i = 0; i < numItems; i++) {
        if (*instanceVisibility[i])
            break;
    }
    if (i == numItems) {
        numVisible = 0;
        return;
    }

    int instancerNumLevels = GetInstancePrimvarNumLevels();
    int instanceIndexWidth = instancerNumLevels + 1;

    HdBufferArrayRangeSharedPtr const & instanceIndexRange = GetInstanceIndexRange();
    int instanceOffset = instanceIndexRange->GetOffset();
    
    HdBufferResourceSharedPtr const & instanceIndexRes = instanceIndexRange->GetResource(HdTokens->instanceIndices);
    
    uint8_t *instanceIndexBuffer = const_cast<uint8_t*>(instanceIndexRes->GetBufferContents());
    uint32_t *instanceBuffer = reinterpret_cast<uint32_t*>(instanceIndexBuffer) + instanceOffset;

    HdBufferResourceSharedPtr const & culledInstanceIndexRes = instanceIndexRange->GetResource(HdTokens->culledInstanceIndices);
    
    uint8_t *culledInstanceIndexBuffer = const_cast<uint8_t*>(culledInstanceIndexRes->GetBufferContents());
    uint32_t *culledInstanceBuffer = reinterpret_cast<uint32_t*>(culledInstanceIndexBuffer) + instanceOffset;
    
    bool modified = false;
    int numVisible = 0;
    for(i = 0; i < numItems; i++) {
        if (!*instanceVisibility[i])
            continue;
        
        int instanceIndex = instanceBuffer[i * instanceIndexWidth];
        auto const & bounds = _instancedCullingBounds[i];

        if (*culledInstanceBuffer != instanceIndex) {
            modified = true;
            *culledInstanceBuffer++ = instanceIndex;
            for(int j = 1; j < instanceIndexWidth; j++)
                *culledInstanceBuffer++ = instanceBuffer[i * instanceIndexWidth + j];
        }
        else {
            culledInstanceBuffer+=instanceIndexWidth;
        }
        numVisible++;
    }
    
    if (modified) {
#if defined(ARCH_OS_MACOS)
        MtlfMetalContext::MtlfMultiBuffer h = culledInstanceIndexRes->GetId();
        id<MTLBuffer> metalBuffer = h.forCurrentGPU();
        
        uint32_t start = instanceOffset * sizeof(uint32_t);
        uint32_t length = numVisible * sizeof(uint32_t) * instanceIndexWidth;
        MtlfMetalContext::GetMetalContext()->QueueBufferFlush(metalBuffer, start, start + length);
#endif
    }
    
    HdDrawItem* _this = const_cast<HdDrawItem*>(this);
    _this->numVisible = numVisible;
}
HD_API
std::ostream &operator <<(std::ostream &out, 
                          const HdDrawItem& self) {
    out << "Draw Item:\n";
    out << "    Bound: "    << self._sharedData->bounds << "\n";
    out << "    Visible: "  << self._sharedData->visible << "\n";
    if (self.GetTopologyRange()) {
        out << "    Topology:\n";
        out << "        numElements=" << self.GetTopologyRange()->GetNumElements() << "\n";
        out << *self.GetTopologyRange();
    }
    if (self.GetConstantPrimvarRange()) {
        out << "    Constant Primvars:\n";
        out << *self.GetConstantPrimvarRange();
    }
    if (self.GetElementPrimvarRange()) {
        out << "    Element Primvars:\n";
        out << "        numElements=" << self.GetElementPrimvarRange()->GetNumElements() << "\n";
        out << *self.GetElementPrimvarRange();
    }
    if (self.GetVertexPrimvarRange()) {
        out << "    Vertex Primvars:\n";
        out << "        numElements=" << self.GetVertexPrimvarRange()->GetNumElements() << "\n";
        out << *self.GetVertexPrimvarRange();
    }
    if (self.GetFaceVaryingPrimvarRange()) {
        out << "    Fvar Primvars:\n";
        out << "        numElements=" << self.GetFaceVaryingPrimvarRange()->GetNumElements() << "\n";
        out << *self.GetFaceVaryingPrimvarRange();
    }
    if (self.GetTopologyVisibilityRange()) {
        out << "    Topology visibility:\n";
        out << *self.GetTopologyVisibilityRange();
    }
    return out;
}

PXR_NAMESPACE_CLOSE_SCOPE

