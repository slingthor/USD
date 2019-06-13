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
#include "pxr/imaging/mtlf/OSDMetalContext.h"
#include "pxr/imaging/mtlf/mtlDevice.h"


PXR_NAMESPACE_OPEN_SCOPE


void OSDMetalContext::Init()
{
    device       = MtlfMetalContext::GetMetalContext()->device;
    commandQueue = MtlfMetalContext::GetMetalContext()->commandQueue;
}

#if OSD_METAL_DEFERRED

void OSDMetalContext::MetalWaitUntilCompleted(id<MTLCommandBuffer> cmdBuf)
{
    // We ignore this because we want to defer OSD execution until later
    // Non deferred version would be - [cmdBuf waitUntilCompleted];
}

id<MTLCommandBuffer> OSDMetalContext::MetalGetCommandBuffer(id<MTLCommandQueue> cmdQueue)
{
    // Ignore the provided command queue as we're using the Hydra one
    // Non deferred version would be -  return [cmdQueue commandBuffer];
    // OSD workloads go in the GS buffer as they need to go in the same pass as smooth normals and anything else done before we draw
    return MtlfMetalContext::GetMetalContext()->GetCommandBuffer(METALWORKQUEUE_GEOMETRY_SHADER);
}

void  OSDMetalContext::MetalCommitCommandBuffer(id<MTLCommandBuffer> cmdBuf)
{
    // We ignore this because we want to defer OSD execution until later
    // Non deferred version would be - [cmdBuf commit];
}

#if !__has_feature(objc_arc)
void OSDMetalContext::MetalReleaseMetalBuffer(id<MTLBuffer> buffer)
{
    bufferReleaseList.push_back(buffer);
}

#if 0
void OSDMetalContext::ReleaseOSDMetalBuffers()
{
    NSLog(@"Releasing %lu buffers", bufferReleaseList.size());
    // Free all of the OSD buffers
    for(auto buffer : bufferReleaseList) {
        [buffer release]
        delete buffer;
    }
    bufferReleaseList.clear();
}
#endif
#endif

#endif

PXR_NAMESPACE_CLOSE_SCOPE

