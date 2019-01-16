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
#ifndef HDST_BUFFER_RELOCATOR_GL_H
#define HDST_BUFFER_RELOCATOR_GL_H

#include "pxr/pxr.h"
#include "pxr/imaging/hdSt/api.h"
#include "pxr/imaging/hdSt/bufferRelocator.h"
#include "pxr/imaging/hd/resource.h"

PXR_NAMESPACE_OPEN_SCOPE

/// \class HdStBufferRelocatorGL
///
/// A utility class to perform batched buffer copy.
///
class HdStBufferRelocatorGL: public HdStBufferRelocator {
public:
    HDST_API
    HdStBufferRelocatorGL(HdResourceGPUHandle srcBuffer, HdResourceGPUHandle dstBuffer);

    /// Execute GL buffer copy command to flush all scheduled range copies.
    HDST_API
    virtual void Commit() override;

private:

    GLuint _srcBuffer;
    GLuint _dstBuffer;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDST_BUFFER_RELOCATOR_GL_H
