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

#include "pxr/imaging/hd/engine.h"

#include "pxr/imaging/hd/debugCodes.h"
#include "pxr/imaging/hd/drawItem.h"
#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/renderContextCaps.h"
#include "pxr/imaging/hd/renderDelegate.h"
#include "pxr/imaging/hd/renderIndex.h"
#include "pxr/imaging/hd/renderPass.h"
#include "pxr/imaging/hd/renderPassState.h"
#include "pxr/imaging/hd/resourceRegistry.h"
#include "pxr/imaging/hd/rprim.h"
#include "pxr/imaging/hd/shader.h"
#include "pxr/imaging/hd/task.h"
#include "pxr/imaging/hd/tokens.h"

#include "pxr/imaging/hd/GL/codeGenGLSL.h"
#include "pxr/imaging/hd/GL/bufferResourceGL.h"
#include "pxr/imaging/hd/GL/bufferRelocatorGL.h"
#include "pxr/imaging/hd/GL/glslProgram.h"
#include "pxr/imaging/hd/GL/persistentBufferGL.h"
#include "pxr/imaging/hd/GL/textureResourceGL.h"
#include "pxr/imaging/glf/drawTarget.h"
#include "pxr/imaging/glf/glslfx.h"

#if defined(ARCH_GFX_METAL)
#include "pxr/imaging/hd/Metal/codeGenMSL.h"
#include "pxr/imaging/hd/Metal/bufferResourceMetal.h"
#include "pxr/imaging/hd/Metal/bufferRelocatorMetal.h"
#include "pxr/imaging/hd/Metal/mslProgram.h"
#include "pxr/imaging/hd/Metal/persistentBufferMetal.h"
#include "pxr/imaging/hd/Metal/textureResourceMetal.h"
#include "pxr/imaging/mtlf/drawTarget.h"
#include "pxr/imaging/mtlf/glslfx.h"
#endif

#include "pxr/base/tf/envSetting.h"

#include <sstream>

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_ENV_SETTING(HD_ENABLE_GPU_TINY_PRIM_CULLING, true,
                      "Enable tiny prim culling");
TF_DEFINE_ENV_SETTING(HD_ENABLE_GPU_FRUSTUM_CULLING, true,
                      "Enable GPU frustum culling");
TF_DEFINE_ENV_SETTING(HD_ENABLE_GPU_COUNT_VISIBLE_INSTANCES, false,
                      "Enable GPU frustum culling visible count query");
TF_DEFINE_ENV_SETTING(HD_ENABLE_GPU_INSTANCE_FRUSTUM_CULLING, true,
                      "Enable GPU per-instance frustum culling");


HdEngine::RenderAPI HdEngine::_renderAPI = HdEngine::RenderAPI::Unset;

HdEngine::HdEngine(RenderAPI api)
 : _taskContext()
{
    if(_renderAPI != RenderAPI::Unset) {
        TF_FATAL_CODING_ERROR("Only one HdEngine instance can be created at one time");
    }
    _renderAPI = api;
}

HdEngine::~HdEngine()
{
    _renderAPI = RenderAPI::Unset;
}

void 
HdEngine::SetTaskContextData(const TfToken &id, VtValue &data)
{
    // See if the token exists in the context and if not add it.
    std::pair<HdTaskContext::iterator, bool> result =
                                                 _taskContext.emplace(id, data);
    if (!result.second) {
        // Item wasn't new, so need to update it
        result.first->second = data;
    }
}

void
HdEngine::RemoveTaskContextData(const TfToken &id)
{
    _taskContext.erase(id);
}

void
HdEngine::_InitCaps() const
{
    // Make sure we initialize caps in main thread.
    HdRenderContextCaps::GetInstance();
}

