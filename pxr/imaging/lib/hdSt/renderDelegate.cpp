//
// Copyright 2017 Pixar
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
#include "pxr/pxr.h"
#include "pxr/imaging/hdSt/renderDelegate.h"

#include "pxr/imaging/hdSt/basisCurves.h"
#include "pxr/imaging/hd/camera.h"
#include "pxr/imaging/hdSt/drawTarget.h"
#include "pxr/imaging/hdSt/extComputation.h"
#include "pxr/imaging/hdSt/field.h"
#include "pxr/imaging/hdSt/glslfxShader.h"
#include "pxr/imaging/hdSt/instancer.h"
#include "pxr/imaging/hdSt/light.h"
#include "pxr/imaging/hdSt/material.h"
#include "pxr/imaging/hdSt/mesh.h"
#include "pxr/imaging/hdSt/package.h"
#include "pxr/imaging/hdSt/points.h"
#include "pxr/imaging/hdSt/renderBuffer.h"
#include "pxr/imaging/hdSt/renderPass.h"
#include "pxr/imaging/hdSt/renderPassState.h"
#include "pxr/imaging/hdSt/resourceFactory.h"
#include "pxr/imaging/hdSt/texture.h"
#include "pxr/imaging/hdSt/tokens.h"
#include "pxr/imaging/hdSt/resourceRegistry.h"
#include "pxr/imaging/hdSt/volume.h"

#include "pxr/imaging/hd/engine.h"
#include "pxr/imaging/hd/extComputation.h"
#include "pxr/imaging/hd/perfLog.h"

#include "pxr/imaging/hio/glslfx.h"

#include "pxr/imaging/garch/contextCaps.h"
#include "pxr/imaging/garch/resourceFactory.h"
#include "pxr/imaging/garch/textureRegistry.h"

#include "pxr/imaging/glf/diagnostic.h"

#if defined(ARCH_GFX_OPENGL)
#include "pxr/imaging/glf/contextCaps.h"
#endif
#if defined(ARCH_GFX_METAL)
#include "pxr/imaging/mtlf/contextCaps.h"
#endif

#include "pxr/base/tf/envSetting.h"
#include "pxr/base/tf/getenv.h"
#include "pxr/base/tf/staticTokens.h"

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_ENV_SETTING(HD_ENABLE_GPU_TINY_PRIM_CULLING, false,
                      "Enable tiny prim culling");

TF_DEFINE_ENV_SETTING(HDST_ENABLE_EXPERIMENTAL_VOLUME_ELLIPSOID_STANDINS, false,
                      "Render constant density ellipsoid standins for "
                      "volume prims");

// This token is repeated from usdVolImaging which we cannot access from here.
// Should we even instantiate bprims of different types for OpenVDB vs Field3d?
TF_DEFINE_PRIVATE_TOKENS(
    _tokens,
    (openvdbAsset)
);

const TfTokenVector HdStRenderDelegate::SUPPORTED_RPRIM_TYPES =
{
    HdPrimTypeTokens->mesh,
    HdPrimTypeTokens->basisCurves,
    HdPrimTypeTokens->points,
    HdPrimTypeTokens->volume
};

const TfTokenVector HdStRenderDelegate::SUPPORTED_SPRIM_TYPES =
{
    HdPrimTypeTokens->camera,
    HdPrimTypeTokens->drawTarget,
    HdPrimTypeTokens->extComputation,
    HdPrimTypeTokens->material,
    HdPrimTypeTokens->domeLight,
    HdPrimTypeTokens->rectLight,
    HdPrimTypeTokens->simpleLight,
    HdPrimTypeTokens->sphereLight
};

const TfTokenVector HdStRenderDelegate::SUPPORTED_BPRIM_TYPES =
{
    HdPrimTypeTokens->texture,
    _tokens->openvdbAsset
    // XXX Wait with enabling AOV in HdSt until TaskController has HdSt AOV code
    //HdPrimTypeTokens->renderBuffer
};

std::mutex HdStRenderDelegate::_mutexResourceRegistry;
std::atomic_int HdStRenderDelegate::_counterResourceRegistry;
HdStResourceRegistrySharedPtr HdStRenderDelegate::_resourceRegistry;

HdStRenderDelegate::HdStRenderDelegate()
{
    _Initialize();
}

