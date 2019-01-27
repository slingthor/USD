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
#ifndef ARCH_DEFINES_H
#define ARCH_DEFINES_H

#include "pxr/pxr.h"

PXR_NAMESPACE_OPEN_SCOPE

//
// OS
//

#if defined(__linux__)
#define ARCH_OS_LINUX
#elif defined(__APPLE__)
#include "TargetConditionals.h"
#include <Availability.h>
#define ARCH_OS_DARWIN
#if TARGET_OS_IPHONE
#define ARCH_OS_IOS
#else
#define ARCH_OS_MACOS
#endif
#elif defined(_WIN32) || defined(_WIN64)
#define ARCH_OS_WINDOWS
#endif

//
// Processor
//

#if defined(ARCH_OS_IOS)
# if defined(__i386__)
#  define ARCH_CPU_INTEL
# else
#  define ARCH_CPU_ARM
# endif
#else
#   if defined(i386) || defined(__i386__) || defined(__x86_64__) || \
defined(_M_IX86) || defined(_M_X64)
#       define ARCH_CPU_INTEL
#   elif defined(__arm__) || defined(__aarch64__) || defined(_M_ARM)
#       define ARCH_CPU_ARM
#   endif
#endif

//
// Bits
//

#if defined(__x86_64__) || defined(__aarch64__) || defined(_M_X64) || defined(ARCH_CPU_ARM)
#define ARCH_BITS_64
#elif defined(ARCH_OS_IOS) && defined(ARCH_CPU_INTEL)
#define ARCH_BITS_32
#else
#error "Unsupported architecture.  x86_64 or ARM64 required."
#endif

//
// Compiler
//

#if defined(__clang__)
#define ARCH_COMPILER_CLANG
#define ARCH_COMPILER_CLANG_MAJOR __clang_major__
#define ARCH_COMPILER_CLANG_MINOR __clang_minor__
#define ARCH_COMPILER_CLANG_PATCHLEVEL __clang_patchlevel__
#elif defined(__GNUC__)
#define ARCH_COMPILER_GCC
#define ARCH_COMPILER_GCC_MAJOR __GNUC__
#define ARCH_COMPILER_GCC_MINOR __GNUC_MINOR__
#define ARCH_COMPILER_GCC_PATCHLEVEL __GNUC_PATCHLEVEL__
#elif defined(__ICC)
#define ARCH_COMPILER_ICC
#elif defined(_MSC_VER)
#define ARCH_COMPILER_MSVC
#define ARCH_COMPILER_MSVC_VERSION _MSC_VER
#endif

//
// Features
//

// Only use the GNU STL extensions on Linux when using gcc.
#if defined(ARCH_OS_LINUX) && defined(ARCH_COMPILER_GCC)
#define ARCH_HAS_GNU_STL_EXTENSIONS
#endif

// The current version of Apple clang does not support the thread_local
// keyword.
#define ARCH_HAS_THREAD_LOCAL

// Memmap usage - iOS has constraints on the size that can be mapped.
// Disable for now as it's currently always or never
#if !defined(ARCH_OS_IOS)
#define ARCH_HAS_MMAP
#endif

// The MAP_POPULATE flag for mmap calls only exists on Linux platforms.
#if defined(ARCH_OS_LINUX)
#define ARCH_HAS_MMAP_MAP_POPULATE
#endif

// OpenGL API present
#if defined(PXR_OPENGL_SUPPORT_ENABLED)
#define ARCH_GFX_OPENGL
#endif

// Metal API present
#if defined(PXR_METAL_SUPPORT_ENABLED)
#define ARCH_GFX_METAL
#define PXR_UNITTEST_GFX_ARCH HdEngine::Metal
#if (__MAC_OS_X_VERSION_MAX_ALLOWED >= 101400) || (__IPHONE_OS_VERSION_MAX_ALLOWED >= 120000) /* __MAC_10_14 __IOS_12_00 */
#define METAL_EVENTS_API_PRESENT
#endif
#if defined(ARCH_OS_MACOS)
#define MTLResourceStorageModeDefault MTLResourceStorageModeManaged
#else
#define MTLResourceStorageModeDefault MTLResourceStorageModeShared
#endif
#endif

#if !defined(PXR_UNITTEST_GFX_ARCH)
#define PXR_UNITTEST_GFX_ARCH HdEngine::OpenGL
#endif

PXR_NAMESPACE_CLOSE_SCOPE

#endif // ARCH_DEFINES_H 
