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
#include "pxr/imaging/glf/glew.h"

#include "pxr/imaging/hdSt/GL/bufferResourceGL.h"
#include "pxr/imaging/hdSt/GL/glConversions.h"
#include "pxr/imaging/hdSt/renderContextCaps.h"


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

HdStBufferResourceGL::HdStBufferResourceGL(TfToken const &role,
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

HdStBufferResourceGL::~HdStBufferResourceGL()
{
    TF_VERIFY(_texId == 0);
}

void
HdStBufferResourceGL::SetAllocation(HdBufferResourceGPUHandle id, size_t size)
{
    _id = (GLuint)(uint64_t)id;
    HdResource::SetSize(size);

    HdStRenderContextCaps const & caps = HdStRenderContextCaps::GetInstance();

    // note: gpu address remains valid until the buffer object is deleted,
    // or when the data store is respecified via BufferData/BufferStorage.
    // It doesn't change even when we make the buffer resident or non-resident.
    // https://www.opengl.org/registry/specs/NV/shader_buffer_load.txt
    if (id != 0 && caps.bindlessBufferEnabled) {
        glGetNamedBufferParameterui64vNV(
            _id, GL_BUFFER_GPU_ADDRESS_NV, (GLuint64EXT*)&_gpuAddr);
    } else {
        _gpuAddr = 0;
    }

    // release texid if exist. SetAllocation is guaranteed to be called
    // at the destruction of the hosting buffer array.
    if (_texId) {
        glDeleteTextures(1, &_texId);
        _texId = 0;
    }
}

GarchTextureGPUHandle
HdStBufferResourceGL::GetTextureBuffer()
{
    // XXX: need change tracking.

    if (_tupleType.count != 1) {
        TF_CODING_ERROR("unsupported tuple size: %zu\n", _tupleType.count);
        return GarchTextureGPUHandle();
    }
    
    if (_texId == 0) {
        glGenTextures(1, &_texId);
        
        
        GLenum format = GL_R32F;
        switch(_tupleType.type) {
            case HdTypeFloat:
                format = GL_R32F;
                break;
            case HdTypeFloatVec2:
                format = GL_RG32F;
                break;
            case HdTypeFloatVec3:
                format = GL_RGB32F;
                break;
            case HdTypeFloatVec4:
                format = GL_RGBA32F;
                break;
            case HdTypeInt32:
                format = GL_R32I;
                break;
            case HdTypeInt32Vec2:
                format = GL_RG32I;
                break;
            case HdTypeInt32Vec3:
                format = GL_RGB32I;
                break;
            case HdTypeInt32Vec4:
                format = GL_RGBA32I;
                break;
            default:
                TF_CODING_ERROR("unsupported type: 0x%x\n", _tupleType.type);
        }
        
        glBindTexture(GL_TEXTURE_BUFFER, _texId);
        glTexBuffer(GL_TEXTURE_BUFFER, format, (GLuint)(uint64_t)GetId());
        glBindTexture(GL_TEXTURE_BUFFER, 0);
    }
    return _texId;
}

void
HdStBufferResourceGL::CopyData(size_t vboOffset, size_t dataSize, void const *data)
{
    HdStRenderContextCaps const &caps = HdStRenderContextCaps::GetInstance();
    if (ARCH_LIKELY(caps.directStateAccessEnabled)) {
        glNamedBufferSubDataEXT(_id,
                                vboOffset,
                                dataSize,
                                data);
    } else {
        glBindBuffer(GL_ARRAY_BUFFER, _id);
        glBufferSubData(GL_ARRAY_BUFFER, vboOffset, dataSize, data);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}

VtValue
HdStBufferResourceGL::ReadBuffer(HdTupleType tupleType,
                                 int vboOffset,
                                 int stride,
                                 int numElems)
{
    if (glBufferSubData == NULL) return VtValue();
    
    // HdTupleType represents scalar, vector, matrix, and array types.
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
    
    HdStRenderContextCaps const &caps = HdStRenderContextCaps::GetInstance();
    
    // read data
    std::vector<unsigned char> tmp(vboSize);
    
    if (caps.directStateAccessEnabled) {
        glGetNamedBufferSubDataEXT(_id, vboOffset, vboSize, &tmp[0]);
    } else {
        glBindBuffer(GL_ARRAY_BUFFER, _id);
        glGetBufferSubData(GL_ARRAY_BUFFER, vboOffset, vboSize, &tmp[0]);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    
    VtValue result;
    // create VtArray
    // Convert data to Vt
    switch (tupleType.type) {
        case HdTypeInt8:
            return _CreateVtArray<char>(numElems, arraySize, stride, tmp);
        case HdTypeInt16:
            return _CreateVtArray<int16_t>(numElems, arraySize, stride, tmp);
        case HdTypeUInt16:
            return _CreateVtArray<uint16_t>(numElems, arraySize, stride, tmp);
        case HdTypeUInt32:
            return _CreateVtArray<uint32_t>(numElems, arraySize, stride, tmp);
        case HdTypeInt32:
            return _CreateVtArray<int32_t>(numElems, arraySize, stride, tmp);
        case HdTypeInt32Vec2:
            return _CreateVtArray<GfVec2i>(numElems, arraySize, stride, tmp);
        case HdTypeInt32Vec3:
            return _CreateVtArray<GfVec3i>(numElems, arraySize, stride, tmp);
        case HdTypeInt32Vec4:
            return _CreateVtArray<GfVec4i>(numElems, arraySize, stride, tmp);
        case HdTypeFloat:
            return _CreateVtArray<float>(numElems, arraySize, stride, tmp);
        case HdTypeFloatVec2:
            return _CreateVtArray<GfVec2f>(numElems, arraySize, stride, tmp);
        case HdTypeFloatVec3:
            return _CreateVtArray<GfVec3f>(numElems, arraySize, stride, tmp);
        case HdTypeFloatVec4:
            return _CreateVtArray<GfVec4f>(numElems, arraySize, stride, tmp);
        case HdTypeFloatMat4:
            return _CreateVtArray<GfMatrix4f>(numElems, arraySize, stride, tmp);
        case HdTypeDouble:
            return _CreateVtArray<double>(numElems, arraySize, stride, tmp);
        case HdTypeDoubleVec2:
            return _CreateVtArray<GfVec2d>(numElems, arraySize, stride, tmp);
        case HdTypeDoubleVec3:
            return _CreateVtArray<GfVec3d>(numElems, arraySize, stride, tmp);
        case HdTypeDoubleVec4:
            return _CreateVtArray<GfVec4d>(numElems, arraySize, stride, tmp);
        case HdTypeDoubleMat4:
            return _CreateVtArray<GfMatrix4d>(numElems, arraySize, stride, tmp);
        default:
            TF_CODING_ERROR("Unhandled data type %i", tupleType.type);
    }
    return VtValue();
}

uint8_t const*
HdStBufferResourceGL::GetBufferContents() const
{
    void* bufferData;

    HdStRenderContextCaps const &caps = HdStRenderContextCaps::GetInstance();
    if (caps.directStateAccessEnabled) {
        bufferData = glMapNamedBufferEXT(_id, GL_READ_ONLY);
    } else {
        glBindBuffer(GL_ARRAY_BUFFER, _id);
        bufferData = glMapBuffer(GL_ARRAY_BUFFER, GL_READ_ONLY);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    return (uint8_t const*)bufferData;
}

PXR_NAMESPACE_CLOSE_SCOPE