HdStRenderDelegate::HdStRenderDelegate(HdRenderSettingsMap const& settingsMap)
    : HdRenderDelegate(settingsMap)
{
    _Initialize();
}

void
HdStRenderDelegate::_Initialize()
{
    // Initialize one resource registry for all St plugins
    // It will also add the resource to the logging object so we
    // can query the resources used by all St plugins later
    std::lock_guard<std::mutex> guard(_mutexResourceRegistry);
    
    if (_counterResourceRegistry.fetch_add(1) == 0) {
        _resourceRegistry.reset( new HdStResourceRegistry() );
        HdPerfLog::GetInstance().AddResourceRegistry(_resourceRegistry);
    }

    // Initialize the settings and settings descriptors.
    _settingDescriptors.resize(1);
    _settingDescriptors[0] = { "Enable Tiny Prim Culling",
        HdStRenderSettingsTokens->enableTinyPrimCulling,
        VtValue(bool(TfGetEnvSetting(HD_ENABLE_GPU_TINY_PRIM_CULLING))) };
    _PopulateDefaultSettings(_settingDescriptors);
}

HdRenderSettingDescriptorList
HdStRenderDelegate::GetRenderSettingDescriptors() const
{
    return _settingDescriptors;
}

VtDictionary 
HdStRenderDelegate::GetRenderStats() const
{
    VtDictionary ra = _resourceRegistry->GetResourceAllocation();

    const VtDictionary::iterator gpuMemIt = 
        ra.find(HdPerfTokens->gpuMemoryUsed.GetString());
    if (gpuMemIt != ra.end()) {
        // If we find gpuMemoryUsed, add the texture memory to it.
        // XXX: We should look into fixing this in the resource registry itself
        size_t texMem = 
            VtDictionaryGet<size_t>(ra, HdPerfTokens->textureMemory.GetString(),
                VtDefault = 0);
        size_t gpuMemTotal = gpuMemIt->second.Get<size_t>();
        gpuMemIt->second = VtValue(gpuMemTotal + texMem);
    }

    return ra;
}

HdStRenderDelegate::~HdStRenderDelegate()
{
    // Here we could destroy the resource registry when the last render
    // delegate HdSt is destroyed, however we prefer to keep the resources
    // around to match previous singleton behaviour (for now).
    
    // ... and now freeing resources
    if (_counterResourceRegistry.fetch_sub(1) == 1) {
        HdPerfLog::GetInstance().RemoveResourceRegistry(_resourceRegistry);
        _resourceRegistry.reset();
    }
    
    GarchTextureRegistry::GetInstance().GarbageCollectIfNeeded();
}

const TfTokenVector &
HdStRenderDelegate::GetSupportedRprimTypes() const
{
    return SUPPORTED_RPRIM_TYPES;
}

const TfTokenVector &
HdStRenderDelegate::GetSupportedSprimTypes() const
{
    return SUPPORTED_SPRIM_TYPES;
}

const TfTokenVector &
HdStRenderDelegate::GetSupportedBprimTypes() const
{
    return SUPPORTED_BPRIM_TYPES;
}

HdRenderParam *
HdStRenderDelegate::GetRenderParam() const
{
    return nullptr;
}

HdResourceRegistrySharedPtr
HdStRenderDelegate::GetResourceRegistry() const
{
    return _resourceRegistry;
}

HdAovDescriptor
HdStRenderDelegate::GetDefaultAovDescriptor(TfToken const& name) const
{
    const bool colorDepthMSAA = true; // GL requires color/depth to be matching.

    if (name == HdAovTokens->color) {
        HdFormat colorFormat = 
            GarchResourceFactory::GetInstance()->GetContextCaps().floatingPointBuffersEnabled ?
            HdFormatFloat16Vec4 : HdFormatUNorm8Vec4;
        return HdAovDescriptor(colorFormat,colorDepthMSAA, VtValue(GfVec4f(0)));
    } else if (name == HdAovTokens->normal || name == HdAovTokens->Neye) {
        return HdAovDescriptor(HdFormatFloat32Vec3, /*msaa*/ false,
                               VtValue(GfVec3f(-1.0f)));
    } else if (name == HdAovTokens->depth) {
        return HdAovDescriptor(HdFormatFloat32, colorDepthMSAA, VtValue(1.0f));
    } else if (name == HdAovTokens->linearDepth) {
        return HdAovDescriptor(HdFormatFloat32, /*msaa*/ false, VtValue(0.0f));
    } else if (name == HdAovTokens->primId ||
               name == HdAovTokens->instanceId ||
               name == HdAovTokens->elementId) {
        return HdAovDescriptor(HdFormatInt32, /*msaa*/ false, VtValue(-1));
    } else {
        HdParsedAovToken aovId(name);
        if (aovId.isPrimvar) {
            return HdAovDescriptor(HdFormatFloat32Vec3, /*msaa*/ false,
                                   VtValue(GfVec3f(0.0f)));
        }
    }

    return HdAovDescriptor();
}

