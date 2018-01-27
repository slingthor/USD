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
#include "pxr/imaging/glf/glew.h"

#include "pxr/imaging/hdx/drawTargetResolveTask.h"
#include "pxr/imaging/hdx/drawTargetRenderPass.h"
#include "pxr/imaging/hdx/tokens.h"

#include "pxr/imaging/hd/engine.h"

#include "pxr/imaging/hdSt/drawTarget.h"

#include "pxr/imaging/glf/drawTarget.h"
#if defined(ARCH_GFX_METAL)
#include "pxr/imaging/mtlf/drawTarget.h"
#endif

PXR_NAMESPACE_OPEN_SCOPE


HdxDrawTargetResolveTask::HdxDrawTargetResolveTask(HdSceneDelegate* delegate,
                                                   SdfPath const& id)
 : HdSceneTask(delegate, id)
{
}

void
HdxDrawTargetResolveTask::_Sync(HdTaskContext* ctx)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();
}

void
HdxDrawTargetResolveTask::_Execute(HdTaskContext* ctx)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    // Extract the list of render pass for draw targets from the task context.
    // This list is set from drawTargetTask.cpp during Sync phase.
    std::vector< std::unique_ptr<HdxDrawTargetRenderPass> >  *passes;
    if (!_GetTaskContextData(ctx, HdxTokens->drawTargetRenderPasses, &passes)) {
        return;
    }

    // Iterate through all renderpass (drawtarget renderpass), extract the
    // draw target and resolve them if needed. We need to resolve them to 
    // regular buffers so use them in the rest of the pipeline.
    size_t numDrawTargets = passes->size();
    if (numDrawTargets > 0) {
        std::vector<GarchDrawTarget*> drawTargets(numDrawTargets);

        for (size_t i = 0; i < numDrawTargets; ++i) {
            drawTargets[i] = boost::get_pointer((*passes)[i]->GetDrawTarget());
        }
        switch(HdEngine::GetRenderAPI()) {
            case HdEngine::OpenGL:
                GlfDrawTarget::Resolve(drawTargets);
                break;
#if defined(ARCH_GFX_METAL)
            case HdEngine::Metal:
                MtlfDrawTarget::Resolve(drawTargets);
                break;
#endif
            default:
                TF_FATAL_CODING_ERROR("No program for this API");
        }
    }
}

PXR_NAMESPACE_CLOSE_SCOPE

