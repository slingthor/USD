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
#ifndef GARCH_GL_H
#define GARCH_GL_H

#include "pxr/pxr.h"
#include "pxr/base/arch/defines.h"

#if defined(ARCH_GFX_OPENGL)
#include <GL/glew.h>
#endif

#if defined(ARCH_OS_OSX)
// Apple installs OpenGL headers in a non-standard location.
#include <OpenGL/gl.h>
#elif defined(ARCH_OS_IOS)
#include <OpenGLES/ES3/gl.h>
#elif defined(ARCH_OS_WINDOWS)
// Windows must include Windows.h prior to gl.h
#include <Windows.h>
#include <GL/gl.h>
#else
#include <GL/gl.h>
#endif

#ifdef ARCH_OS_DARWIN

PXR_NAMESPACE_OPEN_SCOPE

typedef GLvoid (*ArchGLCallbackType)(...);

#if !defined(ARCH_GFX_OPENGL)
#define GL_RGBA16F 0x881A
#define GL_RGB16F 0x881B
#define GL_RGBA32F 0x8814
#define GL_RGB32F 0x8815
#define GL_COMPRESSED_RGBA_BPTC_UNORM 0x8E8C
#define GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT 0x8E8F
#define GL_TEXTURE_2D_ARRAY 0x8C1A
#define GL_TEXTURE_BUFFER 0x8C2A
#define GL_GEOMETRY_SHADER 0x8DD9
#define GL_LINES_ADJACENCY 0x000A
#define GL_LINE_STRIP_ADJACENCY 0x000B
#define GL_TRIANGLES_ADJACENCY 0x000C
#define GL_TRIANGLE_STRIP_ADJACENCY 0x000D
#define GL_PATCHES 0xE
#define GL_COMPUTE_SHADER 0x91B9
#define GL_TESS_EVALUATION_SHADER 0x8E87
#define GL_TESS_CONTROL_SHADER 0x8E88


#if defined(ARCH_OS_OSX)
#define GL_R8_SNORM 0x8F94
#define GL_RG8_SNORM 0x8F95
#define GL_RGB8_SNORM 0x8F96
#define GL_RGBA8_SNORM 0x8F97
#define GL_RGB32I 0x8D83
#define GL_RGBA32I 0x8D82
#define GL_UNSIGNED_INT64_ARB 0x140F
#define GL_SAMPLER_2D_ARRAY 0x8DC1
#define GL_INT_SAMPLER_BUFFER 0x8DD0
#define GL_INT_2_10_10_10_REV 0x8D9F
#endif

#if defined(ARCH_OS_IOS)
#define GL_DOUBLE 0x140A
#define GL_R16 0x822A
#define GL_RGB16 0x8054
#define GL_RGBA16 0x805B
#define GL_SRGB_ALPHA 0x8C42
#define GL_COLOR_INDEX 0x1900
#define GL_TEXTURE_1D 0x0DE0
#define GL_HALF_FLOAT_ARB 0x140B
#define GL_SAMPLES_PASSED 0x8914
#define GL_2_BYTES 0x1407
#define GL_3_BYTES 0x1408
#define GL_4_BYTES 0x1409
#define GL_UNSIGNED_INT64_ARB 0x140F
#define GL_INT_SAMPLER_BUFFER 0x8DD0
#define GL_CLAMP_TO_BORDER 0x812D
#define GL_SAMPLER_1D 0x8B5D
typedef double GLdouble;
typedef int64_t GLint64EXT;
typedef uint64_t GLuint64EXT;
typedef GLint64EXT  GLint64;
typedef GLuint64EXT GLuint64;
#endif // ARCH_OS_IOS
#endif // !ARCH_GFX_OPENGL

PXR_NAMESPACE_CLOSE_SCOPE

#else // !ARCH_OS_DARWIN

PXR_NAMESPACE_OPEN_SCOPE

typedef GLvoid (*ArchGLCallbackType)();

PXR_NAMESPACE_CLOSE_SCOPE

#endif // ARCH_OS_DARWIN

#endif // GARCH_GL_H
