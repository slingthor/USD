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

    HDST_API
    virtual ~HdSt_IndirectDrawBatch();

    // HdSt_DrawBatch overrides
    HDST_API
    bool Validate(bool deepValidation);

    /// Prepare draw commands and apply view frustum culling for this batch.
    HDST_API
    virtual void PrepareDraw(
        HdStRenderPassStateSharedPtr const &renderPassState,
        HdStResourceRegistrySharedPtr const &resourceRegistry) override;

    /// Executes the drawing commands for this batch.
    HDST_API
    virtual void ExecuteDraw(
        HdStRenderPassStateSharedPtr const &renderPassState,
        HdStResourceRegistrySharedPtr const &resourceRegistry) override;

    HDST_API
    void DrawItemInstanceChanged(HdStDrawItemInstance const* instance);

protected:
    HDST_API
    HdSt_IndirectDrawBatch(HdStDrawItemInstance * drawItemInstance);

    HDST_API
    void _Init(HdStDrawItemInstance * drawItemInstance);
    
protected:
    void _ValidateCompatibility(HdBufferArrayRangeSharedPtr const& constantBar,
                                HdBufferArrayRangeSharedPtr const& indexBar,
								HdBufferArrayRangeSharedPtr const& topologyVisibilityBar,
                                HdBufferArrayRangeSharedPtr const& elementBar,
                                HdBufferArrayRangeSharedPtr const& fvarBar,
                                HdBufferArrayRangeSharedPtr const& vertexBar,
                                int instancerNumLevels,
                                HdBufferArrayRangeSharedPtr const& instanceIndexBar,
                                std::vector<HdBufferArrayRangeSharedPtr> const& instanceBars) const;
    
    HDST_API
    virtual void _PrepareDraw(bool gpuCulling, bool freezeCulling) = 0;
    
    HDST_API
    virtual void _ExecuteDraw(_DrawingProgram &program, int batchCount) = 0;

    HDST_API
    virtual void _SyncFence() = 0;
    
    HDST_API
    virtual void _GPUFrustumCullingExecute(
                       HdStResourceRegistrySharedPtr const &resourceRegistry,
                       HdStProgramSharedPtr const &program,
                       HdSt_ResourceBinder const &binder,
                       HdBufferResourceSharedPtr cullCommandBuffer) = 0;
    
    HDST_API
    virtual void _GPUFrustumCullingXFBExecute(
                        HdStResourceRegistrySharedPtr const &resourceRegistry,
                        HdStProgramSharedPtr const &program) = 0;

    // Culling requires custom resource binding.
    class _CullingProgram : public _DrawingProgram {
    public:
        void Initialize(bool useDrawArrays, bool useInstanceCulling,
                        size_t bufferArrayHash);
    protected:
        _CullingProgram()
            : _useDrawArrays(false)
            , _useInstanceCulling(false)
            , _bufferArrayHash(0) { }
        
        virtual ~_CullingProgram() {}
        
        // _DrawingProgram overrides
        virtual void _GetCustomBindings(HdBindingRequestVector *customBindings,
                                        bool *enableInstanceDraw) const;
        virtual bool _Link(HdStProgramSharedPtr const & program) = 0;

        bool _useDrawArrays;
        bool _useInstanceCulling;
        size_t _bufferArrayHash;
    };
    
    HDST_API
    virtual HdSt_IndirectDrawBatch::_CullingProgram *NewCullingProgram() const = 0;
    
    _CullingProgram &_GetCullingProgram(HdStResourceRegistrySharedPtr const &resourceRegistry);
    
    void _CompileBatch(HdStResourceRegistrySharedPtr const &resourceRegistry);
    
    void _GPUFrustumCulling(HdStDrawItem const *item,
                            HdStRenderPassStateSharedPtr const &renderPassState,
                            HdStResourceRegistrySharedPtr const &resourceRegistry);
    
    void _GPUFrustumCullingXFB(HdStDrawItem const *item,
                               HdStRenderPassStateSharedPtr const &renderPassState,
                               HdStResourceRegistrySharedPtr const &resourceRegistry);
    
    HdStDispatchBufferSharedPtr _dispatchBuffer;
    HdStDispatchBufferSharedPtr _dispatchBufferCullInput;
    
    std::vector<GLuint> _drawCommandBuffer;
    bool _drawCommandBufferDirty;
    size_t _bufferArraysHash;

    size_t _numVisibleItems;
    size_t _numTotalVertices;
    size_t _numTotalElements;
    
    _CullingProgram *_cullingProgram;
    bool _lastTinyPrimCulling;
    
    bool _useDrawArrays;
    bool _useInstancing;
    bool _useGpuCulling;
    bool _useGpuInstanceCulling;
    
    int _instanceCountOffset;
    int _cullInstanceCountOffset;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDST_INDIRECT_DRAW_BATCH_H
