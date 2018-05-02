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

#include "pxr/imaging/garch/image.h"
#include "pxr/imaging/garch/utils.h"

#include "pxr/base/tf/stringUtils.h"
#include "pxr/base/tf/envSetting.h"

#include "pxr/base/tracelite/trace.h"

#include <boost/shared_ptr.hpp>

PXR_NAMESPACE_OPEN_SCOPE


TF_DEFINE_ENV_SETTING(MTLF_DRAW_TARGETS_NUM_SAMPLES, 4,
                      "Number of samples greater than 1 forces MSAA.");

static unsigned int 
GetNumSamples()
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
    _mtlRenderPassDescriptor(nil),
    _bindDepth(0),
    _size(size),
    _numSamples(1)
{
    // If MSAA has been requested and it is enabled then we will create
    // msaa buffers
    if (requestMSAA) {
        _numSamples = GetNumSamples();
    }

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
    _mtlRenderPassDescriptor(nil),
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
    _DeleteAttachments( );
}

void
MtlfDrawTarget::_AddAttachment( std::string const & name,
                                GLenum format, GLenum type,
                                GLenum internalFormat )
{
    if (!IsBound()) {
        TF_CODING_ERROR("Cannot change the size of an unbound MtlfDrawTarget");
    }

    AttachmentsMap & attachments = _GetAttachments();
    AttachmentsMap::iterator it = attachments.find( name );

    if (it==attachments.end()) {

        MtlfDrawTarget::MtlfAttachment::MtlfAttachmentRefPtr attachment =
            MtlfAttachment::New((uint32_t)attachments.size(),
                                format, type, _size, _numSamples);

        attachments.insert(AttachmentsMap::value_type(name, attachment));


        TF_VERIFY( attachment->GetMtlTextureName() != 0 , "%s", 
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

    for (AttachmentsMap::value_type const& p :  _attachmentsPtr->attachments) {
        _BindAttachment( TfStatic_cast<TfRefPtr<MtlfDrawTarget::MtlfAttachment>>(p.second) );
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
    _mtlRenderPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
}

// Attach a texture to one of the attachment points of the framebuffer.
// We assume that the framebuffer is currently bound !
void
MtlfDrawTarget::_BindAttachment( MtlfAttachmentRefPtr const & a )
{
    id<MTLTexture> tid = a->GetMtlTextureName();
    id<MTLTexture> tidMS = a->GetMtlTextureMSName();

    int attach = a->GetAttach();

    GLenum attachment = GL_COLOR_ATTACHMENT0;
    if (a->GetFormat()==GL_DEPTH_COMPONENT) {
        MTLRenderPassDepthAttachmentDescriptor *depthAttachment = _mtlRenderPassDescriptor.depthAttachment;
        if (HasMSAA()) {
            depthAttachment.texture = tidMS;
        } else {
            depthAttachment.texture = tid;
        }
        
        // make sure to clear every frame for best performance
        depthAttachment.loadAction = MTLLoadActionClear;
        depthAttachment.clearDepth = 0.0;
        
        // store only attachments that will be presented to the screen, as in this case
        depthAttachment.storeAction = MTLStoreActionStore;
    } else if (a->GetFormat()==GL_DEPTH_STENCIL) {
        MTLRenderPassStencilAttachmentDescriptor *stencilAttachment = _mtlRenderPassDescriptor.stencilAttachment;
        if (HasMSAA()) {
            stencilAttachment.texture = tidMS;
        } else {
            stencilAttachment.texture = tid;
        }
        
        // make sure to clear every frame for best performance
        stencilAttachment.loadAction = MTLLoadActionClear;
        stencilAttachment.clearStencil = 0;
        
        // store only attachments that will be presented to the screen, as in this case
        stencilAttachment.storeAction = MTLStoreActionStore;
    } else {
        if (attach < 0) {
            TF_CODING_ERROR("Attachment index cannot be negative");
            return;
        }

        TF_VERIFY( attach < _GetMaxAttachments(),
            "Exceeding number of Attachments available ");

        MTLRenderPassColorAttachmentDescriptor *colorAttachment = _mtlRenderPassDescriptor.colorAttachments[attach];
        if (HasMSAA()) {
            colorAttachment.texture = tidMS;
        } else {
            colorAttachment.texture = tid;
        }
        
        // make sure to clear every frame for best performance
        colorAttachment.loadAction = MTLLoadActionClear;
        colorAttachment.clearColor = MTLClearColorMake(1.0f, 0.25f, 0.25f, 1.0f);
        
        // store only attachments that will be presented to the screen, as in this case
        colorAttachment.storeAction = MTLStoreActionStore;
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
    
    // Create a render command encoder so we can render into something
    TF_VERIFY(MtlfMetalContext::GetMetalContext()->commandBuffer == nil, "A command buffer is already active");
    
    id<MTLCommandBuffer> commandBuffer = [MtlfMetalContext::GetMetalContext()->commandQueue commandBuffer];
    id <MTLRenderCommandEncoder> renderEncoder =
    [commandBuffer renderCommandEncoderWithDescriptor:_mtlRenderPassDescriptor];
    MtlfMetalContext::GetMetalContext()->commandBuffer = commandBuffer;

    _renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:_mtlRenderPassDescriptor];
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
    
    [_renderEncoder endEncoding];
    
    id<MTLCommandBuffer> commandBuffer = MtlfMetalContext::GetMetalContext()->commandBuffer;
    MtlfMetalContext::GetMetalContext()->commandBuffer = nil;

    TF_VERIFY(commandBuffer != nil, "No active command buffer");
    [commandBuffer commit];

    TouchContents();
}

void 
MtlfDrawTarget::_Resolve()
{
    TF_FATAL_CODING_ERROR("Not Implemented");

    // Resolve MSAA fbo to a regular fbo
//    glBindFramebuffer(GL_READ_FRAMEBUFFER, _framebufferMS);
//    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _framebuffer);
//    glBlitFramebuffer(0, 0, _size[0], _size[1],
//                      0, 0, _size[0], _size[1],
//                      GL_COLOR_BUFFER_BIT |
//                      GL_DEPTH_BUFFER_BIT |
//                      GL_STENCIL_BUFFER_BIT ,
//                      GL_NEAREST);
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
    return MtlfCheckMetalFrameBufferStatus(GL_FRAMEBUFFER, reason);
}

void
MtlfDrawTarget::GetImage(std::string const & name, void* buffer) const
{
    MtlfAttachmentRefPtr attachment = TfStatic_cast<TfRefPtr<MtlfDrawTarget::MtlfAttachment>>(_GetAttachments().at(name));

    id<MTLTexture> texture = attachment->GetTextureName();
    int bytesPerPixel = attachment->GetBytesPerPixel();
    int width = [texture width];
    int height = [texture height];

    [texture getBytes:buffer
          bytesPerRow:width * bytesPerPixel
           fromRegion:MTLRegionMake2D(0, 0, width, height)
          mipmapLevel:0];
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

    {
        TF_FATAL_CODING_ERROR("Not Implemented");
        /*
        glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);

        glPixelStorei(GL_PACK_ROW_LENGTH, 0);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
        glPixelStorei(GL_PACK_SKIP_ROWS, 0);

        GLint restoreBinding, restoreActiveTexture;
        glGetIntegerv( GL_TEXTURE_BINDING_2D, &restoreBinding );
        glGetIntegerv( GL_ACTIVE_TEXTURE, & restoreActiveTexture);

        glActiveTexture( GL_TEXTURE0 );
        glBindTexture( GL_TEXTURE_2D, a->GetMtlTextureName() );

        glGetTexImage(GL_TEXTURE_2D, 0, a->GetFormat(), a->GetType(), buf);

        glActiveTexture( restoreActiveTexture );
        glBindTexture( GL_TEXTURE_2D, restoreBinding );

        glPopClientAttrib();

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
         */
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

    GarchImage::StorageSpec storage;
    storage.width = _size[0];
    storage.height = _size[1];
    storage.format = a->GetFormat();
    storage.type = a->GetType();
    storage.flipped = true;
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
    _textureName(0),
    _textureNameMS(0),
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

    _bytesPerPixel = (_type == GL_FLOAT) ? 4 : 1;
    int numChannel;
    MTLPixelFormat mtlFormat = MTLPixelFormatInvalid;

    switch (_format)
    {
        case GL_RG:
            numChannel = 2;
            if (type == GL_FLOAT) {
                mtlFormat = MTLPixelFormatRG32Float;
            }
            break;

        case GL_RGB:
            TF_CODING_ERROR("3 channel textures are unsupported on Metal");
            // Drop through

        case GL_RGBA:
            numChannel = 4;
            if (type == GL_FLOAT) {
                mtlFormat = MTLPixelFormatRGBA32Float;
            }
            else if (type == GL_UNSIGNED_BYTE) {
                mtlFormat = MTLPixelFormatRGBA8Unorm;
            }
            break;

        default:
            numChannel = 1;
            if (type == GL_FLOAT) {
                mtlFormat = MTLPixelFormatR32Float;
            }
            else if (type == GL_UNSIGNED_INT_24_8) {
                mtlFormat = MTLPixelFormatR8Uint;
            }
            else if (type == GL_UNSIGNED_BYTE) {
                mtlFormat = MTLPixelFormatR8Unorm;
            }
            break;
    }

    if (mtlFormat == MTLPixelFormatInvalid) {
        TF_FATAL_CODING_ERROR("Unsupported render target format");
    }

    size_t baseImageSize = (size_t)(_bytesPerPixel *
                                    numChannel     *
                                    _size[0]       *
                                    _size[1]);

    id<MTLDevice> device = MtlfMetalContext::GetMetalContext()->device;
    MTLTextureDescriptor* desc =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:mtlFormat
                                                           width:_size[0]
                                                          height:_size[1]
                                                       mipmapped:NO];
    desc.usage = MTLTextureUsageRenderTarget;
    desc.resourceOptions = MTLResourceStorageModeManaged;
    _textureName = [device newTextureWithDescriptor:desc];
    
     memoryUsed += baseImageSize;

    if (_numSamples > 1) {
        desc.sampleCount = _numSamples;
        _textureNameMS = [device newTextureWithDescriptor:desc];
        memoryUsed = baseImageSize * _numSamples;
    }
    
    _SetMemoryUsed(memoryUsed);
}

void
MtlfDrawTarget::MtlfAttachment::_DeleteTexture()
{
    TF_FATAL_CODING_ERROR("Not Implemented");

    if (_textureName) {
        [_textureName release];
        _textureName = nil;
    }

    if (_textureNameMS) {
        [_textureNameMS release];
        _textureNameMS = nil;
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
                                            GarchSamplerGPUHandle samplerName) const
{
    return BindingVector(1,
                Binding(identifier, GarchTextureTokens->texels,
                        GL_TEXTURE_2D, GetTextureName(), samplerName));
}

/* virtual */
VtDictionary
MtlfDrawTarget::MtlfAttachment::GetTextureInfo() const
{
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

