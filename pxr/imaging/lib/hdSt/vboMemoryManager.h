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
#ifndef HDST_VBO_MEMORY_MANAGER_H
#define HDST_VBO_MEMORY_MANAGER_H

#include "pxr/pxr.h"
#include "pxr/imaging/hdSt/api.h"
#include "pxr/imaging/hd/version.h"
#include "pxr/imaging/hd/bufferArray.h"
#include "pxr/imaging/hd/bufferArrayRange.h"
#include "pxr/imaging/hd/bufferSpec.h"
#include "pxr/imaging/hd/bufferSource.h"
#include "pxr/imaging/hd/strategyBase.h"

#include "pxr/base/tf/mallocTag.h"
#include "pxr/base/tf/token.h"

#include <boost/shared_ptr.hpp>
#include <list>

PXR_NAMESPACE_OPEN_SCOPE


/// \class HdStVBOMemoryManager
///
/// VBO memory manager.
///
class HdStVBOMemoryManager : public HdAggregationStrategy {
public:
    HdStVBOMemoryManager(bool isImmutable) : _isImmutable(isImmutable) {}

    /// Factory for creating HdBufferArray managed by
    /// HdStVBOMemoryManager aggregation.
    HDST_API
    virtual HdBufferArraySharedPtr CreateBufferArray(
        TfToken const &role,
        HdBufferSpecVector const &bufferSpecs);

    /// Factory for creating HdBufferArrayRange managed by
    /// HdStVBOMemoryManager aggregation.
    HDST_API
    virtual HdBufferArrayRangeSharedPtr CreateBufferArrayRange();

    /// Returns id for given bufferSpecs to be used for aggregation
    HDST_API
    virtual AggregationId ComputeAggregationId(
        HdBufferSpecVector const &bufferSpecs) const;

    /// Returns the buffer specs from a given buffer array
    HDST_API
    virtual HdBufferSpecVector GetBufferSpecs(
        HdBufferArraySharedPtr const &bufferArray) const;

    /// Returns the size of the GPU memory used by the passed buffer array
    HDST_API
    virtual size_t GetResourceAllocation(
        HdBufferArraySharedPtr const &bufferArray, 
        VtDictionary &result) const;

private:
    bool _isImmutable;

public:
    class _StripedBufferArray;

    /// specialized buffer array range
    class _StripedBufferArrayRange : public HdBufferArrayRange {
    public:
        /// Constructor.
        HDST_API
        _StripedBufferArrayRange()
         : _stripedBufferArray(nullptr),
           _offset(0),
           _numElements(0),
           _capacity(0)
        {
        }

        /// Destructor.
        HDST_API
        virtual ~_StripedBufferArrayRange();

        /// Returns true if this range is valid
        HDST_API
        virtual bool IsValid() const override {
            return (bool)_stripedBufferArray;
        }

        /// Returns true is the range has been assigned to a buffer
        HDST_API
        virtual bool IsAssigned() const override;

        /// Returns true if this bar is marked as immutable.
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
        virtual VtValue ReadData(TfToken const &name) const override;

        /// Returns the relative offset in aggregated buffer
        HDST_API
        virtual int GetOffset() const override {
            return _offset;
        }

        /// Returns the index for this range
        HDST_API
        virtual int GetIndex() const override {
            // note: range doesn't store index, so we need to sweep rangeLists
            // to find the index of this range.
            TF_CODING_ERROR("vboMemoryManager doesn't support GetIndex() for "
                            "memory and performance reasons\n");
            return 0;
        }

        /// Returns the number of elements
        HDST_API
        virtual size_t GetNumElements() const override {
            return _numElements;
        }

        /// Returns the version of the buffer array.
        HDST_API
        virtual size_t GetVersion() const override {
            return _stripedBufferArray->GetVersion();
        }

        /// Increment the version of the buffer array.
        HDST_API
        virtual void IncrementVersion() override {
            _stripedBufferArray->IncrementVersion();
        }

        /// Returns the max number of elements
        HDST_API
        virtual size_t GetMaxNumElements() const override;

        /// Returns the GPU resource. If the buffer array contains more than one
        /// resource, this method raises a coding error.
        HDST_API
        virtual HdBufferResourceSharedPtr GetResource() const override;

        /// Returns the named GPU resource.
        HDST_API
        virtual HdBufferResourceSharedPtr GetResource(TfToken const& name) override;

        /// Returns the list of all named GPU resources for this bufferArrayRange.
        HDST_API
        virtual HdBufferResourceNamedList const& GetResources() const override;

