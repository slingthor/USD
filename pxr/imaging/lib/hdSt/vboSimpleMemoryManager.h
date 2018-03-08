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
#ifndef HDST_VBO_SIMPLE_MEMORY_MANAGER_H
#define HDST_VBO_SIMPLE_MEMORY_MANAGER_H

#include "pxr/pxr.h"
#include "pxr/imaging/hdSt/api.h"
#include "pxr/imaging/hd/bufferArrayRange.h"
#include "pxr/imaging/hd/version.h"
#include "pxr/imaging/hd/strategyBase.h"
#include "pxr/imaging/hd/bufferArray.h"
#include "pxr/imaging/hd/bufferSpec.h"
#include "pxr/imaging/hd/bufferSource.h"

PXR_NAMESPACE_OPEN_SCOPE


/// \class HdStVBOSimpleMemoryManager
///
/// VBO simple memory manager.
///
/// This class doesn't perform any aggregation.
///
class HdStVBOSimpleMemoryManager : public HdAggregationStrategy {
public:
    /// Factory for creating HdBufferArray managed by
    /// HdStVBOSimpleMemoryManager.
    HDST_API
    virtual HdBufferArraySharedPtr CreateBufferArray(
        TfToken const &role,
        HdBufferSpecVector const &bufferSpecs);

    /// Factory for creating HdBufferArrayRange
    HDST_API
    virtual HdBufferArrayRangeSharedPtr CreateBufferArrayRange();

    /// Returns id for given bufferSpecs to be used for aggregation
    HDST_API
    virtual HdAggregationStrategy::AggregationId ComputeAggregationId(
        HdBufferSpecVector const &bufferSpecs) const;

    /// Returns the buffer specs from a given buffer array
    virtual HdBufferSpecVector GetBufferSpecs(
        HdBufferArraySharedPtr const &bufferArray) const;

    /// Returns the size of the GPU memory used by the passed buffer array
    virtual size_t GetResourceAllocation(
        HdBufferArraySharedPtr const &bufferArray, 
        VtDictionary &result) const;

protected:
    class _SimpleBufferArray;

    /// \class _SimpleBufferArrayRange
    ///
    /// Specialized buffer array range for SimpleBufferArray.
    ///
    class _SimpleBufferArrayRange : public HdBufferArrayRange
    {
    public:
        /// Constructor.
        _SimpleBufferArrayRange() :
            _bufferArray(nullptr), _numElements(0) {
        }

        /// Returns true if this range is valid
        virtual bool IsValid() const override {
            return (bool)_bufferArray;
        }

        /// Returns true is the range has been assigned to a buffer
        HDST_API
        virtual bool IsAssigned() const override;

        /// Returns true if this range is marked as immutable.
        virtual bool IsImmutable() const override;

        /// Resize memory area for this range. Returns true if it causes container
        /// buffer reallocation.
        virtual bool Resize(int numElements) override {
            _numElements = numElements;
            return _bufferArray->Resize(numElements);
        }

        /// Copy source data into buffer
        HDST_API
        virtual void CopyData(HdBufferSourceSharedPtr const &bufferSource) override;

        /// Read back the buffer content
        HDST_API
        virtual VtValue ReadData(TfToken const &name) const override;

        /// Returns the relative offset in aggregated buffer
        virtual int GetOffset() const override {
            return 0;
        }

        /// Returns the index in aggregated buffer
        virtual int GetIndex() const override {
            return 0;
        }

        /// Returns the number of elements allocated
        virtual int GetNumElements() const override {
            return _numElements;
        }

        /// Returns the capacity of allocated area for this range
        virtual int GetCapacity() const {
            return _bufferArray->GetCapacity();
        }

        /// Returns the version of the buffer array.
        virtual size_t GetVersion() const override {
            return _bufferArray->GetVersion();
        }

        /// Increment the version of the buffer array.
        virtual void IncrementVersion() override {
            _bufferArray->IncrementVersion();
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

        /// Debug dump
        HDST_API
        virtual void DebugDump(std::ostream &out) const override;

        HD_API
        virtual void AddBufferSpecs(HdBufferSpecVector *bufferSpecs) const override {}
        
        /// Make this range invalid
        void Invalidate() {
            _bufferArray = nullptr;
        }

    protected:
        /// Returns the aggregation container
        HDST_API
        virtual const void *_GetAggregation() const override;

        /// Adds a new, named GPU resource and returns it.
        HDST_API
        HdStBufferResourceSharedPtr _AddResource(TfToken const& name,
                                                 HdTupleType tupleType,
                                                 int offset,
                                                 int stride);

    private:
        _SimpleBufferArray * _bufferArray;
        int _numElements;
    };

    typedef boost::shared_ptr<_SimpleBufferArray>
        _SimpleBufferArraySharedPtr;
    typedef boost::shared_ptr<_SimpleBufferArrayRange>
        _SimpleBufferArrayRangeSharedPtr;
    typedef boost::weak_ptr<_SimpleBufferArrayRange>
        _SimpleBufferArrayRangePtr;

    /// \class _SimpleBufferArray
    ///
    /// Simple buffer array (non-aggregated).
    ///
    class _SimpleBufferArray : public HdBufferArray
    {
    public:
        /// Constructor.
        HDST_API
        _SimpleBufferArray(TfToken const &role, HdBufferSpecVector const &bufferSpecs);

        /// Destructor. It invalidates _range
        HDST_API
        virtual ~_SimpleBufferArray();

        /// perform compaction if necessary, returns true if it becomes empty.
        HDST_API
        virtual bool GarbageCollect() override;

        /// Debug output
        HDST_API
        virtual void DebugDump(std::ostream &out) const override;

        /// Set to resize buffers. Actual reallocation happens on Reallocate()
        HDST_API
        bool Resize(int numElements);

        /// Performs reallocation.
        /// GLX context has to be set when calling this function.
        HDST_API
        virtual void Reallocate(
                std::vector<HdBufferArrayRangeSharedPtr> const &ranges,
                HdBufferArraySharedPtr const &curRangeOwner) override;

        /// Returns the maximum number of elements capacity.
        HDST_API
        virtual size_t GetMaxNumElements() const override;

        /// Returns current capacity. It could be different from numElements.
        int GetCapacity() const {
            return _capacity;
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
        HdBufferResourceNamedList const& GetResources() const {return _resourceList;}

        /// Reconstructs the bufferspecs and returns it (for buffer splitting)
        HDST_API
        HdBufferSpecVector GetBufferSpecs() const;

    protected:
        HDST_API
        void _DeallocateResources();

        /// Adds a new, named GPU resource and returns it.
        HDST_API
        HdStBufferResourceSharedPtr _AddResource(TfToken const& name,
                                                 HdTupleType tupleType,
                                                 int offset,
                                                 int stride);
    private:
        int _capacity;
        size_t _maxBytesPerElement;

        HdBufferResourceNamedList _resourceList;

        _SimpleBufferArrayRangeSharedPtr _GetRangeSharedPtr() const {
            return GetRangeCount() > 0
                ? boost::static_pointer_cast<_SimpleBufferArrayRange>(GetRange(0).lock())
                : _SimpleBufferArrayRangeSharedPtr();
        }
    };
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif  // HDST_VBO_SIMPLE_MEMORY_MANAGER_H
