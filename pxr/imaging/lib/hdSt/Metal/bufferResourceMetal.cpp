//
// Copyright 2017 Pixar
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
#include "pxr/imaging/mtlf/mtlDevice.h"

#include "pxr/imaging/glf/glew.h"

#include "pxr/imaging/hdSt/Metal/bufferResourceMetal.h"
#include "pxr/imaging/hdSt/GL/glConversions.h"

#include "pxr/base/gf/vec2d.h"
#include "pxr/base/gf/vec2f.h"
#include "pxr/base/gf/vec2i.h"
#include "pxr/base/gf/vec3d.h"
#include "pxr/base/gf/vec3f.h"
#include "pxr/base/gf/vec3i.h"
#include "pxr/base/gf/vec4d.h"
#include "pxr/base/gf/vec4f.h"
#include "pxr/base/gf/vec4i.h"
#include "pxr/base/gf/matrix4f.h"
#include "pxr/base/gf/matrix4d.h"

#include "pxr/base/tf/diagnostic.h"
#include "pxr/base/tf/staticTokens.h"

#include "pxr/base/vt/array.h"
#include "pxr/base/vt/value.h"


PXR_NAMESPACE_OPEN_SCOPE

template <typename T>
VtValue
_CreateVtArray(int numElements, int arraySize, int stride,
               uint8_t const* const data, size_t dataSize)
{
    VtArray<T> array(numElements*arraySize);
    if (numElements == 0)
    return VtValue(array);
    
    const unsigned char *src = data;
    unsigned char *dst = (unsigned char *)array.data();
    
    TF_VERIFY(dataSize == stride*(numElements-1) + arraySize*sizeof(T));
    
    if (stride == sizeof(T)) {
        memcpy(dst, src, numElements*arraySize*sizeof(T));
    } else {
        // deinterleaving
        for (int i = 0; i < numElements; ++i) {
            memcpy(dst, src, arraySize*sizeof(T));
            dst += arraySize*sizeof(T);
            src += stride;
        }
    }
    return VtValue(array);
}

HdStBufferResourceMetal::HdStBufferResourceMetal(TfToken const &role,
                                                 HdTupleType tupleType,
                                                 int offset,
                                                 int stride)
    : HdStBufferResource(role, tupleType, offset, stride),
      _gpuAddr(0),
      _texId(0),
      _id(0)
{
    /*NOTHING*/
}

HdStBufferResourceMetal::~HdStBufferResourceMetal()
{
    TF_VERIFY(_texId == 0);
}

void
HdStBufferResourceMetal::SetAllocation(HdResourceGPUHandle idBuffer, size_t size)
{
    // release texid if exist. SetAllocation is guaranteed to be called
    // at the destruction of the hosting buffer array.
    if (_texId) {
        [_texId release];
        _texId = nil;
    }

    _id = idBuffer;
    HdResource::SetSize(size);

    _gpuAddr = 0;
}

GarchTextureGPUHandle
HdStBufferResourceMetal::GetTextureBuffer()
{
    // XXX: need change tracking.

    if (_texId == nil) {
        MTLPixelFormat format = MTLPixelFormatInvalid;
        switch(_tupleType.type) {
            case HdTypeFloat:
                format = MTLPixelFormatR32Float;
                break;
            case HdTypeFloatVec2:
                format = MTLPixelFormatRG32Float;
                break;
            case HdTypeFloatVec4:
                format = MTLPixelFormatRGBA32Float;
                break;
            case HdTypeInt32:
                format = MTLPixelFormatR32Sint;
                break;
            case HdTypeInt32Vec2:
                format = MTLPixelFormatRG32Sint;
                break;
            case HdTypeInt32Vec4:
                format = MTLPixelFormatRGBA32Sint;
                break;
            default:
                TF_CODING_ERROR("unsupported type: 0x%x\n", _tupleType.type);
        }

        if (format==MTLPixelFormatInvalid)
        {
            TF_CODING_ERROR("Invalid buffer format for representation as texture");
        }
        
        size_t pixelSize = HdDataSizeOfTupleType(_tupleType);
        size_t numPixels = [_id length] / pixelSize;

        MTLTextureDescriptor* texDesc =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format
                                                           width:numPixels
                                                          height:1
                                                       mipmapped:NO];
        _texId = [_id newTextureWithDescriptor:texDesc offset:0 bytesPerRow:pixelSize * numPixels];
    }
    return _texId;
}

