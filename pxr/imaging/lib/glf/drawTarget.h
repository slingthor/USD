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
#ifndef GLF_DRAWTARGET_H
#define GLF_DRAWTARGET_H

/// \file glf/drawTarget.h

#include "pxr/pxr.h"
#include "pxr/imaging/glf/api.h"
#include "pxr/imaging/garch/drawTarget.h"
#include "pxr/imaging/garch/texture.h"

#include "pxr/base/gf/vec2i.h"
#include "pxr/base/gf/matrix4d.h"
#include "pxr/base/tf/declarePtrs.h"
#include "pxr/base/tf/refBase.h"
#include "pxr/base/tf/weakBase.h"

#include "pxr/imaging/garch/gl.h"

#include <map>
#include <set>
#include <string>

#include <boost/shared_ptr.hpp>

PXR_NAMESPACE_OPEN_SCOPE

TF_DECLARE_WEAK_AND_REF_PTRS(GlfDrawTarget);
typedef boost::shared_ptr<class GlfGLContext> GlfGLContextSharedPtr;

/// \class GlfDrawTarget
///
/// A class representing a GL render target with mutliple image attachments.
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
class GlfDrawTarget : public GarchDrawTarget {
public:
    typedef GlfDrawTarget This;

public:
    
    /// Returns a new instance.
    GLF_API
    static GlfDrawTarget * New( GfVec2i const & size,
                                    bool requestMSAA = false );

    class GlfAttachment : public GarchDrawTarget::Attachment {
    public:
        typedef TfDeclarePtrs<class GlfAttachment>::RefPtr GlfAttachmentRefPtr;
        
        GARCH_API
        static GlfAttachmentRefPtr New(int glIndex, GLenum format, GLenum type,
                                    GLenum internalFormat, GfVec2i size,
                                    unsigned int numSamples);
        
        GARCH_API
        virtual ~GlfAttachment();
        
        /// Returns the GL texture index (can be used as any regular GL texture)
        virtual GarchTextureGPUHandle GetTextureName() const override { return _textureName; }

        /// Returns the GL texture index multisampled of this attachment
        GarchTextureGPUHandle GetTextureMSName() const { return _textureNameMS; }
        
        /// Returns the GL format of the texture (GL_RGB, GL_DEPTH_COMPONENT...)
        GLenum GetFormat() const { return _format; }
        
        /// Returns the GL type of the texture (GL_BYTE, GL_INT, GL_FLOAT...)
        GLenum GetType() const { return _type; }
        
        /// Returns the GL attachment point index in the framebuffer.
        int GetAttach() const { return _glIndex; }

        /// Resize the attachment recreating the texture
        GARCH_API
        virtual void ResizeTexture(const GfVec2i &size);
        
        // GarchTexture overrides
        GARCH_API
        virtual BindingVector GetBindings(TfToken const & identifier,
                                          GarchSamplerGPUHandle samplerName) const override;
        GARCH_API
        virtual VtDictionary GetTextureInfo() const override;
        
        /// Updates the contents signature for the underlying texture
        /// to allow downstream consumers to know that the texture image
        /// data may have changed.
        GARCH_API
        virtual void TouchContents();
        
    private:
        GlfAttachment(int glIndex, GLenum format, GLenum type,
                      GLenum internalFormat, GfVec2i size,
                      unsigned int numSamples);
        
        void _GenTexture();
        void _DeleteTexture();
        
        GLuint       _textureName;
        GLuint       _textureNameMS;
        
        GLenum       _format,
        _type,
        _internalFormat;
        
        int          _glIndex;
        
        GfVec2i      _size;
        
        unsigned int _numSamples;
    };
    
    typedef TfDeclarePtrs<class GlfAttachment>::RefPtr GlfAttachmentRefPtr;

    /// Returns a new instance.
    /// GL framebuffers cannot be shared across contexts, but texture
    /// attachments can. In order to reflect this, GlfDrawTargets hold
    /// onto their maps of attachments through a RefPtr that can be shared
    /// by multiple GlfDrawTargets, one for each of the active GL contexts
    /// (ex. one for each active QT viewer).
    /// This constructor creates a new framebuffer, but populates its map of
    /// attachments by sharing the RefPtr of the source GlfDrawTarget.
    GLF_API
    static GlfDrawTarget * New( GarchDrawTargetPtr const & drawtarget );

    /// Clears all the attachments for this DrawTarget.
    GLF_API
    virtual void ClearAttachments() override;
    
