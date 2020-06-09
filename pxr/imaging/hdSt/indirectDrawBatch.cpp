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
#include "pxr/imaging/garch/resourceFactory.h"

#include "pxr/imaging/hio/glslfx.h"

#include "pxr/imaging/hdSt/commandBuffer.h"
#include "pxr/imaging/hdSt/cullingShaderKey.h"
#include "pxr/imaging/hdSt/debugCodes.h"
#include "pxr/imaging/hdSt/drawItemInstance.h"
#include "pxr/imaging/hdSt/geometricShader.h"
#include "pxr/imaging/hdSt/indirectDrawBatch.h"
#include "pxr/imaging/hdSt/renderPassState.h"
#include "pxr/imaging/hdSt/resourceRegistry.h"
#include "pxr/imaging/hdSt/shaderCode.h"
#include "pxr/imaging/hdSt/shaderKey.h"

#include "pxr/imaging/hd/binding.h"
#include "pxr/imaging/hd/bufferArrayRange.h"

#include "pxr/imaging/hd/debugCodes.h"
#include "pxr/imaging/hd/engine.h"
#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/tokens.h"

#include "pxr/base/tf/diagnostic.h"
#include "pxr/base/tf/envSetting.h"
#include "pxr/base/tf/getenv.h"
#include "pxr/base/tf/iterator.h"
#include "pxr/base/tf/staticTokens.h"

#include <iostream>
#include <limits>

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PUBLIC_TOKENS(HdStIndirectDrawTokens, HDST_INDIRECT_DRAW_TOKENS);

HdSt_IndirectDrawBatch::HdSt_IndirectDrawBatch(
    HdStDrawItemInstance * drawItemInstance)
    : HdSt_DrawBatch(drawItemInstance)
    , _drawCommandBufferDirty(false)
    , _bufferArraysHash(0)
    , _numVisibleItems(0)
    , _numTotalVertices(0)
    , _numTotalElements(0)
    /* The following two values are set before draw by
     * SetEnableTinyPrimCulling(). */
    , _useTinyPrimCulling(false)
    , _dirtyCullingProgram(false)
    /* The following four values are initialized in _Init(). */
    , _useDrawArrays(false)
    , _useInstancing(false)
    , _useGpuCulling(false)
    , _useGpuInstanceCulling(false)

    , _instanceCountOffset(0)
    , _cullInstanceCountOffset(0)
{
}

HdSt_IndirectDrawBatch::~HdSt_IndirectDrawBatch()
{
}

void
HdSt_IndirectDrawBatch::_Init(HdStDrawItemInstance * drawItemInstance)
{
    HdSt_DrawBatch::_Init(drawItemInstance);
    drawItemInstance->SetBatchIndex(0);
    drawItemInstance->SetBatch(this);
    
    GarchContextCaps const &caps =
        GarchResourceFactory::GetInstance()->GetContextCaps();
    
    // remember buffer arrays version for dispatch buffer updating
    HdDrawItem const* drawItem = drawItemInstance->GetDrawItem();
    _bufferArraysHash = drawItem->GetBufferArraysHash();
    
    // determine gpu culling program by the first drawitem
    _useDrawArrays  = !drawItem->GetTopologyRange();
    _useInstancing = static_cast<bool>(drawItem->GetInstanceIndexRange());
    _useGpuCulling = caps.IsEnabledGPUFrustumCulling();
    
    // note: _useInstancing condition is not necessary. it can be removed
    //       if we decide always to use instance culling.
    _useGpuInstanceCulling = _useInstancing &&
    _useGpuCulling && caps.IsEnabledGPUInstanceFrustumCulling();
    
    if (_useGpuCulling) {
        _cullingProgram = NewCullingProgram();
        _cullingProgram->Initialize(
            _useDrawArrays, _useGpuInstanceCulling, _bufferArraysHash);
    }
}

HdSt_IndirectDrawBatch::_CullingProgram &
HdSt_IndirectDrawBatch::_GetCullingProgram(
    HdStResourceRegistrySharedPtr const &resourceRegistry)
{
	GarchContextCaps const &caps =
        GarchResourceFactory::GetInstance()->GetContextCaps();

    if (!_cullingProgram->GetProgram() || _dirtyCullingProgram) {
        // create a culling shader key
        HdSt_CullingShaderKey shaderKey(_useGpuInstanceCulling,
                                        _useTinyPrimCulling,
                                        caps.IsEnabledGPUCountVisibleInstances());
        
        // sharing the culling geometric shader for the same configuration.
        HdSt_GeometricShaderSharedPtr cullShader =
            HdSt_GeometricShader::Create(shaderKey, resourceRegistry);
        _cullingProgram->SetGeometricShader(cullShader);
        
        _cullingProgram->CompileShader(_drawItemInstances.front()->GetDrawItem(),
                                       /*indirect=*/true,
                                       resourceRegistry);
        
        _dirtyCullingProgram = false;
    }
    return *_cullingProgram;
}

void
HdSt_IndirectDrawBatch::SetEnableTinyPrimCulling(bool tinyPrimCulling)
{
    if (_useTinyPrimCulling != tinyPrimCulling) {
        _useTinyPrimCulling = tinyPrimCulling;
        _dirtyCullingProgram = true;
    }
}

