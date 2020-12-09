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

#include "pxr/imaging/garch/contextCaps.h"
#include "pxr/imaging/garch/resourceFactory.h"

#include "pxr/base/gf/frustum.h"
#include "pxr/base/gf/matrix4f.h"
#include "pxr/base/gf/matrix4d.h"

#include "pxr/imaging/hdSt/drawItem.h"
#include "pxr/imaging/hdSt/shaderCode.h"
#include "pxr/imaging/hdSt/bufferArrayRange.h"
#include "pxr/imaging/hdSt/bufferResource.h"

#include "pxr/imaging/hgiMetal/buffer.h"

PXR_NAMESPACE_OPEN_SCOPE

HdStDrawItem::HdStDrawItem(HdRprimSharedData const *sharedData)
    : HdDrawItem(sharedData)
{
    HF_MALLOC_TAG_FUNCTION();
}

HdStDrawItem::~HdStDrawItem()
{
    /*NOTHING*/
}

/*virtual*/
size_t
HdStDrawItem::_GetBufferArraysHash() const
{
    if (const HdStShaderCodeSharedPtr& shader = GetMaterialShader()) {
        if (const HdBufferArrayRangeSharedPtr& shaderBAR =
            shader->GetShaderData()) {
            return shaderBAR->GetVersion();
        }
    }
    
    return 0;
}

/*virtual*/
size_t
HdStDrawItem::_GetElementOffsetsHash() const
{
    if (const HdStShaderCodeSharedPtr& shader = GetMaterialShader()) {
        if (const HdBufferArrayRangeSharedPtr& shaderBAR =
                shader->GetShaderData()) {
            return shaderBAR->GetElementOffset();
        }
    }
    
    return 0;
}