void
HdEngine::Execute(HdRenderIndex& index, HdTaskSharedPtrVector const &tasks)
{
    // Note: For Hydra Stream render delegate.
    //
    //
    // The following order is important, be careful.
    //
    // If Sync updates topology varying prims, it triggers both:
    //   1. changing drawing coordinate and bumps up the global collection
    //      version to invalidate the (indirect) batch.
    //   2. marking garbage collection needed so that the unused BAR
    //      resources will be reclaimed.
    //   Also resizing ranges likely cause the buffer reallocation
    //   (==drawing coordinate changes) anyway.
    //
    // Note that the garbage collection also changes the drawing coordinate,
    // so the collection should be invalidated in that case too.
    //
    // Once we reflect all conditions which provoke the batch recompilation
    // into the collection dirtiness, we can call
    // HdRenderPass::GetCommandBuffer() to get the right batch.

    _InitCaps();

    // --------------------------------------------------------------------- //
    // DATA DISCOVERY PHASE
    // --------------------------------------------------------------------- //
    // Discover all required input data needed to render the required render
    // prim representations. At this point, we must read enough data to
    // establish the resource dependency graph, but we do not yet populate CPU-
    // nor GPU-memory with data.

    // As a result of the next call, the resource registry will be populated
    // with both BufferSources that need to be resolved (possibly generating
    // data on the CPU) and computations to run on the GPU.


    // Process all pending dirty lists
    index.SyncAll(tasks, &_taskContext);

    HdRenderDelegate *renderDelegate = index.GetRenderDelegate();
    renderDelegate->CommitResources(&index.GetChangeTracker());

    TF_FOR_ALL(it, tasks) {
        (*it)->Execute(&_taskContext);
    }
}

void
HdEngine::ReloadAllShaders(HdRenderIndex& index)
{
    HdChangeTracker &tracker = index.GetChangeTracker();

    // 1st dirty all rprims, so they will trigger shader reload
    tracker.MarkAllRprimsDirty(HdChangeTracker::AllDirty);

    // Dirty all surface shaders
    SdfPathVector shaders = index.GetSprimSubtree(HdPrimTypeTokens->shader,
                                                  SdfPath::EmptyPath());

    for (SdfPathVector::iterator shaderIt  = shaders.begin();
                                 shaderIt != shaders.end();
                               ++shaderIt) {

        tracker.MarkSprimDirty(*shaderIt, HdChangeTracker::AllDirty);
    }

    // Invalidate Geometry shader cache in Resource Registry.
    index.GetResourceRegistry()->InvalidateGeometricShaderRegistry();

    // Fallback Shader
    HdShader *shader = static_cast<HdShader *>(
                              index.GetFallbackSprim(HdPrimTypeTokens->shader));
    shader->Reload();


    // Note: Several Shaders are not currently captured in this
    // - Lighting Shaders
    // - Render Pass Shaders
    // - Culling Shader

}

Hd_CodeGen *HdEngine::CreateCodeGen(Hd_GeometricShaderPtr const &geometricShader,
                                    HdShaderCodeSharedPtrVector const &shaders)
{
    switch(_renderAPI) {
        case RenderAPI::OpenGL: return new Hd_CodeGenGLSL(geometricShader, shaders);
#if defined(ARCH_GFX_METAL)
        case RenderAPI::Metal: return new Hd_CodeGenMSL(geometricShader, shaders);
#endif
        default:
            TF_FATAL_CODING_ERROR("No Hd_CodeGen for this API");
    }
    return nullptr;
}

Hd_CodeGen *HdEngine::CreateCodeGen(HdShaderCodeSharedPtrVector const &shaders)
{
    switch(_renderAPI) {
        case RenderAPI::OpenGL: return new Hd_CodeGenGLSL(shaders);
#if defined(ARCH_GFX_METAL)
        case RenderAPI::Metal: return new Hd_CodeGenMSL(shaders);
#endif
        default:
        TF_FATAL_CODING_ERROR("No Hd_CodeGen for this API");
    }
    return nullptr;
}

GLSLFX *HdEngine::CreateGLSLFX()
{
    switch(_renderAPI) {
        case RenderAPI::OpenGL: return new GlfGLSLFX();
#if defined(ARCH_GFX_METAL)
        case RenderAPI::Metal: return new MtlfGLSLFX();
#endif
        default:
        TF_FATAL_CODING_ERROR("No GLSLFX for this API");
    }
    return nullptr;
}