        /// Sets the buffer array associated with this buffer;
        HDST_API
        virtual void SetBufferArray(HdBufferArray *bufferArray) override;

        HDST_API
        virtual void GetBufferSpecs(HdBufferSpecVector *bufferSpecs) const override {}

        /// Debug dump
        HDST_API
        virtual void DebugDump(std::ostream &out) const override;

        /// Set the relative offset for this range.
        HDST_API
        void SetOffset(int offset) {
            _offset = offset;
        }

        /// Set the number of elements for this range.
        HDST_API
        void SetNumElements(int numElements) {
            _numElements = numElements;
        }

        /// Returns the capacity of allocated area
        HDST_API
        int GetCapacity() const {
            return _capacity;
        }

        /// Set the capacity of allocated area for this range.
        HDST_API
        void SetCapacity(int capacity) {
            _capacity = capacity;
        }

        /// Make this range invalid
        HDST_API
        void Invalidate() {
            _stripedBufferArray = NULL;
        }

    protected:
        /// Returns the aggregation container
        HDST_API
        virtual const void *_GetAggregation() const override;

    private:
        // holding a weak reference to container.
        // this pointer becomes null when the StripedBufferArray gets destructed,
        // in case if any drawItem still holds this bufferRange.
        _StripedBufferArray *_stripedBufferArray;
        int _offset;
        size_t _numElements;
        int _capacity;
    };

    typedef boost::shared_ptr<_StripedBufferArray>
        _StripedBufferArraySharedPtr;
    typedef boost::shared_ptr<_StripedBufferArrayRange>
        _StripedBufferArrayRangeSharedPtr;
    typedef boost::weak_ptr<_StripedBufferArrayRange>
        _StripedBufferArrayRangePtr;

    /// striped buffer array
    class _StripedBufferArray : public HdBufferArray {
    public:
        /// Constructor.
        HDST_API
        _StripedBufferArray(TfToken const &role,
                            HdBufferSpecVector const &bufferSpecs,
                            bool isImmutable);

        /// Destructor. It invalidates _rangeList
        HDST_API
        virtual ~_StripedBufferArray();

        /// perform compaction if necessary. If it becomes empty, release all
        /// resources and returns true
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
            HdBufferArraySharedPtr const &curRangeOwner) = 0;

        /// Returns the maximum number of elements capacity.
        HDST_API
        virtual size_t GetMaxNumElements() const;

        /// Mark to perform reallocation on Reallocate()
        HDST_API
        void SetNeedsReallocation() {
            _needsReallocation = true;
        }

        /// Mark to perform compaction on GarbageCollect()
        HDST_API
        void SetNeedsCompaction() {
            _needsCompaction = true;
        }

        /// TODO: We need to distinguish between the primvar types here, we should
        /// tag each HdBufferSource and HdBufferResource with Constant, Uniform,
        /// Varying, Vertex, or FaceVarying and provide accessors for the specific
        /// buffer types.

        /// Returns the GPU resource. If the buffer array contains more than one
        /// resource, this method raises a coding error.
        HDST_API
        HdBufferResourceSharedPtr GetResource() const;

        /// Returns the named GPU resource. This method returns the first found
        /// resource. In HD_SAFE_MODE it checks all underlying GL buffers
        /// in _resourceMap and raises a coding error if there are more than
        /// one GL buffers exist.
        HDST_API
        HdBufferResourceSharedPtr GetResource(TfToken const& name);

        /// Returns the list of all named GPU resources for this bufferArray.
        HDST_API
        HdBufferResourceNamedList const& GetResources() const
            {return _resourceList;}

        /// Reconstructs the bufferspecs and returns it (for buffer splitting)
        HDST_API
        HdBufferSpecVector GetBufferSpecs() const;

    protected:
        HDST_API
        virtual void _DeallocateResources() = 0;

        /// Adds a new, named GPU resource and returns it.
        HDST_API
        HdBufferResourceSharedPtr _AddResource(TfToken const& name,
                                               HdTupleType tupleType,
                                               int offset,
                                               int stride);

        bool _needsCompaction;
        int _totalCapacity;
        size_t _maxBytesPerElement;

        HdBufferResourceNamedList _resourceList;

        // Helper routine to cast the range shared pointer.
        _StripedBufferArrayRangeSharedPtr _GetRangeSharedPtr(size_t idx) const {
            return boost::static_pointer_cast<_StripedBufferArrayRange>(GetRange(idx).lock());
        }
    };
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif  // HDST_VBO_MEMORY_MANAGER_H
