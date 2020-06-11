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
#include "pxr/imaging/glf/glew.h"
#include "pxr/imaging/glf/glContext.h"

#include "pxr/imaging/hdSt/drawTarget.h"
#include "pxr/imaging/hdSt/drawTargetAttachmentDescArray.h"
#include "pxr/imaging/hdSt/drawTargetTextureResource.h"
#include "pxr/imaging/hdSt/resourceFactory.h"
#include "pxr/imaging/hdSt/glConversions.h"
#include "pxr/imaging/hdSt/hgiConversions.h"
#include "pxr/imaging/hdSt/resourceRegistry.h"
#include "pxr/imaging/hdSt/dynamicUvTextureObject.h"
#include "pxr/imaging/hdSt/subtextureIdentifier.h"

#include "pxr/imaging/hd/camera.h"
#include "pxr/imaging/hd/engine.h"
#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/sceneDelegate.h"
#include "pxr/imaging/hd/sprim.h"

#include "pxr/base/tf/envSetting.h"
#include "pxr/base/tf/stl.h"
#include "pxr/base/gf/vec2f.h"
#include "pxr/base/gf/vec3f.h"

PXR_NAMESPACE_OPEN_SCOPE


TF_DEFINE_ENV_SETTING(HDST_USE_STORM_TEXTURE_SYSTEM_FOR_DRAW_TARGETS, false,
                      "Use Storm texture system for draw targets.");

TF_DEFINE_ENV_SETTING(HDST_DRAW_TARGETS_NUM_SAMPLES, 4,
                      "Number of samples, greater than 1 forces MSAA.");

TF_DEFINE_PUBLIC_TOKENS(HdStDrawTargetTokens, HDST_DRAW_TARGET_TOKENS);

HdStDrawTarget::HdStDrawTarget(SdfPath const &id)
    : HdSprim(id)
    , _version(1) // Clients tacking start at 0.
    , _enabled(true)
    , _resolution(512, 512)
    , _depthClearValue(1.0)
    , _texturesDirty(true)
{
}

HdStDrawTarget::~HdStDrawTarget() = default;

/*static*/
bool
HdStDrawTarget::GetUseStormTextureSystem()
{
    static bool result =
        TfGetEnvSetting(HDST_USE_STORM_TEXTURE_SYSTEM_FOR_DRAW_TARGETS);
    return result;
}

static
HgiSampleCount
_GetSampleCountUncached()
{
    const int c = TfGetEnvSetting(HDST_DRAW_TARGETS_NUM_SAMPLES);
    switch(c) {
    case 1:
        return HgiSampleCount1;
    case 4:
        return HgiSampleCount4;
    case 16:
        return HgiSampleCount16;
    default:
        TF_RUNTIME_ERROR(
            "Unsupported value %d for HDST_DRAW_TARGETS_NUM_SAMPLES", c);
        return HgiSampleCount4;
    }
}

// How many MSAA samples to use (1 means no MSAA)
static
HgiSampleCount
_GetSampleCount()
{
    // Cached so we only getting the runtime error once.
    static const HgiSampleCount result = _GetSampleCountUncached();
    return result;

}

