#line 1 "/Volumes/Data/USDMetal/pxr/imaging/lib/mtlf/mtlDevice.h"
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
#ifndef MTLF_GL_INTEROP_H
#define MTLF_GL_INTEROP_H

#include "pxr/pxr.h"
#include "pxr/imaging/mtlf/api.h"
#include "pxr/imaging/mtlf/mtlDevice.h"

#include <boost/noncopyable.hpp>

PXR_NAMESPACE_OPEN_SCOPE


/// \class MtlfGlInterop
///
/// Provides window system independent access to Metal devices.
///


class MtlfGlInterop : public boost::noncopyable {
public:
    typedef struct {
        float position[2];
        float uv[2];
    } Vertex;
    
    MTLF_API
    MtlfGlInterop(id<MTLDevice> device);

    MTLF_API
    virtual ~MtlfGlInterop();
    
    MTLF_API
    void AllocateAttachments(int width, int height);
    
    MTLF_API
    void FreeTransientTextureCacheRefs();

    /// Blit the current render target contents to the OpenGL FBO
    MTLF_API
    void BlitColorTargetToOpenGL();
    
    MTLF_API
    void CopyDepthTextureToOpenGL(id<MTLComputeCommandEncoder> computeEncoder);

    id<MTLTexture> mtlColorTexture;
    id<MTLTexture> mtlDepthTexture;
    id<MTLTexture> mtlDepthRegularFloatTexture;

private:

    id<MTLDevice>               device;

    id<MTLLibrary>              defaultLibrary;
    id<MTLFunction>             computeDepthCopyProgram;
    
    CVPixelBufferRef pixelBuffer;
    CVPixelBufferRef depthBuffer;
    CVOpenGLTextureCacheRef cvglTextureCache;
    CVMetalTextureCacheRef cvmtlTextureCache;
    CVOpenGLTextureRef cvglColorTexture;
    CVOpenGLTextureRef cvglDepthTexture;
    CVMetalTextureRef cvmtlColorTexture;
    CVMetalTextureRef cvmtlDepthTexture;
    uint32_t glColorTexture;
    uint32_t glDepthTexture;
    
    typedef struct StaticGLState {
        uint32_t glShaderProgram;
        uint32_t glVAO;
        uint32_t glVBO;
        int32_t posAttrib;
        int32_t texAttrib;
        uint32_t blitTexSizeUniform;
    } StaticGLState;
    
    static void _InitializeStaticState();
    
    static StaticGLState   staticState;

    id<MTLFunction> renderVertexFunction;
    id<MTLFunction> renderFragmentFunction;
    id<MTLFunction> renderComputeGSFunction;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif  // MTLF_GL_INTEROP_H