static GfBBox3f 
_BakeBoundsTransform(GfBBox3f const& bounds)
{
    GfVec3f const &localMin = bounds.GetRange().GetMin();
    GfVec3f const &localMax = bounds.GetRange().GetMax();
 
    if (localMin[0] == FLT_MAX) {
        // Short test for a default bounding box - leave unmodified
        return bounds;
    }
    
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

static
uint8_t const*
_GetBufferContents(HdStBufferResourceSharedPtr const & buffer)
{
#if defined(PXR_METAL_SUPPORT_ENABLED)
    return (uint8_t const*)buffer->GetGPUAddress();
#else
    void* bufferData = nullptr;

    GarchContextCaps const &caps = GarchResourceFactory::GetInstance()->GetContextCaps();
    if (caps.directStateAccessEnabled) {
        bufferData = glMapNamedBufferEXT(buffer->GetId()->GetRawResource(), GL_READ_ONLY);
    } else {
        glBindBuffer(GL_ARRAY_BUFFER, buffer->GetId()->GetRawResource());
        bufferData = glMapBuffer(GL_ARRAY_BUFFER, GL_READ_ONLY);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    return (uint8_t const*)bufferData;
#endif
}

bool
HdStDrawItem::IntersectsViewVolume(matrix_float4x4 const &viewProjMatrix,
                                   vector_float2 windowDimensions) const
{
    HdBufferArrayRangeSharedPtr const & instanceIndexRange = GetInstanceIndexRange();
    HdStBufferArrayRangeSharedPtr instanceIndexRangeGL = std::static_pointer_cast<HdStBufferArrayRange>(instanceIndexRange);

    if (instanceIndexRange) {
        int instancerNumLevels = GetInstancePrimvarNumLevels();
        int instanceIndexWidth = instancerNumLevels + 1;
        int numInstances = instanceIndexRange->GetNumElements() / instanceIndexWidth;
        
        if (instancerNumLevels == 1) {
            int instanceOffset = instanceIndexRange->GetElementOffset();

            HdStBufferResourceSharedPtr const & instanceIndexRes = instanceIndexRangeGL->GetResource(HdInstancerTokens->instanceIndices);
            
            uint8_t *instanceIndexBuffer = const_cast<uint8_t*>(_GetBufferContents(instanceIndexRes));
            uint32_t *instanceBuffer = reinterpret_cast<uint32_t*>(instanceIndexBuffer) + instanceOffset;
            
            if (!_instancedCullingBoundsCalculated) {
                _instancedCullingBoundsCalculated = true;

                HdBufferArrayRangeSharedPtr const & primvar = GetConstantPrimvarRange();
                HdStBufferArrayRangeSharedPtr primvarGL = std::static_pointer_cast<HdStBufferArrayRange>(primvar);
                HdStBufferResourceSharedPtr const & transformRes = primvarGL->GetResource(HdTokens->transform);
                HdStBufferResourceSharedPtr const & instancerTransformRes = primvarGL->GetResource(HdInstancerTokens->instancerTransform);

                HdBufferArrayRangeSharedPtr const & instanceBar = GetInstancePrimvarRange(0);
                HdStBufferArrayRangeSharedPtr instanceBarGL = std::static_pointer_cast<HdStBufferArrayRange>(instanceBar);
                HdStBufferResourceSharedPtr const & instanceTransformRes = instanceBarGL->GetResource(HdInstancerTokens->instanceTransform);
                HdStBufferResourceSharedPtr const & translateRes = instanceBarGL->GetResource(HdInstancerTokens->translate);
                HdStBufferResourceSharedPtr const & rotateRes = instanceBarGL->GetResource(HdInstancerTokens->rotate);
                HdStBufferResourceSharedPtr const & scaleRes = instanceBarGL->GetResource(HdInstancerTokens->scale);

                // Item transform
                size_t stride = transformRes->GetStride();
                uint8_t const* rawBuffer = _GetBufferContents(transformRes);
                GfMatrix4f const *itemTransform =
                    (GfMatrix4f const*)&rawBuffer[stride * primvar->GetElementOffset() + transformRes->GetOffset()];

                // Instancer transform
                stride = instancerTransformRes->GetStride();
                rawBuffer = _GetBufferContents(instancerTransformRes);
                GfMatrix4f const *instancerTransform =
                    (GfMatrix4f const*)&rawBuffer[stride * primvar->GetElementOffset() + instancerTransformRes->GetOffset()];
                GfMatrix4f m;

                int instanceDrawingCoord = instanceBar->GetElementOffset();

                for (int i = 0; i < numInstances; i++) {
                    
                    int instanceIndex = instanceBuffer[i * instanceIndexWidth + 1] + instanceDrawingCoord;
                    
                    // instance coordinates

                    if (instanceTransformRes) {
                        // Instance transform
                        stride = instanceTransformRes->GetStride();
                        rawBuffer = _GetBufferContents(instanceTransformRes);
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
                        rawBuffer = _GetBufferContents(scaleRes);
                        scale = *(GfVec3f const*)&rawBuffer[stride * instanceIndex];
                    }
                    
                    if (rotateRes) {
                        stride = rotateRes->GetStride();
                        rawBuffer = _GetBufferContents(rotateRes);
                        float const* const floatArray = (float const*)&rawBuffer[stride * instanceIndex];
                        rotate = GfQuaternion(floatArray[0], GfVec3d(floatArray[1], floatArray[2], floatArray[3]));
                    }
                    
                    if (translateRes) {
                        stride = translateRes->GetStride();
                        rawBuffer = _GetBufferContents(translateRes);
                        translate = *(GfVec3f const*)&rawBuffer[stride * instanceIndex];
                    }
                    
                    GfMatrix4f mtxScale, mtxRotate, mtxTranslate;
                    mtxScale.SetScale(scale);
                    mtxRotate.SetRotate(rotate);
                    mtxTranslate.SetTranslate(translate);
                    
                    m = (*itemTransform) * (m * mtxScale * mtxRotate * mtxTranslate * (*instancerTransform));

                    GfBBox3f box(GetBounds().GetRange(), m);
                    _instancedCullingBounds.push_back(_BakeBoundsTransform(box));
                }
            }
            
            static bool perInstanceCulling = false;

            if (!perInstanceCulling) {
                _numVisible = _instancedCullingBounds.size();

                for(auto& bounds : _instancedCullingBounds) {
                    if (GfFrustum::IntersectsViewVolumeFloat(bounds, viewProjMatrix, windowDimensions))
                        return true;
                }
                return false;
            }

            bool result = false;
            HdStBufferResourceSharedPtr const & culledInstanceIndexRes = instanceIndexRangeGL->GetResource(HdInstancerTokens->culledInstanceIndices);

            uint8_t *culledInstanceIndexBuffer = const_cast<uint8_t*>(_GetBufferContents(culledInstanceIndexRes));
            uint32_t *culledInstanceBuffer = reinterpret_cast<uint32_t*>(culledInstanceIndexBuffer) + instanceOffset;
            
            bool modified = false;
            _numVisible = 0;
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
                    _numVisible++;
                }
            }

            if (modified) {
#if defined(PXR_METAL_SUPPORT_ENABLED)
                id<MTLBuffer> metalBuffer_id = HgiMetalBuffer::MTLBuffer(culledInstanceIndexRes->GetId());
                
                uint32_t start = instanceOffset * sizeof(uint32_t);
                uint32_t length = _numVisible * sizeof(uint32_t) * instanceIndexWidth;
                MtlfMetalContext::GetMetalContext()->QueueBufferFlush(metalBuffer_id, start, start + length);
#endif
            }
            
            return result;
        }
        // We don't process multiple levels of instancer yet
        return true;
    }
    else {
        if (!_instancedCullingBoundsCalculated) {
            _instancedCullingBoundsCalculated = true;
            _instancedCullingBounds.push_back(_BakeBoundsTransform(GetBounds()));
        }
        if( GfFrustum::IntersectsViewVolumeFloat(_instancedCullingBounds.front(), viewProjMatrix, windowDimensions)) {
            return true;
        }
        return false;
    }
}

