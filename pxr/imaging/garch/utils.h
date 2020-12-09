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
#ifndef GARCH_UTILS_H
#define GARCH_UTILS_H

/// \file garch/utils.h

#include "pxr/pxr.h"
#include "pxr/imaging/garch/api.h"
#include "pxr/imaging/hio/image.h"
#include "pxr/imaging/garch/glApi.h"
#include "pxr/imaging/hio/types.h"

#include <string>

PXR_NAMESPACE_OPEN_SCOPE

/// Number of elements.
///
/// Returns the number of elements (channels) in a given GL enum format.
///
/// Supported formats are : GL_DEPTH_COMPONENT, GL_COLOR_INDEX, GL_ALPHA,
/// GL_RED, GL_LUMINANCE, GL_RG, GL_LUMINANCE_ALPHA, GL_RGB, GL_RGBA
GARCH_API
int GarchGetNumElements(GLenum format);

/// Byte size of a GL type.
///
/// Returns the size in bytes of a given GL type.
///
/// Supported types are : GL_UNSIGNED_BYTE, GL_BYTE, GL_UNSIGNED_SHORT,
/// GL_SHORT, GL_FLOAT, GL_DOUBLE
GARCH_API
int GarchGetElementSize(GLenum type);

/// HioFormat
///
/// Returns the HioFormat for the given GL format and GL type
///
/// Supported formats are : GL_DEPTH_COMPONENT, GL_COLOR_INDEX, GL_ALPHA,
/// GL_RED, GL_LUMINANCE, GL_RG, GL_LUMINANCE_ALPHA, GL_RGB, GL_RGBA
///
/// Supported types are : GL_UNSIGNED_BYTE, GL_BYTE, GL_UNSIGNED_SHORT,
/// GL_SHORT, GL_FLOAT, GL_DOUBLE
GARCH_API
HioFormat GarchGetHioFormat(uint32_t glFormat, uint32_t glType, bool isSRGB);


PXR_NAMESPACE_CLOSE_SCOPE

#endif // GARCH_UTILS_H