/*virtual*/
void
HdStDrawTarget::Sync(HdSceneDelegate *sceneDelegate,
                     HdRenderParam   *renderParam,
                     HdDirtyBits     *dirtyBits)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    TF_UNUSED(renderParam);

    SdfPath const &id = GetId();
    if (!TF_VERIFY(sceneDelegate != nullptr)) {
        return;
    }

    const HdDirtyBits bits = *dirtyBits;

    if (bits & DirtyDTEnable) {
        VtValue vtValue =  sceneDelegate->Get(id, HdStDrawTargetTokens->enable);

        // Optional attribute.
        _enabled = vtValue.GetWithDefault<bool>(true);
    }

    if (bits & DirtyDTCamera) {
        VtValue vtValue =  sceneDelegate->Get(id, HdStDrawTargetTokens->camera);
        _cameraId = vtValue.Get<SdfPath>();
        _renderPassState.SetCamera(_cameraId);
    }

    if (bits & DirtyDTResolution) {
        VtValue vtValue =
                        sceneDelegate->Get(id, HdStDrawTargetTokens->resolution);

        _resolution = vtValue.Get<GfVec2i>();

        if (GetUseStormTextureSystem()) {
            _texturesDirty = true;
        } else {
            // No point in Resizing the textures if new ones are going to
            // be created (see _SetAttachments())
            if (_drawTarget && ((bits & DirtyDTAttachment) == Clean)) {
                _ResizeDrawTarget();
            }
        }
    }

    if (bits & DirtyDTAttachment) {
        // Depends on resolution being set correctly.
        VtValue vtValue =
                       sceneDelegate->Get(id, HdStDrawTargetTokens->attachments);


        const HdStDrawTargetAttachmentDescArray &attachments =
            vtValue.GetWithDefault<HdStDrawTargetAttachmentDescArray>(
                    HdStDrawTargetAttachmentDescArray());

        if (GetUseStormTextureSystem()) {
            _SetAttachmentData(sceneDelegate, attachments);
        } else {
            _SetAttachments(sceneDelegate, attachments);
        }
    }

    if (bits & DirtyDTDepthClearValue) {
        VtValue vtValue =
                  sceneDelegate->Get(id, HdStDrawTargetTokens->depthClearValue);

        _depthClearValue = vtValue.GetWithDefault<float>(1.0f);

        if (GetUseStormTextureSystem()) {
            _SetAttachmentDataDepthClearValue();
        }

        _renderPassState.SetDepthClearValue(_depthClearValue);
    }

    if (bits & DirtyDTCollection) {
        VtValue vtValue =
                       sceneDelegate->Get(id, HdStDrawTargetTokens->collection);

        HdRprimCollection collection = vtValue.Get<HdRprimCollection>();

        TfToken const &collectionName = collection.GetName();

        HdChangeTracker& changeTracker =
                         sceneDelegate->GetRenderIndex().GetChangeTracker();

        if (_collection.GetName() != collectionName) {
            // Make sure collection has been added to change tracker
            changeTracker.AddCollection(collectionName);
        }

        // Always mark collection dirty even if added - as we don't
        // know if this is a re-add.
        changeTracker.MarkCollectionDirty(collectionName);

        _renderPassState.SetRprimCollection(collection);
        _collection = collection;
    }

    *dirtyBits = Clean;
}

// virtual
HdDirtyBits
HdStDrawTarget::GetInitialDirtyBitsMask() const
{
    return AllDirty;
}

bool
HdStDrawTarget::WriteToFile(const HdRenderIndex &renderIndex,
                           const std::string &attachment,
                           const std::string &path) const
{
    HF_MALLOC_TAG_FUNCTION();

    // Check the draw target's been allocated
#if defined(PXR_OPENGL_SUPPORT_ENABLED)
    if (!_drawTarget ||
        (HdStResourceFactory::GetInstance()->IsOpenGL()
          && !_drawTargetContextGL)) {
        TF_WARN("Missing draw target");
        return false;
    }
#endif

    // XXX: The GarchDrawTarget will throw an error if attachment is invalid,
    // so need to check that it is valid first.
    //
    // This ends in a double-search of the map, but this path is for
    // debug and testing and not meant to be a performance path.
    if (!_drawTarget->GetAttachment(attachment)) {
        TF_WARN("Missing attachment\n");
        return false;
    }

    const HdCamera * const camera = _GetCamera(renderIndex);
    if (camera == nullptr) {
        TF_WARN("Missing camera\n");
        return false;
    }


    // embed camera matrices into metadata
    const GfMatrix4d &viewMatrix = camera->GetViewMatrix();
    const GfMatrix4d &projMatrix = camera->GetProjectionMatrix();

#if defined(PXR_OPENGL_SUPPORT_ENABLED)
    GlfGLContextSharedPtr oldContext;
    if (HdStResourceFactory::GetInstance()->IsOpenGL())
    {
        // Make sure all draw target operations happen on the same
        // context.
        oldContext = GlfGLContext::GetCurrentGLContext();
        GlfGLContext::MakeCurrent(_drawTargetContextGL);
    }
#endif
    const bool result = _drawTarget->WriteToFile(attachment, path,
                                                 viewMatrix, projMatrix);
#if defined(PXR_OPENGL_SUPPORT_ENABLED)
    if (HdStResourceFactory::GetInstance()->IsOpenGL())
        GlfGLContext::MakeCurrent(oldContext);
#endif

    return result;
}