static int
_GetElementOffset(HdBufferArrayRangeSharedPtr const& range)
{
    return range? range->GetElementOffset() : 0;
}
void
HdSt_IndirectDrawBatch::_CompileBatch(
    HdStResourceRegistrySharedPtr const &resourceRegistry)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();
    
    int drawCount = _drawItemInstances.size();
    if (_drawItemInstances.empty()) return;

    // drawcommand is configured as one of followings:
    //
    // DrawArrays + non-instance culling  : 14 integers (+ numInstanceLevels)
    struct _DrawArraysCommand {
        GLuint count;
        GLuint instanceCount;
        GLuint first;
        GLuint baseInstance;
        
        // XXX: This is just padding to avoid configuration changes during
        // transform feedback, which are not accounted for during shader
        // caching. We should find a better solution.
        GLuint __reserved_0;
        
        GLuint modelDC;
        GLuint constantDC;
        GLuint elementDC;
        GLuint primitiveDC;
        GLuint fvarDC;
        GLuint instanceIndexDC;
        GLuint shaderDC;
        GLuint vertexDC;
        GLuint topologyVisibilityDC;
    };
    
    // DrawArrays + Instance culling : 17 integers (+ numInstanceLevels)
    struct _DrawArraysInstanceCullCommand {
        GLuint count;
        GLuint instanceCount;
        GLuint first;
        GLuint baseInstance;
        GLuint cullCount;
        GLuint cullInstanceCount;
        GLuint cullFirstVertex;
        GLuint cullBaseInstance;
        GLuint modelDC;
        GLuint constantDC;
        GLuint elementDC;
        GLuint primitiveDC;
        GLuint fvarDC;
        GLuint instanceIndexDC;
        GLuint shaderDC;
        GLuint vertexDC;
        GLuint topologyVisibilityDC;
    };

    // DrawElements + non-instance culling : 14 integers (+ numInstanceLevels)
    struct _DrawElementsCommand {
        GLuint count;
        GLuint instanceCount;
        GLuint first;
        GLuint baseVertex;
        GLuint baseInstance;
        GLuint modelDC;
        GLuint constantDC;
        GLuint elementDC;
        GLuint primitiveDC;
        GLuint fvarDC;
        GLuint instanceIndexDC;
        GLuint shaderDC;
        GLuint vertexDC;
        GLuint topologyVisibilityDC;
    };
    
    // DrawElements + Instance culling : 18 integers (+ numInstanceLevels)
    struct _DrawElementsInstanceCullCommand {
        GLuint count;
        GLuint instanceCount;
        GLuint first;
        GLuint baseVertex;
        GLuint baseInstance;
        GLuint cullCount;
        GLuint cullInstanceCount;
        GLuint cullFirstVertex;
        GLuint cullBaseInstance;
        GLuint modelDC;
        GLuint constantDC;
        GLuint elementDC;
        GLuint primitiveDC;
        GLuint fvarDC;
        GLuint instanceIndexDC;
        GLuint shaderDC;
        GLuint vertexDC;
        GLuint topologyVisibilityDC;
    };

    // Count the number of visible items. We may actually draw fewer
    // items than this when GPU frustum culling is active
    _numVisibleItems = 0;
    
    // elements to be drawn (early out for empty batch)
    _numTotalElements = 0;
    _numTotalVertices = 0;
    
    size_t instancerNumLevels
        = _drawItemInstances[0]->GetDrawItem()->GetInstancePrimvarNumLevels();
    
    // how many integers in the dispatch struct
    int commandNumUints = _useDrawArrays
        ? (_useGpuInstanceCulling
           ? sizeof(_DrawArraysInstanceCullCommand)/sizeof(GLuint)
           : sizeof(_DrawArraysCommand)/sizeof(GLuint))
        : (_useGpuInstanceCulling
           ? sizeof(_DrawElementsInstanceCullCommand)/sizeof(GLuint)
           : sizeof(_DrawElementsCommand)/sizeof(GLuint));
    // followed by instanceDC[numlevels]
    commandNumUints += instancerNumLevels;
    
    TF_DEBUG(HD_MDI).Msg("\nCompile MDI Batch\n");
    TF_DEBUG(HD_MDI).Msg(" - num uints: %d\n", commandNumUints);
    TF_DEBUG(HD_MDI).Msg(" - useDrawArrays: %d\n", _useDrawArrays);
    TF_DEBUG(HD_MDI).Msg(" - useGpuInstanceCulling: %d\n",
                         _useGpuInstanceCulling);
    
    size_t numDrawItemInstances = _drawItemInstances.size();
    TF_DEBUG(HD_MDI).Msg(" - num draw items: %zu\n", numDrawItemInstances);
    
    // Note: GL specifies baseVertex as 'int' and other as 'uint' in
    // drawcommand struct, but we never set negative baseVertex in our
    // usecases for bufferArray so we use uint for all fields here.
    _drawCommandBuffer.resize(numDrawItemInstances * commandNumUints);
    std::vector<GLuint>::iterator cmdIt = _drawCommandBuffer.begin();
    
    TF_DEBUG(HD_MDI).Msg(" - Processing Items:\n");
    for (size_t item = 0; item < numDrawItemInstances; ++item) {
        HdStDrawItemInstance const * instance = _drawItemInstances[item];
        HdStDrawItem const * drawItem = _drawItemInstances[item]->GetDrawItem();
        
        //
        // index buffer data
        //
        HdBufferArrayRangeSharedPtr const &
            indexBar_ = drawItem->GetTopologyRange();
        HdBufferArrayRangeSharedPtr indexBar =
            std::static_pointer_cast<HdBufferArrayRange>(indexBar_);

        //
        // topology visiibility buffer data
        //
        HdBufferArrayRangeSharedPtr const &
            topVisBar_ = drawItem->GetTopologyVisibilityRange();
        HdBufferArrayRangeSharedPtr topVisBar =
            std::static_pointer_cast<HdBufferArrayRange>(topVisBar_);

        //
        // element (per-face) buffer data
        //
        HdBufferArrayRangeSharedPtr const &
            elementBar_ = drawItem->GetElementPrimvarRange();
        HdBufferArrayRangeSharedPtr elementBar =
            std::static_pointer_cast<HdBufferArrayRange>(elementBar_);
        
        //
        // vertex attrib buffer data
        //
        HdBufferArrayRangeSharedPtr const &
            vertexBar_ = drawItem->GetVertexPrimvarRange();
        HdBufferArrayRangeSharedPtr vertexBar =
            std::static_pointer_cast<HdBufferArrayRange>(vertexBar_);
        
        //
        // constant buffer data
        //
        HdBufferArrayRangeSharedPtr const &
            constantBar_ = drawItem->GetConstantPrimvarRange();
        HdBufferArrayRangeSharedPtr constantBar =
            std::static_pointer_cast<HdBufferArrayRange>(constantBar_);
        
        //
        // face varying buffer data
        //
        HdBufferArrayRangeSharedPtr const &
            fvarBar_ = drawItem->GetFaceVaryingPrimvarRange();
        HdBufferArrayRangeSharedPtr fvarBar =
            std::static_pointer_cast<HdBufferArrayRange>(fvarBar_);
        
        //
        // instance buffer data
        //
        int instanceIndexWidth = instancerNumLevels + 1;
        std::vector<HdBufferArrayRangeSharedPtr> instanceBars(instancerNumLevels);
        for (int i = 0; i < instancerNumLevels; ++i) {
            HdBufferArrayRangeSharedPtr const &
                ins_ = drawItem->GetInstancePrimvarRange(i);
            HdBufferArrayRangeSharedPtr ins =
                std::static_pointer_cast<HdBufferArrayRange>(ins_);
            
            instanceBars[i] = ins;
        }
        
        //
        // instance indices
        //
        HdBufferArrayRangeSharedPtr const &
            instanceIndexBar_ = drawItem->GetInstanceIndexRange();
        HdBufferArrayRangeSharedPtr instanceIndexBar =
            std::static_pointer_cast<HdBufferArrayRange>(instanceIndexBar_);
        
        //
        // shader parameter
        //
        HdBufferArrayRangeSharedPtr const &
            shaderBar_ = drawItem->GetMaterialShader()->GetShaderData();
        HdBufferArrayRangeSharedPtr shaderBar =
            std::static_pointer_cast<HdBufferArrayRange>(shaderBar_);
        
        // 3 for triangles, 4 for quads, n for patches
        GLuint numIndicesPerPrimitive
            = drawItem->GetGeometricShader()->GetPrimitiveIndexSize();
        
        //
        // Get parameters from our buffer range objects to
        // allow drawing to access the correct elements from
        // aggregated buffers.
        //
        GLuint numElements = indexBar ? indexBar->GetNumElements() : 0;
        GLuint vertexOffset = 0;
        GLuint vertexCount = 0;
        if (vertexBar) {
            vertexOffset = vertexBar->GetElementOffset();
            vertexCount = vertexBar->GetNumElements();
        }
        // if delegate fails to get vertex primvars, it could be empty.
        // skip the drawitem to prevent drawing uninitialized vertices.
        if (vertexCount == 0) numElements = 0;
        GLuint baseInstance      = (GLuint)item;
        
        // drawing coordinates.
        GLuint modelDC         = 0; // reserved for future extension
        GLuint constantDC      = _GetElementOffset(constantBar);
        GLuint vertexDC        = vertexOffset;
        GLuint topologyVisibilityDC
                               = _GetElementOffset(topVisBar);
        GLuint elementDC       = _GetElementOffset(elementBar);
        GLuint primitiveDC     = _GetElementOffset(indexBar);
        GLuint fvarDC          = _GetElementOffset(fvarBar);
        GLuint instanceIndexDC = _GetElementOffset(instanceIndexBar);
        GLuint shaderDC        = _GetElementOffset(shaderBar);

        GLuint indicesCount  = numElements * numIndicesPerPrimitive;
        // It's possible to have instanceIndexBar which is empty, and no instancePrimvars.
        // in that case instanceCount should be 0, instead of 1, otherwise
        // frustum culling shader writes the result out to out-of-bound buffer.
        // this is covered by testHdDrawBatching/EmptyDrawBatchTest
        GLuint instanceCount = instanceIndexBar
            ? instanceIndexBar->GetNumElements()/instanceIndexWidth
            : 1;
        if (!instance->IsVisible()) instanceCount = 0;
        GLuint firstIndex = indexBar ?
            indexBar->GetElementOffset() * numIndicesPerPrimitive : 0;

        if (_useDrawArrays) {
            if (_useGpuInstanceCulling) {
                *cmdIt++ = vertexCount;
                *cmdIt++ = instanceCount;
                *cmdIt++ = vertexOffset;
                *cmdIt++ = baseInstance;
                *cmdIt++ = 1;             /* cullCount (always 1) */
                *cmdIt++ = instanceCount; /* cullInstanceCount */
                *cmdIt++ = 0;             /* cullFirstVertex (not used)*/
                *cmdIt++ = baseInstance;  /* cullBaseInstance */
                *cmdIt++ = modelDC;
                *cmdIt++ = constantDC;
                *cmdIt++ = elementDC;
                *cmdIt++ = primitiveDC;
                *cmdIt++ = fvarDC;
                *cmdIt++ = instanceIndexDC;
                *cmdIt++ = shaderDC;
                *cmdIt++ = vertexDC;
                *cmdIt++ = topologyVisibilityDC;
            } else {
                *cmdIt++ = vertexCount;
                *cmdIt++ = instanceCount;
                *cmdIt++ = vertexOffset;
                *cmdIt++ = baseInstance;
                cmdIt++; // __reserved_0
                *cmdIt++ = modelDC;
                *cmdIt++ = constantDC;
                *cmdIt++ = elementDC;
                *cmdIt++ = primitiveDC;
                *cmdIt++ = fvarDC;
                *cmdIt++ = instanceIndexDC;
                *cmdIt++ = shaderDC;
                *cmdIt++ = vertexDC;
                *cmdIt++ = topologyVisibilityDC;
            }
        } else {
            if (_useGpuInstanceCulling) {
                *cmdIt++ = indicesCount;
                *cmdIt++ = instanceCount;
                *cmdIt++ = firstIndex;
                *cmdIt++ = vertexOffset;
                *cmdIt++ = baseInstance;
                *cmdIt++ = 1;             /* cullCount (always 1) */
                *cmdIt++ = instanceCount; /* cullInstanceCount */
                *cmdIt++ = 0;             /* cullFirstVertex (not used)*/
                *cmdIt++ = baseInstance;  /* cullBaseInstance */
                *cmdIt++ = modelDC;
                *cmdIt++ = constantDC;
                *cmdIt++ = elementDC;
                *cmdIt++ = primitiveDC;
                *cmdIt++ = fvarDC;
                *cmdIt++ = instanceIndexDC;
                *cmdIt++ = shaderDC;
                *cmdIt++ = vertexDC;
                *cmdIt++ = topologyVisibilityDC;
            } else {
                *cmdIt++ = indicesCount;
                *cmdIt++ = instanceCount;
                *cmdIt++ = firstIndex;
                *cmdIt++ = vertexOffset;
                *cmdIt++ = baseInstance;
                *cmdIt++ = modelDC;
                *cmdIt++ = constantDC;
                *cmdIt++ = elementDC;
                *cmdIt++ = primitiveDC;
                *cmdIt++ = fvarDC;
                *cmdIt++ = instanceIndexDC;
                *cmdIt++ = shaderDC;
                *cmdIt++ = vertexDC;
                *cmdIt++ = topologyVisibilityDC;
            }
        }
        for (size_t i = 0; i < instancerNumLevels; ++i) {
            GLuint instanceDC = _GetElementOffset(instanceBars[i]);
            *cmdIt++ = instanceDC;
        }
        
        if (TfDebug::IsEnabled(HD_MDI)) {
            std::vector<GLuint>::iterator cmdIt2 = cmdIt - commandNumUints;
            std::cout << "   - ";
            while (cmdIt2 != cmdIt) {
                std::cout << *cmdIt2 << " ";
                cmdIt2++;
            }
            std::cout << std::endl;
        }
        
        _numVisibleItems += instanceCount;
        _numTotalElements += numElements;
        _numTotalVertices += vertexCount;
    }
    
    TF_DEBUG(HD_MDI).Msg(" - Num Visible: %zu\n", _numVisibleItems);
    TF_DEBUG(HD_MDI).Msg(" - Total Elements: %zu\n", _numTotalElements);
    TF_DEBUG(HD_MDI).Msg(" - Total Verts: %zu\n", _numTotalVertices);
    
    // make sure we filled all
    TF_VERIFY(cmdIt == _drawCommandBuffer.end());
    
    // allocate draw dispatch buffer
    _dispatchBuffer =
        resourceRegistry->RegisterDispatchBuffer(HdStIndirectDrawTokens->drawIndirect,
                                                 drawCount,
                                                 commandNumUints);
    // define binding views
    if (_useDrawArrays) {
        if (_useGpuInstanceCulling) {
            // draw indirect command
            _dispatchBuffer->AddBufferResourceView(
                HdTokens->drawDispatch, {HdTypeInt32, 1},
                offsetof(_DrawArraysInstanceCullCommand, count));
            // drawing coords 0
            _dispatchBuffer->AddBufferResourceView(
                HdTokens->drawingCoord0, {HdTypeInt32Vec4, 1},
                offsetof(_DrawArraysInstanceCullCommand, modelDC));
            // drawing coords 1
            _dispatchBuffer->AddBufferResourceView(
                HdTokens->drawingCoord1, {HdTypeInt32Vec4, 1},
                offsetof(_DrawArraysInstanceCullCommand, fvarDC));
            // drawing coords 2
            _dispatchBuffer->AddBufferResourceView(
                HdTokens->drawingCoord2, {HdTypeInt32, 1},
                offsetof(_DrawArraysInstanceCullCommand, topologyVisibilityDC));
            // instance drawing coords
            if (instancerNumLevels > 0) {
                _dispatchBuffer->AddBufferResourceView(
                    HdTokens->drawingCoordI,
                    {HdTypeInt32, instancerNumLevels},
                    sizeof(_DrawArraysInstanceCullCommand));
            }
        } else {
            // draw indirect command
            _dispatchBuffer->AddBufferResourceView(
                HdTokens->drawDispatch, {HdTypeInt32, 1},
                offsetof(_DrawArraysCommand, count));
            // drawing coords 0
            _dispatchBuffer->AddBufferResourceView(
                HdTokens->drawingCoord0, {HdTypeInt32Vec4, 1},
                offsetof(_DrawArraysCommand, modelDC));
            // drawing coords 1
            _dispatchBuffer->AddBufferResourceView(
                HdTokens->drawingCoord1, {HdTypeInt32Vec4, 1},
                offsetof(_DrawArraysCommand, fvarDC));
            // drawing coords 2
            _dispatchBuffer->AddBufferResourceView(
                HdTokens->drawingCoord2, {HdTypeInt32, 1},
                offsetof(_DrawArraysCommand, topologyVisibilityDC));
            // instance drawing coords
            if (instancerNumLevels > 0) {
                _dispatchBuffer->AddBufferResourceView(
                    HdTokens->drawingCoordI,
                    {HdTypeInt32, instancerNumLevels},
                    sizeof(_DrawArraysCommand));
            }
        }
    } else {
        if (_useGpuInstanceCulling) {
            // draw indirect command
            _dispatchBuffer->AddBufferResourceView(
                HdTokens->drawDispatch, {HdTypeInt32, 1},
                offsetof(_DrawElementsInstanceCullCommand, count));
            // drawing coords 0
            _dispatchBuffer->AddBufferResourceView(
                HdTokens->drawingCoord0, {HdTypeInt32Vec4, 1},
                offsetof(_DrawElementsInstanceCullCommand, modelDC));
            // drawing coords 1
            _dispatchBuffer->AddBufferResourceView(
                HdTokens->drawingCoord1, {HdTypeInt32Vec4, 1},
                offsetof(_DrawElementsInstanceCullCommand, fvarDC));
            // drawing coords 2
            _dispatchBuffer->AddBufferResourceView(
                HdTokens->drawingCoord2, {HdTypeInt32, 1},
                offsetof(_DrawElementsInstanceCullCommand,
                         topologyVisibilityDC));
            // instance drawing coords
            if (instancerNumLevels > 0) {
                _dispatchBuffer->AddBufferResourceView(
                    HdTokens->drawingCoordI,
                    {HdTypeInt32, instancerNumLevels},
                    sizeof(_DrawElementsInstanceCullCommand));
            }
        } else {
            // draw indirect command
            _dispatchBuffer->AddBufferResourceView(
                HdTokens->drawDispatch, {HdTypeInt32, 1},
                offsetof(_DrawElementsCommand, count));
            // drawing coords 0
            _dispatchBuffer->AddBufferResourceView(
                HdTokens->drawingCoord0, {HdTypeInt32Vec4, 1},
                offsetof(_DrawElementsCommand, modelDC));
            // drawing coords 1
            _dispatchBuffer->AddBufferResourceView(
                HdTokens->drawingCoord1, {HdTypeInt32Vec4, 1},
                offsetof(_DrawElementsCommand, fvarDC));
            // drawing coords 2
            _dispatchBuffer->AddBufferResourceView(
                HdTokens->drawingCoord2, {HdTypeInt32, 1},
                offsetof(_DrawElementsCommand, topologyVisibilityDC));
            // instance drawing coords
            if (instancerNumLevels > 0) {
                _dispatchBuffer->AddBufferResourceView(
                    HdTokens->drawingCoordI,
                    {HdTypeInt32, instancerNumLevels},
                    sizeof(_DrawElementsCommand));
            }
        }
    }
    
    // copy data
    _dispatchBuffer->CopyData(_drawCommandBuffer);
    
    if (_useGpuCulling) {
        // Make a duplicate of the draw dispatch buffer to use as an input
        // for GPU frustum culling (a single buffer cannot be bound for
        // both reading and xform feedback). We use only the instanceCount
        // and drawingCoord parameters, but it is simplest to just make
        // a copy.
        _dispatchBufferCullInput =
            resourceRegistry->RegisterDispatchBuffer(
                HdStIndirectDrawTokens->drawIndirectCull,
                drawCount,
                commandNumUints);

        // define binding views
        //
        // READ THIS CAREFULLY whenever you try to add/remove/shuffle
        // the drawing coordinate struct.
        //
        // We use vec2 as a type of drawingCoord1 for GPU culling:
        //
        // DrawingCoord1 is defined as 4 integers struct:
        //   GLuint fvarDC;
        //   GLuint instanceIndexDC;
        //   GLuint shaderDC;
        //   GLuint vertexDC;
        //
        // And CodeGen generates GetInstanceIndexCoord() as
        //
        //  int GetInstanceIndexCoord() { return GetDrawingCoord1().y; }
        //
        // So the instanceIndex coord must be the second element.
        // That is why we need to add, at minimum, vec2 for drawingCoord1.
        //
        // We don't add a vec4, since we prefer smaller number of attributes
        // to be processed in the vertex input assembler, which in general gives
        // better performance especially in older hardware. In this case we
        // can't skip fvarDC without changing CodeGen logic, but we can
        // skip shaderDC and vertexDC for culling.
        //
        // XXX: Reorder members of drawingCoord0 and drawingCoord1 in CodeGen,
        // so we can minimize the vertex attributes fetched during culling.
        // 
        // Since drawingCoord2 contains only topological visibility, we skip it
        // for the culling pass.
        // 
        if (_useDrawArrays) {
            if (_useGpuInstanceCulling) {
                // cull indirect command
                _dispatchBufferCullInput->AddBufferResourceView(
                    HdTokens->drawDispatch, {HdTypeInt32, 1},
                    offsetof(_DrawArraysInstanceCullCommand, cullCount));
                // cull drawing coord 0
                _dispatchBufferCullInput->AddBufferResourceView(
                    HdTokens->drawingCoord0, {HdTypeInt32Vec4, 1},
                    offsetof(_DrawArraysInstanceCullCommand, modelDC));
                // cull drawing coord 1
                _dispatchBufferCullInput->AddBufferResourceView(
                    // see the comment above
                    HdTokens->drawingCoord1, {HdTypeInt32Vec2, 1},
                    offsetof(_DrawArraysInstanceCullCommand, fvarDC));
                // cull instance drawing coord
                if (instancerNumLevels > 0) {
                    _dispatchBufferCullInput->AddBufferResourceView(
                        HdTokens->drawingCoordI,
                        {HdTypeInt32, instancerNumLevels},
                        sizeof(_DrawArraysInstanceCullCommand));
                }
                // cull draw index
                _dispatchBufferCullInput->AddBufferResourceView(
                    HdStIndirectDrawTokens->drawCommandIndex, {HdTypeInt32, 1},
                    offsetof(_DrawArraysInstanceCullCommand, baseInstance));
            } else {
                // cull indirect command
                _dispatchBufferCullInput->AddBufferResourceView(
                    HdTokens->drawDispatch, {HdTypeInt32, 1},
                    offsetof(_DrawArraysCommand, count));
                // cull drawing coord 0
                _dispatchBufferCullInput->AddBufferResourceView(
                    HdTokens->drawingCoord0, {HdTypeInt32Vec4, 1},
                    offsetof(_DrawArraysCommand, modelDC));
                // cull draw index
                _dispatchBufferCullInput->AddBufferResourceView(
                    HdStIndirectDrawTokens->drawCommandIndex, {HdTypeInt32, 1},
                    offsetof(_DrawArraysCommand, baseInstance));
                // cull instance count input
                _dispatchBufferCullInput->AddBufferResourceView(
                    HdStIndirectDrawTokens->instanceCountInput, {HdTypeInt32, 1},
                    offsetof(_DrawArraysCommand, instanceCount));
            }
        } else {
            if (_useGpuInstanceCulling) {
                // cull indirect command
                _dispatchBufferCullInput->AddBufferResourceView(
                    HdTokens->drawDispatch, {HdTypeInt32, 1},
                    offsetof(_DrawElementsInstanceCullCommand, cullCount));
                // cull drawing coord 0
                _dispatchBufferCullInput->AddBufferResourceView(
                    HdTokens->drawingCoord0, {HdTypeInt32Vec4, 1},
                    offsetof(_DrawElementsInstanceCullCommand, modelDC));
                // cull drawing coord 1
                _dispatchBufferCullInput->AddBufferResourceView(
                    // see the comment above
                    HdTokens->drawingCoord1, {HdTypeInt32Vec2, 1},
                    offsetof(_DrawElementsInstanceCullCommand, fvarDC));
                // cull instance drawing coord
                if (instancerNumLevels > 0) {
                    _dispatchBufferCullInput->AddBufferResourceView(
                        HdTokens->drawingCoordI,
                        {HdTypeInt32, instancerNumLevels},
                        sizeof(_DrawElementsInstanceCullCommand));
                }
                // cull draw index
                _dispatchBufferCullInput->AddBufferResourceView(
                    HdStIndirectDrawTokens->drawCommandIndex, {HdTypeInt32, 1},
                    offsetof(_DrawElementsInstanceCullCommand, baseInstance));
            } else {
                // cull indirect command
                _dispatchBufferCullInput->AddBufferResourceView(
                    HdTokens->drawDispatch, {HdTypeInt32, 1},
                    offsetof(_DrawElementsCommand, count));
                // cull drawing coord 0
                _dispatchBufferCullInput->AddBufferResourceView(
                    HdTokens->drawingCoord0, {HdTypeInt32Vec4, 1},
                    offsetof(_DrawElementsCommand, modelDC));
                // cull draw index
                _dispatchBufferCullInput->AddBufferResourceView(
                    HdStIndirectDrawTokens->drawCommandIndex, {HdTypeInt32, 1},
                    offsetof(_DrawElementsCommand, baseInstance));
                // cull instance count input
                _dispatchBufferCullInput->AddBufferResourceView(
                    HdStIndirectDrawTokens->instanceCountInput, {HdTypeInt32, 1},
                    offsetof(_DrawElementsCommand, instanceCount));
            }
        }
        
        // copy data
        _dispatchBufferCullInput->CopyData(_drawCommandBuffer);
    }
    
    // cache the location of instanceCount, to be used at
    // DrawItemInstanceChanged().
    if (_useDrawArrays) {
        if (_useGpuInstanceCulling) {
            _instanceCountOffset =
                offsetof(_DrawArraysInstanceCullCommand, instanceCount)/sizeof(GLuint);
            _cullInstanceCountOffset =
                offsetof(_DrawArraysInstanceCullCommand, cullInstanceCount)/sizeof(GLuint);
        } else {
            _instanceCountOffset = _cullInstanceCountOffset =
                offsetof(_DrawArraysCommand, instanceCount)/sizeof(GLuint);
        }
    } else {
        if (_useGpuInstanceCulling) {
            _instanceCountOffset =
                offsetof(_DrawElementsInstanceCullCommand, instanceCount)/sizeof(GLuint);
            _cullInstanceCountOffset =
                offsetof(_DrawElementsInstanceCullCommand, cullInstanceCount)/sizeof(GLuint);
        } else {
            _instanceCountOffset = _cullInstanceCountOffset =
                offsetof(_DrawElementsCommand, instanceCount)/sizeof(GLuint);
        }
    }
}

