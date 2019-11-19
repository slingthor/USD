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
#ifndef MTLF_DIAGNOSTIC_H
#define MTLF_DIAGNOSTIC_H

/// \file mtlf/diagnostic.h

#include "pxr/pxr.h"
#include "pxr/imaging/mtlf/api.h"
#include "pxr/imaging/garch/gl.h"
#include "pxr/base/tf/diagnostic.h"

#include <string>
#include <cstdint>

PXR_NAMESPACE_OPEN_SCOPE

/// Posts diagnostic errors for all GL errors in the current context.
/// This macro tags the diagnostic errors with the name of the calling
/// function.
#define GLF_POST_PENDING_GL_ERRORS() \
MtlfPostPendingGLErrors(__ARCH_PRETTY_FUNCTION__)

/// Posts diagnostic errors for all GL errors in the current context.
MTLF_API
void MtlfPostPendingGLErrors(std::string const & where = std::string());

/// Registers GlfDefaultDebugOutputMessageCallback as the
/// debug message callback for the current GL context.
MTLF_API
void MtlfRegisterDefaultDebugOutputMessageCallback();

/// A GL debug output message callback method which posts diagnostic
/// errors for messages of type DEBUG_TYPE_ERROR and diagnostic warnings
/// for other message types.
MTLF_API
void MtlfDefaultDebugOutputMessageCallback(
                                          GLenum source, GLenum type, GLuint id, GLenum severity,
                                          GLsizei length, char const * message, GLvoid const * userParam);

/// Returns a string representation of debug output enum values.
MTLF_API
char const * MtlfDebugEnumToString(GLenum debugEnum);

/// \class MtlfDebugGroup
///
/// Represents a GL debug group in Mtlf
///
class MtlfDebugGroup {
    public:
    /// Pushes a new debug group onto the GL api debug trace stack
    MTLF_API
    MtlfDebugGroup(char const *message);
    
    /// Pops a debug group off the GL api debug trace stack
    MTLF_API
    ~MtlfDebugGroup();

    MtlfDebugGroup() = delete;
    MtlfDebugGroup(MtlfDebugGroup const&) = delete;
    MtlfDebugGroup& operator =(MtlfDebugGroup const&) = delete;
};

/// \class MtlfMetalQueryObject
///
/// Represents a GL query object in Mtlf
///
class MtlfMetalQueryObject {
public:
    MTLF_API
    MtlfMetalQueryObject();
    MTLF_API
    ~MtlfMetalQueryObject();

    /// Begin query for the given \p target
    /// target has to be one of
    ///   GL_SAMPLES_PASSED, GL_ANY_SAMPLES_PASSED,
    ///   GL_ANY_SAMPLES_PASSED_CONSERVATIVE, GL_PRIMITIVES_GENERATED
    ///   GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN
    ///   GL_TIME_ELAPSED, GL_TIMESTAMP
    MTLF_API
    void Begin(GLenum target);

    /// equivalent to Begin(GL_SAMPLES_PASSED).
    /// The number of samples that pass the depth test for all drawing
    /// commands within the scope of the query will be returned.
    MTLF_API
    void BeginSamplesPassed();

    /// equivalent to Begin(GL_PRIMITIVES_GENERATED).
    /// The number of primitives sent to the rasterizer by the scoped
    /// drawing command will be returned.
    MTLF_API
    void BeginPrimitivesGenerated();

    /// equivalent to Begin(GL_TIME_ELAPSED).
    /// The time that it takes for the GPU to execute all of the scoped commands
    /// will be returned in nanoseconds.
    MTLF_API
    void BeginTimeElapsed();

    /// End query
    MTLF_API
    void End();

    /// Return the query result (synchronous)
    /// stalls CPU until the result becomes available.
    MTLF_API
    int64_t GetResult();

    /// Return the query result (asynchronous)
    /// returns 0 if the result hasn't been available.
    MTLF_API
    int64_t GetResultNoWait();

    MtlfMetalQueryObject(MtlfMetalQueryObject const&) = delete;
    MtlfMetalQueryObject& operator =(MtlfMetalQueryObject const&) = delete;
private:
    GLenum  _target;
    int64_t _value;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif
