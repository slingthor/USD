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
#include "pxr/imaging/glf/contextCaps.h"
#if defined(ARCH_GFX_METAL)
#include "pxr/imaging/mtlf/mtlDevice.h"
#endif

#include "pxr/imaging/hdSt/dispatchBuffer.h"
#include "pxr/imaging/hd/engine.h"
#include "pxr/imaging/hd/perfLog.h"

#include "pxr/imaging/hf/perfLog.h"

using namespace boost;

PXR_NAMESPACE_OPEN_SCOPE


class Hd_DispatchBufferArrayRange : public HdBufferArrayRange {
public:
    /// Constructor.
    Hd_DispatchBufferArrayRange(HdStDispatchBuffer *buffer) :
        _buffer(buffer) {
    }

    /// Returns true if this range is valid
    virtual bool IsValid() const override {
        return true;
    }

    /// Returns true is the range has been assigned to a buffer
    virtual bool IsAssigned() const override {
        return (_buffer != nullptr);
    }

    /// Dispatch buffer array range is always mutable
    virtual bool IsImmutable() const override {
        return false;
    }

    /// Resize memory area for this range. Returns true if it causes container
    /// buffer reallocation.
    virtual bool Resize(int numElements) override {
        TF_CODING_ERROR("Hd_DispatchBufferArrayRange doesn't support this operation");
        return false;
    }

    /// Copy source data into buffer
    virtual void CopyData(HdBufferSourceSharedPtr const &bufferSource) override {
        TF_CODING_ERROR("Hd_DispatchBufferArrayRange doesn't support this operation");
    }

    /// Read back the buffer content
    virtual VtValue ReadData(TfToken const &name) const override {
        TF_CODING_ERROR("Hd_DispatchBufferArrayRange doesn't support this operation");
        return VtValue();
    }

    /// Returns the relative offset in aggregated buffer
    virtual int GetOffset() const override {
        TF_CODING_ERROR("Hd_DispatchBufferArrayRange doesn't support this operation");
        return 0;
    }

    /// Returns the index in aggregated buffer
    virtual int GetIndex() const override {
        TF_CODING_ERROR("Hd_DispatchBufferArrayRange doesn't support this operation");
        return 0;
    }

    /// Returns the number of elements allocated
    virtual int GetNumElements() const override {
        TF_CODING_ERROR("Hd_DispatchBufferArrayRange doesn't support this operation");
        return 0;
    }

    /// Returns the capacity of allocated area for this range
    virtual int GetCapacity() const {
        TF_CODING_ERROR("Hd_DispatchBufferArrayRange doesn't support this operation");
        return 0;
    }

    /// Returns the version of the buffer array.
    virtual size_t GetVersion() const override {
        TF_CODING_ERROR("Hd_DispatchBufferArrayRange doesn't support this operation");
        return 0;
    }

    /// Increment the version of the buffer array.
    virtual void IncrementVersion() override {
        TF_CODING_ERROR("Hd_DispatchBufferArrayRange doesn't support this operation");
    }

    /// Returns the max number of elements
    virtual size_t GetMaxNumElements() const override {
        TF_CODING_ERROR("Hd_DispatchBufferArrayRange doesn't support this operation");
        return 1;
    }

    /// Returns the GPU resource. If the buffer array contains more than one
    /// resource, this method raises a coding error.
    virtual HdBufferResourceSharedPtr GetResource() const {
        return _buffer->GetResource();
    }

    /// Returns the named GPU resource.
    virtual HdBufferResourceSharedPtr GetResource(TfToken const& name) {
        return _buffer->GetResource(name);
    }

    /// Returns the list of all named GPU resources for this bufferArrayRange.
    virtual HdBufferResourceNamedList const& GetResources() const {
        return _buffer->GetResources();
    }

    /// Sets the buffer array associated with this buffer;
    virtual void SetBufferArray(HdBufferArray *bufferArray) override {
        TF_CODING_ERROR("Hd_DispatchBufferArrayRange doesn't support this operation");
    }

    /// Debug dump
    virtual void DebugDump(std::ostream &out) const override {
    }

    /// Sets the bufferSpecs for all resources.
    virtual void AddBufferSpecs(HdBufferSpecVector *bufferSpecs) const override {}
    
    /// Make this range invalid
    void Invalidate() {
        TF_CODING_ERROR("Hd_DispatchBufferArrayRange doesn't support this operation");
    }

protected:
    /// Returns the aggregation container
    virtual const void *_GetAggregation() const override {
        return this;
    }

private:
    HdStDispatchBuffer *_buffer;
};


HdStDispatchBuffer::HdStDispatchBuffer(TfToken const &role, int count,
                                   unsigned int commandNumUints)
    : HdBufferArray(role, TfToken()), _count(count), _commandNumUints(commandNumUints)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    GlfContextCaps const &caps = GlfContextCaps::GetInstance();

    size_t stride = commandNumUints * sizeof(GLuint);
    size_t dataSize = count * stride;
    
    HdResourceGPUHandle newId;

#if defined(ARCH_GFX_METAL)
    if(HdEngine::GetRenderAPI() == HdEngine::Metal)
    {
        id<MTLBuffer> nid = nil;
        nid = [MtlfMetalContext::GetMetalContext()->device newBufferWithLength:dataSize options:MTLResourceStorageModeManaged];
        
        newId = nid;
    }
    else