void
HdStDrawItem::CalculateCullingBounds() const
{
    if (_instancedCullingBoundsCalculated) {
       return;
    }

    HdBufferArrayRangeSharedPtr const & instanceIndexRange = GetInstanceIndexRange();
    if (instanceIndexRange) {
        HdStBufferArrayRangeSharedPtr instanceIndexRangeGL = std::static_pointer_cast<HdStBufferArrayRange>(instanceIndexRange);
        int instancerNumLevels = GetInstancePrimvarNumLevels();
        int instanceIndexWidth = instancerNumLevels + 1;
        int numInstances = instanceIndexRange->GetNumElements() / instanceIndexWidth;
        
        if (instancerNumLevels == 1) {
            int instanceOffset = instanceIndexRange->GetElementOffset();
            
            HdStBufferResourceSharedPtr const & instanceIndexRes = instanceIndexRangeGL->GetResource(HdInstancerTokens->instanceIndices);
            
            uint8_t *instanceIndexBuffer = const_cast<uint8_t*>(_GetBufferContents(instanceIndexRes));
            uint32_t *instanceBuffer = reinterpret_cast<uint32_t*>(instanceIndexBuffer) + instanceOffset;
            
            HdBufferArrayRangeSharedPtr const & primvar = GetConstantPrimvarRange();
            HdStBufferArrayRangeSharedPtr primvarGL = std::static_pointer_cast<HdStBufferArrayRange>(primvar);
            HdStBufferResourceSharedPtr const & transformRes = primvarGL->GetResource(HdTokens->transform);
            HdStBufferResourceSharedPtr const & instancerTransformRes = primvarGL->GetResource(HdInstancerTokens->instancerTransform);

            HdBufferArrayRangeSharedPtr const & instanceBar = GetInstancePrimvarRange(0);
            HdStBufferArrayRangeSharedPtr instanceBarGL = std::static_pointer_cast<HdStBufferArrayRange>(instanceBar);
            HdStBufferResourceSharedPtr const & instanceTransformRes = instanceBarGL->GetResource(HdInstancerTokens->instanceTransform);
            HdStBufferResourceSharedPtr const & translateRes = instanceBarGL->GetResource(HdTokens->translate);
            HdStBufferResourceSharedPtr const & rotateRes = instanceBarGL->GetResource(HdTokens->rotate);
            HdStBufferResourceSharedPtr const & scaleRes = instanceBarGL->GetResource(HdTokens->scale);
            
            // Item transform
            size_t stride = transformRes->GetStride();
            uint8_t const* rawBuffer = _GetBufferContents(transformRes);
            GfMatrix4f const *itemTransform =
            (GfMatrix4f const*)&rawBuffer[stride * primvar->GetElementOffset() + transformRes->GetOffset()];
            
            // Instancer transform
            stride = instancerTransformRes->GetStride();
            rawBuffer = _GetBufferContents(instancerTransformRes);
            GfMatrix4f const *instancerTransform =
            (GfMatrix4f const*)&rawBuffer[stride * primvar->GetElementOffset() + instancerTransformRes->GetOffset()];
            GfMatrix4f m;
            
            int instanceDrawingCoord = instanceBar->GetElementOffset();
            
            _instancedCullingBounds.clear();
                
            for (int i = 0; i < numInstances; i++) {
                
                int instanceIndex = instanceBuffer[i * instanceIndexWidth + 1] + instanceDrawingCoord;
                
                // instance coordinates
                
                if (instanceTransformRes) {
                    // Instance transform
                    stride = instanceTransformRes->GetStride();
                    rawBuffer = _GetBufferContents(instanceTransformRes);
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
                    rawBuffer = _GetBufferContents(scaleRes);
                    scale = *(GfVec3f const*)&rawBuffer[stride * instanceIndex];
                }
                
                if (rotateRes) {
                    stride = rotateRes->GetStride();
                    rawBuffer = _GetBufferContents(rotateRes);
                    float const* const floatArray = (float const*)&rawBuffer[stride * instanceIndex];
                    rotate = GfQuaternion(floatArray[0], GfVec3d(floatArray[1], floatArray[2], floatArray[3]));
                }
                
                if (translateRes) {
                    stride = translateRes->GetStride();
                    rawBuffer = _GetBufferContents(translateRes);
                    translate = *(GfVec3f const*)&rawBuffer[stride * instanceIndex];
                }
                
                GfMatrix4f mtxScale, mtxRotate, mtxTranslate;
                mtxScale.SetScale(scale);
                mtxRotate.SetRotate(rotate);
                mtxTranslate.SetTranslate(translate);
                
                m = (*itemTransform) * (m * mtxScale * mtxRotate * mtxTranslate * (*instancerTransform));
                
                GfBBox3f box(GetBounds().GetRange(), m);
                _instancedCullingBounds.push_back(_BakeBoundsTransform(box));
            }
        }
        else {
            TF_CODING_WARNING("Only expected to find one instance level, found %d with %d instances", instancerNumLevels, numInstances);
            _instancedCullingBounds.push_back(_BakeBoundsTransform(GetBounds()));
        }
    }
    else {
        _instancedCullingBounds.push_back(_BakeBoundsTransform(GetBounds()));
    }
    
    _instancedCullingBoundsCalculated = true;
}

