//
// Copyright 2020 Pixar
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
#include "pxr/imaging/hdx/aovInputTask.h"
#include "pxr/imaging/hdx/hgiConversions.h"

#include "pxr/imaging/hd/aov.h"
#include "pxr/imaging/hd/tokens.h"
#include "pxr/imaging/hdSt/renderBuffer.h"
#include "pxr/imaging/hgi/tokens.h"


PXR_NAMESPACE_OPEN_SCOPE

HdxAovInputTask::HdxAovInputTask(HdSceneDelegate* delegate, SdfPath const& id)
 : HdxTask(id)
 , _converged(false)
 , _aovBufferPath()
 , _depthBufferPath()
 , _aovBuffer(nullptr)
 , _depthBuffer(nullptr)
{
}

HdxAovInputTask::~HdxAovInputTask()
{
    if (_aovTexture) {
        _GetHgi()->DestroyTexture(&_aovTexture);
    }
    if (_depthTexture) {
        _GetHgi()->DestroyTexture(&_depthTexture);
    }
}

bool
HdxAovInputTask::IsConverged() const
{
    return _converged;
}

void
HdxAovInputTask::_Sync(HdSceneDelegate* delegate,
                      HdTaskContext* ctx,
                      HdDirtyBits* dirtyBits)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    if ((*dirtyBits) & HdChangeTracker::DirtyParams) {
        HdxAovInputTaskParams params;

        if (_GetTaskParams(delegate, &params)) {
            _aovBufferPath = params.aovBufferPath;
            _depthBufferPath = params.depthBufferPath;
        }
    }
    *dirtyBits = HdChangeTracker::Clean;
}

void
HdxAovInputTask::Prepare(HdTaskContext* ctx, HdRenderIndex *renderIndex)
{
    _aovBuffer = nullptr;
    _depthBuffer = nullptr;

    // An empty _aovBufferPath disables the task
    if (!_aovBufferPath.IsEmpty()) {
        _aovBuffer = static_cast<HdRenderBuffer*>(
            renderIndex->GetBprim(
                HdPrimTypeTokens->renderBuffer, _aovBufferPath));
    }

    if (!_depthBufferPath.IsEmpty()) {
        _depthBuffer = static_cast<HdRenderBuffer*>(
            renderIndex->GetBprim(
                HdPrimTypeTokens->renderBuffer, _depthBufferPath));
    }
}

void
HdxAovInputTask::Execute(HdTaskContext* ctx)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    // This task requires an aov buffer to have been set and is immediately
    // converged if there is no aov buffer.
    if (!_aovBuffer) {
        _converged = true;
        return;
    }

    // Check converged state of buffer(s)
    _converged = _aovBuffer->IsConverged();
    if (_depthBuffer) {
        _converged = _converged && _depthBuffer->IsConverged();
    }

    // Resolve the buffers before we read them.
    _aovBuffer->Resolve();
    if (_depthBuffer) {
        _depthBuffer->Resolve();
    }

    // Start by clearing aov texture handles from task context.
    // These are last frames textures and we may be visualizing different aovs.
    ctx->erase(HdAovTokens->color);
    ctx->erase(HdAovTokens->depth);

    // If the aov is already backed by a HgiTexture we skip creating a new
    // GPU HgiTexture for it and place it directly on the shared task context
    // for consecutive tasks to find and operate on.
    // The lifetime management of that HgiTexture remains with the aov.

    bool hgiHandleProvidedByAov = false;
    const bool mulSmp = false;

    VtValue aov = _aovBuffer->GetResource(mulSmp);
    if (aov.IsHolding<HgiTextureHandle>()) {
        hgiHandleProvidedByAov = true;
        (*ctx)[HdAovTokens->color] = aov;
    }

    if (_depthBuffer) {
        VtValue depth = _depthBuffer->GetResource(mulSmp);
        if (depth.IsHolding<HgiTextureHandle>()) {
            (*ctx)[HdAovTokens->depth] = depth;
        }
    }

    if (hgiHandleProvidedByAov) {
        return;
    }

    // If the aov is not backed by a HgiTexture (e.g. RenderMan, Embree) we
    // convert the aov pixel data to a HgiTexture and place that new texture
    // in the shared task context.
    // The lifetime of this new HgiTexture is managed by this task. 

    _UpdateTexture(ctx, _aovTexture, _aovBuffer);
    if (_aovTexture) {
        (*ctx)[HdAovTokens->color] = VtValue(_aovTexture);
    }

    if (_depthBuffer) {
        _UpdateTexture(ctx, _depthTexture, _depthBuffer);
        if (_depthTexture) {
            (*ctx)[HdAovTokens->depth] = VtValue(_depthTexture);
        }
    }
}

void
HdxAovInputTask::_UpdateTexture(
    HdTaskContext* ctx,
    HgiTextureHandle& texture,
    HdRenderBuffer* buffer)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    GfVec3i dim(
        buffer->GetWidth(),
        buffer->GetHeight(),
        buffer->GetDepth());

    size_t pixelByteSize = HdDataSizeOfFormat(buffer->GetFormat());

    // XXX If dimension and format have not changed we could try to re-use the
    // existing texture resource and upload new pixels instead of destroying.
    // But atm we don't have Hgi api for this.
    if (texture) {
        _GetHgi()->DestroyTexture(&texture);
    }

    HgiTextureDesc texDesc;
    texDesc.debugName = "AovInput Texture";
    texDesc.dimensions = dim;

    const void* pixelData = buffer->Map();

    texDesc.format = HdxHgiConversions::GetHgiFormat(buffer->GetFormat());
    texDesc.initialData = pixelData;
    texDesc.layerCount = 1;
    texDesc.mipLevels = 1;
    texDesc.pixelsByteSize = dim[0] * dim[1] * dim[2] * pixelByteSize;
    texDesc.sampleCount = HgiSampleCount1;
    texDesc.usage = HgiTextureUsageBitsColorTarget | 
                    HgiTextureUsageBitsShaderRead;

    texture = _GetHgi()->CreateTexture(texDesc);

    buffer->Unmap();
}

// --------------------------------------------------------------------------- //
// VtValue Requirements
// --------------------------------------------------------------------------- //

std::ostream& operator<<(std::ostream& out, const HdxAovInputTaskParams& pv)
{
    out << "AovInputTask Params: (...) "
        << pv.aovBufferPath << " "
        << pv.depthBufferPath;
    return out;
}

bool operator==(const HdxAovInputTaskParams& lhs,
                const HdxAovInputTaskParams& rhs)
{
    return lhs.aovBufferPath   == rhs.aovBufferPath    &&
           lhs.depthBufferPath == rhs.depthBufferPath;
}

bool operator!=(const HdxAovInputTaskParams& lhs,
                const HdxAovInputTaskParams& rhs)
{
    return !(lhs == rhs);
}

PXR_NAMESPACE_CLOSE_SCOPE
