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
#ifndef HDST_INDIRECT_DRAW_BATCH_METAL_H
#define HDST_INDIRECT_DRAW_BATCH_METAL_H

#include "pxr/imaging/hdSt/indirectDrawBatch.h"
#include "pxr/imaging/hdSt/Metal/persistentBufferMetal.h"

PXR_NAMESPACE_OPEN_SCOPE

/// \class HdSt_IndirectDrawBatchMetal
///
/// Drawing batch that is executed from an indirect dispatch buffer.
///
/// An indirect drawing batch accepts draw items that have the same
/// primitive mode and that share aggregated drawing resources,
/// e.g. uniform and non uniform primvar buffers.
///
class HdSt_IndirectDrawBatchMetal : public HdSt_IndirectDrawBatch {
public:
    HDST_API
    virtual ~HdSt_IndirectDrawBatchMetal();
    
    /// Prepare draw commands and apply view frustum culling for this batch.
    HDST_API
    virtual void _PrepareDraw(bool gpuCulling, bool freezeCulling) override;
    
    /// Executes the drawing commands for this batch.
    HDST_API
    virtual void _ExecuteDraw(_DrawingProgram &program, int batchCount) override;
    
    HDST_API
    virtual void _SyncFence() override;
    
    HDST_API
    virtual void _GPUFrustumCullingExecute(
                       HdStResourceRegistrySharedPtr const &resourceRegistry,
                       HdStProgramSharedPtr const &program,
                       HdSt_ResourceBinder const &binder,
                       HdBufferResourceSharedPtr cullCommandBuffer) override;
    
    HDST_API
    virtual void _GPUFrustumCullingXFBExecute(
                      HdStResourceRegistrySharedPtr const &resourceRegistry,
                      HdStProgramSharedPtr const &program) override;

protected:
    HDST_API
    HdSt_IndirectDrawBatchMetal(HdStDrawItemInstance * drawItemInstance);

    class _CullingProgramMetal : public HdSt_IndirectDrawBatch::_CullingProgram {
    public:
        _CullingProgramMetal() { }
        virtual ~_CullingProgramMetal() {}
        
    protected:
        virtual bool _Link(HdStProgramSharedPtr const & program) override;
        
        friend class HdSt_IndirectDrawBatch::_CullingProgram;
    };
    
    HDST_API
    virtual HdSt_IndirectDrawBatch::_CullingProgram *NewCullingProgram() const override;
    
    friend class HdStResourceFactoryMetal;

private:
    void _BeginGPUCountVisibleInstances(HdStResourceRegistrySharedPtr const &resourceRegistry);
    
    void _EndGPUCountVisibleInstances(GLsync resultSync, size_t * result);
    
    HdStPersistentBufferMetalSharedPtr _resultBuffer;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDST_INDIRECT_DRAW_BATCH_METAL_H
