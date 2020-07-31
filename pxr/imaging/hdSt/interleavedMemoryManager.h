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
#ifndef PXR_IMAGING_HD_ST_INTERLEAVED_MEMORY_MANAGER_H
#define PXR_IMAGING_HD_ST_INTERLEAVED_MEMORY_MANAGER_H

#include "pxr/pxr.h"
#include "pxr/imaging/hdSt/api.h"
#include "pxr/imaging/hdSt/bufferArrayRangeGL.h"
#include "pxr/imaging/hdSt/bufferResourceGL.h"
#include "pxr/imaging/hdSt/resourceRegistry.h"

#include "pxr/imaging/hd/bufferArray.h"
#include "pxr/imaging/hd/bufferSpec.h"
#include "pxr/imaging/hd/bufferSource.h"
#include "pxr/imaging/hd/resource.h"
#include "pxr/imaging/hd/strategyBase.h"
#include "pxr/imaging/hd/tokens.h"
#include "pxr/imaging/hd/version.h"
#include "pxr/base/tf/mallocTag.h"
#include "pxr/base/tf/token.h"

#include <memory>
#include <list>

PXR_NAMESPACE_OPEN_SCOPE

class HdStResourceRegistry;

/// \class HdStInterleavedMemoryManager
///
/// Interleaved memory manager (base class).
///
class HdStInterleavedMemoryManager : public HdAggregationStrategy {
public:
    class _StripedInterleavedBuffer;

    /// specialized buffer array range
    class _StripedInterleavedBufferRange : public HdStBufferArrayRangeGL {
    public:
        /// Constructor.
        _StripedInterleavedBufferRange() :
        _stripedBuffer(nullptr), _index(NOT_ALLOCATED), _numElements(1) {
        }

        /// Destructor.
        HDST_API
        virtual ~_StripedInterleavedBufferRange();

        /// Returns true if this range is valid
        virtual bool IsValid() const override {
            // note: a range is valid even its index is NOT_ALLOCATED.
            return (bool)_stripedBuffer;
        }

        /// Returns true is the range has been assigned to a buffer
        HDST_API
        virtual bool IsAssigned() const override;

        /// Returns true if this range is marked as immutable.
        HDST_API
        virtual bool IsImmutable() const override;

        /// Resize memory area for this range. Returns true if it causes container
        /// buffer reallocation.
        HDST_API
        virtual bool Resize(int numElements) override;

        /// Copy source data into buffer
        HDST_API
        virtual void CopyData(HdBufferSourceSharedPtr const &bufferSource) override;

        /// Read back the buffer content
        HDST_API
        virtual VtValue ReadData(TfToken const &name) const;

        /// Returns the offset at which this range begins in the underlying  
        /// buffer array in terms of elements.
        virtual int GetElementOffset() const {
            return _index;
        }

        /// Returns the byte offset at which this range begins in the underlying
        /// buffer array for the given resource.
        virtual int GetByteOffset(TfToken const& resourceName) const {
            TF_UNUSED(resourceName);
            if (!TF_VERIFY(_stripedBuffer) ||
                !TF_VERIFY(_index != NOT_ALLOCATED)) return 0;
            return _stripedBuffer->GetStride() * _index;
        }

        /// Returns the number of elements
        virtual size_t GetNumElements() const override {
            return _numElements;
        }

        /// Returns the version of the buffer array.
        virtual size_t GetVersion() const override {
            return _stripedBuffer->GetVersion();
        }

        /// Increment the version of the buffer array.
        virtual void IncrementVersion() override {
            _stripedBuffer->IncrementVersion();
        }

        /// Returns the max number of elements
        HDST_API
        virtual size_t GetMaxNumElements() const override;

        /// Returns the usage hint from the underlying buffer array
        HDST_API
        virtual HdBufferArrayUsageHint GetUsageHint() const override;

        /// Returns the GPU resource. If the buffer array contains more than one
        /// resource, this method raises a coding error.
        HDST_API
        virtual HdStBufferResourceGLSharedPtr GetResource() const override;