HdStTextureIdentifier
HdStDrawTarget::GetTextureIdentifier(
    const std::string &attachmentName,
    const HdSceneDelegate * const sceneDelegate,
    const bool multiSampled) const
{
    // Create an ID that is unique to:
    // - the draw target
    // - the attachment
    // - the MSAA vs resolved texture
    // - the scene delegate (texture object registry is shared across scene
    //   delegates, so the above could result in name space collision)
    std::string idStr =
        TfStringPrintf("[%p] ", sceneDelegate);
    idStr += GetId().GetString();
    idStr += " attachment: " + attachmentName;
    if (multiSampled) {
        idStr += " [MSAA]";
    }
    
    return HdStTextureIdentifier(
        TfToken(idStr),
        // Tag as texture not being loaded from an asset by
        // texture registry but populated by us, the draw target.
        std::make_unique<HdStDynamicUvSubtextureIdentifier>());
}

// Clear values are always vec4f in HgiGraphicsCmdDesc.
static
GfVec4f _ToVec4f(const VtValue &v)
{
    if (v.IsHolding<float>()) {
        const float val = v.UncheckedGet<float>();
        return GfVec4f(val);
    }
    if (v.IsHolding<double>()) {
        const double val = v.UncheckedGet<double>();
        return GfVec4f(val);
    }
    if (v.IsHolding<GfVec2f>()) {
        const GfVec2f val = v.UncheckedGet<GfVec2f>();
        return GfVec4f(val[0], val[1], 0.0, 1.0);
    }
    if (v.IsHolding<GfVec2d>()) {
        const GfVec2d val = v.UncheckedGet<GfVec2d>();
        return GfVec4f(val[0], val[1], 0.0, 1.0);
    }
    if (v.IsHolding<GfVec3f>()) {
        const GfVec3f val = v.UncheckedGet<GfVec3f>();
        return GfVec4f(val[0], val[1], val[2], 1.0);
    }
    if (v.IsHolding<GfVec3d>()) {
        const GfVec3d val = v.UncheckedGet<GfVec3d>();
        return GfVec4f(val[0], val[1], val[2], 1.0);
    }
    if (v.IsHolding<GfVec4f>()) {
        return v.UncheckedGet<GfVec4f>();
    }
    if (v.IsHolding<GfVec4d>()) {
        return GfVec4f(v.UncheckedGet<GfVec4d>());
    }

    TF_CODING_ERROR("Unsupported clear value for draw target attachment.");
    return GfVec4f(0.0);
}

HdStDynamicUvTextureObjectSharedPtr
HdStDrawTarget::_CreateTextureObject(
    const std::string &name,
    HdSceneDelegate * const sceneDelegate,
    HdStResourceRegistry * const resourceRegistry,
    bool multiSampled)
{
    if (multiSampled && _GetSampleCount() == HgiSampleCount1) {
        // Do not allocate an MSAA texture when not using MSAA.
        return nullptr;
    }
    
    // Allocate texture object (the actual GPU resource is allocated later).
    return
        std::static_pointer_cast<HdStDynamicUvTextureObject>(
            resourceRegistry->AllocateTextureObject(
                GetTextureIdentifier(name, sceneDelegate, multiSampled),
                HdTextureType::Uv));
}

void
HdStDrawTarget::_SetAttachmentData(
    HdSceneDelegate * const sceneDelegate,
    const HdStDrawTargetAttachmentDescArray &attachments)
{
    _attachmentDataVector.clear();

    HdStResourceRegistry * const resourceRegistry =
        static_cast<HdStResourceRegistry *>(
            sceneDelegate->GetRenderIndex().GetResourceRegistry().get());
    
    for (size_t i = 0; i < attachments.GetNumAttachments(); i++) {
        const HdStDrawTargetAttachmentDesc &desc = attachments.GetAttachment(i);

        _attachmentDataVector.push_back(
            { desc.GetName(),
              _ToVec4f(desc.GetClearColor()),
              desc.GetFormat(),
              _CreateTextureObject(
                  desc.GetName(),
                  sceneDelegate,
                  resourceRegistry,
                  /* multiSampled = */ false),
              _CreateTextureObject(
                  desc.GetName(),
                  sceneDelegate,
                  resourceRegistry,
                  /* multiSampled = */ true) });
    }
    
    // We always need a depth attachment and it is not among the attachments
    // in the attachment descriptor.
    _attachmentDataVector.push_back(
        { HdStDrawTargetTokens->depth.GetString(),
          GfVec4f(_depthClearValue),
          HdFormatFloat32,
          _CreateTextureObject(
              HdStDrawTargetTokens->depth.GetString(),
              sceneDelegate,
              resourceRegistry,
              /* multiSampled = */ false),
          _CreateTextureObject(
              HdStDrawTargetTokens->depth.GetString(),
              sceneDelegate,
              resourceRegistry,
              /* multiSampled = */ true) });
 
    _texturesDirty = true;
}

