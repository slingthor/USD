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
#ifndef HDST_PERSISTENT_BUFFER_H
#define HDST_PERSISTENT_BUFFER_H

#include "pxr/pxr.h"
#include "pxr/imaging/hdSt/api.h"
#include "pxr/base/tf/token.h"

#include <boost/shared_ptr.hpp>

PXR_NAMESPACE_OPEN_SCOPE


typedef boost::shared_ptr<class HdStPersistentBuffer> HdStPersistentBufferSharedPtr;
typedef boost::shared_ptr<class HdResource> HdResourceSharedPtr;


/// \class HdStPersistentBuffer
///
/// A buffer used to prepare data on the GPU that has a persistent mapping
/// from the CPU.
///
class HdStPersistentBuffer {
public:
    HDST_API
    static HdStPersistentBuffer *New(TfToken const &role, size_t dataSize, void* data);
    
    HDST_API
    virtual ~HdStPersistentBuffer();

    /// Returns the mapped address
    HDST_API
    virtual void * GetMappedAddress() const { return _mappedAddress; }

    /// Returns the GPU resource.
    HDST_API
    HdResourceSharedPtr GetResource() const { return _resource; }
    
protected:
    HDST_API
    HdStPersistentBuffer(HdResourceSharedPtr resource);

private:
    HdResourceSharedPtr _resource;
    void * _mappedAddress;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif  // HDST_PERSISTENT_BUFFER_H