        /// Returns the named GPU resource.
        HDST_API
        virtual HdStBufferResourceGLSharedPtr GetResource(TfToken const& name) override;

        /// Returns the list of all named GPU resources for this bufferArrayRange.
        HDST_API
        virtual HdStBufferResourceGLNamedList const& GetResources() const override;

        /// Sets the buffer array associated with this buffer;
        HDST_API
        virtual void SetBufferArray(HdBufferArray *bufferArray) override;

        /// Debug dump
        HDST_API
        virtual void DebugDump(std::ostream &out) const override;
        
        /// Reconstructs the bufferspecs and returns it (for buffer splitting)
        HDST_API
        virtual void GetBufferSpecs(HdBufferSpecVector *specs) const override {
            HdStBufferResourceGLNamedList const &resources = _stripedBuffer->GetResources();
                
            TF_FOR_ALL(it, resources) {
                specs->emplace_back(it->first, it->second->GetTupleType());
            }
        }

        /// Set the relative offset for this range.
        HDST_API
        void SetIndex(int index) {
            _index = index;
        }

        /// Make this range invalid
        HDST_API
        void Invalidate() {
            _stripedBuffer = nullptr;
        }

    protected:
        /// Returns the aggregation container
        HDST_API
        virtual const void *_GetAggregation() const override;

    private:
        enum { NOT_ALLOCATED = -1 };
        _StripedInterleavedBuffer *_stripedBuffer;
        int _index;
        size_t _numElements;
    };

    using _StripedInterleavedBufferSharedPtr =
        std::shared_ptr<_StripedInterleavedBuffer>;
    using _StripedInterleavedBufferRangeSharedPtr =
        std::shared_ptr<_StripedInterleavedBufferRange>;
    using _StripedInterleavedBufferRangePtr =
        std::weak_ptr<_StripedInterleavedBufferRange>;

    /// striped buffer
    class _StripedInterleavedBuffer : public HdBufferArray {
    public:
        /// Constructor.
        HDST_API
        _StripedInterleavedBuffer(HdStResourceRegistry* resourceRegistry,
                                  TfToken const &role,
                                  HdBufferSpecVector const &bufferSpecs,
                                  HdBufferArrayUsageHint usageHint,
                                  int bufferOffsetAlignment,
                                  int structAlignment,
                                  size_t maxSize,
                                  TfToken const &garbageCollectionPerfToken);

        /// Destructor. It invalidates _rangeList
        HDST_API
        virtual ~_StripedInterleavedBuffer();

        /// perform compaction if necessary, returns true if it becomes empty.
        HDST_API
        virtual bool GarbageCollect();

        /// Debug output
        HDST_API
        virtual void DebugDump(std::ostream &out) const;

        /// Performs reallocation.
        /// GLX context has to be set when calling this function.
        HDST_API
        virtual void Reallocate(
                std::vector<HdBufferArrayRangeSharedPtr> const &ranges,
                HdBufferArraySharedPtr const &curRangeOwner);

        /// Mark to perform reallocation on Reallocate()
        void SetNeedsReallocation() {
            _needsReallocation = true;
        }

        /// Mark to perform compaction on GarbageCollect()
        void SetNeedsCompaction() {
            _needsCompaction = true;
        }

        /// Returns the stride.
        int GetStride() const {
            return _stride;
        }

        /// TODO: We need to distinguish between the primvar types here, we should
        /// tag each HdBufferSource and HdBufferResource with Constant, Uniform,
        /// Varying, Vertex, or FaceVarying and provide accessors for the specific
        /// buffer types.

        /// Returns the GPU resource. If the buffer array contains more than one
        /// resource, this method raises a coding error.
        HDST_API
        HdStBufferResourceGLSharedPtr GetResource() const;

        /// Returns the named GPU resource. This method returns the first found
        /// resource. In HD_SAFE_MODE it checks all underlying GL buffers
        /// in _resourceMap and raises a coding error if there are more than
        /// one GL buffers exist.
        HDST_API
        HdStBufferResourceGLSharedPtr GetResource(TfToken const& name);

