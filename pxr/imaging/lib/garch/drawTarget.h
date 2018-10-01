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
#ifndef GARCH_DRAWTARGET_H
#define GARCH_DRAWTARGET_H

/// \file garch/drawTarget.h

#include "pxr/pxr.h"
#include "pxr/imaging/garch/api.h"
#include "pxr/imaging/garch/gl.h"
#include "pxr/imaging/garch/texture.h"

#include "pxr/base/gf/vec2i.h"
#include "pxr/base/gf/matrix4d.h"
#include "pxr/base/tf/declarePtrs.h"
#include "pxr/base/tf/refBase.h"
#include "pxr/base/tf/weakBase.h"

#include <map>
#include <set>
#include <string>

#include <boost/shared_ptr.hpp>

PXR_NAMESPACE_OPEN_SCOPE

TF_DECLARE_WEAK_AND_REF_PTRS(GarchDrawTarget);

/// \class GarchDrawTarget
///
/// A class representing a render target with mutliple image attachments.
///
/// A DrawTarget is essentially a custom render pass into which several
/// arbitrary variables can be output into. These can later be used as
/// texture samplers by GLSL shaders.
///
/// The DrawTarget maintains a map of named attachments that correspond
/// to GL_TEXTURE_2D mages. By default, DrawTargets also create a depth
/// component that is used both as a depth buffer during the draw pass,
/// and can later be accessed as a regular GL_TEXTURE_2D data. Stencils
/// are also available (by setting the format to GL_DEPTH_STENCIL and
/// the internalFormat to GL_DEPTH24_STENCIL8)
///
class GarchDrawTarget: public TfRefBase, public TfWeakBase {
public:
    typedef GarchDrawTarget This;

public:
    /// Returns a new instance.
    GARCH_API
    static GarchDrawTargetRefPtr New(GfVec2i const &size,
                                     bool requestMSAA = false);
    
    /// Returns a new instance.
    /// GL framebuffers cannot be shared across contexts, but texture
    /// attachments can. In order to reflect this, GarchDrawTargets hold
    /// onto their maps of attachments through a RefPtr that can be shared
    /// by multiple GarchDrawTargets, one for each of the active GL contexts
    /// (ex. one for each active QT viewer).
    /// This constructor creates a new framebuffer, but populates its map of
    /// attachments by sharing the RefPtr of the source GarchDrawTarget.
    GARCH_API
    static GarchDrawTargetRefPtr New(GarchDrawTargetPtr const &drawtarget);

    struct AttachmentDesc {
        AttachmentDesc(std::string const &_name,
                       GLenum _format, GLenum _type, GLenum _internalFormat)
            : name(_name)
            , format(_format)
            , type(_type)
            , internalFormat(_internalFormat) {}

        std::string const name;
        GLenum format;
        GLenum type;
        GLenum internalFormat;
    };
    
    class Attachment : public GarchTexture {
    public:
        typedef TfDeclarePtrs<class Attachment>::RefPtr AttachmentRefPtr;

        GARCH_API
        virtual ~Attachment() {}
        
        /// Returns the texture handle (can be used as any regular native graphics API texture)
        virtual GarchTextureGPUHandle GetTextureName() override = 0;

        /// Resize the attachment recreating the texture
        GARCH_API
        virtual void ResizeTexture(const GfVec2i &size) = 0;

        // GarchTexture overrides
        GARCH_API
        virtual BindingVector GetBindings(TfToken const & identifier,
                                          GarchSamplerGPUHandle samplerName) override = 0;
        GARCH_API
        virtual VtDictionary GetTextureInfo(bool forceLoad) override  = 0;

        /// Updates the contents signature for the underlying texture
        /// to allow downstream consumers to know that the texture image
        /// data may have changed.
        GARCH_API
        virtual void TouchContents() = 0;

    protected:
        Attachment() {}
        
        GARCH_API
        virtual void _ReadTexture() override final {}
    };

    typedef TfDeclarePtrs<class Attachment>::RefPtr AttachmentRefPtr;

    typedef std::map<std::string, AttachmentRefPtr> AttachmentsMap;
    
    /// Clears all the attachments for this DrawTarget.
    GARCH_API
    virtual void ClearAttachments() = 0;
    
    /// Copies the list of attachments from DrawTarget. This binds and unbinds the frame buffer.
    GARCH_API
    virtual void CloneAttachments( GarchDrawTargetPtr const & drawtarget ) = 0;
    
    /// Returns the list of Attachments for this DrawTarget.
    GARCH_API
    virtual AttachmentsMap const & GetAttachments() const = 0;
    
    /// Returns the attachment with a given name or TfNullPtr;
    GARCH_API
    virtual AttachmentRefPtr GetAttachment(std::string const & name) = 0;
    
    /// Save the Attachment buffer to an array.
    GARCH_API
    virtual void GetImage(std::string const & name, void* buffer) const = 0;

    /// Write the Attachment buffer to an image file (debugging).
    GARCH_API
    virtual bool WriteToFile(std::string const & name,
                     std::string const & filename,
                     GfMatrix4d const & viewMatrix = GfMatrix4d(1),
                     GfMatrix4d const & projectionMatrix = GfMatrix4d(1)) const = 0;

    /// Resize the DrawTarget.
    GARCH_API
    virtual void SetSize( GfVec2i ) = 0;

    /// Returns the size of the DrawTarget.
    GARCH_API
    virtual GfVec2i const & GetSize() const = 0;

    /// Returns if the draw target uses msaa
    GARCH_API
    virtual bool HasMSAA() const = 0;
    
    /// Returns the number of msaa samples the draw target uses.
    GARCH_API
    virtual uint32_t const & GetNumSamples() const = 0;

    /// Returns the framebuffer object Id.
    GARCH_API
    virtual GLuint GetFramebufferId() const = 0;
    
    /// Returns the id of the framebuffer object with MSAA buffers.
    GARCH_API
    virtual GLuint GetFramebufferMSId() const = 0;

    /// Binds the framebuffer.
    GARCH_API
    virtual void Bind() = 0;

    /// Sets the attachments to the framebuffer. There is no bound frame buffer when this method returns.
    GARCH_API
    virtual void SetAttachments(std::vector<GarchDrawTarget::AttachmentDesc>& attachments) = 0;

    /// Unbinds the framebuffer.
    GARCH_API
    virtual void Unbind() = 0;

    /// Returns whether the framebuffer is currently bound.
    GARCH_API
    virtual bool IsBound() const = 0;

    /// Resolve the MSAA framebuffer to a regular framebuffer. If there
    /// is no MSAA enabled, this function does nothing.
    GARCH_API
    virtual void Resolve() = 0;

    /// Updates the contents signature for attached textures
    /// to allow downstream consumers to know that the texture image
    /// data may have changed.
    GARCH_API
    virtual void TouchContents() = 0;

    /// Returns whether the enclosed framebuffer object is complete.
    /// If \a reason is non-NULL, and this framebuffer is not valid,
    /// sets \a reason to the reason why not.
    GARCH_API
    virtual bool IsValid(std::string * reason = NULL) = 0;

    /// Weak/Ref-based container for the the map of texture attachments.
    /// Multiple GarchDrawTargets can jointly share their attachment textures :
    /// this construction allows the use of a RefPtr on the map of attachments.
    class AttachmentsContainer : public TfRefBase, public TfWeakBase {
    public:
        AttachmentsMap attachments;
    };
    
    TfRefPtr<AttachmentsContainer> _attachmentsPtr;

protected:

    GARCH_API
    GarchDrawTarget();

    GARCH_API
    virtual ~GarchDrawTarget();
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif  // GARCH_DRAWTARGET_H