    /// Copies the list of attachments from DrawTarget. This binds and unbinds the frame buffer.
    GLF_API
    virtual void CloneAttachments( GarchDrawTargetPtr const & drawtarget ) override;
    
    /// Returns the list of Attachments for this DrawTarget.
    GLF_API
    virtual GarchDrawTarget::AttachmentsMap const & GetAttachments() const override;
    
    /// Returns the attachment with a given name or TfNullPtr;
    GLF_API
    virtual GarchDrawTarget::AttachmentRefPtr GetAttachment(std::string const & name) override;
    
    /// Save the Attachment buffer to an array.
    GLF_API
    virtual void GetImage(std::string const & name, void* buffer) const override;

    /// Write the Attachment buffer to an image file (debugging).
    GLF_API
    virtual bool WriteToFile(std::string const & name,
                             std::string const & filename,
                             GfMatrix4d const & viewMatrix = GfMatrix4d(1),
                             GfMatrix4d const & projectionMatrix = GfMatrix4d(1)) const override;

    /// Resize the DrawTarget.
    GLF_API
    virtual void SetSize( GfVec2i ) override;

    /// Returns the size of the DrawTarget.
    GLF_API
    virtual GfVec2i const & GetSize() const override { return _size; }

    /// Returns if the draw target uses msaa
    GLF_API
    virtual bool HasMSAA() const override { return (_numSamples > 1); }
    
    /// Returns the number of msaa samples the draw target uses.
    GLF_API
    virtual uint32_t const & GetNumSamples() const override { return _numSamples; }

    /// Returns the framebuffer object Id.
    GLF_API
    virtual GLuint GetFramebufferId() const override;
    
    /// Returns the id of the framebuffer object with MSAA buffers.
    GLF_API
    virtual GLuint GetFramebufferMSId() const override;

    /// Binds the framebuffer.
    GLF_API
    virtual void Bind() override;
    
    /// Sets the attachments to the framebuffer. There is no bound frame buffer when this method returns.
    GLF_API
    virtual void SetAttachments(std::vector<GarchDrawTarget::AttachmentDesc>& attachments) override;

    /// Unbinds the framebuffer.
    GLF_API
    virtual void Unbind() override;

    /// Returns whether the framebuffer is currently bound.
    GLF_API
    virtual bool IsBound() const override;

    /// Resolve the MSAA framebuffer to a regular framebuffer. If there
    /// is no MSAA enabled, this function does nothing.
    GLF_API
    virtual void Resolve() override;

    /// Resolve several MSAA framebuffers at once. If any framebuffers don't
    /// have MSAA enabled, nothing happens to them.
    GLF_API
    static void Resolve(const std::vector<GarchDrawTarget*>& drawTargets);

    /// Updates the contents signature for attached textures
    /// to allow downstream consumers to know that the texture image
    /// data may have changed.
    GLF_API
    virtual void TouchContents() override;

    /// Returns whether the enclosed framebuffer object is complete.
    /// If \a reason is non-NULL, and this framebuffer is not valid,
    /// sets \a reason to the reason why not.
    GLF_API
    virtual bool IsValid(std::string * reason = NULL) override;

protected:

    GLF_API
    GlfDrawTarget( GfVec2i const & size, bool requestMSAA );

    GLF_API
    GlfDrawTarget( GarchDrawTargetPtr const & drawtarget );

    GLF_API
    virtual ~GlfDrawTarget();

    friend class GlfResourceFactory;
    
private:
    void _GenFrameBuffer();

    /// Add an attachment to the DrawTarget.
    void _AddAttachment( std::string const & name,
                         GLenum format, GLenum type, GLenum internalFormat );

    void _BindAttachment( GlfAttachmentRefPtr const & a );
    
    GLuint _AllocAttachment( GLenum format, GLenum type );

    AttachmentsMap & _GetAttachments() const;

    void _DeleteAttachments( );
    
    void _AllocDepth( );

    bool _Validate(std::string * reason = NULL);

    void _SaveBindingState();

    void _RestoreBindingState();

    void _Resolve();

    GLuint _framebuffer;
    GLuint _framebufferMS;
    
    GLuint _unbindRestoreReadFB,
           _unbindRestoreDrawFB;

    int _bindDepth;

    GfVec2i _size;
    
    unsigned int _numSamples;

    GlfGLContextSharedPtr _owningContext;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif  // GLF_DRAW_TARGET_H