        /// Returns the list of all named GPU resources for this bufferArray.
        HdStBufferResourceGLNamedList const& GetResources() const {return _resourceList;}

        /// Reconstructs the bufferspecs and returns it (for buffer splitting)
        HDST_API
        HdBufferSpecVector GetBufferSpecs() const;
        
        /// APPLE METAL: HGI accessor needed for _StripedInterleavedBufferRange::CopyData()
        Hgi* GetHgi() { return _resourceRegistry->GetHgi(); }

    protected:
        HDST_API
        void _DeallocateResources();

        /// Adds a new, named GPU resource and returns it.
        HDST_API
        HdStBufferResourceGLSharedPtr _AddResource(TfToken const& name,
                                                   HdTupleType tupleType,
                                                   int offset,
                                                   int stride);

    private:
        HdStResourceRegistry* const _resourceRegistry;
        bool _needsCompaction;
        int _stride;
        int _bufferOffsetAlignment;  // ranged binding offset alignment
        size_t _maxSize;             // maximum size of single buffer

        HdStBufferResourceGLNamedList _resourceList;

        _StripedInterleavedBufferRangeSharedPtr _GetRangeSharedPtr(size_t idx) const {
            return std::static_pointer_cast<_StripedInterleavedBufferRange>(GetRange(idx).lock());
        }

    };
    
    HdStInterleavedMemoryManager(HdStResourceRegistry* resourceRegistry)
        : _resourceRegistry(resourceRegistry) {}

protected:
    /// Factory for creating HdBufferArrayRange
    virtual HdBufferArrayRangeSharedPtr CreateBufferArrayRange();

    /// Returns the buffer specs from a given buffer array
    virtual HdBufferSpecVector GetBufferSpecs(
        HdBufferArraySharedPtr const &bufferArray) const;

    /// Returns the size of the GPU memory used by the passed buffer array
    virtual size_t GetResourceAllocation(
        HdBufferArraySharedPtr const &bufferArray, 
        VtDictionary &result) const;
    
    HdStResourceRegistry* const _resourceRegistry;
};

class HdStInterleavedUBOMemoryManager : public HdStInterleavedMemoryManager {
public:
    HdStInterleavedUBOMemoryManager(HdStResourceRegistry* resourceRegistry)
    : HdStInterleavedMemoryManager(resourceRegistry) {}

    /// Factory for creating HdBufferArray managed by
    /// HdStVBOMemoryManager aggregation.
    HDST_API
    virtual HdBufferArraySharedPtr CreateBufferArray(
        TfToken const &role,
        HdBufferSpecVector const &bufferSpecs,
        HdBufferArrayUsageHint usageHint);

    /// Returns id for given bufferSpecs to be used for aggregation
    HDST_API
    virtual AggregationId ComputeAggregationId(
        HdBufferSpecVector const &bufferSpecs,
        HdBufferArrayUsageHint usageHint) const;
};

class HdStInterleavedSSBOMemoryManager : public HdStInterleavedMemoryManager {
public:
    HdStInterleavedSSBOMemoryManager(HdStResourceRegistry* resourceRegistry)
    : HdStInterleavedMemoryManager(resourceRegistry) {}

    /// Factory for creating HdBufferArray managed by
    /// HdStVBOMemoryManager aggregation.
    HDST_API
    virtual HdBufferArraySharedPtr CreateBufferArray(
        TfToken const &role,
        HdBufferSpecVector const &bufferSpecs,
        HdBufferArrayUsageHint usageHint);

    /// Returns id for given bufferSpecs to be used for aggregation
    HDST_API
    virtual AggregationId ComputeAggregationId(
        HdBufferSpecVector const &bufferSpecs,
        HdBufferArrayUsageHint usageHint) const;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif  // PXR_IMAGING_HD_ST_INTERLEAVED_MEMORY_MANAGER_H