GLSLFX *HdEngine::CreateGLSLFX(std::string const & filePath)
{
    switch(_renderAPI) {
        case RenderAPI::OpenGL: return new GlfGLSLFX(filePath);
#if defined(ARCH_GFX_METAL)
        case RenderAPI::Metal: return new MtlfGLSLFX(filePath);
#endif
        default:
        TF_FATAL_CODING_ERROR("No GLSLFX for this API");
    }
    return nullptr;
}

GLSLFX *HdEngine::CreateGLSLFX(std::istream &is)
{
    switch(_renderAPI) {
        case RenderAPI::OpenGL: return new GlfGLSLFX(is);
#if defined(ARCH_GFX_METAL)
        case RenderAPI::Metal: return new MtlfGLSLFX(is);
#endif
        default:
        TF_FATAL_CODING_ERROR("No GLSLFX for this API");
    }
    return nullptr;
}

HdBufferResource *HdEngine::CreateResourceBuffer(TfToken const &role,
                                             int glDataType,
                                             short numComponents,
                                             int arraySize,
                                             int offset,
                                             int stride)
{
    switch(_renderAPI) {
        case RenderAPI::OpenGL: return new HdBufferResourceGL(
            role, glDataType, numComponents, arraySize, offset, stride);
#if defined(ARCH_GFX_METAL)
        case RenderAPI::Metal: return new HdBufferResourceMetal(
            role, glDataType, numComponents, arraySize, offset, stride);
#endif
        default:
        TF_FATAL_CODING_ERROR("No resource buffer for this API");
    }
    return nullptr;
}

HdProgram *HdEngine::CreateProgram(TfToken const &role)
{
    switch(_renderAPI) {
        case RenderAPI::OpenGL: return new HdGLSLProgram(role);
#if defined(ARCH_GFX_METAL)
        case RenderAPI::Metal: return new HdMSLProgram(role);
#endif
        default:
        TF_FATAL_CODING_ERROR("No program for this API");
    }
    return nullptr;
}

HdBufferRelocator *HdEngine::CreateBufferRelocator(HdBufferResourceGPUHandle srcBuffer, HdBufferResourceGPUHandle dstBuffer)
{
    switch(_renderAPI) {
        case RenderAPI::OpenGL: return new HdBufferRelocatorGL(srcBuffer, dstBuffer);
#if defined(ARCH_GFX_METAL)
        case RenderAPI::Metal: return new HdBufferRelocatorMetal(srcBuffer, dstBuffer);
#endif
        default:
        TF_FATAL_CODING_ERROR("No program for this API");
    }
    return nullptr;
}

HdPersistentBuffer *HdEngine::CreatePersistentBuffer(TfToken const &role, size_t dataSize, void* data)
{
    switch(_renderAPI) {
        case RenderAPI::OpenGL: return new HdPersistentBufferGL(role, dataSize, data);
#if defined(ARCH_GFX_METAL)
        case RenderAPI::Metal: return new HdPersistentBufferMetal(role, dataSize, data);
#endif
        default:
        TF_FATAL_CODING_ERROR("No program for this API");
    }
    return nullptr;
}

GarchDrawTargetRefPtr HdEngine::CreateDrawTarget(GfVec2i const & size, bool requestMSAA)
{
    switch(_renderAPI) {
        case RenderAPI::OpenGL: return TfCreateRefPtr(GlfDrawTarget::New(size, requestMSAA));
#if defined(ARCH_GFX_METAL)
        case RenderAPI::Metal: return TfCreateRefPtr(MtlfDrawTarget::New(size, requestMSAA));
#endif
        default:
            TF_FATAL_CODING_ERROR("No program for this API");
    }
    return nullptr;
}

HdTextureResource *HdEngine::CreateSimpleTextureResource(GarchTextureHandleRefPtr const &textureHandle, bool isPtex)
{
    switch(_renderAPI) {
        case RenderAPI::OpenGL: return new HdSimpleTextureResourceGL(textureHandle, isPtex);
#if defined(ARCH_GFX_METAL)
        case RenderAPI::Metal: return new HdSimpleTextureResourceMetal(textureHandle, isPtex);
#endif
        default:
            TF_FATAL_CODING_ERROR("No program for this API");
    }
    return nullptr;
}

