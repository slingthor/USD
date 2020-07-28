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
#include "pxr/imaging/hdSt/bufferResourceGL.h"

#include "pxr/imaging/garch/contextCaps.h"
#include "pxr/imaging/garch/resourceFactory.h"

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

#include "pxr/imaging/hgi/blitCmds.h"
#include "pxr/imaging/hgi/blitCmdsOps.h"
#include "pxr/imaging/hgi/buffer.h"
#include "pxr/imaging/hgi/hgi.h"
#include "pxr/imaging/hgi/tokens.h"

#if defined(ARCH_OS_MACOS)
#include "pxr/imaging/hgiMetal/buffer.h"
#endif

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

HdStBufferResourceGL::HdStBufferResourceGL(TfToken const &role,
                                           HdTupleType tupleType,
                                           int offset,
                                           int stride)
    : HdBufferResource(role, tupleType, offset, stride)
    , _lastFrameModified(0)
    , _activeBuffer(0)
    , _firstFrameBeingFilled(true)
{
    for (int32_t i = 0; i < MULTIBUFFERING; i++) {
        _ids[i] = HgiBufferHandle();
        _gpuAddr[i] = 0;
    }
}

HdStBufferResourceGL::~HdStBufferResourceGL()
{
    /*NOTHING*/
}

void
HdStBufferResourceGL::SetAllocation(HgiBufferHandle const& id, size_t size)
{
    SetAllocations(id, id, id, size);
}

void
HdStBufferResourceGL::SetAllocations(HgiBufferHandle const& id0,
                                     HgiBufferHandle const& id1,
                                     HgiBufferHandle const& id2,
                                     size_t size)
{
    _ids[0] = id0;
    _ids[1] = id1;
    _ids[2] = id2;

#if defined(PXR_METAL_SUPPORT_ENABLED)
    for (int32_t i = 0; i < MULTIBUFFERING; i++) {
        id<MTLBuffer> b = HgiMetalBuffer::MTLBuffer(_ids[i]);
        if (b) {
            _gpuAddr[i] = (uint64_t)[b contents];
        }
        else {
            _gpuAddr[i] = 0;
        }
    }
    
    MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();
    if (context) {
        _lastFrameModified = context->GetCurrentFrame();
    }
    _activeBuffer = 0;
    id<MTLBuffer> b = HgiMetalBuffer::MTLBuffer(_ids[1]);
    _firstFrameBeingFilled = b != nil;
#else
    for (int32_t i = 0; i < MULTIBUFFERING; i++) {
        _gpuAddr[i] = 0;
    }
    _activeBuffer = 0;
    _firstFrameBeingFilled = false;
#endif

    HdResource::SetSize(size);
}

// APPLE METAL: Multibuffering support.
void HdStBufferResourceGL::CopyData(Hgi* hgi,
                                    size_t vboOffset,
                                    size_t dataSize,
                                    void const *data)
{
    MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();
    
    if (_ids[1]) {
        int64_t currentFrame = context->GetCurrentFrame();
        
        if (currentFrame != _lastFrameModified) {
            _firstFrameBeingFilled = false;
            _activeBuffer++;
            _activeBuffer = (_activeBuffer < MULTIBUFFERING) ? _activeBuffer : 0;
        }
        _lastFrameModified = currentFrame;
    }

    HgiBlitCmdsUniquePtr blitCmds = hgi->CreateBlitCmds();
    HgiBufferCpuToGpuOp blitOp;
    blitOp.byteSize = dataSize;
    blitOp.cpuSourceBuffer = data;
    blitOp.sourceByteOffset = 0;
    blitOp.gpuDestinationBuffer = GetId();
    blitOp.destinationByteOffset = vboOffset;
    blitCmds->CopyBufferCpuToGpu(blitOp);
    hgi->SubmitCmds(blitCmds.get());
}

// APPLE METAL: Multibuffering support.
VtValue HdStBufferResourceGL::ReadBuffer(Hgi* hgi,
                                         HdTupleType tupleType,
                                         int vboOffset,
                                         int stride,
                                         int numElems)
{
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
    
    // read data
    uint8_t* data = nullptr;
    size_t dataSize = 0;

#if defined(PXR_OPENGL_SUPPORT_ENABLED)
    GarchContextCaps const &caps = GarchResourceFactory::GetInstance()->GetContextCaps();
    
    if (!caps.hasSubDataCopy) return VtValue();
    
    GLsizeiptr vboSize = stride * (numElems - 1) + bytesPerElement * arraySize;
    std::vector<uint8_t> tmp(vboSize);
    
    if (hgi->GetAPIName() == HgiTokens->OpenGL) {
        data = tmp.data();
        dataSize = vboSize;

        if (caps.directStateAccessEnabled) {
            glGetNamedBufferSubDataEXT(_ids[_activeBuffer]->GetRawResource(), vboOffset, vboSize, data);
        } else {
            glBindBuffer(GL_ARRAY_BUFFER, _ids[_activeBuffer]->GetRawResource());
            glGetBufferSubData(GL_ARRAY_BUFFER, vboOffset, vboSize, data);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
    }
    else
#endif
#if defined(PXR_METAL_SUPPORT_ENABLED)
    if (hgi->GetAPIName() == HgiTokens->Metal) {
        data = (uint8_t*)_gpuAddr[_activeBuffer];
        dataSize = GetSize();
    }
    else
#endif
    {
        TF_FATAL_CODING_ERROR("No valid rendering API specified");
    }

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

PXR_NAMESPACE_CLOSE_SCOPE
