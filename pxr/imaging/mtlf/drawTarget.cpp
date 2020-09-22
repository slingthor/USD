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
// mtl/drawTarget.cpp
//
#include "pxr/imaging/mtlf/mtlDevice.h"

#include "pxr/imaging/mtlf/drawTarget.h"
#include "pxr/imaging/mtlf/diagnostic.h"
#include "pxr/imaging/mtlf/utils.h"

#include "pxr/imaging/hgiMetal/hgi.h"
#include "pxr/imaging/hgiMetal/texture.h"

#include "pxr/imaging/garch/image.h"
#include "pxr/imaging/garch/utils.h"

#include "pxr/base/tf/stringUtils.h"
#include "pxr/base/tf/envSetting.h"

#include "pxr/base/trace/trace.h"

#include <boost/shared_ptr.hpp>

PXR_NAMESPACE_OPEN_SCOPE


TF_DEFINE_ENV_SETTING(MTLF_DRAW_TARGETS_NUM_SAMPLES, 4,
                      "Number of samples greater than 1 forces MSAA.");

static unsigned int 
_GetNumSamples()
{
    static int reqNumSamples = TfGetEnvSetting(MTLF_DRAW_TARGETS_NUM_SAMPLES);
    unsigned int numSamples = 1;
    if (reqNumSamples > 1) {
        numSamples = (reqNumSamples & (reqNumSamples - 1)) ? 1 : reqNumSamples;    
    }
    return numSamples;
}

MtlfDrawTarget*
MtlfDrawTarget::New( GfVec2i const & size, bool requestMSAA )
{
    return new This(size, requestMSAA);
}

MtlfDrawTarget::MtlfDrawTarget( GfVec2i const & size, bool requestMSAA /* =false */) :
    _bindDepth(0),
    _size(size),
    _numSamples(1)
{
    // If MSAA has been requested and it is enabled then we will create
    // msaa buffers
    if (requestMSAA) {
        _numSamples = _GetNumSamples();
    }

    rpd = [[MTLRenderPassDescriptor alloc] init];
    
    _GenFrameBuffer();
}

MtlfDrawTarget*
MtlfDrawTarget::New( MtlfDrawTargetPtr const & drawtarget )
{
    return new This(drawtarget);
}

// clone constructor : generates a new GL framebuffer, but share the texture
// attachments.
MtlfDrawTarget::MtlfDrawTarget( GarchDrawTargetPtr const & drawtarget ) :
    _bindDepth(0),
    _size(drawtarget->GetSize()),
    _numSamples(drawtarget->GetNumSamples())
{
    _GenFrameBuffer();

    // share the RefPtr to the map of attachments
    _attachmentsPtr = drawtarget->_attachmentsPtr;

    Bind();

    // attach the textures to the correct framebuffer mount points
    for (AttachmentsMap::value_type const& p :  _attachmentsPtr->attachments) {
        _BindAttachment( TfStatic_cast<TfRefPtr<MtlfDrawTarget::MtlfAttachment>>(p.second) );
    }

    Unbind();
}

MtlfDrawTarget::~MtlfDrawTarget( )
{
    _DeleteAttachments();
    [rpd release];
    rpd = nil;
}

void
MtlfDrawTarget::_AddAttachment( std::string const & name,
                                GLenum format, GLenum type,
                                GLenum internalFormat )
{
    if (IsBound()) {
        TF_CODING_ERROR("Cannot change the size of a bound MtlfDrawTarget");
    }

    AttachmentsMap & attachments = _GetAttachments();
    AttachmentsMap::iterator it = attachments.find( name );

    if (it==attachments.end()) {

        MtlfDrawTarget::MtlfAttachment::MtlfAttachmentRefPtr attachment =
            MtlfAttachment::New((uint32_t)attachments.size(),
                                format, type, _size, _numSamples);

        attachments.insert(AttachmentsMap::value_type(name, attachment));


        TF_VERIFY( attachment->GetTextureName().IsSet() , "%s",
                   std::string("Attachment \""+name+"\" was not added "
                       "and cannot be bound in MatDisplayMaterial").c_str());

        _BindAttachment( attachment );

    } else {
        TF_CODING_ERROR( "Attachment \""+name+"\" already exists for this "
                         "DrawTarget" );
    }
}

GarchDrawTarget::AttachmentRefPtr
MtlfDrawTarget::GetAttachment(std::string const & name)
{
    AttachmentsMap & attachments = _GetAttachments();
    AttachmentsMap::iterator it = attachments.find( name );

    if (it!=attachments.end()) {
        return it->second;
    } else {
        return TfNullPtr;
    }
}

void 
MtlfDrawTarget::ClearAttachments()
{
    _DeleteAttachments();
}

void
MtlfDrawTarget::CloneAttachments( GarchDrawTargetPtr const & drawtarget )
{
    if (!drawtarget) {
        TF_CODING_ERROR( "Cannot clone TfNullPtr attachments." );
    }

    // garbage collection will take care of the existing instance pointed to
    // by the RefPtr
    _attachmentsPtr = drawtarget->_attachmentsPtr;

    for (AttachmentsMap::value_type const& p : _attachmentsPtr->attachments) {
        _BindAttachment(
            TfStatic_cast<TfRefPtr<MtlfDrawTarget::MtlfAttachment>>(p.second) );
    }
}

GarchDrawTarget::AttachmentsMap const &
MtlfDrawTarget::GetAttachments() const
{
    return _GetAttachments();
}

GarchDrawTarget::AttachmentsMap &
MtlfDrawTarget::_GetAttachments() const
{
    TF_VERIFY( _attachmentsPtr,
        "DrawTarget has uninitialized attachments map.");

    return _attachmentsPtr->attachments;
}

void
MtlfDrawTarget::SetSize( GfVec2i size )
{
    if (size==_size) {
        return;
    }

    if (!IsBound()) {
        TF_CODING_ERROR( "Cannot change the size of an unbound DrawTarget" );
    }

    _size = size;

    AttachmentsMap & attachments = _GetAttachments();

    for (AttachmentsMap::value_type const& p :  attachments) {
        AttachmentRefPtr var = p.second;

        var->ResizeTexture(_size);

        _BindAttachment(TfStatic_cast<TfRefPtr<MtlfDrawTarget::MtlfAttachment>>(var));
    }
}

void
MtlfDrawTarget::_DeleteAttachments()
{
    // Can't delete the attachment textures while someone else is still holding
    // onto them.
    // XXX This code needs refactoring so that Attachment & AttachmentsContainer
    // own the methods over their data (with casccading calls coming from the
    // DrawTarget API). Checking for the RefPtr uniqueness is somewhat working
    // against the nature of RefPtr..
    if (!_attachmentsPtr->IsUnique()) {
        return;
    }

    AttachmentsMap & attachments = _GetAttachments();

    attachments.clear();
}

static int _GetMaxAttachments( )
{
    int maxAttach = 8;
    return maxAttach;
}

void 
MtlfDrawTarget::_GenFrameBuffer()
{
    _desc.reset(new HgiGraphicsCmdsDesc);
}

// Attach a texture to one of the attachment points of the framebuffer.
// We assume that the framebuffer is currently bound !
void
MtlfDrawTarget::_BindAttachment( MtlfAttachmentRefPtr const & a )
{
    HgiTextureHandle tid = a->GetHgiTextureName();
    HgiTextureHandle tidMS = a->GetTextureMSName();

    int attach = a->GetAttach();
    HgiAttachmentDesc attachmentDesc;

    _desc->width = tid->GetDescriptor().dimensions[0];
    _desc->height = tid->GetDescriptor().dimensions[1];

    if (a->GetFormat()==GL_DEPTH_COMPONENT || a->GetFormat()==GL_DEPTH_STENCIL) {
        
        if (HasMSAA()) {
            _desc->depthTexture = tidMS;
            _desc->depthResolveTexture = tid;
        } else {
            _desc->depthTexture = tid;
        }
        attachmentDesc.format = tid->GetDescriptor().format;
        
        // make sure to clear every frame for best performance
        attachmentDesc.loadOp = HgiAttachmentLoadOpClear;
        attachmentDesc.clearValue = GfVec4f(1.0f);
        
        // store only attachments that will be presented to the screen, as in this case
        attachmentDesc.storeOp = HgiAttachmentStoreOpStore;
        
//        if (a->GetFormat()==GL_DEPTH_STENCIL) {
//            MTLRenderPassStencilAttachmentDescriptor *stencilAttachment = _mtlRenderPassDescriptor.stencilAttachment;
//
//            if (HasMSAA()) {
//                stencilAttachment.texture = a->GetStencilTextureMSName();
//            } else {
//                stencilAttachment.texture = a->GetStencilTextureName();
//            }
//
//            // make sure to clear every frame for best performance
//            stencilAttachment.loadAction = MTLLoadActionClear;
//            stencilAttachment.clearStencil = 0;
//            stencilAttachment.storeAction = MTLStoreActionStore;
//        }
        _desc->depthAttachmentDesc = std::move(attachmentDesc);
    } else {
        if (attach < 0) {
            TF_CODING_ERROR("Attachment index cannot be negative");
            return;
        }

        TF_VERIFY( attach < _GetMaxAttachments(),
            "Exceeding number of Attachments available ");
        
        if (attach >= _desc->colorTextures.size()) {
            _desc->colorTextures.resize(attach + 1);
            _desc->colorAttachmentDescs.resize(attach + 1);
            if (HasMSAA()) {
                _desc->colorResolveTextures.resize(attach + 1);
            }
        }
        
        if (HasMSAA()) {
            _desc->colorTextures[attach] = tidMS;
            _desc->colorResolveTextures[attach] = tid;
        } else {
            _desc->colorTextures[attach] = tid;
        }
        
        attachmentDesc.format = tid->GetDescriptor().format;

        attachmentDesc.storeOp = HgiAttachmentStoreOpStore;

        // make sure to clear every frame for best performance
        attachmentDesc.loadOp = HgiAttachmentLoadOpClear;
        attachmentDesc.clearValue = GfVec4f(1.0f, 1.0f, 1.0f, 1.0f);
        
        _desc->colorAttachmentDescs[attach] = attachmentDesc;
    }
}

void
MtlfDrawTarget::Bind()
{
    if (++_bindDepth != 1) {
        return;
    }

    TF_VERIFY(!GetAttachments().empty(), "No attachments set. Bind() is only valid after a call "
              "to Bind(GarchDrawTarget::AttachmentsMap const &attachments)");
    
    MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();
   
    // Begin rendering
    _gfxCmds = context->GetHgi()->CreateGraphicsCmds(*_desc.get());

    // Set the render pass descriptor to use for the render encoders
    [rpd init];

    MTLPixelFormat colorFormat = MTLPixelFormatInvalid;
    MTLPixelFormat depthFormat = MTLPixelFormatInvalid;
    size_t i;
    bool resolve = !_desc->colorResolveTextures.empty();
    for (i=0; i<_desc->colorTextures.size(); i++) {
        HgiMetalTexture *metalTexture =
            static_cast<HgiMetalTexture*>(_desc->colorTextures[i].Get());
        rpd.colorAttachments[i].texture = metalTexture->GetTextureId();
        colorFormat = rpd.colorAttachments[i].texture.pixelFormat;

        if (resolve) {
            metalTexture =
                static_cast<HgiMetalTexture*>(_desc->colorResolveTextures[i].Get());
            rpd.colorAttachments[i].resolveTexture = metalTexture->GetTextureId();
            rpd.colorAttachments[i].storeAction = MTLStoreActionStoreAndMultisampleResolve;
        }
        else {
            rpd.colorAttachments[i].storeAction = MTLStoreActionStore;
        }
        rpd.colorAttachments[i].loadAction = MTLLoadActionClear;
        GfVec4f const& clearColor = _desc->colorAttachmentDescs[i].clearValue;
        rpd.colorAttachments[i].clearColor =
            MTLClearColorMake(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
    }
    while (i < METAL_MAX_COLOR_ATTACHMENTS) {
        rpd.colorAttachments[i].texture = nil;
        rpd.colorAttachments[i].resolveTexture = nil;
        i++;
    }
    if (_desc->depthTexture) {
        HgiMetalTexture *metalTexture =
            static_cast<HgiMetalTexture*>(_desc->depthTexture.Get());
        rpd.depthAttachment.texture = metalTexture->GetTextureId();
        depthFormat = rpd.depthAttachment.texture.pixelFormat;
            
        if (resolve) {
            metalTexture =
                static_cast<HgiMetalTexture*>(_desc->depthResolveTexture.Get());
            rpd.depthAttachment.resolveTexture = metalTexture->GetTextureId();
            rpd.depthAttachment.storeAction = MTLStoreActionStoreAndMultisampleResolve;
        }
        else {
            rpd.depthAttachment.storeAction = MTLStoreActionStore;
        }
        
        rpd.depthAttachment.loadAction = MTLLoadActionClear;
        rpd.depthAttachment.clearDepth = _desc->depthAttachmentDesc.clearValue[0];
    }
    else {
        rpd.depthAttachment.texture = nil;
        rpd.depthAttachment.resolveTexture = nil;
    }
    context->SetRenderPassDescriptor(rpd);
    context->SetOutputPixelFormats(colorFormat, depthFormat);
}

void
MtlfDrawTarget::SetAttachments(std::vector<GarchDrawTarget::AttachmentDesc>& attachmentDesc)
{
    if (!TF_VERIFY(GetAttachments().empty(), "There's already attachments bound to this draw target")) {
        return;
    }

    for(auto desc : attachmentDesc) {
        _AddAttachment(desc.name, desc.format, desc.type, desc.internalFormat);
    }
}

bool
MtlfDrawTarget::IsBound() const
{
    return (_bindDepth > 0);
}

void
MtlfDrawTarget::Unbind()
{
    if (--_bindDepth != 0) {
        return;
    }
    MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();
    
    // Used to dirty the descriptor state
    context->DirtyDrawTargets();
  
    context->GetHgi()->SubmitCmds(_gfxCmds.get());

    TouchContents();
}

void 
MtlfDrawTarget::_Resolve()
{
    // Do nothing - already resolved
}

void
MtlfDrawTarget::Resolve()
{
    if (HasMSAA()) {
        _Resolve();
    }
}

/* static */
void
MtlfDrawTarget::Resolve(const std::vector<GarchDrawTarget*>& drawTargets)
{
    MtlfDrawTarget* firstDrawTarget = NULL;

    for(GarchDrawTarget* dt : drawTargets) {
        if (dt->HasMSAA()) {
            MtlfDrawTarget* metaldt = dynamic_cast<MtlfDrawTarget*>(drawTargets[0]);
            if (!firstDrawTarget) {
                // If this is the first draw target to be resolved,
                // save the old binding state.
                firstDrawTarget = dynamic_cast<MtlfDrawTarget*>(drawTargets[0]);
            }
            metaldt->_Resolve();
        }
    }
}

void
MtlfDrawTarget::TouchContents()
{
    GarchDrawTarget::AttachmentsMap const & attachments = GetAttachments();

    for (AttachmentsMap::value_type const& p :  attachments) {
        p.second->TouchContents();
    }
}

bool
MtlfDrawTarget::IsValid(std::string * reason)
{
    return _Validate(reason);
}

bool
MtlfDrawTarget::_Validate(std::string * reason)
{
    return true;
}

void
MtlfDrawTarget::GetImage(std::string const & name, void* buffer) const
{
    MtlfAttachmentRefPtr attachment = TfStatic_cast<TfRefPtr<MtlfDrawTarget::MtlfAttachment>>(_GetAttachments().at(name));

    MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();

    id<MTLDevice> device = context->currentDevice;
    HgiMetal* hgiMetal = context->GetHgi();
    
    id<MTLTexture> texture = attachment->GetTextureName();
 
    int bytesPerPixel = attachment->GetBytesPerPixel();
    int width = [texture width];
    int height = [texture height];
    MTLPixelFormat mtlFormat = [texture pixelFormat];
    MTLBlitOption blitOptions = MTLBlitOptionNone;
    if (mtlFormat == MTLPixelFormatDepth32Float_Stencil8) {
        mtlFormat = MTLPixelFormatDepth32Float;
        blitOptions = MTLBlitOptionDepthFromDepthStencil;
    }
#if defined(ARCH_OS_MACOS)
    else if (mtlFormat == MTLPixelFormatDepth24Unorm_Stencil8) {
        mtlFormat = MTLPixelFormatR32Uint; //MTL_FIXME - This might not be the right format for this texture
        bytesPerPixel = 4;
        blitOptions = MTLBlitOptionDepthFromDepthStencil;
    }
#endif
    if (mtlFormat == MTLPixelFormatDepth32Float) {
        bytesPerPixel = 4;
    }

    // While Mtlf exists, we need to force a flush and generation of a new
    // command buffer, to ensure the blit happens after any work Mtlf has
    // queued
    hgiMetal->CommitPrimaryCommandBuffer(HgiMetal::CommitCommandBuffer_NoWait, true);
    
    id<MTLCommandBuffer> commandBuffer = hgiMetal->GetPrimaryCommandBuffer();
    id<MTLBlitCommandEncoder> blitEncoder =
        [commandBuffer blitCommandEncoder];
    
    id<MTLBuffer> const &cpuBuffer =
        context->GetMetalBuffer((bytesPerPixel * width * height),
                                MTLResourceStorageModeShared);
    
    [blitEncoder copyFromTexture:texture
                     sourceSlice:0
                     sourceLevel:0
                    sourceOrigin:MTLOriginMake(0, 0, 0)
                      sourceSize:MTLSizeMake(width, height, 1)
                        toBuffer:cpuBuffer
               destinationOffset:0
          destinationBytesPerRow:(bytesPerPixel * width)
        destinationBytesPerImage:(bytesPerPixel * width * height)
                         options:blitOptions];

    [blitEncoder endEncoding];

    hgiMetal->CommitPrimaryCommandBuffer(
        HgiMetal::CommitCommandBuffer_WaitUntilCompleted);

    memcpy(buffer, [cpuBuffer contents], bytesPerPixel * width * height);
	context->ReleaseMetalBuffer(cpuBuffer);
}

bool
MtlfDrawTarget::WriteToFile(std::string const & name,
                            std::string const & filename,
                            GfMatrix4d const & viewMatrix,
                            GfMatrix4d const & projectionMatrix) const
{
    AttachmentsMap const & attachments = GetAttachments();
    AttachmentsMap::const_iterator it = attachments.find( name );

    if (it==attachments.end()) {
        TF_CODING_ERROR( "\""+name+"\" is not a valid variable name for this"
                         " DrawTarget" );
        return false;
    }

    MtlfDrawTarget::MtlfAttachment::MtlfAttachmentRefPtr const & a = TfStatic_cast<TfRefPtr<MtlfDrawTarget::MtlfAttachment>>(it->second);

    int nelems = GarchGetNumElements(a->GetFormat()),
        elemsize = GarchGetElementSize(a->GetType()),
        stride = _size[0] * nelems * elemsize,
        bufsize = _size[1] * stride;

    void * buf = malloc( bufsize );
    GetImage(name, buf);
    
    if (a->GetFormat() == GL_RGBA && a->GetType() == GL_FLOAT) {
        // The data we just got is actually halfs rather than floats. Convert.
        union float32 {
            float f;
            uint32_t u;
        };
        float32 *floats = static_cast<float32*>(buf);
        uint16_t *halfs = static_cast<uint16_t*>(buf);
        float32 convert;
        convert.u = (254 - 15) << 23;

        int pixel = _size[0] * _size[1] * 4;
        do {
            float32 out;
            uint16_t in = halfs[pixel];

            out.u = (in & 0x7fff) << 13;
            out.f *= convert.f;
            out.u |= (in & 0x8000) << 16;
            
            floats[pixel] = out;
        } while(pixel--);
    }

    VtDictionary metadata;

    std::string ext = TfStringGetSuffix(filename);
    if (name == "depth" && ext == "zfile") {
        // transform depth value from normalized to camera space length
        float *p = (float*)buf;
        for (size_t i = 0; i < bufsize/sizeof(float); ++i){
            p[i] = (float)(-2*p[i] / projectionMatrix[2][2]);
        }

        // embed matrices into metadata
        GfMatrix4d worldToCameraTransform = viewMatrix;
        GfMatrix4d worldToScreenTransform = viewMatrix * projectionMatrix;

        GfMatrix4d invZ = GfMatrix4d().SetScale(GfVec3d(1, 1, -1));
        worldToCameraTransform *= invZ;

        metadata["Nl"] = worldToCameraTransform;
        metadata["NP"] = worldToScreenTransform;
    }

    HgiMetal* hgiMetal = MtlfMetalContext::GetMetalContext()->GetHgi();

    GarchImage::StorageSpec storage;
    storage.width = _size[0];
    storage.height = _size[1];
    storage.format = a->GetFormat();
    storage.type = a->GetType();
    storage.flipped = hgiMetal->_needsFlip;
    storage.data = buf;

    GarchImageSharedPtr image = GarchImage::OpenForWriting(filename);
    bool writeSuccess = image && image->Write(storage, metadata);

    free(buf);

    if (!writeSuccess) {
        TF_RUNTIME_ERROR("Failed to write image to %s", filename.c_str());
        return false;
    }

    return true;
}

//----------------------------------------------------------------------

MtlfDrawTarget::MtlfAttachment::MtlfAttachmentRefPtr
MtlfDrawTarget::MtlfAttachment::New(uint32_t attachmentIndex, GLenum format,
                                    GLenum type, GfVec2i size, uint32_t numSamples)
{
    return TfCreateRefPtr(
        new MtlfDrawTarget::MtlfAttachment(attachmentIndex, format, type,
                                           size, numSamples));
}

MtlfDrawTarget::MtlfAttachment::MtlfAttachment(uint32_t attachmentIndex, GLenum format,
                                               GLenum type, GfVec2i size,
                                               uint32_t numSamples) :
    _format(format),
    _type(type),
    _internalFormat(MTLPixelFormatInvalid),
    _attachmentIndex(attachmentIndex),
    _size(size),
    _numSamples(numSamples),
    _bytesPerPixel(0)
{
    _GenTexture();
}

MtlfDrawTarget::MtlfAttachment::~MtlfAttachment()
{
    _DeleteTexture();
}

// Generate a simple GL_TEXTURE_2D to use as an attachment
// we assume that the framebuffer is currently bound !
void
MtlfDrawTarget::MtlfAttachment::_GenTexture()
{
    GLenum type = _type;
    size_t memoryUsed = 0;

    if (_format==GL_DEPTH_COMPONENT) {
        if (type!=GL_FLOAT) {
            TF_CODING_ERROR("Only GL_FLOAT textures can be used for the"
                            " depth attachment point");
            type = GL_FLOAT;
        }
    }

    int numChannel;
    bool depth24stencil8 = false;
    uint32_t bytesPerValue = 1;
    
    HgiTextureDesc texDesc;
    texDesc.usage = HgiTextureUsageBitsColorTarget;

    HgiFormat hgiFormat = HgiFormatInvalid;
    MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();

    switch (_format)
    {
        case GL_RG:
            numChannel = 2;
            if (type == GL_FLOAT) {
                hgiFormat = HgiFormatFloat16Vec2;
                bytesPerValue = 2;
            }
            break;

        case GL_RGB:
            TF_CODING_ERROR("3 channel textures are unsupported on Metal");
            // Drop through

        case GL_RGBA:
            numChannel = 4;
            if (type == GL_FLOAT) {
                hgiFormat = HgiFormatFloat16Vec4;
                bytesPerValue = 2;
            }
            else if (type == GL_UNSIGNED_BYTE) {
                hgiFormat = HgiFormatUNorm8Vec4;
            }
            break;

        default:
            numChannel = 1;
            if (type == GL_FLOAT) {
                hgiFormat = HgiFormatFloat32;
                bytesPerValue = 4;
                texDesc.usage = HgiTextureUsageBitsDepthTarget;
            }
            else if (type == GL_UNSIGNED_BYTE) {
                hgiFormat = HgiFormatUNorm8;
            }
            break;
    }
    
    _bytesPerPixel = numChannel * bytesPerValue;

    if (hgiFormat == HgiFormatInvalid) {
        TF_FATAL_CODING_ERROR("Unsupported render target format");
    }

    size_t baseImageSize = (size_t)(_bytesPerPixel *
                                    _size[0]       *
                                    _size[1]);

    texDesc.type = HgiTextureType2D;
    texDesc.dimensions = GfVec3i(_size[0], _size[1], 0);
    texDesc.format = hgiFormat;
    _textureName = context->GetHgi()->CreateTexture(texDesc);

    memoryUsed += baseImageSize;

    if (_numSamples > 1) {
        texDesc.sampleCount = HgiSampleCount(_numSamples);
        _textureNameMS = context->GetHgi()->CreateTexture(texDesc);
        memoryUsed = baseImageSize * _numSamples;
    }
    
    if (_format == GL_DEPTH_STENCIL) {
        //Use the same texture for stencil as it's a packed depth/stencil format.
        _stencilTextureName = _textureName;
        _stencilTextureNameMS = _textureNameMS;
    }

    _SetMemoryUsed(memoryUsed);
}

void
MtlfDrawTarget::MtlfAttachment::_DeleteTexture()
{
    MtlfMetalContextSharedPtr context = MtlfMetalContext::GetMetalContext();
    if (_textureName) {
        context->GetHgi()->DestroyTexture(&_textureName);
    }

    if (_textureNameMS) {
        context->GetHgi()->DestroyTexture(&_textureNameMS);
    }
}

void
MtlfDrawTarget::MtlfAttachment::ResizeTexture(const GfVec2i &size)
{
    _size = size;

    _DeleteTexture();
    _GenTexture();
}

/* virtual */
GarchTexture::BindingVector
MtlfDrawTarget::MtlfAttachment::GetBindings(TfToken const & identifier,
                                            GarchSamplerGPUHandle const& samplerName)
{
    return BindingVector(1,
                Binding(identifier, GarchTextureTokens->texels,
                        GL_TEXTURE_2D, GetTextureName(), samplerName));
}

/* virtual */
VtDictionary
MtlfDrawTarget::MtlfAttachment::GetTextureInfo(bool forceLoad)
{
    TF_UNUSED(forceLoad);

    VtDictionary info;

    info["width"] = (int)_size[0];
    info["height"] = (int)_size[1];
    info["memoryUsed"] = GetMemoryUsed();
    info["depth"] = 1;
    info["format"] = (int)_internalFormat;
    info["imageFilePath"] = TfToken("DrawTarget");
    info["referenceCount"] = GetRefCount().Get();
    info["numSamples"] = _numSamples;

    return info;
}

void
MtlfDrawTarget::MtlfAttachment::TouchContents()
{
    _UpdateContentsID();
}

PXR_NAMESPACE_CLOSE_SCOPE