bool
HdSt_IndirectDrawBatch::Validate(bool deepValidation)
{
    if (!TF_VERIFY(!_drawItemInstances.empty())) return false;
    
    // check the hash to see they've been reallocated/migrated or not.
    // note that we just need to compare the hash of the first item,
    // since drawitems are aggregated and ensure that they are sharing
    // same buffer arrays.
    
    HdStDrawItem const* batchItem = _drawItemInstances.front()->GetDrawItem();
    
    size_t bufferArraysHash = batchItem->GetBufferArraysHash();
    
    if (_bufferArraysHash != bufferArraysHash) {
        _bufferArraysHash = bufferArraysHash;
        _dispatchBuffer.reset();
        return false;
    }
    
    // Deep validation is needed when a drawItem changes its buffer spec,
    // surface shader or geometric shader.
    if (deepValidation) {
        // look through all draw items to be still compatible
        
        size_t numDrawItemInstances = _drawItemInstances.size();
        for (size_t item = 0; item < numDrawItemInstances; ++item) {
            HdStDrawItem const * drawItem
                = _drawItemInstances[item]->GetDrawItem();
            
            if (!TF_VERIFY(drawItem->GetGeometricShader())) {
                return false;
            }
            
            if (!_IsAggregated(batchItem, drawItem)) {
                return false;
            }
        }
        
    }
    
    return true;
}

