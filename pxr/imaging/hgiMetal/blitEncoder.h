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
#ifndef HGIMETAL_BLIT_ENCODER_H
#define HGIMETAL_BLIT_ENCODER_H

#include "pxr/pxr.h"
#include "pxr/imaging/hgiMetal/api.h"
#include "pxr/imaging/hgi/blitEncoder.h"

PXR_NAMESPACE_OPEN_SCOPE

class HgiMetal;
class HgiMetalImmediateCommandBuffer;


/// \class HgiMetalBlitEncoder
///
/// Metal implementation of HgiBlitEncoder.
///
class HgiMetalBlitEncoder final : public HgiBlitEncoder
{
public:
    HGIMETAL_API
    virtual ~HgiMetalBlitEncoder();

    HGIMETAL_API
    void EndEncoding() override;

    HGIMETAL_API
    void PushDebugGroup(const char* label) override;

    HGIMETAL_API
    void PopDebugGroup() override;

    HGIMETAL_API
    void CopyTextureGpuToCpu(HgiTextureGpuToCpuOp const& copyOp) override;

    HGIMETAL_API
    void CopyBufferCpuToGpu(HgiBufferCpuToGpuOp const& copyOp) override;

    HGIMETAL_API
    void ResolveImage(HgiResolveImageOp const& resolveOp) override;

protected:
    friend class HgiMetal;
    friend class HgiMetalImmediateCommandBuffer;

    HGIMETAL_API
    HgiMetalBlitEncoder(HgiMetal* hgi, HgiMetalImmediateCommandBuffer* cmdBuf);

private:
    HgiMetalBlitEncoder() = delete;
    HgiMetalBlitEncoder & operator=(const HgiMetalBlitEncoder&) = delete;
    HgiMetalBlitEncoder(const HgiMetalBlitEncoder&) = delete;

    HgiMetal* _hgi;
    HgiMetalImmediateCommandBuffer* _commandBuffer;
    id<MTLBlitCommandEncoder> _blitEncoder;

    // Encoder is used only one frame so storing multi-frame state on encoder
    // will not survive. Store onto HgiMetalImmediateCommandBuffer instead.
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif
