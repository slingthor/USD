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
#ifndef HDST_RESOURCE_GL_H
#define HDST_RESOURCE_GL_H

#include "pxr/pxr.h"
#include "pxr/imaging/hdSt/api.h"
#include "pxr/imaging/garch/gl.h"
#include "pxr/imaging/hd/resource.h"
#include "pxr/base/tf/token.h"

#include <boost/shared_ptr.hpp>

#include <cstddef>

PXR_NAMESPACE_OPEN_SCOPE


typedef boost::shared_ptr<class HdStResourceGL> HdStResourceGLSharedPtr;

/// \class HdStResourceGL
///
/// Base class for simple OpenGL resource objects.
///
class HdStResourceGL : public HdResource {
public:
    HD_API
    HdStResourceGL(TfToken const & role);
    HD_API
    virtual ~HdStResourceGL();

    /// The graphics API name/identifier for this resource and its size
    HDST_API
    virtual void SetAllocation(HdBufferResourceGPUHandle id, size_t size) override;
    
    /// The abstract name/identifier for this resource
    HDST_API
    virtual HdBufferResourceGPUHandle GetId() const override { return (HdBufferResourceGPUHandle)(uint64_t)_id; }
    
    /// The OpenGL name/identifier for this resource
    HDST_API
    virtual GLuint GetOpenGLId() const { return _id; }
    
    /// The OpenGL name/identifier for this resource and its size
    HDST_API
    virtual void SetAllocation(GLuint id, size_t size);

    
private:
    GLuint _id;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif //HDST_RESOURCE_GL_H