void
HdSt_IndirectDrawBatch::_ValidateCompatibility(
            HdBufferArrayRangeSharedPtr const& constantBar,
            HdBufferArrayRangeSharedPtr const& indexBar,
            HdBufferArrayRangeSharedPtr const& topologyVisibilityBar,
            HdBufferArrayRangeSharedPtr const& elementBar,
            HdBufferArrayRangeSharedPtr const& fvarBar,
            HdBufferArrayRangeSharedPtr const& vertexBar,
            int instancerNumLevels,
            HdBufferArrayRangeSharedPtr const& instanceIndexBar,
            std::vector<HdBufferArrayRangeSharedPtr> const& instanceBars) const
{
    HdStDrawItem const* failed = nullptr;

    for (HdStDrawItemInstance const* itemInstance : _drawItemInstances) {
        HdStDrawItem const* itm = itemInstance->GetDrawItem();

        if (constantBar && !TF_VERIFY(constantBar 
                        ->IsAggregatedWith(itm->GetConstantPrimvarRange())))
                        { failed = itm; break; }
        if (indexBar && !TF_VERIFY(indexBar
                        ->IsAggregatedWith(itm->GetTopologyRange())))
                        { failed = itm; break; }
        if (topologyVisibilityBar && !TF_VERIFY(topologyVisibilityBar
                        ->IsAggregatedWith(itm->GetTopologyVisibilityRange())))
                        { failed = itm; break; }
        if (elementBar && !TF_VERIFY(elementBar
                        ->IsAggregatedWith(itm->GetElementPrimvarRange())))
                        { failed = itm; break; }
        if (fvarBar && !TF_VERIFY(fvarBar
                        ->IsAggregatedWith(itm->GetFaceVaryingPrimvarRange())))
                        { failed = itm; break; }
        if (vertexBar && !TF_VERIFY(vertexBar
                        ->IsAggregatedWith(itm->GetVertexPrimvarRange())))
                        { failed = itm; break; }
        if (!TF_VERIFY(instancerNumLevels
                        == itm->GetInstancePrimvarNumLevels()))
                        { failed = itm; break; }
        if (instanceIndexBar && !TF_VERIFY(instanceIndexBar
                        ->IsAggregatedWith(itm->GetInstanceIndexRange())))
                        { failed = itm; break; }
        if (!TF_VERIFY(instancerNumLevels == (int)instanceBars.size()))
                        { failed = itm; break; }

        std::vector<HdBufferArrayRangeSharedPtr> itmInstanceBars(
                                                            instancerNumLevels);
        if (instanceIndexBar) {
            for (int i = 0; i < instancerNumLevels; ++i) {
                if (itmInstanceBars[i] && !TF_VERIFY(itmInstanceBars[i]
                                                     ->IsAggregatedWith(itm->GetInstancePrimvarRange(i)),
                                                     "%d", i)) { failed = itm; break; }
            }
        }
    }
    
    if (failed) {
        std::cout << failed->GetRprimID() << std::endl;
    }
}

