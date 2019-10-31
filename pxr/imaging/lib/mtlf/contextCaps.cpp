//
// Copyright 2018 Pixar
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
#include "pxr/imaging/mtlf/contextCaps.h"
#include "pxr/imaging/mtlf/mtlDevice.h"

#include "pxr/base/tf/diagnostic.h"
#include "pxr/base/tf/envSetting.h"

#include <iostream>
#include <mutex>

PXR_NAMESPACE_OPEN_SCOPE

// Initialize members to ensure a sane starting state.
MtlfContextCaps::MtlfContextCaps()
{
    _LoadCaps();
}

int MtlfContextCaps::GetAPIVersion()
{
#if defined(ARCH_OS_IOS)
#define SYSTEM_VERSION_GREATER_THAN_OR_EQUAL_TO(v) ([[[UIDevice currentDevice] systemVersion] compare:v options:NSNumericSearch] != NSOrderedAscending)
    
    static bool sysVerGreaterThanOrEqualTo11_0 = SYSTEM_VERSION_GREATER_THAN_OR_EQUAL_TO(@"11.0");
    static bool sysVerGreaterThanOrEqualTo12_0 = SYSTEM_VERSION_GREATER_THAN_OR_EQUAL_TO(@"12.0");
    static bool sysVerGreaterThanOrEqualTo13_0 = SYSTEM_VERSION_GREATER_THAN_OR_EQUAL_TO(@"13.0");

    if (sysVerGreaterThanOrEqualTo13_0) {
        return APIVersion_Metal3_0;
    }
    else if (sysVerGreaterThanOrEqualTo11_0) {
        return APIVersion_Metal2_0;
    }
    
#else // ARCH_OS_IOS
    static NSOperatingSystemVersion minimumSupportedOSVersion13_0 = { .majorVersion = 10, .minorVersion = 13, .patchVersion = 0 };
    static NSOperatingSystemVersion minimumSupportedOSVersion14_0 = { .majorVersion = 10, .minorVersion = 14, .patchVersion = 0 };
    static NSOperatingSystemVersion minimumSupportedOSVersion15_0 = { .majorVersion = 10, .minorVersion = 15, .patchVersion = 0 };
    static bool sysVerGreaterOrEqualTo13_0 = [NSProcessInfo.processInfo isOperatingSystemAtLeastVersion:minimumSupportedOSVersion13_0];
    static bool sysVerGreaterOrEqualTo14_0 = [NSProcessInfo.processInfo isOperatingSystemAtLeastVersion:minimumSupportedOSVersion14_0];
    static bool sysVerGreaterOrEqualTo15_0 = [NSProcessInfo.processInfo isOperatingSystemAtLeastVersion:minimumSupportedOSVersion15_0];

    if (sysVerGreaterOrEqualTo15_0) {
        return APIVersion_Metal3_0;
    }
    else if (sysVerGreaterOrEqualTo13_0) {
        return APIVersion_Metal2_0;
    }
    
#endif // ARCH_OS_IOS

    return APIVersion_Metal1_0;
}

void
MtlfContextCaps::_LoadCaps()
{
    apiVersion                   = GetAPIVersion();
    
    if (apiVersion == APIVersion_Metal1_0)
        return;

    glslVersion                  = 450;
    arrayTexturesEnabled         = false;
    shaderStorageBufferEnabled   = true;
    bindlessTextureEnabled       = false;
    bindlessBufferEnabled        = false;
    multiDrawIndirectEnabled     = false;
    directStateAccessEnabled     = false;
    bufferStorageEnabled         = true;
    shadingLanguage420pack       = true;
    explicitUniformLocation      = true;
    maxArrayTextureLayers        = 2048;
    maxUniformBlockSize          = 64*1024;
    maxShaderStorageBlockSize    = 1*1024*1024*1024;
    maxTextureBufferSize         = 16*1024;
    uniformBufferOffsetAlignment = 16;  //This limit isn't an actual thing for Metal. 16 is equal to the alignment rules of std140, which is convenient, nothing more.
    flipTexturesOnLoad           = true;
    useCppShaderPadding          = true;
    hasSubDataCopy               = true;
    alwaysNeedsBinding           = true;
    floatingPointBuffersEnabled  = true;
    hasDispatchCompute           = true;
    hasBufferBindOffset          = true;
    maxClipPlanes                = 16;
#if defined(ARCH_OS_IOS)
    hasMipLevelTextureWrite      = true;
#else
    hasMipLevelTextureWrite      = false;
#endif

#if OPENSUBDIV_HAS_METAL_COMPUTE
    //METAL_TODO: Metal always has compute capabilities. gpuComputeNormals only affects
    //            normal generation which currently has some problems for Metal.
    gpuComputeEnabled            = IsGPUComputeEnabled();
    gpuComputeNormalsEnabled     = true;
#endif
}


PXR_NAMESPACE_CLOSE_SCOPE

