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

bool
HdDrawItem::IntersectsViewVolume(GfMatrix4d const &viewProjMatrix, int viewport_width, int viewport_height) const
{
    if (GetInstanceIndexRange()) {
        int instancerNumLevels = GetInstancePrimvarNumLevels();
        int instanceIndexWidth = instancerNumLevels + 1;
        int numInstances = GetInstanceIndexRange()->GetNumElements() / instanceIndexWidth;

        if (instancerNumLevels == 1) {
            if (numInstances >= 1) {
                if (!_instancedCullingBoundsCalculated) {
                    const_cast<HdDrawItem*>(this)->_instancedCullingBoundsCalculated = true;

                    HdBufferArrayRangeSharedPtr const & primvar = GetConstantPrimvarRange();
                    HdBufferResourceSharedPtr const & primvarRes = primvar->GetResource(HdTokens->instancerTransform);

                    // Instancer transform
                    size_t stride = primvarRes->GetStride();
                    uint8_t const* rawBuffer = primvarRes->GetBufferContents();
                    GfMatrix4f const *instancerTransform = (GfMatrix4f const*)&rawBuffer[stride * primvar->GetIndex() + primvarRes->GetOffset()];
                    GfMatrix4f m;

                    for (int i = 0; i < numInstances; i++) {
                        HdBufferArrayRangeSharedPtr const & instanceBar = GetInstancePrimvarRange(0);
                        HdBufferResourceSharedPtr const & instanceTransformRes = instanceBar->GetResource(HdTokens->instanceTransform);
                        int instanceIndex = instanceBar->GetOffset() + i;
                        if (instanceTransformRes) {
                            // Instance transform
                            stride = instanceTransformRes->GetStride();
                            rawBuffer = instanceTransformRes->GetBufferContents();
                            GfMatrix4f const *instanceTransform = (GfMatrix4f const*)&rawBuffer[stride * instanceIndex];
                            m = (*instancerTransform) * (*instanceTransform);
                        }
                        else {
                            HdBufferResourceSharedPtr const & translateRes = instanceBar->GetResource(HdTokens->translate);
                            HdBufferResourceSharedPtr const & rotateRes = instanceBar->GetResource(HdTokens->rotate);
                            HdBufferResourceSharedPtr const & scaleRes = instanceBar->GetResource(HdTokens->scale);
                            
                            GfVec3f translate(0), scale(1);
                            GfQuaternion rotate(GfQuaternion::GetIdentity());

                            if (translateRes) {
                                stride = translateRes->GetStride();
                                rawBuffer = translateRes->GetBufferContents();
                                translate = *(GfVec3f const*)&rawBuffer[stride * instanceIndex];
                            }

                            if (rotateRes) {
                                stride = rotateRes->GetStride();
                                rawBuffer = rotateRes->GetBufferContents();
                                float const* const floatArray = (float const*)&rawBuffer[stride * instanceIndex];
                                rotate = GfQuaternion(floatArray[0], GfVec3d(floatArray[1], floatArray[2], floatArray[3]));
                            }

                            if (scaleRes) {
                                stride = scaleRes->GetStride();
                                rawBuffer = scaleRes->GetBufferContents();
                                scale = *(GfVec3f const*)&rawBuffer[stride * instanceIndex];
                            }
                            GfMatrix4f mxtScale, mtxRotate, mtxTranslate;
                            mxtScale.SetScale(scale);
                            mtxRotate.SetRotate(rotate);
                            mtxTranslate.SetTranslate(translate);
                            
                            m = (*instancerTransform) * mxtScale * mtxRotate * mtxTranslate;
                        }
                        
                        HdDrawItem* _this = const_cast<HdDrawItem*>(this);
                        _this->_instancedCullingBounds.push_back(GetBounds());
                        _this->_instancedCullingBounds.back().Transform(GfMatrix4d(m));
                    }
                }

                for(auto& bounds : _instancedCullingBounds) {
                    if (GfFrustum::IntersectsViewVolume(bounds, viewProjMatrix, viewport_width, viewport_height))
                        return true;
                }
                return false;
            }
        }
        return true;
    } else {
        return GfFrustum::IntersectsViewVolume(GetBounds(), viewProjMatrix, viewport_width, viewport_height);
    }
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

