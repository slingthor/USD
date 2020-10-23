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
#ifndef PXR_IMAGING_GLF_UTILS_H
#define PXR_IMAGING_GLF_UTILS_H

/// \file glf/utils.h

#include "pxr/pxr.h"
#include "pxr/imaging/glf/api.h"
#include "pxr/imaging/hio/image.h"
#include "pxr/imaging/garch/gl.h"
#include "pxr/imaging/hio/types.h"

#include <string>

PXR_NAMESPACE_OPEN_SCOPE

/// Base image format
///
/// Returns the base image format for the given number of components
///
/// Supported number of components: 1, 2, 3, 4
GLF_API
GLenum GlfGetBaseFormat(int numComponents);

/// GL type.
///
/// Returns the GL type for a given HioFormat.
GLF_API
GLenum GlfGetGLType(HioFormat format);

/// GL format.
///
/// Returns the GL format for a given HioFormat.
GLF_API
GLenum GlfGetGLFormat(HioFormat format);

/// GL Internal Format.
///
/// Returns the GL Internal Format for a given HioFormat.
GLF_API
GLenum GlfGetGLInternalFormat(HioFormat format);

/// Checks the valitidy of a GL framebuffer
///
/// True if the currently bound GL framebuffer is valid and can be bound
/// or returns the cause of the problem
GLF_API
bool GlfCheckGLFrameBufferStatus(GLuint target, std::string * reason);

PXR_NAMESPACE_CLOSE_SCOPE

#endif
