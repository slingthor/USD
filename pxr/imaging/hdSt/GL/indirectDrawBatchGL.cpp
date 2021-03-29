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
#include "pxr/imaging/garch/glApi.h"
#include "pxr/imaging/glf/diagnostic.h"

#include "pxr/imaging/garch/contextCaps.h"
#include "pxr/imaging/hio/glslfx.h"
#include "pxr/imaging/garch/resourceFactory.h"

#include "pxr/imaging/hdSt/commandBuffer.h"
#include "pxr/imaging/hdSt/cullingShaderKey.h"
#include "pxr/imaging/hdSt/drawItemInstance.h"
#include "pxr/imaging/hdSt/geometricShader.h"
#include "pxr/imaging/hdSt/glslProgram.h"
#include "pxr/imaging/hdSt/renderPassState.h"
#include "pxr/imaging/hdSt/resourceRegistry.h"
#include "pxr/imaging/hdSt/shaderCode.h"
#include "pxr/imaging/hdSt/shaderKey.h"

#include "pxr/imaging/hdSt/GL/glslProgramGL.h"
#include "pxr/imaging/hdSt/GL/indirectDrawBatchGL.h"

#include "pxr/imaging/hd/binding.h"
#include "pxr/imaging/hd/bufferArrayRange.h"
#include "pxr/imaging/hd/debugCodes.h"
#include "pxr/imaging/hd/engine.h"
#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/tokens.h"

#include "pxr/imaging/hgi/blitCmds.h"
#include "pxr/imaging/hgi/blitCmdsOps.h"

#include "pxr/base/tf/diagnostic.h"
#include "pxr/base/tf/envSetting.h"
#include "pxr/base/tf/getenv.h"
#include "pxr/base/tf/iterator.h"

#include <iostream>
#include <limits>

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PRIVATE_TOKENS(
    _tokens,
                         
    (drawIndirectResult)

    (ulocResetPass)
);


static const GLuint64 HD_CULL_RESULT_TIMEOUT_NS = 5e9; // XXX how long to wait?

HdSt_IndirectDrawBatchGL::HdSt_IndirectDrawBatchGL(
    HdStDrawItemInstance * drawItemInstance)
: HdSt_IndirectDrawBatch(drawItemInstance)
{
    _Init(drawItemInstance);
}

HdSt_IndirectDrawBatchGL::~HdSt_IndirectDrawBatchGL()
{
    /* empty */
}

HdSt_IndirectDrawBatch::_CullingProgram *HdSt_IndirectDrawBatchGL::NewCullingProgram() const
{
    return new HdSt_IndirectDrawBatchGL::_CullingProgramGL();
}

void
HdSt_IndirectDrawBatchGL::_PrepareDraw(
    HdStResourceRegistrySharedPtr const &resourceRegistry,
    bool gpuCulling,
    bool freezeCulling)
{
    if (!glBindBuffer) return; // glew initialized

    GLF_GROUP_FUNCTION();

    GarchContextCaps const &caps = GarchResourceFactory::GetInstance()->GetContextCaps();
    
    if (TfDebug::IsEnabled(HD_DRAWITEM_DRAWN)) {
        void const *bufferData = NULL;
        // instanceCount is a second entry of drawcommand for both
        // DrawArraysIndirect and DrawElementsIndirect.
        const void *instanceCountOffset =
            (const void*)
            (_dispatchBuffer->GetResource(HdTokens->drawDispatch)->GetOffset()
             + sizeof(GLuint));
        const int dispatchBufferStride =
            _dispatchBuffer->GetEntireResource()->GetStride();

        if (gpuCulling) {
            HgiBufferHandle const& buffer =
                _dispatchBuffer->GetEntireResource()->GetId();
            if (caps.directStateAccessEnabled) {
                bufferData = glMapNamedBufferEXT(
                    buffer->GetRawResource(), GL_READ_ONLY);

            } else {
                glBindBuffer(GL_ARRAY_BUFFER, buffer->GetRawResource());
                bufferData = glMapBuffer(GL_ARRAY_BUFFER, GL_READ_ONLY);
                glBindBuffer(GL_ARRAY_BUFFER, 0);
            }
        }

        for (size_t item=0; item<_drawItemInstances.size(); ++item) {
            HdStDrawItemInstance const * drawItemInstance =
                _drawItemInstances[item];

            if(!drawItemInstance->IsVisible()) {
                continue;
            }

            HdDrawItem const * drawItem = drawItemInstance->GetDrawItem();

            if (gpuCulling) {
                GLint const *instanceCount =
                    (GLint const *)(
                        (ptrdiff_t)(bufferData)
                        + (ptrdiff_t)(instanceCountOffset)
                        + item*dispatchBufferStride);

                bool isVisible = (*instanceCount > 0);
                if (!isVisible) {
                    continue;
                }
            }

            std::stringstream ss;
            ss << *drawItem;
            TF_DEBUG(HD_DRAWITEM_DRAWN).Msg("PREP DRAW: \n%s\n", 
                    ss.str().c_str());
        }

        if (gpuCulling) {
            HgiBufferHandle const& buffer =
                _dispatchBuffer->GetEntireResource()->GetId();
            if (caps.directStateAccessEnabled) {
                glUnmapNamedBuffer(buffer->GetRawResource());
            } else {
                glBindBuffer(GL_ARRAY_BUFFER, buffer->GetRawResource());
                glUnmapBuffer(GL_ARRAY_BUFFER);
                glBindBuffer(GL_ARRAY_BUFFER, 0);
            }
        }
    }

    if (gpuCulling && !freezeCulling) {
        if (caps.IsEnabledGPUCountVisibleInstances()) {
            _EndGPUCountVisibleInstances(resourceRegistry, &_numVisibleItems);
        }
    }
}

void
HdSt_IndirectDrawBatchGL::_ExecuteDraw(_DrawingProgram &program, int batchCount)
{
    if (!glBindBuffer) return; // glew initialized

    GLF_GROUP_FUNCTION();

    if (_useDrawArrays) {
        TF_DEBUG(HD_MDI).Msg("MDI Drawing Arrays:\n"
                " - primitive mode: %d\n"
                " - indirect: %d\n"
                " - drawCount: %d\n"
                " - stride: %zu\n",
               program.GetGeometricShader()->GetPrimitiveMode(),
               0, batchCount,
               _dispatchBuffer->GetCommandNumUints()*sizeof(uint32_t));

        glMultiDrawArraysIndirect(
            program.GetGeometricShader()->GetPrimitiveMode(),
            0, // draw command always starts with 0
            batchCount,
            _dispatchBuffer->GetCommandNumUints()*sizeof(uint32_t));
    } else {
        TF_DEBUG(HD_MDI).Msg("MDI Drawing Elements:\n"
                " - primitive mode: %d\n"
                " - buffer type: GL_UNSIGNED_INT\n"
                " - indirect: %d\n"
                " - drawCount: %d\n"
                " - stride: %zu\n",
               program.GetGeometricShader()->GetPrimitiveMode(),
               0, batchCount,
               _dispatchBuffer->GetCommandNumUints()*sizeof(uint32_t));

        glMultiDrawElementsIndirect(
            program.GetGeometricShader()->GetPrimitiveMode(),
            GL_UNSIGNED_INT,
            0, // draw command always starts with 0
            batchCount,
            _dispatchBuffer->GetCommandNumUints()*sizeof(uint32_t));
    }
}

void
HdSt_IndirectDrawBatchGL::_GPUFrustumInstanceCullingExecute(
    HdStResourceRegistrySharedPtr const &resourceRegistry,
    HdStGLSLProgramSharedPtr const &program,
    HdSt_ResourceBinder const &binder,
    HdBufferResourceSharedPtr cullCommandBuffer)
{
    GarchContextCaps const &caps = GarchResourceFactory::GetInstance()->GetContextCaps();
    if (caps.IsEnabledGPUCountVisibleInstances()) {
        _BeginGPUCountVisibleInstances(resourceRegistry);
        binder.BindBuffer(_tokens->drawIndirectResult, _resultBuffer);
    }

    glEnable(GL_RASTERIZER_DISCARD);

    int resetPass = 1;
    binder.BindUniformi(_tokens->ulocResetPass, 1, &resetPass);
    glMultiDrawArraysIndirect(
        GL_POINTS,
        reinterpret_cast<const GLvoid*>(
            static_cast<intptr_t>(cullCommandBuffer->GetOffset())),
        _dispatchBufferCullInput->GetCount(),
        cullCommandBuffer->GetStride());

    // dispatch buffer is bound via SSBO
    // (see _CullingProgram::_GetCustomBindings)
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    resetPass = 0;
    binder.BindUniformi(_tokens->ulocResetPass, 1, &resetPass);
    glMultiDrawArraysIndirect(
        GL_POINTS,
        reinterpret_cast<const GLvoid*>(
            static_cast<intptr_t>(cullCommandBuffer->GetOffset())),
        _dispatchBufferCullInput->GetCount(),
        cullCommandBuffer->GetStride());

    glDisable(GL_RASTERIZER_DISCARD);
}

void
HdSt_IndirectDrawBatchGL::_SyncFence() {

    // make sure the culling results (instanceIndices and instanceCount)
    // are synchronized for the next drawing.
    glMemoryBarrier(
        GL_COMMAND_BARRIER_BIT |         // instanceCount for MDI
        GL_SHADER_STORAGE_BARRIER_BIT |  // instanceCount for shader
        GL_UNIFORM_BARRIER_BIT);         // instanceIndices
}

void
HdSt_IndirectDrawBatchGL::_GPUFrustumNonInstanceCullingExecute(
    HdStResourceRegistrySharedPtr const &resourceRegistry,
    HdStGLSLProgramSharedPtr const &program,
    const HdSt_ResourceBinder &binder)
{
    GarchContextCaps const &caps = GarchResourceFactory::GetInstance()->GetContextCaps();
    if (caps.IsEnabledGPUCountVisibleInstances()) {
        _BeginGPUCountVisibleInstances(resourceRegistry);
        binder.BindBuffer(_tokens->drawIndirectResult, _resultBuffer);
    }

    // bind destination buffer (using entire buffer bind to start from offset=0)
    binder.BindBuffer(HdStIndirectDrawTokens->dispatchBuffer,
                      _dispatchBuffer->GetEntireResource());

    glEnable(GL_RASTERIZER_DISCARD);
    glDrawArrays(GL_POINTS, 0, _dispatchBufferCullInput->GetCount());
    glDisable(GL_RASTERIZER_DISCARD);

    // unbind destination dispatch buffer
    binder.UnbindBuffer(HdStIndirectDrawTokens->dispatchBuffer,
                        _dispatchBuffer->GetEntireResource());
    
    if (caps.IsEnabledGPUCountVisibleInstances()) {
        binder.UnbindBuffer(_tokens->drawIndirectResult, _resultBuffer);
    }

    // make sure the culling results (instanceCount)
    // are synchronized for the next drawing.
    glMemoryBarrier(
        GL_COMMAND_BARRIER_BIT |      // instanceCount for MDI
        GL_SHADER_STORAGE_BARRIER_BIT // instanceCount for shader
    );
}

void
HdSt_IndirectDrawBatchGL::_BeginGPUCountVisibleInstances(
    HdStResourceRegistrySharedPtr const &resourceRegistry)
{
    if (!_resultBuffer) {
        HdTupleType tupleType;
            tupleType.type = HdTypeInt32;
            tupleType.count = 1;

        _resultBuffer =
            resourceRegistry->RegisterBufferResource(
                _tokens->drawIndirectResult, tupleType);
    }

    // Reset visible item count
    static const int32_t count = 0;
    HgiBlitCmds* blitCmds = resourceRegistry->GetGlobalBlitCmds();
    HgiBufferCpuToGpuOp op;
    op.cpuSourceBuffer = &count;
    op.sourceByteOffset = 0;
    op.gpuDestinationBuffer = _resultBuffer->GetId();
    op.destinationByteOffset = 0;
    op.byteSize = sizeof(count);
    blitCmds->CopyBufferCpuToGpu(op);

    // For now we need to submit here, because there are raw gl calls after
    // _BeginGPUCountVisibleInstances that rely on this having executed on GPU.
    // XXX Remove this once the rest of indirectDrawBatch is using Hgi.
    resourceRegistry->SubmitBlitWork();
}

void
HdSt_IndirectDrawBatchGL::_EndGPUCountVisibleInstances(
    HdStResourceRegistrySharedPtr const &resourceRegistry,
    size_t * result)
{
    // Submit and wait for all the work recorded up to this point.
    // The GPU work must complete before we can read-back the GPU buffer.
    // GPU frustum culling is (currently) a vertex shader without a fragment
    // shader, so we submit the blit work, but do not have any compute work.
    resourceRegistry->SubmitBlitWork(HgiSubmitWaitTypeWaitUntilCompleted);

    int32_t count = 0;

    // Submit GPU buffer read back
    HgiBufferGpuToCpuOp copyOp;
    copyOp.byteSize = sizeof(count);
    copyOp.cpuDestinationBuffer = &count;
    copyOp.destinationByteOffset = 0;
    copyOp.gpuSourceBuffer = _resultBuffer->GetId();
    copyOp.sourceByteOffset = 0;

    HgiBlitCmds* blitCmds = resourceRegistry->GetGlobalBlitCmds();
        blitCmds->CopyBufferGpuToCpu(copyOp);
        resourceRegistry->SubmitBlitWork(HgiSubmitWaitTypeWaitUntilCompleted);

    *result = count;
}

/* virtual */
bool
HdSt_IndirectDrawBatchGL::_CullingProgramGL::_Link(
    HdStGLSLProgramSharedPtr const & program)
{
    if (!TF_VERIFY(program)) return false;
    if (!glTransformFeedbackVaryings) return false; // glew initialized

    if (!_useInstanceCulling) {
        // This must match the layout of draw command.
        // (WBN to encode this in the shader using GL_ARB_enhanced_layouts
        // but that's not supported in 319.32)
        
        // CAUTION: this is currently padded to match drawElementsOutputs, since
        // our shader hash cannot take the XFB varying configuration into
        // account.
        const char *drawArraysOutputs[] = {
            "gl_SkipComponents1",  // count
            "resultInstanceCount", // instanceCount
            "gl_SkipComponents4",  // firstIndex - modelDC
            // (includes __reserved_0 to match drawElementsOutput)
            "gl_SkipComponents4",  // constantDC - fvarDC
            "gl_SkipComponents4",  // instanceIndexDC - topologyVisibilityDC
        };
        const char *drawElementsOutputs[] = {
            "gl_SkipComponents1",  // count
            "resultInstanceCount", // instanceCount
            "gl_SkipComponents4",  // firstIndex - modelDC
            "gl_SkipComponents4",  // constantDC - fvarDC
            "gl_SkipComponents4",  // instanceIndexDC - topologyVisibilityDC
        };
        const char **outputs = _useDrawArrays
            ? drawArraysOutputs
            : drawElementsOutputs;
        
        const int nOutputs = 5;
        static_assert(
                      sizeof(drawArraysOutputs)/sizeof(drawArraysOutputs[0])
                      == nOutputs,
                      "Size of drawArraysOutputs element must equal nOutputs.");
        static_assert(
                      sizeof(drawElementsOutputs)/sizeof(drawElementsOutputs[0])
                      == nOutputs,
                      "Size of drawElementsOutputs element must equal nOutputs.");
        
        glTransformFeedbackVaryings(std::dynamic_pointer_cast<HdStGLSLProgramGLSL>(program)->GetGLProgram(),
                                    nOutputs,
                                    outputs, GL_INTERLEAVED_ATTRIBS);
    }

    return HdSt_DrawBatch::_DrawingProgram::_Link(program);
}

PXR_NAMESPACE_CLOSE_SCOPE

