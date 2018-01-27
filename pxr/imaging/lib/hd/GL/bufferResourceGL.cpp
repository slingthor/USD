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

#include "pxr/imaging/hd/GL/bufferResourceGL.h"

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

HdBufferResourceGL::HdBufferResourceGL(TfToken const &role,
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

HdBufferResourceGL::~HdBufferResourceGL()
{
    TF_VERIFY(_texId == 0);
}

void
HdBufferResourceGL::SetAllocation(HdBufferResourceGPUHandle id, size_t size)
{
    _id = (GLuint)(uint64_t)id;
    HdResource::SetSize(size);

    HdRenderContextCaps const & caps = HdRenderContextCaps::GetInstance();

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

GLuint
HdBufferResourceGL::GetTextureBuffer()
{
    // XXX: need change tracking.

    if (_texId == 0) {
        glGenTextures(1, &_texId);

        GLenum format = GL_R32F;
        if (_glDataType == GL_FLOAT) {
            if (_numComponents <= 4) {
                static const GLenum floats[]
                    = { GL_R32F, GL_RG32F, GL_RGB32F, GL_RGBA32F };
                format = floats[_numComponents-1];
            }
        } else if (_glDataType == GL_INT) {
            if (_numComponents <= 4) {
                static const GLenum ints[]
                    = { GL_R32I, GL_RG32I, GL_RGB32I, GL_RGBA32I };
                format = ints[_numComponents-1];
            }
        } else {
            TF_CODING_ERROR("unsupported type: 0x%x numComponents = %d\n",
                            _glDataType, _numComponents);
        }

        glBindTexture(GL_TEXTURE_BUFFER, _texId);
        glTexBuffer(GL_TEXTURE_BUFFER, format, _id);
        glBindTexture(GL_TEXTURE_BUFFER, 0);
    }
    return _texId;
}

void
HdBufferResourceGL::CopyData(size_t vboOffset, size_t dataSize, void const *data)
{
    HdRenderContextCaps const &caps = HdRenderContextCaps::GetInstance();
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
HdBufferResourceGL::ReadBuffer(int glDataType,
                               int numComponents,
                               int arraySize,
                               int vboOffset,
                               int stride,
                               int numElements)
{
    if (glBufferSubData == NULL) return VtValue();
    
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
    
    if (caps.directStateAccessEnabled) {
        glGetNamedBufferSubDataEXT(_id, vboOffset, vboSize, &tmp[0]);
    } else {
        glBindBuffer(GL_ARRAY_BUFFER, _id);
        glGetBufferSubData(GL_ARRAY_BUFFER, vboOffset, vboSize, &tmp[0]);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    
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
HdBufferResourceGL::GetBufferContents() const
{
    void* bufferData;

    HdRenderContextCaps const &caps = HdRenderContextCaps::GetInstance();
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