int 
HdStDrawItem::BuildInstanceBuffer(uint8_t** instanceVisibility) const
{
    int numItems = _instancedCullingBounds.size();
    int i;

    int instancerNumLevels = GetInstancePrimvarNumLevels();
    int instanceIndexWidth = instancerNumLevels + 1;
    
    if (instanceIndexWidth != 2) {
        // We use 64 bit read/writes below for a more efficient copy
        TF_FATAL_CODING_ERROR("Only expected to find one instance level, found %d", instancerNumLevels);
        return 0;
    }

    HdBufferArrayRangeSharedPtr const & instanceIndexRange = GetInstanceIndexRange();
    HdStBufferArrayRangeSharedPtr instanceIndexRangeGL = std::static_pointer_cast<HdStBufferArrayRange>(instanceIndexRange);
    int instanceOffset = instanceIndexRange->GetElementOffset();
    
    HdStBufferResourceSharedPtr const & instanceIndexRes = instanceIndexRangeGL->GetResource(HdInstancerTokens->instanceIndices);
    
    uint8_t *instanceIndexBuffer = const_cast<uint8_t*>(_GetBufferContents(instanceIndexRes));
    
    _numVisible = 0;

    if (!instanceIndexBuffer) {
        return 0;
    }
    
    uint32_t *instanceBuffer = reinterpret_cast<uint32_t*>(instanceIndexBuffer) + instanceOffset;

    HdStBufferResourceSharedPtr const & culledInstanceIndexRes = instanceIndexRangeGL->GetResource(HdInstancerTokens->culledInstanceIndices);
    
    uint8_t *culledInstanceIndexBuffer = const_cast<uint8_t*>(_GetBufferContents(culledInstanceIndexRes));
    uint32_t *culledInstanceBuffer = reinterpret_cast<uint32_t*>(culledInstanceIndexBuffer) + instanceOffset;
    uint64_t *instanceBuffer64 = reinterpret_cast<uint64_t*>(instanceBuffer);
    uint64_t *culledInstanceBuffer64 = reinterpret_cast<uint64_t*>(culledInstanceBuffer);
    
    bool modified = false;

    for(i = 0; i < numItems; i++) {
        if (!*instanceVisibility[i])
            continue;
        
        _numVisible++;

        uint64_t instanceIndex64 = instanceBuffer64[i];
        if (*culledInstanceBuffer64 != instanceIndex64) {
            *culledInstanceBuffer64++ = instanceIndex64;
            
            // Exit early, and perform a more efficient loop for the remainder
            i++;
            modified = true;
            break;
        }
        else {
            culledInstanceBuffer64++;
        }
    }

    if (modified) {
        for(; i < numItems; i++) {
            if (!*instanceVisibility[i])
                continue;

            _numVisible++;
            *culledInstanceBuffer64++ = instanceBuffer64[i];
        }

#if defined(ARCH_OS_MACOS)
        id<MTLBuffer> metalBuffer_id = HgiMetalBuffer::MTLBuffer(culledInstanceIndexRes->GetId());

        uint32_t start = instanceOffset * sizeof(uint32_t);
        uint32_t length = _numVisible * sizeof(uint32_t) * 2;
        MtlfMetalContext::GetMetalContext()->QueueBufferFlush(metalBuffer_id, start, start + length);
#endif
    }
    
    return _numVisible;
}

PXR_NAMESPACE_CLOSE_SCOPE

