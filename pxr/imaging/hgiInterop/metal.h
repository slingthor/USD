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
#ifndef PXR_IMAGING_HGIINTEROP_HGIINTEROPMETAL_H
#define PXR_IMAGING_HGIINTEROP_HGIINTEROPMETAL_H

#include <GL/glew.h>
#include <Metal/Metal.h>
#include <AppKit/AppKit.h>

#include "pxr/pxr.h"
#include "pxr/imaging/hgiInterop/api.h"


PXR_NAMESPACE_OPEN_SCOPE

class Hgi;

/// \class HgiInteropMetal
///
/// Provides Metal/GL interop
///
class HgiInteropMetal {
public:
    
    HGIINTEROP_API
    HgiInteropMetal(
        id<MTLDevice> interopDevice);

    HGIINTEROP_API
    virtual ~HgiInteropMetal();
    
    HGIINTEROP_API
    void AllocateAttachments(int width, int height);
        
    HGIINTEROP_API
    void CopyToInterop(
        Hgi* hgi,
        id<MTLTexture> sourceColorTexture,
        id<MTLTexture> sourceDepthTexture,
        bool flipImage);

private:

    void _BlitToOpenGL(bool flipY);
    void _FreeTransientTextureCacheRefs();
    void _CaptureOpenGlState();
    void _RestoreOpenGlState();

    id<MTLDevice>               _device;

    id<MTLTexture>              _mtlAliasedColorTexture;
    id<MTLTexture>              _mtlAliasedDepthRegularFloatTexture;

    id<MTLLibrary>              _defaultLibrary;
    id<MTLFunction>             _computeDepthCopyProgram;
    id<MTLFunction>             _computeColorCopyProgram;
    id<MTLComputePipelineState> _computePipelineStateColor;
    id<MTLComputePipelineState> _computePipelineStateDepth;

    NSOpenGLContext*            _glInteropCtx;

    CVPixelBufferRef            _pixelBuffer;
    CVPixelBufferRef            _depthBuffer;
    CVMetalTextureCacheRef      _cvmtlTextureCache;
    CVMetalTextureRef           _cvmtlColorTexture;
    CVMetalTextureRef           _cvmtlDepthTexture;

    CVOpenGLTextureCacheRef     _cvglTextureCache;
    CVOpenGLTextureRef          _cvglColorTexture;
    CVOpenGLTextureRef          _cvglDepthTexture;

    uint32_t                    _glColorTexture;
    uint32_t                    _glDepthTexture;
    
    uint32_t                    _glShaderProgram;
    uint32_t                    _glVAO;
    uint32_t                    _glVBO;
    int32_t                     _posAttrib;
    int32_t                     _texAttrib;
    uint32_t                    _blitTexSizeUniform;
    
    int32_t _restoreVao;
    int32_t _restoreVbo;
    bool _restoreDepthTest;
    bool _restoreDepthWriteMask;
    bool _restoreStencilWriteMask;
    int32_t _restoreDepthFunc;
    int32_t _restoreViewport[4];
    bool _restoreblendEnabled;
    int32_t _restoreColorOp;
    int32_t _restoreAlphaOp;
    int32_t _restoreColorSrcFnOp;
    int32_t _restoreAlphaSrcFnOp;
    int32_t _restoreColorDstFnOp;
    int32_t _restoreAlphaDstFnOp;
    bool _restoreAlphaToCoverage;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif
