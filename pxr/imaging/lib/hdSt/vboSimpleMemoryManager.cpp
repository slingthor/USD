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
#include "pxr/imaging/glf/glew.h"

#include "pxr/base/tf/diagnostic.h"
#include "pxr/base/tf/envSetting.h"
#include "pxr/base/tf/iterator.h"

#include "pxr/imaging/hdSt/bufferResource.h"
#include "pxr/imaging/hdSt/GL/glConversions.h"
#include "pxr/imaging/hdSt/glUtils.h"
#include "pxr/imaging/hdSt/renderContextCaps.h"
#include "pxr/imaging/hdSt/vboSimpleMemoryManager.h"

#include "pxr/imaging/hdSt/GL/vboSimpleMemoryBufferGL.h"
#include "pxr/imaging/hdSt/Metal/vboSimpleMemoryBufferMetal.h"

#include "pxr/imaging/hd/bufferArrayRange.h"
#include "pxr/imaging/hd/bufferSource.h"
#include "pxr/imaging/hd/engine.h"
#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/tokens.h"

#include "pxr/imaging/hf/perfLog.h"

#include <atomic>

#include <boost/functional/hash.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

using namespace boost;

PXR_NAMESPACE_OPEN_SCOPE


extern TfEnvSetting<int> HD_MAX_VBO_SIZE;

// ---------------------------------------------------------------------------
//  HdStVBOSimpleMemoryManager
// ---------------------------------------------------------------------------

HdBufferArraySharedPtr
HdStVBOSimpleMemoryManager::CreateBufferArray(
    TfToken const &role,
    HdBufferSpecVector const &bufferSpecs)
{
    HdEngine::RenderAPI api = HdEngine::GetRenderAPI();
    switch(api)
    {
        case HdEngine::OpenGL:
            return boost::make_shared<HdStVBOSimpleMemoryBufferGL>(
                role, bufferSpecs);
#if defined(ARCH_GFX_METAL)
        case HdEngine::Metal:
            return boost::make_shared<HdStVBOSimpleMemoryBufferMetal>(
                role, bufferSpecs);
#endif
        default:
            TF_FATAL_CODING_ERROR("No HdStVBOSimpleMemoryBuffer for this API");
    }
    return NULL;
}

HdBufferArrayRangeSharedPtr
HdStVBOSimpleMemoryManager::CreateBufferArrayRange()
{
    return boost::make_shared<HdStVBOSimpleMemoryManager::_SimpleBufferArrayRange>();
}

HdAggregationStrategy::AggregationId
HdStVBOSimpleMemoryManager::ComputeAggregationId(
    HdBufferSpecVector const &bufferSpecs) const
{
    // Always returns different value
    static std::atomic_uint id(0);

    AggregationId hash = id++;  // Atomic

    return hash;
}

/// Returns the buffer specs from a given buffer array
HdBufferSpecVector 
HdStVBOSimpleMemoryManager::GetBufferSpecs(
    HdBufferArraySharedPtr const &bufferArray) const
{
    _SimpleBufferArraySharedPtr bufferArray_ =
        boost::static_pointer_cast<_SimpleBufferArray> (bufferArray);
    return bufferArray_->GetBufferSpecs();
}

/// Returns the size of the GPU memory used by the passed buffer array
size_t 
HdStVBOSimpleMemoryManager::GetResourceAllocation(
    HdBufferArraySharedPtr const &bufferArray, 
    VtDictionary &result) const 
{ 
    std::set<void*> idSet;
    size_t gpuMemoryUsed = 0;

    _SimpleBufferArraySharedPtr bufferArray_ =
        boost::static_pointer_cast<_SimpleBufferArray> (bufferArray);

    TF_FOR_ALL(resIt, bufferArray_->GetResources()) {
        HdBufferResourceSharedPtr const & resource = resIt->second;

        // XXX avoid double counting of resources shared within a buffer
        void *id = resource->GetId();
        if (idSet.count(id) == 0) {
            idSet.insert(id);

            std::string const & role = resource->GetRole().GetString();
            size_t size = size_t(resource->GetSize());

            if (result.count(role)) {
                size_t currentSize = result[role].Get<size_t>();
                result[role] = VtValue(currentSize + size);
            } else {
                result[role] = VtValue(size);
            }

            gpuMemoryUsed += size;
        }
    }

    return gpuMemoryUsed;
}

// ---------------------------------------------------------------------------
//  _SimpleBufferArray
// ---------------------------------------------------------------------------
HdStVBOSimpleMemoryManager::_SimpleBufferArray::_SimpleBufferArray(
    TfToken const &role,
    HdBufferSpecVector const &bufferSpecs)
    : HdBufferArray(role, TfToken()), _capacity(0), _maxBytesPerElement(0)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    // populate BufferResources
    TF_FOR_ALL(it, bufferSpecs) {
        int stride = HdDataSizeOfTupleType(it->tupleType);
        _AddResource(it->name, it->tupleType, /*offset=*/0, stride);
    }

    _SetMaxNumRanges(1);

    // compute max bytes / elements
    TF_FOR_ALL (it, GetResources()) {
        HdBufferResourceSharedPtr const &bres = it->second;
        _maxBytesPerElement = std::max(
            _maxBytesPerElement,
            HdDataSizeOfTupleType(bres->GetTupleType()));
    }
}

HdBufferResourceSharedPtr
HdStVBOSimpleMemoryManager::_SimpleBufferArray::_AddResource(
    TfToken const& name,
    HdTupleType tupleType,
                            int offset,
                            int stride)
{
    HD_TRACE_FUNCTION();
    if (TfDebug::IsEnabled(HD_SAFE_MODE)) {
        // duplication check
        HdBufferResourceSharedPtr bufferRes = GetResource(name);
        if (!TF_VERIFY(!bufferRes)) {
            return dynamic_pointer_cast<HdStBufferResource>(bufferRes);
        }
    }

    HdBufferResourceSharedPtr bufferRes = HdBufferResourceSharedPtr(
        HdStBufferResource::New(GetRole(), tupleType, offset, stride));
    _resourceList.emplace_back(name, bufferRes);
    return bufferRes;
}

HdStVBOSimpleMemoryManager::_SimpleBufferArray::~_SimpleBufferArray()
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    // invalidate buffer array range
    // (the range may still be held by drawItems)
    _SimpleBufferArrayRangeSharedPtr range = _GetRangeSharedPtr();
    if (range) {
        range->Invalidate();
    }
}


bool
HdStVBOSimpleMemoryManager::_SimpleBufferArray::GarbageCollect()
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    // no range referring this buffer = empty
    if (GetRangeCount() > 0 && GetRange(0).expired()) {
        _DeallocateResources();
        HD_PERF_COUNTER_INCR(HdPerfTokens->garbageCollectedVbo);
        return true;
    }
    return false;
}

void
HdStVBOSimpleMemoryManager::_SimpleBufferArray::DebugDump(std::ostream &out) const
{
    out << "  HdStVBOSimpleMemoryManager";
    out << "  total capacity = " << _capacity << "\n";
}

bool
HdStVBOSimpleMemoryManager::_SimpleBufferArray::Resize(int numElements)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    // see the comment in
    // HdStVBOMemoryManager::_StripedBufferArrayRange::Resize(int numElements)
    // this change is for the unit test consistency.
    //
    // if (_capacity < numElements) {
    if (_capacity != numElements) {
        _needsReallocation = true;
        return true;
    }
    return false;
}

size_t
HdStVBOSimpleMemoryManager::_SimpleBufferArray::GetMaxNumElements() const
{
    static size_t vboMaxSize = TfGetEnvSetting(HD_MAX_VBO_SIZE);
    return vboMaxSize / _maxBytesPerElement;
}

HdBufferResourceSharedPtr
HdStVBOSimpleMemoryManager::_SimpleBufferArray::GetResource() const
{
    HD_TRACE_FUNCTION();

    if (_resourceList.empty()) return HdStBufferResourceSharedPtr();

    if (TfDebug::IsEnabled(HD_SAFE_MODE)) {
        // make sure this buffer array has only one resource.
        void *id = _resourceList.begin()->second->GetId();
        TF_FOR_ALL (it, _resourceList) {
            if (it->second->GetId() != id) {
                TF_CODING_ERROR("GetResource(void) called on"
                                "HdBufferArray having multiple GL resources");
            }
        }
    }

    // returns the first item
    return _resourceList.begin()->second;
}

HdBufferResourceSharedPtr
HdStVBOSimpleMemoryManager::_SimpleBufferArray::GetResource(TfToken const& name)
{
    HD_TRACE_FUNCTION();

    // linear search.
    // The number of buffer resources should be small (<10 or so).
    for (HdBufferResourceNamedList::iterator it = _resourceList.begin();
         it != _resourceList.end(); ++it) {
        if (it->first == name) return it->second;
    }
    return HdStBufferResourceSharedPtr();
}