void
HdStDrawTarget::_SetAttachmentDataDepthClearValue()
{
    // Find the depth attachment ...
    for (AttachmentData &data : _attachmentDataVector) {
        if (data.name == HdStDrawTargetTokens->depth.GetString()) {
            // ... and set its clear value.
            data.clearValue = GfVec4f(_depthClearValue);
        }
    }
}

// Debug name for texture.
static
std::string
_GetDebugName(const HdStDynamicUvTextureObjectSharedPtr &textureObject)
{
    return textureObject->GetTextureIdentifier().GetFilePath().GetString();
}

static
HgiTextureDesc
_GetTextureDescriptor(const HdStDrawTarget::AttachmentData &data,
                      const GfVec2i &resolution,
                      const bool multiSample)
{
    HgiTextureDesc desc;
    desc.debugName =
        _GetDebugName(multiSample ? data.textureMSAA : data.texture);
    desc.format = HdStHgiConversions::GetHgiFormat(data.format);
    desc.type = HgiTextureType2D;
    desc.dimensions = GfVec3i(resolution[0], resolution[1], 1);
    if (data.name == HdStDrawTargetTokens->depth.GetString()) {
        desc.usage = HgiTextureUsageBitsDepthTarget;
    } else {
        desc.usage = HgiTextureUsageBitsColorTarget;
    }
 
    if (multiSample) {
        desc.sampleCount = _GetSampleCount();
    }

    return desc;
}

void
HdStDrawTarget::AllocateTexturesIfNecessary()
{
    if (!_texturesDirty) {
        return;
    }

    for (const AttachmentData &data : _attachmentDataVector) {        
        data.texture->CreateTexture(
            _GetTextureDescriptor(
                data, _resolution, /* multiSample = */ false));

        if (data.textureMSAA) {
            data.textureMSAA->CreateTexture(
                _GetTextureDescriptor(
                    data, _resolution, /* multiSample = */ true));
        }
    }

    _texturesDirty = false;
}