HdTextureResource *HdEngine::CreateSimpleTextureResource(GarchTextureHandleRefPtr const &textureHandle, bool isPtex,
                                                         HdWrap wrapS, HdWrap wrapT, HdMinFilter minFilter, HdMagFilter magFilter)
{
    switch(_renderAPI) {
        case RenderAPI::OpenGL: return new HdSimpleTextureResourceGL(textureHandle, isPtex,
                                                                     wrapS, wrapT, minFilter, magFilter);
#if defined(ARCH_GFX_METAL)
        case RenderAPI::Metal: return new HdSimpleTextureResourceMetal(textureHandle, isPtex,
                                                                       wrapS, wrapT, minFilter, magFilter);
#endif
        default:
            TF_FATAL_CODING_ERROR("No program for this API");
    }
    return nullptr;
}

bool
HdEngine::IsEnabledGPUFrustumCulling()
{
    HdRenderContextCaps const &caps = HdRenderContextCaps::GetInstance();
    
    switch(_renderAPI) {
        case RenderAPI::OpenGL:
            // GPU XFB frustum culling should work since GL 4.0, but for now
            // the shader frustumCull.glslfx requires explicit uniform location
            static bool isEnabledGPUFrustumCulling =
            TfGetEnvSetting(HD_ENABLE_GPU_FRUSTUM_CULLING) &&
            (caps.explicitUniformLocation);
            return isEnabledGPUFrustumCulling &&
                !TfDebug::IsEnabled(HD_DISABLE_FRUSTUM_CULLING);
#if defined(ARCH_GFX_METAL)
        case RenderAPI::Metal:
            return true;
#endif
        default:
            TF_FATAL_CODING_ERROR("No program for this API");
    }
    return false;
}

bool
HdEngine::IsEnabledGPUCountVisibleInstances()
{
    switch(_renderAPI) {
        case RenderAPI::OpenGL:
            static bool isEnabledGPUCountVisibleInstances =
            TfGetEnvSetting(HD_ENABLE_GPU_COUNT_VISIBLE_INSTANCES);
            return isEnabledGPUCountVisibleInstances;
#if defined(ARCH_GFX_METAL)
        case RenderAPI::Metal:
            return true;
#endif
        default:
            TF_FATAL_CODING_ERROR("No program for this API");
    }
    return false;
}

bool
HdEngine::IsEnabledGPUTinyPrimCulling()
{
    switch(_renderAPI) {
        case RenderAPI::OpenGL:
            static bool isEnabledGPUTinyPrimCulling =
            TfGetEnvSetting(HD_ENABLE_GPU_TINY_PRIM_CULLING);
            return isEnabledGPUTinyPrimCulling &&
                    !TfDebug::IsEnabled(HD_DISABLE_TINY_PRIM_CULLING);
#if defined(ARCH_GFX_METAL)
        case RenderAPI::Metal:
            return true;
#endif
        default:
            TF_FATAL_CODING_ERROR("No program for this API");
    }
    return false;
}

bool
HdEngine::IsEnabledGPUInstanceFrustumCulling()
{
    HdRenderContextCaps const &caps = HdRenderContextCaps::GetInstance();
 
    switch(_renderAPI) {
        case RenderAPI::OpenGL:
            // GPU instance frustum culling requires SSBO of bindless buffer
            static bool isEnabledGPUInstanceFrustumCulling =
            TfGetEnvSetting(HD_ENABLE_GPU_INSTANCE_FRUSTUM_CULLING) &&
                (caps.shaderStorageBufferEnabled || caps.bindlessBufferEnabled);
            return isEnabledGPUInstanceFrustumCulling;
#if defined(ARCH_GFX_METAL)
        case RenderAPI::Metal:
            return true;
#endif
        default:
            TF_FATAL_CODING_ERROR("No program for this API");
    }
    return false;
}

PXR_NAMESPACE_CLOSE_SCOPE