HdBufferSpecVector
HdStVBOSimpleMemoryManager::_SimpleBufferArray::GetBufferSpecs() const
{
    HdBufferSpecVector result;
    result.reserve(_resourceList.size());
    TF_FOR_ALL (it, _resourceList) {
        result.emplace_back(it->first, it->second->GetTupleType());
    }
    return result;
}

// ---------------------------------------------------------------------------
//  _SimpleBufferArrayRange
// ---------------------------------------------------------------------------
bool
HdStVBOSimpleMemoryManager::_SimpleBufferArrayRange::IsAssigned() const
{
    return (_bufferArray != nullptr);
}

bool
HdStVBOSimpleMemoryManager::_SimpleBufferArrayRange::IsImmutable() const
{
    return (_bufferArray != nullptr)
         && _bufferArray->IsImmutable();
}

void
HdStVBOSimpleMemoryManager::_SimpleBufferArrayRange::CopyData(
    HdBufferSourceSharedPtr const &bufferSource)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    if (!TF_VERIFY(_bufferArray)) return;

    int offset = 0;

    HdBufferResourceSharedPtr VBO =
        _bufferArray->GetResource(bufferSource->GetName());

    if (!VBO || !VBO->GetId().IsSet()) {
        TF_CODING_ERROR("VBO doesn't exist for %s",
                        bufferSource->GetName().GetText());
        return;
    }
    HdStRenderContextCaps const &caps = HdStRenderContextCaps::GetInstance();

    if (glBufferSubData != NULL) {
        int bytesPerElement = HdDataSizeOfTupleType(VBO->GetTupleType());
        // overrun check. for graceful handling of erroneous assets,
        // issue warning here and continue to copy for the valid range.
        size_t dstSize = _numElements * bytesPerElement;
        size_t srcSize =
            bufferSource->GetNumElements() *
            HdDataSizeOfTupleType(bufferSource->GetTupleType());
        if (srcSize > dstSize) {
            TF_WARN("%s: size %ld is larger than the range (%ld)",
                    bufferSource->GetName().GetText(), srcSize, dstSize);
            srcSize = dstSize;
        }

        GLintptr vboOffset = bytesPerElement * offset;

        HD_PERF_COUNTER_INCR(HdPerfTokens->glBufferSubData);

        VBO->CopyData(vboOffset, srcSize, bufferSource->GetData());
    }
}

VtValue
HdStVBOSimpleMemoryManager::_SimpleBufferArrayRange::ReadData(TfToken const &name) const
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    if (!TF_VERIFY(_bufferArray)) return VtValue();

    HdBufferResourceSharedPtr VBO = _bufferArray->GetResource(name);

    if (!VBO || (VBO->GetId() == 0 && _numElements > 0)) {
        TF_CODING_ERROR("VBO doesn't exist for %s", name.GetText());
        return VtValue();
    }

    return VBO->ReadBuffer(VBO->GetTupleType(),
                          /*offset=*/0,
                          /*stride=*/0,  // not interleaved.
                          _numElements);
}

size_t
HdStVBOSimpleMemoryManager::_SimpleBufferArrayRange::GetMaxNumElements() const
{
    return _bufferArray->GetMaxNumElements();
}

HdBufferResourceSharedPtr
HdStVBOSimpleMemoryManager::_SimpleBufferArrayRange::GetResource() const
{
    if (!TF_VERIFY(_bufferArray)) return HdStBufferResourceSharedPtr();

    return _bufferArray->GetResource();
}

HdBufferResourceSharedPtr
HdStVBOSimpleMemoryManager::_SimpleBufferArrayRange::GetResource(TfToken const& name)
{
    if (!TF_VERIFY(_bufferArray)) return HdStBufferResourceSharedPtr();
    return _bufferArray->GetResource(name);
}

HdBufferResourceNamedList const&
HdStVBOSimpleMemoryManager::_SimpleBufferArrayRange::GetResources() const
{
    if (!TF_VERIFY(_bufferArray)) {
        static HdBufferResourceNamedList empty;
        return empty;
    }
    return _bufferArray->GetResources();
}

void
HdStVBOSimpleMemoryManager::_SimpleBufferArrayRange::SetBufferArray(HdBufferArray *bufferArray)
{
    _bufferArray = static_cast<_SimpleBufferArray *>(bufferArray);    
}

void
HdStVBOSimpleMemoryManager::_SimpleBufferArrayRange::DebugDump(std::ostream &out) const
{
    out << "[SimpleBAR] numElements = " << _numElements
        << "\n";
}

const void *
HdStVBOSimpleMemoryManager::_SimpleBufferArrayRange::_GetAggregation() const
{
    return _bufferArray;
}

PXR_NAMESPACE_CLOSE_SCOPE