void
HdSt_IndirectDrawBatch::PrepareDraw(
      HdStRenderPassStateSharedPtr const &renderPassState,
      HdStResourceRegistrySharedPtr const &resourceRegistry)
{
    HD_TRACE_FUNCTION();

    //
    // compile
    //
    
    if (!_dispatchBuffer) {
        _CompileBatch(resourceRegistry);
    }

    // there is no non-zero draw items.
    if (( _useDrawArrays && _numTotalVertices == 0) ||
        (!_useDrawArrays && _numTotalElements == 0)) return;

    HdStDrawItem const* batchItem = _drawItemInstances.front()->GetDrawItem();

    // Bypass freezeCulling if the command buffer is dirty.
    bool freezeCulling = TfDebug::IsEnabled(HD_FREEZE_CULL_FRUSTUM)
    && !_drawCommandBufferDirty;
    
    bool gpuCulling = _useGpuCulling;
    
    if (gpuCulling && !_useGpuInstanceCulling) {
        // disable GPU culling when instancing enabled and
        // not using instance culling.
        if (batchItem->GetInstanceIndexRange()) gpuCulling = false;
    }
    
    // Do we have to update our dispatch buffer because drawitem instance
    // data has changed?
    // On the first time through, after batches have just been compiled,
    // the flag will be false because the resource registry will have already
    // uploaded the buffer.
    if (_drawCommandBufferDirty) {
        _dispatchBuffer->CopyData(_drawCommandBuffer);
        
        if (gpuCulling) {
            _dispatchBufferCullInput->CopyData(_drawCommandBuffer);
        }
        _drawCommandBufferDirty = false;
    }
    
    //
    // cull
    //
    
    if (gpuCulling && !freezeCulling) {
        if (_useGpuInstanceCulling) {
            _GPUFrustumInstanceCulling(
                batchItem, renderPassState, resourceRegistry);
        } else {
            _GPUFrustumNonInstanceCulling(
                batchItem, renderPassState, resourceRegistry);
        }
    }
    
    GarchContextCaps const &caps = GarchResourceFactory::GetInstance()->GetContextCaps();
    
    _PrepareDraw(gpuCulling, freezeCulling);
}