HdRenderPassSharedPtr
HdStRenderDelegate::CreateRenderPass(HdRenderIndex *index,
                        HdRprimCollection const& collection)
{
    return HdRenderPassSharedPtr(new HdSt_RenderPass(index, collection));
}

HdRenderPassStateSharedPtr
HdStRenderDelegate::CreateRenderPassState() const
{
    return HdStRenderPassStateSharedPtr(HdStResourceFactory::GetInstance()->NewRenderPassState());
}

HdInstancer *
HdStRenderDelegate::CreateInstancer(HdSceneDelegate *delegate,
                                    SdfPath const& id,
                                    SdfPath const& instancerId)
{
    return new HdStInstancer(delegate, id, instancerId);
}

void
HdStRenderDelegate::DestroyInstancer(HdInstancer *instancer)
{
    delete instancer;
}

HdRprim *
HdStRenderDelegate::CreateRprim(TfToken const& typeId,
                                    SdfPath const& rprimId,
                                    SdfPath const& instancerId)
{
    if (typeId == HdPrimTypeTokens->mesh) {
        return new HdStMesh(rprimId, instancerId);
    } else if (typeId == HdPrimTypeTokens->basisCurves) {
        return new HdStBasisCurves(rprimId, instancerId);
    } else  if (typeId == HdPrimTypeTokens->points) {
        return new HdStPoints(rprimId, instancerId);
    } else  if (typeId == HdPrimTypeTokens->volume &&
                bool(TfGetEnvSetting(
                         HDST_ENABLE_EXPERIMENTAL_VOLUME_ELLIPSOID_STANDINS))) {
        return new HdStVolume(rprimId, instancerId);
    } else {
        TF_CODING_ERROR("Unknown Rprim Type %s", typeId.GetText());
    }

    return nullptr;
}

void
HdStRenderDelegate::DestroyRprim(HdRprim *rPrim)
{
    delete rPrim;
}

HdSprim *
HdStRenderDelegate::CreateSprim(TfToken const& typeId,
                                    SdfPath const& sprimId)
{
    if (typeId == HdPrimTypeTokens->camera) {
        return new HdCamera(sprimId);
    } else  if (typeId == HdPrimTypeTokens->drawTarget) {
        return new HdStDrawTarget(sprimId);
    } else  if (typeId == HdPrimTypeTokens->extComputation) {
        return new HdStExtComputation(sprimId);
    } else  if (typeId == HdPrimTypeTokens->material) {
        return new HdStMaterial(sprimId);
    } else if (typeId == HdPrimTypeTokens->domeLight ||
                typeId == HdPrimTypeTokens->simpleLight ||
                typeId == HdPrimTypeTokens->sphereLight ||
                typeId == HdPrimTypeTokens->rectLight) {
        return new HdStLight(sprimId, typeId);
    } else {
        TF_CODING_ERROR("Unknown Sprim Type %s", typeId.GetText());
    }

    return nullptr;
}

HdSprim *
HdStRenderDelegate::CreateFallbackSprim(TfToken const& typeId)
{
    if (typeId == HdPrimTypeTokens->camera) {
        return new HdCamera(SdfPath::EmptyPath());
    } else  if (typeId == HdPrimTypeTokens->drawTarget) {
        return new HdStDrawTarget(SdfPath::EmptyPath());
    } else  if (typeId == HdPrimTypeTokens->extComputation) {
        return new HdStExtComputation(SdfPath::EmptyPath());
    } else  if (typeId == HdPrimTypeTokens->material) {
        return _CreateFallbackMaterialPrim();
    } else if (typeId == HdPrimTypeTokens->domeLight ||
                typeId == HdPrimTypeTokens->simpleLight ||
                typeId == HdPrimTypeTokens->sphereLight ||
                typeId == HdPrimTypeTokens->rectLight) {
        return new HdStLight(SdfPath::EmptyPath(), typeId);
    } else {
        TF_CODING_ERROR("Unknown Sprim Type %s", typeId.GetText());
    }

    return nullptr;
}

