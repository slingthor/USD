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
#ifndef HDST_PERSISTENT_BUFFER_GL_H
#define HDST_PERSISTENT_BUFFER_GL_H

#include "pxr/pxr.h"
#include "pxr/imaging/hdSt/api.h"
#include "pxr/imaging/hdSt/persistentBuffer.h"
#include "pxr/imaging/hdSt/GL/resourceGL.h"

#include <boost/shared_ptr.hpp>

PXR_NAMESPACE_OPEN_SCOPE


typedef boost::shared_ptr<class HdStPersistentBufferGL> HdStPersistentBufferGLSharedPtr;

/// \class HdStPersistentBuffer
///
/// A buffer used to prepare data on the GPU that has a persistent mapping
/// from the CPU.
///
class HdStPersistentBufferGL : public HdStResourceGL, public HdStPersistentBuffer {
public:
    HD_API
    HdStPersistentBufferGL(TfToken const &role, size_t dataSize, void* data);
    HD_API
    virtual ~HdStPersistentBufferGL();

    /// Returns the mapped address
    virtual void * GetMappedAddress() const override { return _mappedAddress; }

private:
    void * _mappedAddress;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif  // HDST_PERSISTENT_BUFFER_GL_H
