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
HdDrawItem::IntersectsViewVolume(GfMatrix4d const &viewProjMatrix) const
{
    id<MTLTexture> texture = MtlfMetalContext::GetMetalContext()->mtlColorTexture;
    if (GetInstanceIndexRange()) {
        int instancerNumLevels = GetInstancePrimvarNumLevels();
        int instanceIndexWidth = instancerNumLevels + 1;

        if (instancerNumLevels == 1 &&
            GetInstanceIndexRange()->GetNumElements() / instanceIndexWidth == 1) {
            if (!_instancedCullingBoundsCalculated) {
                const_cast<HdDrawItem*>(this)->_instancedCullingBoundsCalculated = true;

                HdBufferArrayRangeSharedPtr const & instanceBar = GetInstancePrimvarRange(0);
                HdBufferResourceSharedPtr const & instanceRes = instanceBar->GetResource(HdTokens->instanceTransform);

                size_t stride = instanceRes->GetStride();
                uint8_t const* rawBuffer = instanceRes->GetBufferContents();
                GfMatrix4f const *instanceTransform = (GfMatrix4f const*)&rawBuffer[stride * instanceBar->GetOffset()];

                
                
                HdBufferArrayRangeSharedPtr const & primvar = GetConstantPrimvarRange();
                HdBufferResourceSharedPtr const & primvarRes = primvar->GetResource(HdTokens->instancerTransform);
                
                stride = primvarRes->GetStride();
                rawBuffer = primvarRes->GetBufferContents();
                GfMatrix4f const *instancerTransform = (GfMatrix4f const*)&rawBuffer[stride * primvar->GetIndex() + primvarRes->GetOffset()];

                GfMatrix4f bla = (*instancerTransform) * (*instanceTransform);
                
                const_cast<HdDrawItem*>(this)->_instancedCullingBounds = GetBounds();
                const_cast<GfBBox3d&>(this->_instancedCullingBounds).Transform(GfMatrix4d(bla));
            }

            return GfFrustum::IntersectsViewVolume(_instancedCullingBounds, viewProjMatrix, texture.width, texture.height);
        }
        
        return true;
    } else {
        return GfFrustum::IntersectsViewVolume(GetBounds(), viewProjMatrix, texture.width, texture.height);
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