void
HdStDrawTarget::_SetAttachments(
                           HdSceneDelegate *sceneDelegate,
                           const HdStDrawTargetAttachmentDescArray &attachments)
{
    HF_MALLOC_TAG_FUNCTION();
#if defined(PXR_OPENGL_SUPPORT_ENABLED)
    if (HdStResourceFactory::GetInstance()->IsOpenGL()) {
        if (!_drawTargetContextGL) {
            // Use one of the shared contexts as the master.
            _drawTargetContextGL = GlfGLContext::GetSharedGLContext();
        }
    }
#endif
    // Clear out old texture resources for the attachments.
    _colorTextureResourceHandles.clear();
    _depthTextureResourceHandle.reset();

#if defined(PXR_OPENGL_SUPPORT_ENABLED)
    GlfGLContextSharedPtr oldContext;

    // Make sure all draw target operations happen on the same
    // context.
    if (HdStResourceFactory::GetInstance()->IsOpenGL()) {
        oldContext = GlfGLContext::GetCurrentGLContext();
        GlfGLContext::MakeCurrent(_drawTargetContextGL);
    }
#endif

    if (_drawTarget) {
        // If we had a prior draw target, we need to garbage collect
        // to clean up it's resources.
        HdChangeTracker& changeTracker =
                         sceneDelegate->GetRenderIndex().GetChangeTracker();

        changeTracker.SetGarbageCollectionNeeded();
    }

    // XXX: Discard old draw target and create a new one
    // This is necessary because a we have to clone the draw target into each
    // gl context.
    // XXX : All draw targets in Storm are currently trying to create MSAA
    // buffers (as long as they are allowed by the environment variables) 
    // because we need alpha to coverage for transparent object.
    _drawTarget = GarchDrawTarget::New(_resolution, /* MSAA */ true);

    size_t numAttachments = attachments.GetNumAttachments();
    _renderPassState.SetNumColorAttachments(numAttachments);

    _colorTextureResourceHandles.resize(numAttachments);

    std::vector<GarchDrawTarget::AttachmentDesc> attachmentDesc;
    for (size_t attachmentNum = 0; attachmentNum < numAttachments;
         ++attachmentNum) {
        const HdStDrawTargetAttachmentDesc &desc =
        attachments.GetAttachment(attachmentNum);
        
        GLenum format = GL_RGBA;
        GLenum type   = GL_BYTE;
        GLenum internalFormat = GL_RGBA8;
        HdStGLConversions::GetGlFormat(desc.GetFormat(),
                                       &format, &type, &internalFormat);
        
        const std::string &name = desc.GetName();
        attachmentDesc.push_back(
            GarchDrawTarget::AttachmentDesc(name, format, type, internalFormat));
    }

    // Always add depth texture
    // XXX: GarchDrawTarget requires the depth texture be added last,
    // otherwise the draw target indexes are off-by-1.
    attachmentDesc.push_back(
         GarchDrawTarget::AttachmentDesc(HdStDrawTargetTokens->depth.GetString(),
                                         GL_DEPTH_COMPONENT,
                                         GL_FLOAT,
                                         GL_DEPTH_COMPONENT32F));
    _drawTarget->SetAttachments(attachmentDesc);
    _drawTarget->Bind();
    
    _RegisterTextureResourceHandle(sceneDelegate,
                             HdStDrawTargetTokens->depth.GetString(),
                             &_depthTextureResourceHandle);

    HdSt_DrawTargetTextureResource *depthResource =
    static_cast<HdSt_DrawTargetTextureResource *>(
        _depthTextureResourceHandle->GetTextureResource().get());
    
    depthResource->SetAttachment(_drawTarget->GetAttachment(HdStDrawTargetTokens->depth.GetString()));
    depthResource->SetSampler(attachments.GetDepthWrapS(),
                              attachments.GetDepthWrapT(),
                              attachments.GetDepthMinFilter(),
                              attachments.GetDepthMagFilter());

    for (size_t attachmentNum = 0; attachmentNum < numAttachments;
                                                              ++attachmentNum) {
      const HdStDrawTargetAttachmentDesc &desc =
                                       attachments.GetAttachment(attachmentNum);

        GLenum format = GL_RGBA;
        GLenum type   = GL_BYTE;
        GLenum internalFormat = GL_RGBA8;
        HdStGLConversions::GetGlFormat(desc.GetFormat(),
                                   &format, &type, &internalFormat);

        const std::string &name = desc.GetName();

        _renderPassState.SetColorClearValue(attachmentNum, desc.GetClearColor());

        _RegisterTextureResourceHandle(sceneDelegate,
                                 name,
                                 &_colorTextureResourceHandles[attachmentNum]);

        HdSt_DrawTargetTextureResource *resource =
                static_cast<HdSt_DrawTargetTextureResource *>(
                    _colorTextureResourceHandles[attachmentNum]->
                        GetTextureResource().get());

        resource->SetAttachment(_drawTarget->GetAttachment(name));
        resource->SetSampler(desc.GetWrapS(),
                             desc.GetWrapT(),
                             desc.GetMinFilter(),
                             desc.GetMagFilter());

    }

    _drawTarget->Unbind();

	_renderPassState.SetDepthPriority(attachments.GetDepthPriority());

#if defined(PXR_OPENGL_SUPPORT_ENABLED)
    if (HdStResourceFactory::GetInstance()->IsOpenGL())
        GlfGLContext::MakeCurrent(oldContext);
#endif

   // The texture bindings have changed so increment the version
   ++_version;
}


const HdCamera *
HdStDrawTarget::_GetCamera(const HdRenderIndex &renderIndex) const
{
    return static_cast<const HdCamera *>(
            renderIndex.GetSprim(HdPrimTypeTokens->camera, _cameraId));
}

void
HdStDrawTarget::_ResizeDrawTarget()
{
    HF_MALLOC_TAG_FUNCTION();
    
#if defined(PXR_OPENGL_SUPPORT_ENABLED)
    // Make sure all draw target operations happen on the same
    // context.
    GlfGLContextSharedPtr oldContext;
    
    if (HdStResourceFactory::GetInstance()->IsOpenGL()) {
        oldContext = GlfGLContext::GetCurrentGLContext();
        GlfGLContext::MakeCurrent(_drawTargetContextGL);
    }
#endif
    _drawTarget->Bind();
    _drawTarget->SetSize(_resolution);
    _drawTarget->Unbind();

    // The texture bindings might have changed so increment the version
    ++_version;

#if defined(PXR_OPENGL_SUPPORT_ENABLED)
    if (HdStResourceFactory::GetInstance()->IsOpenGL())
        GlfGLContext::MakeCurrent(oldContext);
#endif
}

void
HdStDrawTarget::_RegisterTextureResourceHandle(
        HdSceneDelegate *sceneDelegate,
        const std::string &name,
        HdStTextureResourceHandleSharedPtr *handlePtr)
{
    HF_MALLOC_TAG_FUNCTION();

    HdStResourceRegistrySharedPtr const& resourceRegistry =
         std::static_pointer_cast<HdStResourceRegistry>(
             sceneDelegate->GetRenderIndex().GetResourceRegistry());

    // Create Path for the texture resource
    SdfPath resourcePath = GetId().AppendProperty(TfToken(name));

    // Ask delegate for an ID for this tex
    HdTextureResource::ID texID =
                              sceneDelegate->GetTextureResourceID(resourcePath);

    // Use render index to convert local texture id into global
    // texture key.  This is because the instance registry is shared by
    // multiple render indexes, but the scene delegate generated
    // texture id's are only unique to the scene.  (i.e. two draw
    // targets at the same path in the scene are likely to produce the
    // same texture id, even though they refer to textures on different
    // render indexes).
    HdRenderIndex &renderIndex = sceneDelegate->GetRenderIndex();
    HdResourceRegistry::TextureKey texKey = renderIndex.GetTextureKey(texID);


    // Add to resource registry
    HdInstance<HdStTextureResourceSharedPtr> texInstance =
                resourceRegistry->RegisterTextureResource(texKey);

    if (texInstance.IsFirstInstance()) {
        texInstance.SetValue(HdStResourceFactory::GetInstance()->NewDrawTargetTextureResource());
    }

    HdStTextureResourceSharedPtr texResource = texInstance.GetValue();

    HdResourceRegistry::TextureKey handleKey =
        HdStTextureResourceHandle::GetHandleKey(&renderIndex, resourcePath);
    HdInstance<HdStTextureResourceHandleSharedPtr> handleInstance =
                resourceRegistry->RegisterTextureResourceHandle(handleKey);
    if (handleInstance.IsFirstInstance()) {
        handleInstance.SetValue(HdStTextureResourceHandleSharedPtr(   
                                          new HdStTextureResourceHandle(
                                              texResource)));
    } else {
        handleInstance.GetValue()->SetTextureResource(texResource);
    }
    *handlePtr = handleInstance.GetValue();
}


/*static*/
void
HdStDrawTarget::GetDrawTargets(HdRenderIndex* const renderIndex,
                               HdStDrawTargetPtrVector * const drawTargets)
{
    HF_MALLOC_TAG_FUNCTION();

    if (!renderIndex->IsSprimTypeSupported(HdPrimTypeTokens->drawTarget)) {
        return;
    }

    const SdfPathVector paths = renderIndex->GetSprimSubtree(
        HdPrimTypeTokens->drawTarget, SdfPath::AbsoluteRootPath());

    for (const SdfPath &path : paths) {
        if (HdSprim * const drawTarget =
                renderIndex->GetSprim(HdPrimTypeTokens->drawTarget, path)) {
            drawTargets->push_back(static_cast<HdStDrawTarget *>(drawTarget));
        }
    }
}

PXR_NAMESPACE_CLOSE_SCOPE