void
HdStBufferResourceMetal::CopyData(size_t vboOffset, size_t dataSize, void const *data)
{
    memcpy((uint8*)[_id contents] + vboOffset, data, dataSize);
    [_id didModifyRange:NSMakeRange(vboOffset, dataSize)];
}

VtValue
HdStBufferResourceMetal::ReadBuffer(HdTupleType tupleType,
                                    int vboOffset,
                                    int stride,
                                    int numElems)
{
    const int bytesPerElement = HdDataSizeOfTupleType(tupleType);
    const int arraySize = tupleType.count;
    
    if (stride == 0) stride = bytesPerElement;
    TF_VERIFY(stride >= bytesPerElement);
    
    // +---------+---------+---------+
    // |   :SRC: |   :SRC: |   :SRC: |
    // +---------+---------+---------+
    //     <-------read range------>
    //     |       ^           | ^ |
    //     | stride * (n -1)   |   |
    //                       bytesPerElement
    
    GLsizeiptr vboSize = stride * (numElems-1) + bytesPerElement * arraySize;
    
    // read data
    std::vector<unsigned char> tmp(vboSize);
    
    uint8_t const* const data = (uint8_t*)[_id contents];
    size_t const dataSize = [_id length];
    
    VtValue result;
    // create VtArray
    switch (tupleType.type) {
        case HdTypeInt8:
            return _CreateVtArray<char>(numElems, arraySize, stride, data, dataSize);
        case HdTypeInt16:
            return _CreateVtArray<int16_t>(numElems, arraySize, stride, data, dataSize);
        case HdTypeUInt16:
            return _CreateVtArray<uint16_t>(numElems, arraySize, stride, data, dataSize);
        case HdTypeUInt32:
            return _CreateVtArray<uint32_t>(numElems, arraySize, stride, data, dataSize);
        case HdTypeInt32:
            return _CreateVtArray<int32_t>(numElems, arraySize, stride, data, dataSize);
        case HdTypeInt32Vec2:
            return _CreateVtArray<GfVec2i>(numElems, arraySize, stride, data, dataSize);
        case HdTypeInt32Vec3:
            return _CreateVtArray<GfVec3i>(numElems, arraySize, stride, data, dataSize);
        case HdTypeInt32Vec4:
            return _CreateVtArray<GfVec4i>(numElems, arraySize, stride, data, dataSize);
        case HdTypeFloat:
            return _CreateVtArray<float>(numElems, arraySize, stride, data, dataSize);
        case HdTypeFloatVec2:
            return _CreateVtArray<GfVec2f>(numElems, arraySize, stride, data, dataSize);
        case HdTypeFloatVec3:
            return _CreateVtArray<GfVec3f>(numElems, arraySize, stride, data, dataSize);
        case HdTypeFloatVec4:
            return _CreateVtArray<GfVec4f>(numElems, arraySize, stride, data, dataSize);
        case HdTypeFloatMat4:
            return _CreateVtArray<GfMatrix4f>(numElems, arraySize, stride, data, dataSize);
        case HdTypeDouble:
            return _CreateVtArray<double>(numElems, arraySize, stride, data, dataSize);
        case HdTypeDoubleVec2:
            return _CreateVtArray<GfVec2d>(numElems, arraySize, stride, data, dataSize);
        case HdTypeDoubleVec3:
            return _CreateVtArray<GfVec3d>(numElems, arraySize, stride, data, dataSize);
        case HdTypeDoubleVec4:
            return _CreateVtArray<GfVec4d>(numElems, arraySize, stride, data, dataSize);
        case HdTypeDoubleMat4:
            return _CreateVtArray<GfMatrix4d>(numElems, arraySize, stride, data, dataSize);
        default:
            TF_CODING_ERROR("Unhandled data type %i", tupleType.type);
    }
    
    return VtValue();
}

uint8_t const*
HdStBufferResourceMetal::GetBufferContents() const
{
    return (uint8_t const*)[_id contents];
}

PXR_NAMESPACE_CLOSE_SCOPE