void
HdStRenderDelegate::DestroySprim(HdSprim *sPrim)
{
    delete sPrim;
}

HdBprim *
HdStRenderDelegate::CreateBprim(TfToken const& typeId,
                                SdfPath const& bprimId)
{
    if (typeId == HdPrimTypeTokens->texture) {
        return new HdStTexture(bprimId);
    } else if (typeId == _tokens->openvdbAsset) {
        return new HdStField(bprimId, typeId);
    } else if (typeId == HdPrimTypeTokens->renderBuffer) {
        return new HdStRenderBuffer(&_hgiGL, bprimId);
    } else {
        TF_CODING_ERROR("Unknown Bprim Type %s", typeId.GetText());
    }

    return nullptr;
}

HdBprim *
HdStRenderDelegate::CreateFallbackBprim(TfToken const& typeId)
{
    if (typeId == HdPrimTypeTokens->texture) {
        return new HdStTexture(SdfPath::EmptyPath());
    } else if (typeId == _tokens->openvdbAsset) {
        return new HdStField(SdfPath::EmptyPath(), typeId);
    } else if (typeId == HdPrimTypeTokens->renderBuffer) {
        return new HdStRenderBuffer(&_hgiGL, SdfPath::EmptyPath());
    } else {
        TF_CODING_ERROR("Unknown Bprim Type %s", typeId.GetText());
    }

    return nullptr;
}

void
HdStRenderDelegate::DestroyBprim(HdBprim *bPrim)
{
    delete bPrim;
}

HdSprim *
HdStRenderDelegate::_CreateFallbackMaterialPrim()
{
    HioGlslfxSharedPtr glslfx(
        new HioGlslfx(HdStPackageFallbackSurfaceShader()));

    HdStSurfaceShaderSharedPtr fallbackShaderCode(new HdStGLSLFXShader(glslfx));

    HdStMaterial *material = new HdStMaterial(SdfPath::EmptyPath());
    material->SetSurfaceShader(fallbackShaderCode);

    return material;
}

void
HdStRenderDelegate::CommitResources(HdChangeTracker *tracker)
{
    GLF_GROUP_FUNCTION();
    
    // --------------------------------------------------------------------- //
    // RESOLVE, COMPUTE & COMMIT PHASE
    // --------------------------------------------------------------------- //
    // All the required input data is now resident in memory, next we must:
    //
    //     1) Execute compute as needed for normals, tessellation, etc.
    //     2) Commit resources to the GPU.
    //     3) Update any scene-level acceleration structures.

    // Commit all pending source data.
    _resourceRegistry->Commit();

    if (tracker->IsGarbageCollectionNeeded()) {
        _resourceRegistry->GarbageCollect();
        tracker->ClearGarbageCollectionNeeded();
    }

    // see bug126621. currently dispatch buffers need to be released
    //                more frequently than we expect.
    _resourceRegistry->GarbageCollectDispatchBuffers();
}

bool
HdStRenderDelegate::IsSupported()
{
#if defined(ARCH_GFX_METAL)
    if (MtlfContextCaps::GetAPIVersion() >= 400)
        return true;
#endif
#if defined(ARCH_GFX_OPENGL)
    if (GlfContextCaps::GetAPIVersion() >= 400)
        return true;
#endif
    return false;
}

TfTokenVector
HdStRenderDelegate::GetShaderSourceTypes() const
{
    return {HioGlslfxTokens->glslfx};
}

TfToken 
HdStRenderDelegate::GetMaterialNetworkSelector() const
{
    return HioGlslfxTokens->glslfx;
}

Hgi*
HdStRenderDelegate::GetHgi()
{
    return &_hgiGL;
}

PXR_NAMESPACE_CLOSE_SCOPE
