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

#include "pxr/imaging/hd/Metal/bufferResourceMetal.h"

#include "pxr/imaging/hd/conversions.h"
#include "pxr/imaging/hd/renderContextCaps.h"

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
               std::vector<unsigned char> const &data)
{
    VtArray<T> array(numElements*arraySize);
    if (numElements == 0)
    return VtValue(array);
    
    const unsigned char *src = &data[0];
    unsigned char *dst = (unsigned char *)array.data();
    
    TF_VERIFY(data.size() == stride*(numElements-1) + arraySize*sizeof(T));
    
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

HdBufferResourceMetal::HdBufferResourceMetal(TfToken const &role,
                                   int glDataType,
                                   short numComponents,
                                   int arraySize,
                                   int offset,
                                   int stride)
    : HdBufferResource(role, glDataType, numComponents, arraySize, offset, stride),
      _gpuAddr(0),
      _texId(0),
      _id(0)
{
    /*NOTHING*/
}

HdBufferResourceMetal::~HdBufferResourceMetal()
{
    TF_VERIFY(_texId == 0);
}

void
HdBufferResourceMetal::SetAllocation(HdBufferResourceGPUHandle idBuffer, size_t size)
{
    // release texid if exist. SetAllocation is guaranteed to be called
    // at the destruction of the hosting buffer array.
    if (_texId) {
        [_texId release];
        _texId = nil;
    }

    _id = (__bridge id<MTLBuffer>)idBuffer;
    HdResource::SetSize(size);

    _gpuAddr = 0;
}

HdBufferResourceGPUHandle
HdBufferResourceMetal::GetTextureBuffer()
{
    // XXX: need change tracking.

    if (_texId == nil) {
        MTLPixelFormat format = MTLPixelFormatR32Float;
        if (_glDataType == GL_FLOAT) {
            if (_numComponents <= 4) {
                static const MTLPixelFormat floats[]
                    = { MTLPixelFormatR32Float, MTLPixelFormatRG32Float, MTLPixelFormatInvalid, MTLPixelFormatRGBA32Float };
                format = floats[_numComponents-1];
            }
        } else if (_glDataType == GL_INT) {
            if (_numComponents <= 4) {
                static const MTLPixelFormat ints[]
                    = { MTLPixelFormatR32Sint, MTLPixelFormatRG32Sint, MTLPixelFormatInvalid, MTLPixelFormatRGBA32Sint };
                format = ints[_numComponents-1];
            }
        } else {
            TF_CODING_ERROR("unsupported type: 0x%x numComponents = %d\n",
                            _glDataType, _numComponents);
        }

        if (format==MTLPixelFormatInvalid)
        {
            TF_CODING_ERROR("Invalid buffer format for representation as texture");
        }
        
        size_t pixelSize = GetComponentSize() * _numComponents;
        size_t numPixels = [_id length] / pixelSize;
        
        MTLTextureDescriptor* texDesc =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                           width:numPixels
                                                          height:1
                                                       mipmapped:NO];
        _texId = [_id newTextureWithDescriptor:texDesc offset:0 bytesPerRow:pixelSize * numPixels];
    }
    return (__bridge HdBufferResourceGPUHandle)_texId;
}

void
HdBufferResourceMetal::CopyData(size_t vboOffset, size_t dataSize, void const *data)
{
    memcpy((uint8*)[_id contents] + vboOffset, data, dataSize);
    [_id didModifyRange:NSMakeRange(vboOffset, dataSize)];
}

VtValue
HdBufferResourceMetal::ReadBuffer(int glDataType,
                                  int numComponents,
                                  int arraySize,
                                  int vboOffset,
                                  int stride,
                                  int numElements)
{
    int bytesPerElement = numComponents *
    HdConversions::GetComponentSize(glDataType);
    if (stride == 0) stride = bytesPerElement;
    TF_VERIFY(stride >= bytesPerElement);
    
    // +---------+---------+---------+
    // |   :SRC: |   :SRC: |   :SRC: |
    // +---------+---------+---------+
    //     <-------read range------>
    //     |       ^           | ^ |
    //     | stride * (n -1)   |   |
    //                       bytesPerElement
    
    GLsizeiptr vboSize = stride * (numElements-1) + bytesPerElement * arraySize;
    
    HdRenderContextCaps const &caps = HdRenderContextCaps::GetInstance();
    
    // read data
    std::vector<unsigned char> tmp(vboSize);
    
    memcpy(&tmp[0], (uint8_t*)[_id contents] + vboOffset, vboSize);
    
    VtValue result;
    // create VtArray
    switch(glDataType){
        case GL_BYTE:
        switch(numComponents) {
            case 1: result = _CreateVtArray<char>(numElements, arraySize, stride, tmp); break;
            default: break;
        }
        break;
        case GL_SHORT:
        switch(numComponents) {
            case 1: result = _CreateVtArray<short>(numElements, arraySize, stride, tmp); break;
            default: break;
        }
        break;
        case GL_UNSIGNED_SHORT:
        switch(numComponents) {
            case 1: result = _CreateVtArray<unsigned short>(numElements, arraySize, stride, tmp); break;
            default: break;
        }
        break;
        case GL_INT:
        switch(numComponents) {
            case 1: result = _CreateVtArray<int>(numElements, arraySize, stride, tmp); break;
            case 2: result = _CreateVtArray<GfVec2i>(numElements, arraySize, stride, tmp); break;
            case 3: result = _CreateVtArray<GfVec3i>(numElements, arraySize, stride, tmp); break;
            case 4: result = _CreateVtArray<GfVec4i>(numElements, arraySize, stride, tmp); break;
            default: break;
        }
        break;
        case GL_FLOAT:
        switch(numComponents) {
            case 1: result = _CreateVtArray<float>(numElements, arraySize, stride, tmp); break;
            case 2: result = _CreateVtArray<GfVec2f>(numElements, arraySize, stride, tmp); break;
            case 3: result = _CreateVtArray<GfVec3f>(numElements, arraySize, stride, tmp); break;
            case 4: result = _CreateVtArray<GfVec4f>(numElements, arraySize, stride, tmp); break;
            case 16: result = _CreateVtArray<GfMatrix4f>(numElements, arraySize, stride, tmp); break;
            default: break;
        }
        break;
        case GL_DOUBLE:
        switch(numComponents) {
            case 1: result = _CreateVtArray<double>(numElements, arraySize, stride, tmp); break;
            case 2: result = _CreateVtArray<GfVec2d>(numElements, arraySize, stride, tmp); break;
            case 3: result = _CreateVtArray<GfVec3d>(numElements, arraySize, stride, tmp); break;
            case 4: result = _CreateVtArray<GfVec4d>(numElements, arraySize, stride, tmp); break;
            case 16: result = _CreateVtArray<GfMatrix4d>(numElements, arraySize, stride, tmp); break;
            default: break;
        }
        break;
        default:
        TF_CODING_ERROR("Invalid data type");
        break;
    }
    
    return result;
}

uint8_t const*
HdBufferResourceMetal::GetBufferContents() const
{
    return (uint8_t const*)[_id contents];
}

PXR_NAMESPACE_CLOSE_SCOPE