#endif
    if(HdEngine::GetRenderAPI() == HdEngine::OpenGL)
    {
        GLuint nid = 0;
        glGenBuffers(1, &nid);
        // just allocate uninitialized
        if (caps.directStateAccessEnabled) {
            glNamedBufferDataEXT(nid, dataSize, NULL, GL_STATIC_DRAW);
        } else {
            glBindBuffer(GL_ARRAY_BUFFER, nid);
            glBufferData(GL_ARRAY_BUFFER, dataSize, NULL, GL_STATIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }

        newId = nid;
    }
    else
    {
        TF_FATAL_CODING_ERROR("No implementation for this API");
    }

    // monolithic resource
    _entireResource = HdStBufferResourceSharedPtr(
            HdStBufferResource::New(
            role, {HdTypeInt32, 1},
            /*offset=*/0, stride));
    _entireResource->SetAllocation(newId, dataSize);

    // create a buffer array range, which aggregates all views
    // (will be added by AddBufferResourceView)
    _bar = HdBufferArrayRangeSharedPtr(new Hd_DispatchBufferArrayRange(this));
}

HdStDispatchBuffer::~HdStDispatchBuffer()
{
    HdResourceGPUHandle _id = _entireResource->GetId();
#if defined(ARCH_GFX_METAL)
    if(HdEngine::GetRenderAPI() == HdEngine::Metal)
    {
        id<MTLBuffer> oid = _id;
        [oid release];
    }
    else
#endif
    if(HdEngine::GetRenderAPI() == HdEngine::OpenGL)
    {
        GLuint oid = _id;
        glDeleteBuffers(1, &oid);
    }
    else
    {
        TF_FATAL_CODING_ERROR("No implementation for this API");
    }
    
    _entireResource->SetAllocation(HdResourceGPUHandle(), 0);
}

void
HdStDispatchBuffer::CopyData(std::vector<GLuint> const &data)
{
    if (!TF_VERIFY(data.size()*sizeof(GLuint) == static_cast<size_t>(_entireResource->GetSize())))
        return;

#if defined(ARCH_GFX_METAL)
    if(HdEngine::GetRenderAPI() == HdEngine::Metal)
    {
        id<MTLBuffer> buffer = _entireResource->GetId();
        memcpy([buffer contents], &data[0], _entireResource->GetSize());
    }
    else
#endif
    if(HdEngine::GetRenderAPI() == HdEngine::OpenGL)
    {
        GlfContextCaps const &caps = GlfContextCaps::GetInstance();

        if (caps.directStateAccessEnabled) {
            glNamedBufferSubDataEXT(_entireResource->GetId(),
                                    0,
                                    _entireResource->GetSize(),
                                    &data[0]);
        } else {
            glBindBuffer(GL_ARRAY_BUFFER, _entireResource->GetId());
            glBufferSubData(GL_ARRAY_BUFFER, 0,
                            _entireResource->GetSize(),
                            &data[0]);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
    }
    else
    {
        TF_FATAL_CODING_ERROR("No implementation for this API");
    }
}

void
HdStDispatchBuffer::AddBufferResourceView(
    TfToken const &name, HdTupleType tupleType, int offset)
{
    size_t stride = _commandNumUints * sizeof(GLuint);

    // add a binding view (resource binder iterates and automatically binds)
    HdStBufferResourceSharedPtr view =
        _AddResource(name, tupleType, offset, stride);

    // this is just a view, not consuming memory
    view->SetAllocation(_entireResource->GetId(), /*size=*/0);
}


bool
HdStDispatchBuffer::GarbageCollect()
{
    TF_CODING_ERROR("HdStDispatchBuffer doesn't support this operation");
    return false;
}

void
HdStDispatchBuffer::Reallocate(std::vector<HdBufferArrayRangeSharedPtr> const &,
                               HdBufferArraySharedPtr const &)
{
    TF_CODING_ERROR("HdStDispatchBuffer doesn't support this operation");
}

void
HdStDispatchBuffer::DebugDump(std::ostream &out) const
{
    /*nothing*/
}

HdStBufferResourceSharedPtr
HdStDispatchBuffer::GetResource() const
{
    HD_TRACE_FUNCTION();

    if (_resourceList.empty()) return HdStBufferResourceSharedPtr();

    if (TfDebug::IsEnabled(HD_SAFE_MODE)) {
        // make sure this buffer array has only one resource.
        HdResourceGPUHandle id(_resourceList.begin()->second->GetId());
        TF_FOR_ALL (it, _resourceList) {
            if (it->second->GetId() != id) {
                TF_CODING_ERROR("GetResource(void) called on"
                                "HdBufferArray having multiple GL resources");
            }
        }
    }

    // returns the first item
    return dynamic_pointer_cast<HdStBufferResource>(_resourceList.begin()->second);
}

HdStBufferResourceSharedPtr
HdStDispatchBuffer::GetResource(TfToken const& name)
{
    HD_TRACE_FUNCTION();

    // linear search.
    // The number of buffer resources should be small (<10 or so).
    for (HdBufferResourceNamedList::iterator it = _resourceList.begin();
         it != _resourceList.end(); ++it) {
        if (it->first == name)
            return dynamic_pointer_cast<HdStBufferResource>(it->second);
    }
    return HdStBufferResourceSharedPtr();
}

HdStBufferResourceSharedPtr
HdStDispatchBuffer::_AddResource(TfToken const& name,
                                 HdTupleType tupleType,
                                 int offset,
                                 int stride)
{
    HD_TRACE_FUNCTION();

    if (TfDebug::IsEnabled(HD_SAFE_MODE)) {
        // duplication check
        HdStBufferResourceSharedPtr bufferRes = GetResource(name);
        if (!TF_VERIFY(!bufferRes)) {
            return bufferRes;
        }
    }

    HdStBufferResourceSharedPtr bufferRes = HdStBufferResourceSharedPtr(
        HdStBufferResource::New(GetRole(), tupleType,
                                offset, stride));

    _resourceList.emplace_back(name, bufferRes);
    return bufferRes;
}


PXR_NAMESPACE_CLOSE_SCOPE

