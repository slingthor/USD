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
#ifndef HDST_INDIRECT_DRAW_BATCH_H
#define HDST_INDIRECT_DRAW_BATCH_H

#include "pxr/pxr.h"
#include "pxr/imaging/hd/version.h"
#include "pxr/imaging/hdSt/api.h"
#include "pxr/imaging/hdSt/dispatchBuffer.h"
#include "pxr/imaging/hdSt/drawBatch.h"
#include "pxr/imaging/hdSt/persistentBuffer.h"

#include <vector>

PXR_NAMESPACE_OPEN_SCOPE


typedef std::vector<HdBindingRequest> HdBindingRequestVector;

/// \class HdSt_IndirectDrawBatch
///
/// Drawing batch that is executed from an indirect dispatch buffer.
///
/// An indirect drawing batch accepts draw items that have the same
/// primitive mode and that share aggregated drawing resources,
/// e.g. uniform and non uniform primvar buffers.
///
class HdSt_IndirectDrawBatch : public HdSt_DrawBatch {
public:
    static HDST_API
    HdSt_DrawBatchSharedPtr New(HdStDrawItemInstance * drawItemInstance);

    HDST_API
    virtual ~HdSt_IndirectDrawBatch();

    // HdSt_DrawBatch overrides
    HDST_API
    virtual bool Validate(bool deepValidation) = 0;

    /// Prepare draw commands and apply view frustum culling for this batch.
    HDST_API
    virtual void PrepareDraw(
        HdStRenderPassStateSharedPtr const &renderPassState,
        HdStResourceRegistrySharedPtr const &resourceRegistry) override = 0;

    /// Executes the drawing commands for this batch.
    HDST_API
    virtual void ExecuteDraw(
        HdStRenderPassStateSharedPtr const &renderPassState,
        HdStResourceRegistrySharedPtr const &resourceRegistry) override = 0;

    HDST_API
    virtual void DrawItemInstanceChanged(HdStDrawItemInstance const* instance) = 0;

protected:
    HDST_API
    HdSt_IndirectDrawBatch(HdStDrawItemInstance * drawItemInstance);
    HDST_API
    virtual void _Init(HdStDrawItemInstance * drawItemInstance) override = 0;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDST_INDIRECT_DRAW_BATCH_H