void
HdSt_IndirectDrawBatch::ExecuteDraw(
    HdStRenderPassStateSharedPtr const &renderPassState,
    HdStResourceRegistrySharedPtr const &resourceRegistry)
{
    HD_TRACE_FUNCTION();
    
    if (!TF_VERIFY(!_drawItemInstances.empty())) return;
    
    HdStDrawItem const* batchItem = _drawItemInstances.front()->GetDrawItem();
    
    if (!TF_VERIFY(batchItem)) return;
    
    if (!TF_VERIFY(_dispatchBuffer)) return;
    
    // there is no non-zero draw items.
    if (( _useDrawArrays && _numTotalVertices == 0) ||
        (!_useDrawArrays && _numTotalElements == 0)) return;
    
    //
    // draw
    //
    
    // bind program
    _DrawingProgram & program = _GetDrawingProgram(renderPassState,
                                                   /*indirect=*/true,
                                                   resourceRegistry);
    HdStProgramSharedPtr const &hdStProgram(program.GetProgram());
    if (!TF_VERIFY(hdStProgram)) return;
    if (!TF_VERIFY(hdStProgram->Validate())) return;
    
    hdStProgram->SetProgram("DrawingProgram");
    
    const HdSt_ResourceBinder &binder = program.GetBinder();
    const HdStShaderCodeSharedPtrVector &shaders = program.GetComposedShaders();
    
    // XXX: for surfaces shader, we need to iterate all drawItems to
    //      make textures resident, instead of just the first batchItem
    TF_FOR_ALL(it, shaders) {
        (*it)->BindResources(*hdStProgram, binder, *renderPassState);
    }

    // constant buffer bind
    HdBufferArrayRangeSharedPtr constantBar_ = batchItem->GetConstantPrimvarRange();
    HdBufferArrayRangeSharedPtr constantBar =
        std::static_pointer_cast<HdBufferArrayRange>(constantBar_);
    binder.BindConstantBuffer(constantBar);
    
    // index buffer bind
    HdBufferArrayRangeSharedPtr indexBar_ = batchItem->GetTopologyRange();
    HdBufferArrayRangeSharedPtr indexBar =
        std::static_pointer_cast<HdBufferArrayRange>(indexBar_);
    binder.BindBufferArray(indexBar);

    // topology visibility buffer bind
    HdBufferArrayRangeSharedPtr topVisBar_ =
        batchItem->GetTopologyVisibilityRange();
    HdBufferArrayRangeSharedPtr topVisBar =
        std::static_pointer_cast<HdBufferArrayRange>(topVisBar_);
    binder.BindInterleavedBuffer(topVisBar, HdTokens->topologyVisibility);

    // element buffer bind
    HdBufferArrayRangeSharedPtr elementBar_ = batchItem->GetElementPrimvarRange();
    HdBufferArrayRangeSharedPtr elementBar =
        std::static_pointer_cast<HdBufferArrayRange>(elementBar_);
    binder.BindBufferArray(elementBar);
    
    // fvar buffer bind
    HdBufferArrayRangeSharedPtr fvarBar_ = batchItem->GetFaceVaryingPrimvarRange();
    HdBufferArrayRangeSharedPtr fvarBar =
        std::static_pointer_cast<HdBufferArrayRange>(fvarBar_);
    binder.BindBufferArray(fvarBar);
    
    // vertex buffer bind
    HdBufferArrayRangeSharedPtr vertexBar_ = batchItem->GetVertexPrimvarRange();
    HdBufferArrayRangeSharedPtr vertexBar =
        std::static_pointer_cast<HdBufferArrayRange>(vertexBar_);
    binder.BindBufferArray(vertexBar);
    
    // instance buffer bind
    int instancerNumLevels = batchItem->GetInstancePrimvarNumLevels();
    std::vector<HdBufferArrayRangeSharedPtr> instanceBars(instancerNumLevels);
    
    // instance index indirection
    HdBufferArrayRangeSharedPtr instanceIndexBar_ = batchItem->GetInstanceIndexRange();
    HdBufferArrayRangeSharedPtr instanceIndexBar =
        std::static_pointer_cast<HdBufferArrayRange>(instanceIndexBar_);
    if (instanceIndexBar) {
        // note that while instanceIndexBar is mandatory for instancing but
        // instanceBar can technically be empty (it doesn't make sense though)
        // testHdInstance --noprimvars covers that case.
        for (int i = 0; i < instancerNumLevels; ++i) {
            HdBufferArrayRangeSharedPtr ins_ = batchItem->GetInstancePrimvarRange(i);
            HdBufferArrayRangeSharedPtr ins =
                std::static_pointer_cast<HdBufferArrayRange>(ins_);
            instanceBars[i] = ins;
            binder.BindInstanceBufferArray(instanceBars[i], i);
        }
        binder.BindBufferArray(instanceIndexBar);
    }
    
    if (false && ARCH_UNLIKELY(TfDebug::IsEnabled(HD_SAFE_MODE))) {
        _ValidateCompatibility(constantBar,
                               indexBar,
                               topVisBar,
                               elementBar,
                               fvarBar,
                               vertexBar,
                               instancerNumLevels,
                               instanceIndexBar,
                               instanceBars);
    }
    
    // shader buffer bind
    HdBufferArrayRangeSharedPtr shaderBar;
    TF_FOR_ALL(shader, shaders) {
        HdBufferArrayRangeSharedPtr shaderBar_ = (*shader)->GetShaderData();
        shaderBar = std::static_pointer_cast<HdBufferArrayRange>(shaderBar_);
        if (shaderBar) {
            binder.BindBuffer(HdTokens->materialParams,
                              std::dynamic_pointer_cast<HdStBufferResource>(shaderBar->GetResource()));
        }
    }
    
    // drawindirect command, drawing coord, instanceIndexBase bind
    HdBufferArrayRangeSharedPtr dispatchBar =
    _dispatchBuffer->GetBufferArrayRange();
    binder.BindBufferArray(dispatchBar);
    
    // update geometric shader states
    program.GetGeometricShader()->BindResources(
        *hdStProgram, binder, *renderPassState);
    
    GLuint batchCount = _dispatchBuffer->GetCount();
    
    TF_DEBUG(HD_DRAWITEM_DRAWN).Msg("DRAW (indirect): %d\n", batchCount);
    
    _ExecuteDraw(program, batchCount);
    
    HD_PERF_COUNTER_INCR(HdPerfTokens->drawCalls);
    HD_PERF_COUNTER_ADD(HdTokens->itemsDrawn, _numVisibleItems);
    
    //
    // cleanup
    //
    binder.UnbindConstantBuffer(constantBar);
    binder.UnbindInterleavedBuffer(topVisBar, HdTokens->topologyVisibility);
    binder.UnbindBufferArray(elementBar);
    binder.UnbindBufferArray(fvarBar);
    binder.UnbindBufferArray(indexBar);
    binder.UnbindBufferArray(vertexBar);
    binder.UnbindBufferArray(dispatchBar);
    if(shaderBar) {
        binder.UnbindBuffer(HdTokens->materialParams,
                            std::dynamic_pointer_cast<HdStBufferResource>(shaderBar->GetResource()));
    }
    
    if (instanceIndexBar) {
        for (int i = 0; i < instancerNumLevels; ++i) {
            binder.UnbindInstanceBufferArray(instanceBars[i], i);
        }
        binder.UnbindBufferArray(instanceIndexBar);
    }
    
    TF_FOR_ALL(it, shaders) {
        (*it)->UnbindResources(*hdStProgram, binder, *renderPassState);
    }
    program.GetGeometricShader()->UnbindResources(*hdStProgram, binder, *renderPassState);
    
    hdStProgram->UnsetProgram();
}

void
HdSt_IndirectDrawBatch::_GPUFrustumInstanceCulling(
    HdStDrawItem const *batchItem,
    HdStRenderPassStateSharedPtr const &renderPassState,
    HdStResourceRegistrySharedPtr const &resourceRegistry)
{
    HdBufferArrayRangeSharedPtr constantBar_ =
        batchItem->GetConstantPrimvarRange();
    HdBufferArrayRangeSharedPtr constantBar =
        std::static_pointer_cast<HdBufferArrayRange>(constantBar_);
    int instancerNumLevels = batchItem->GetInstancePrimvarNumLevels();
    std::vector<HdBufferArrayRangeSharedPtr> instanceBars(instancerNumLevels);
    for (int i = 0; i < instancerNumLevels; ++i) {
        HdBufferArrayRangeSharedPtr ins_ = batchItem->GetInstancePrimvarRange(i);
        
        HdBufferArrayRangeSharedPtr ins =
            std::static_pointer_cast<HdBufferArrayRange>(ins_);
        
        instanceBars[i] = ins;
    }
    HdBufferArrayRangeSharedPtr instanceIndexBar_ =
        batchItem->GetInstanceIndexRange();
    HdBufferArrayRangeSharedPtr instanceIndexBar =
        std::static_pointer_cast<HdBufferArrayRange>(instanceIndexBar_);
    
    HdBufferArrayRangeSharedPtr cullDispatchBar =
        _dispatchBufferCullInput->GetBufferArrayRange();
    
    _CullingProgram &cullingProgram = _GetCullingProgram(resourceRegistry);
    
    HdStProgramSharedPtr const &
        program = cullingProgram.GetProgram();
    
    if (!TF_VERIFY(program)) return;
    if (!TF_VERIFY(program->Validate())) return;

    // We perform frustum culling on the GPU with the rasterizer disabled,
    // stomping the instanceCount of each drawing command in the
    // dispatch buffer to 0 for primitives that are culled, skipping
    // over other elements.
    
    const HdSt_ResourceBinder &binder = cullingProgram.GetBinder();
    
    program->SetProgram();
    
    // bind buffers
    binder.BindConstantBuffer(constantBar);
    
    // bind per-drawitem attribute (drawingCoord, instanceCount, drawCommand)
    binder.BindBufferArray(cullDispatchBar);
    
    if (instanceIndexBar) {
        int instancerNumLevels = batchItem->GetInstancePrimvarNumLevels();
        for (int i = 0; i < instancerNumLevels; ++i) {
            binder.BindInstanceBufferArray(instanceBars[i], i);
        }
        binder.BindBufferArray(instanceIndexBar);
    }
    
    GarchContextCaps const &caps = GarchResourceFactory::GetInstance()->GetContextCaps();

    // bind destination buffer (using entire buffer bind to start from offset=0)
    binder.BindBuffer(HdStIndirectDrawTokens->dispatchBuffer,
                      _dispatchBuffer->GetEntireResource());

    // set cull parameters
    unsigned int drawCommandNumUints = _dispatchBuffer->GetCommandNumUints();
    GfMatrix4f cullMatrix(renderPassState->GetCullMatrix());
    GfVec2f drawRangeNDC(renderPassState->GetDrawingRangeNDC());
    binder.BindUniformui(HdStIndirectDrawTokens->ulocDrawCommandNumUints, 1, &drawCommandNumUints);
    binder.BindUniformf(HdStIndirectDrawTokens->ulocCullMatrix, 16, cullMatrix.GetArray());
    if (_useTinyPrimCulling) {
        binder.BindUniformf(HdStIndirectDrawTokens->ulocDrawRangeNDC, 2, drawRangeNDC.GetArray());
    }

    // run culling shader
    bool validProgram = true;
    
    // XXX: should we cache cull command offset?
    HdBufferResourceSharedPtr cullCommandBuffer =
        _dispatchBufferCullInput->GetResource(HdTokens->drawDispatch);
    if (!TF_VERIFY(cullCommandBuffer)) {
        validProgram = false;
    }
    
    if (validProgram) {
        _GPUFrustumInstanceCullingExecute(resourceRegistry, program, binder, cullCommandBuffer);
    }

    // Reset all vertex attribs and their divisors. Note that the drawing
    // program has different bindings from the culling program does
    // in general, even though most of buffers will likely be assigned
    // with same attrib divisors again.
    binder.UnbindConstantBuffer(constantBar);
    binder.UnbindBufferArray(cullDispatchBar);
    if (instanceIndexBar) {
        int instancerNumLevels = batchItem->GetInstancePrimvarNumLevels();
        for (int i = 0; i < instancerNumLevels; ++i) {
            binder.UnbindInstanceBufferArray(instanceBars[i], i);
        }
        binder.UnbindBufferArray(instanceIndexBar);
    }

    // unbind destination dispatch buffer
    binder.UnbindBuffer(HdStIndirectDrawTokens->dispatchBuffer,
                        _dispatchBuffer->GetEntireResource());

    // make sure the culling results (instanceIndices and instanceCount)
    // are synchronized for the next drawing.
    _SyncFence();
}

void
HdSt_IndirectDrawBatch::_GPUFrustumNonInstanceCulling(
    HdStDrawItem const *batchItem,
    HdStRenderPassStateSharedPtr const &renderPassState,
    HdStResourceRegistrySharedPtr const &resourceRegistry)
{
    HdBufferArrayRangeSharedPtr constantBar_ =
        batchItem->GetConstantPrimvarRange();
    HdBufferArrayRangeSharedPtr constantBar =
        std::static_pointer_cast<HdBufferArrayRange>(constantBar_);
    
    HdBufferArrayRangeSharedPtr cullDispatchBar =
        _dispatchBufferCullInput->GetBufferArrayRange();
    
    _CullingProgram &cullingProgram = _GetCullingProgram(resourceRegistry);
    
    HdStProgramSharedPtr const &
    program = cullingProgram.GetProgram();
    if (!TF_VERIFY(program)) return;
    if (!TF_VERIFY(program->Validate())) return;
    
    // We perform frustum culling on the GPU with the rasterizer disabled,
    // stomping the instanceCount of each drawing command in the
    // dispatch buffer to 0 for primitives that are culled, skipping
    // over other elements.
    
    program->SetProgram();
    
    const HdSt_ResourceBinder &binder = cullingProgram.GetBinder();
    
    // bind constant
    binder.BindConstantBuffer(constantBar);
    // bind drawing coord, instance count
    binder.BindBufferArray(cullDispatchBar);
    
    GarchContextCaps const &caps = GarchResourceFactory::GetInstance()->GetContextCaps();
    
    // set cull parameters
    unsigned int drawCommandNumUints = _dispatchBuffer->GetCommandNumUints();
    GfMatrix4f cullMatrix(renderPassState->GetCullMatrix());
    GfVec2f drawRangeNDC(renderPassState->GetDrawingRangeNDC());
    binder.BindUniformf(HdStIndirectDrawTokens->ulocCullMatrix, 16, cullMatrix.GetArray());
    binder.BindUniformui(HdStIndirectDrawTokens->ulocDrawCommandNumUints, 1, &drawCommandNumUints);
    if (_useTinyPrimCulling) {
        binder.BindUniformf(HdStIndirectDrawTokens->ulocDrawRangeNDC, 2, drawRangeNDC.GetArray());
    }
    
    _GPUFrustumNonInstanceCullingExecute(resourceRegistry, program, binder);

    // unbind all
    binder.UnbindConstantBuffer(constantBar);
    binder.UnbindBufferArray(cullDispatchBar);
    
    program->UnsetProgram();
}

void
HdSt_IndirectDrawBatch::DrawItemInstanceChanged(HdStDrawItemInstance const* instance)
{
    // We need to check the visibility and update if needed
    if (_dispatchBuffer) {
        size_t batchIndex = instance->GetBatchIndex();
        int commandNumUints = _dispatchBuffer->GetCommandNumUints();
        int numLevels = instance->GetDrawItem()->GetInstancePrimvarNumLevels();
        int instanceIndexWidth = numLevels + 1;

        // When non-instance culling is being used, cullcommand points the same 
        // location as drawcommands. Then we update the same place twice, it 
        // might be better than branching.
        std::vector<GLuint>::iterator instanceCountIt =
            _drawCommandBuffer.begin()
            + batchIndex * commandNumUints
            + _instanceCountOffset;
        std::vector<GLuint>::iterator cullInstanceCountIt =
            _drawCommandBuffer.begin()
            + batchIndex * commandNumUints
            + _cullInstanceCountOffset;
        
        HdBufferArrayRangeSharedPtr const &instanceIndexBar_ =
            instance->GetDrawItem()->GetInstanceIndexRange();
        HdBufferArrayRangeSharedPtr instanceIndexBar =
            std::static_pointer_cast<HdBufferArrayRange>(instanceIndexBar_);
        
        int newInstanceCount = instanceIndexBar
            ? instanceIndexBar->GetNumElements() : 1;
            newInstanceCount = instance->IsVisible()
            ? (newInstanceCount/std::max(1, instanceIndexWidth))
            : 0;
        
        TF_DEBUG(HD_MDI).Msg("\nInstance Count changed: %d -> %d\n",
                             *instanceCountIt,
                             newInstanceCount);
        
        // Update instance count and overall count of visible items.
        if (static_cast<size_t>(newInstanceCount) != (*instanceCountIt)) {
            _numVisibleItems += (newInstanceCount - (*instanceCountIt));
            *instanceCountIt = newInstanceCount;
            *cullInstanceCountIt = newInstanceCount;
            _drawCommandBufferDirty = true;
        }
    }
}

void
HdSt_IndirectDrawBatch::_CullingProgram::Initialize(
    bool useDrawArrays, bool useInstanceCulling, size_t bufferArrayHash)
{
    if (useDrawArrays      != _useDrawArrays      ||
        useInstanceCulling != _useInstanceCulling ||
        bufferArrayHash    != _bufferArrayHash) {
        // reset shader
        Reset();
    }
    
    _useDrawArrays = useDrawArrays;
    _useInstanceCulling = useInstanceCulling;
    _bufferArrayHash = bufferArrayHash;
}

/* virtual */
void
HdSt_IndirectDrawBatch::_CullingProgram::_GetCustomBindings(
    HdBindingRequestVector *customBindings,
    bool *enableInstanceDraw) const
{
    if (!TF_VERIFY(enableInstanceDraw) ||
        !TF_VERIFY(customBindings)) return;

    customBindings->push_back(HdBindingRequest(HdBinding::SSBO,
                                  HdStIndirectDrawTokens->drawIndirectResult));
    customBindings->push_back(HdBindingRequest(HdBinding::SSBO,
                                  HdStIndirectDrawTokens->dispatchBuffer));
    customBindings->push_back(HdBindingRequest(HdBinding::UNIFORM,
                                  HdStIndirectDrawTokens->ulocDrawRangeNDC));
    customBindings->push_back(HdBindingRequest(HdBinding::UNIFORM,
                                  HdStIndirectDrawTokens->ulocCullMatrix));

    if (_useInstanceCulling) {
        customBindings->push_back(
            HdBindingRequest(HdBinding::DRAW_INDEX_INSTANCE,
                HdStIndirectDrawTokens->drawCommandIndex));
        customBindings->push_back(
            HdBindingRequest(HdBinding::UNIFORM,
                HdStIndirectDrawTokens->ulocDrawCommandNumUints));
        customBindings->push_back(
            HdBindingRequest(HdBinding::UNIFORM,
                HdStIndirectDrawTokens->ulocResetPass));
    } else {
        // non-instance culling
        customBindings->push_back(
            HdBindingRequest(HdBinding::DRAW_INDEX,
                HdStIndirectDrawTokens->drawCommandIndex));
        customBindings->push_back(
            HdBindingRequest(HdBinding::DRAW_INDEX,
                HdStIndirectDrawTokens->instanceCountInput));
        customBindings->push_back(
            HdBindingRequest(HdBinding::UNIFORM,
                HdStIndirectDrawTokens->ulocDrawCommandNumUints));
    }

    // set instanceDraw true if instanceCulling is enabled.
    // this value will be used to determine if glVertexAttribDivisor needs to
    // be enabled or not.
    *enableInstanceDraw = _useInstanceCulling;
}

PXR_NAMESPACE_CLOSE_SCOPE
