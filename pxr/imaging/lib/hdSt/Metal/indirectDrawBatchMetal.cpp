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

#include "pxr/imaging/garch/contextCaps.h"
#include "pxr/imaging/garch/glslfx.h"
#include "pxr/imaging/garch/resourceFactory.h"

#include "pxr/imaging/mtlf/diagnostic.h"

#include "pxr/imaging/hdSt/commandBuffer.h"
#include "pxr/imaging/hdSt/cullingShaderKey.h"
#include "pxr/imaging/hdSt/drawItemInstance.h"
#include "pxr/imaging/hdSt/geometricShader.h"
#include "pxr/imaging/hdSt/renderPassState.h"
#include "pxr/imaging/hdSt/resourceRegistry.h"
#include "pxr/imaging/hdSt/shaderCode.h"
#include "pxr/imaging/hdSt/shaderKey.h"

#include "pxr/imaging/hdSt/Metal/indirectDrawBatchMetal.h"
#include "pxr/imaging/hdSt/Metal/mslProgram.h"
#include "pxr/imaging/hdSt/Metal/persistentBufferMetal.h"

#include "pxr/imaging/hd/binding.h"
#include "pxr/imaging/hd/bufferArrayRange.h"
#include "pxr/imaging/hd/debugCodes.h"
#include "pxr/imaging/hd/engine.h"
#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/tokens.h"

#include "pxr/base/tf/diagnostic.h"
#include "pxr/base/tf/iterator.h"

#include <limits>

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PRIVATE_TOKENS(
    _tokens,

    (drawIndirectResult)

    (ulocResetPass)
);


static const GLuint64 HD_CULL_RESULT_TIMEOUT_NS = 5e9; // XXX how long to wait?

HdSt_IndirectDrawBatchMetal::HdSt_IndirectDrawBatchMetal(
    HdStDrawItemInstance * drawItemInstance)
    : HdSt_IndirectDrawBatch(drawItemInstance)
    , _resultBuffer(0)
{
    _Init(drawItemInstance);
}

HdSt_IndirectDrawBatchMetal::~HdSt_IndirectDrawBatchMetal()
{
    /* empty */
}

HdSt_IndirectDrawBatch::_CullingProgram *HdSt_IndirectDrawBatchMetal::NewCullingProgram() const
{
    return new HdSt_IndirectDrawBatchMetal::_CullingProgramMetal();
}

void
HdSt_IndirectDrawBatchMetal::_PrepareDraw(bool gpuCulling, bool freezeCulling)
{
    if (gpuCulling && !freezeCulling) {
        GarchContextCaps const &caps = GarchResourceFactory::GetInstance()->GetContextCaps();
        if (caps.IsEnabledGPUCountVisibleInstances()) {
//            _EndGPUCountVisibleInstances(_cullResultSync, &_numVisibleItems);
//            glDeleteSync(_cullResultSync);
//            _cullResultSync = 0;
        }
    }
}

void
HdSt_IndirectDrawBatchMetal::_ExecuteDraw(_DrawingProgram &program, int batchCount)
{
    if (_useDrawArrays) {
        TF_DEBUG(HD_MDI).Msg("MDI Drawing Arrays:\n"
                " - primitive mode: %d\n"
                " - indirect: %d\n"
                " - drawCount: %d\n"
                " - stride: %zu\n",
               program.GetGeometricShader()->GetPrimitiveMode(),
               0, batchCount,
               _dispatchBuffer->GetCommandNumUints()*sizeof(GLuint));

        TF_FATAL_CODING_ERROR("Not Implemented");
//        glMultiDrawArraysIndirect(
//            program.GetGeometricShader()->GetPrimitiveMode(),
//            0, // draw command always starts with 0
//            batchCount,
//            _dispatchBuffer->GetCommandNumUints()*sizeof(GLuint));
    } else {
        TF_DEBUG(HD_MDI).Msg("MDI Drawing Elements:\n"
                " - primitive mode: %d\n"
                " - buffer type: GL_UNSIGNED_INT\n"
                " - indirect: %d\n"
                " - drawCount: %d\n"
                " - stride: %zu\n",
               program.GetGeometricShader()->GetPrimitiveMode(),
               0, batchCount,
               _dispatchBuffer->GetCommandNumUints()*sizeof(GLuint));

//        TF_FATAL_CODING_ERROR("Not Implemented");
//        glMultiDrawElementsIndirect(
//            program.GetGeometricShader()->GetPrimitiveMode(),
//            GL_UNSIGNED_INT,
//            0, // draw command always starts with 0
//            batchCount,
//            _dispatchBuffer->GetCommandNumUints()*sizeof(GLuint));
    }
}

void
HdSt_IndirectDrawBatchMetal::_GPUFrustumCullingExecute(
    HdStResourceRegistrySharedPtr const &resourceRegistry,
    HdStProgramSharedPtr const &program,
    HdSt_ResourceBinder const &binder,
    HdBufferResourceSharedPtr cullCommandBuffer)
{
    GarchContextCaps const &caps = GarchResourceFactory::GetInstance()->GetContextCaps();
    
    if (caps.IsEnabledGPUCountVisibleInstances()) {
        _BeginGPUCountVisibleInstances(resourceRegistry);
    }

    TF_FATAL_CODING_ERROR("Not Implemented");
        
    int resetPass = 1;
    binder.BindUniformi(_tokens->ulocResetPass, 1, &resetPass);
/*  glMultiDrawArraysIndirect(
        GL_POINTS,
        reinterpret_cast<const GLvoid*>(
            static_cast<intptr_t>(cullCommandBuffer->GetOffset())),
        _dispatchBufferCullInput->GetCount(),
        cullCommandBuffer->GetStride());

    // dispatch buffer is bound via SSBO
    // (see _CullingProgram::_GetCustomBindings)
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
*/
    resetPass = 0;
    binder.BindUniformi(_tokens->ulocResetPass, 1, &resetPass);
/*  glMultiDrawArraysIndirect(
        GL_POINTS,
        reinterpret_cast<const GLvoid*>(
            static_cast<intptr_t>(cullCommandBuffer->GetOffset())),
        _dispatchBufferCullInput->GetCount(),
        cullCommandBuffer->GetStride());
*/
}

void
HdSt_IndirectDrawBatchMetal::_SyncFence() {
    /* empty */
}

void
HdSt_IndirectDrawBatchMetal::_GPUFrustumCullingXFBExecute(
      HdStResourceRegistrySharedPtr const &resourceRegistry,
      HdStProgramSharedPtr const &program)
{
    GarchContextCaps const &caps = GarchResourceFactory::GetInstance()->GetContextCaps();
    if (caps.IsEnabledGPUCountVisibleInstances()) {
        _BeginGPUCountVisibleInstances(resourceRegistry);
    }

//    TF_FATAL_CODING_ERROR("Not Implemented");

    MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();

//    context->SetBuffer(1, _dispatchBuffer->GetEntireResource()->GetId(), TfToken("drawingCoord1"));

//    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0,
//                     _dispatchBuffer->GetEntireResource()->GetId());
//    glBeginTransformFeedback(GL_POINTS);

    program->DrawArrays(GL_POINTS, 0, _dispatchBufferCullInput->GetCount());

//    if (caps.IsEnabledGPUCountVisibleInstances()) {
//        glMemoryBarrier(GL_TRANSFORM_FEEDBACK_BARRIER_BIT);
//        _cullResultSync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
//    } else {
//        _cullResultSync = 0;
//    }

//    glEndTransformFeedback();
//    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);
}

void
HdSt_IndirectDrawBatchMetal::_BeginGPUCountVisibleInstances(
    HdStResourceRegistrySharedPtr const &resourceRegistry)
{
    if (!_resultBuffer) {
        _resultBuffer = boost::dynamic_pointer_cast<HdStPersistentBufferMetal>(
            resourceRegistry->RegisterPersistentBuffer(
                _tokens->drawIndirectResult, sizeof(GLint), 0));
    }

    // Reset visible item count
    *((GLint *)_resultBuffer->GetMappedAddress()) = 0;
}

void
HdSt_IndirectDrawBatchMetal::_EndGPUCountVisibleInstances(GLsync resultSync, size_t * result)
{
    /*
    GLenum status = glClientWaitSync(resultSync,
            GL_SYNC_FLUSH_COMMANDS_BIT, HD_CULL_RESULT_TIMEOUT_NS);

    if (status != GL_ALREADY_SIGNALED && status != GL_CONDITION_SATISFIED) {
        // We could loop, but we don't expect to timeout.
        TF_RUNTIME_ERROR("Unexpected ClientWaitSync timeout");
        *result = 0;
        return;
    }

    // Return visible item count
    *result = *((int*)_resultBuffer->GetMappedAddress());

    // XXX: temporarily hack during refactoring.
    // we'd like to use the same API as other buffers.
    int binding = _cullingProgram.GetBinder().GetBinding(
        HdTokens->drawIndirectResult).GetLocation();
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, 0);*/
}

/* virtual */
bool
HdSt_IndirectDrawBatchMetal::_CullingProgramMetal::_Link(
        HdStProgramSharedPtr const & program)
{
    if (!TF_VERIFY(program)) return false;

    return HdSt_DrawBatch::_DrawingProgram::_Link(program);
}

PXR_NAMESPACE_CLOSE_SCOPE

