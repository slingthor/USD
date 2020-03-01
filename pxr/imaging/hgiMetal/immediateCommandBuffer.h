//
// Copyright 2019 Pixar
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
#ifndef HGIMETAL_IMMEDIATE_COMMAND_BUFFER_H
#define HGIMETAL_IMMEDIATE_COMMAND_BUFFER_H

#include "pxr/pxr.h"
#include "pxr/imaging/hgi/immediateCommandBuffer.h"
#include "pxr/imaging/hgiMetal/api.h"
#include <vector>
#include <Metal/Metal.h>

PXR_NAMESPACE_OPEN_SCOPE

/// \class HgiMetalImmediateCommandBuffer
///
/// Metal implementation of HgiImmediateCommandBuffer
///
class HgiMetalImmediateCommandBuffer final : public HgiImmediateCommandBuffer
{
public:
    HGIMETAL_API
    virtual ~HgiMetalImmediateCommandBuffer();

    HGIMETAL_API
    HgiGraphicsEncoderUniquePtr CreateGraphicsEncoder(
        HgiGraphicsEncoderDesc const& desc) override;

    HGIMETAL_API
    HgiBlitEncoderUniquePtr CreateBlitEncoder() override;
    
    HGIMETAL_API
    void FlushEncoders() override;
    
    /// Metal Specific
    id<MTLCommandBuffer> GetCommandBuffer() {
        return _commandBuffer;
    }

protected:
    friend class HgiMetal;

    HGIMETAL_API
    HgiMetalImmediateCommandBuffer(id<MTLDevice> device);
    
private:
    HgiMetalImmediateCommandBuffer & operator=
        (const HgiMetalImmediateCommandBuffer&) = delete;
    HgiMetalImmediateCommandBuffer(const HgiMetalImmediateCommandBuffer&) = delete;

    friend std::ostream& operator<<(
        std::ostream& out,
        const HgiMetalImmediateCommandBuffer& cmdBuf);

    id<MTLDevice> _device;
    id<MTLCommandQueue> _commandQueue;
    id<MTLCommandBuffer> _commandBuffer;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif
