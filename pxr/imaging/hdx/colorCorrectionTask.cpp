//
// Copyright 2018 Pixar
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

#include "pxr/imaging/hdx/colorCorrectionTask.h"
#include "pxr/imaging/hdx/package.h"
#include "pxr/imaging/hd/aov.h"
#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/renderBuffer.h"
#include "pxr/imaging/hd/tokens.h"
#include "pxr/imaging/hdSt/program.h"
#include "pxr/imaging/hdSt/resourceFactory.h"
#include "pxr/imaging/hdSt/renderBuffer.h"
#include "pxr/imaging/hdSt/renderDelegate.h"
#include "pxr/imaging/hf/perfLog.h"
#include "pxr/imaging/glf/diagnostic.h"
#include "pxr/imaging/hio/glslfx.h"
#include "pxr/imaging/glf/glContext.h"
#include "pxr/base/tf/getenv.h"

#include "pxr/imaging/hgi/hgi.h"
#include "pxr/imaging/hgi/buffer.h"
#include "pxr/imaging/hgi/texture.h"

#if defined(ARCH_GFX_OPENGL)
#include "pxr/imaging/hgiGL/buffer.h"
#include "pxr/imaging/hgiGL/texture.h"

#include "pxr/imaging/hdSt/GL/glslProgram.h"
#endif

#if defined(ARCH_GFX_METAL)
#include "pxr/imaging/hdSt/Metal/codeGenMSL.h"
#endif

#ifdef PXR_OCIO_PLUGIN_ENABLED
    #include <OpenColorIO/OpenColorIO.h>
    namespace OCIO = OCIO_NAMESPACE;
#endif

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PRIVATE_TOKENS(
    _tokens,
    ((colorCorrectionVertex,    "ColorCorrectionVertex"))
    ((colorCorrectionFragment,  "ColorCorrectionFragment"))
    (colorCorrectionShader)
);

namespace {
    enum {
        COLOR_IN = 0,
        POSITION = 1,
        UV_IN = 2,
        LUT3D_IN = 3,
    };
}

HdxColorCorrectionTask::HdxColorCorrectionTask(HdSceneDelegate* delegate, 
                                               SdfPath const& id)
    : HdTask(id)
    , _shaderProgram()
    , _texture(NULL)
    , _textureSize(0)
    , _vertexBuffer(NULL)
    , _copyFramebuffer(0)
    , _framebufferSize(0)
    , _lut3dSizeOCIO(0)
    , _aovBufferPath()
    , _aovBuffer(nullptr)
    , _aovTexture(nullptr)
    , _aovFramebuffer(0)
{
    _texture3dLUT.Clear();
    _isOpenGL = HdStResourceFactory::GetInstance()->IsOpenGL();
    
    HdStRenderDelegate* renderDelegate =
        static_cast<HdStRenderDelegate*>(
            delegate->GetRenderIndex().GetRenderDelegate());
    _hgi = renderDelegate->GetHgi();
}

HdxColorCorrectionTask::~HdxColorCorrectionTask()
{
    if (_texture != NULL) {
        delete _texture;
        _texture = NULL;
    }

    if (_texture3dLUT.IsSet()) {
        if (_isOpenGL) {
#if defined(ARCH_GFX_OPENGL)
            GLuint t = _texture3dLUT;
            glDeleteTextures(1, &t);
#endif
        }
        else {
            for(int i = 0; i < GPUState::gpuCount; i++) {
                id<MTLTexture> t = _texture3dLUT.multiTexture[i];
                if (t) {
                    [t release];
                }
            }
        }
        _texture3dLUT.Clear();
    }

    if (_vertexBuffer != 0) {
        delete _vertexBuffer;
        _vertexBuffer = NULL;
    }

    if (_shaderProgram) {
        _shaderProgram.reset();
    }

    if (_copyFramebuffer != 0) {
#if defined(ARCH_GFX_OPENGL)
        glDeleteFramebuffers(1, &_copyFramebuffer);
#endif
    }

    if (_aovFramebuffer != 0) {
#if defined(ARCH_GFX_OPENGL)
        glDeleteFramebuffers(1, &_aovFramebuffer);
#endif
    }

    if (_isOpenGL) {
        GLF_POST_PENDING_GL_ERRORS();
    }
}

std::string
HdxColorCorrectionTask::_CreateOpenColorIOResources()
{
#ifdef PXR_OCIO_PLUGIN_ENABLED
    // Use client provided OCIO values, or use default fallback values
    OCIO::ConstConfigRcPtr config = OCIO::GetCurrentConfig();

    const char* display = _displayOCIO.empty() ?
                          config->getDefaultDisplay() :
                          _displayOCIO.c_str();

    const char* view = _viewOCIO.empty() ?
                       config->getDefaultView(display) :
                       _viewOCIO.c_str();

    const char* looks = _looksOCIO.empty() ?
                        config->getDisplayLooks(display, view) :
                        _looksOCIO.c_str();

    std::string inputColorSpace = _colorspaceOCIO;
    if (inputColorSpace.empty()) {
        OCIO::ConstColorSpaceRcPtr cs = config->getColorSpace("default");
        if (cs) {
            inputColorSpace = cs->getName();
        } else {
            inputColorSpace = OCIO::ROLE_SCENE_LINEAR;
        }
    }

    // Setup the transformation we need to apply
    OCIO::DisplayTransformRcPtr transform = OCIO::DisplayTransform::Create();
    transform->setDisplay(display);
    transform->setView(view);
    transform->setInputColorSpaceName(inputColorSpace.c_str());
    transform->setLooksOverride(looks);
    transform->setLooksOverrideEnabled(true);

    OCIO::ConstProcessorRcPtr processor = config->getProcessor(transform);

    OCIO::GpuLanguage gpuLanguage = OCIO::GPU_LANGUAGE_GLSL_1_0;
    if (!_isOpenGL) {
//            gpuLanguage = OCIO::GPU_LANGUAGE_MSL;
    }


        // If 3D lut size is 0 then use a reasonable default size.
        // We use 65 (0-64) samples which works well with OCIO resampling.
        if (_lut3dSizeOCIO == 0) {
            _lut3dSizeOCIO = 65;
        }

        // Optionally override similar to KATANA_OCIO_LUT3D_EDGE_SIZE
        int size = TfGetenvInt("USDVIEW_OCIO_LUT3D_EDGE_SIZE", 0);
        if (size > 0) {
            _lut3dSizeOCIO = size;
        }

        // Optionally override similar to KATANA_OCIO_LUT3D_EDGE_SIZE
        int size = TfGetenvInt("USDVIEW_OCIO_LUT3D_EDGE_SIZE", 0);
        if (size > 0) {
            _lut3dSizeOCIO = size;
        }
    // Create a GPU Shader Description
    OCIO::GpuShaderDescRcPtr shaderDesc = OCIO::GpuShaderDesc::CreateLegacyShaderDesc(_lut3dSizeOCIO);
    shaderDesc->setLanguage(gpuLanguage);
    shaderDesc->setFunctionName("OCIODisplay");
    shaderDesc->setResourcePrefix("ocio_");

    OCIO::ConstGPUProcessorRcPtr gpuProcessor
        = processor->getDefaultGPUProcessor();
    gpuProcessor->extractGpuShaderInfo(shaderDesc);

    // Load the data into an OpenGL 3D Texture
    if (_texture3dLUT.IsSet()) {
        if (_isOpenGL) {
#if defined(ARCH_GFX_OPENGL)
            GLuint t = _texture3dLUT;
            glDeleteTextures(1, &t);
#endif
        }
        else {
#if defined(ARCH_GFX_METAL)
            for(int i = 0; i < GPUState::gpuCount; i++) {
                id<MTLTexture> t = _texture3dLUT.multiTexture[i];
                if (t) {
                    [t release];
                }
            }
#endif
        }
        _texture3dLUT.Clear();
    }

    // Compute and the 3D LUT
    // 1. Get the information of the 3D LUT.
    const char* name = 0x0;
    const char* uid  = 0x0;
    unsigned edgelen = 0;
    OCIO::Interpolation interpolation = OCIO::INTERP_NEAREST;
    if (shaderDesc->getNum3DTextures()) {
        shaderDesc->get3DTexture(0, name, uid, edgelen, interpolation);

        const float* values = 0x0;
        shaderDesc->get3DTextureValues(0, values);

        // 2. Allocate the 3D LUT.

        if (_isOpenGL) {
#if defined(ARCH_GFX_OPENGL)
            GLint restoreTexture;
            glGetIntegerv(GL_TEXTURE_BINDING_3D, &restoreTexture);
            GLuint t;
            glGenTextures(1, &t);
            _texture3dLUT = t;
            glBindTexture(GL_TEXTURE_3D, t);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
            glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB32F,
                         edgelen, edgelen, edgelen,
                         0, GL_RGB, GL_FLOAT, values);
            glBindTexture(GL_TEXTURE_3D, restoreTexture);
            GLF_POST_PENDING_GL_ERRORS();
#endif
        }
        else {
#if defined(ARCH_GFX_METAL)
            id<MTLDevice> device = MtlfMetalContext::GetMetalContext()->currentDevice;
            MTLTextureDescriptor* desc =
                [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA16Float
                                                                   width:edgelen
                                                                  height:edgelen
                                                               mipmapped:NO];
            desc.resourceOptions = MTLResourceStorageModeDefault;
            desc.depth = edgelen;
            _texture3dLUT = MtlfMultiTexture(desc);
            
            for(int i = 0; i < GPUState::gpuCount; i++) {
                [_texture3dLUT.multiTexture[i] replaceRegion:MTLRegionMake3D(0, 0, 0, edgelen, edgelen, edgelen)
                                                 mipmapLevel:0
                                                       slice:0
                                                   withBytes:&values[edgelen*edgelen*edgelen]
                                                 bytesPerRow:edgelen*sizeof(float)
                                               bytesPerImage:edgelen*edgelen*sizeof(float)];
            }
#endif
        }
    }

    const char* gpuShaderText = shaderDesc->getShaderText();

    return std::string(gpuShaderText);
#endif
    return std::string();
}

bool
HdxColorCorrectionTask::_CreateShaderResources()
{
    if (_shaderProgram) {
        return true;
    }

    // Client can choose to use Hydra's build-in sRGB color correction or use
    // OpenColorIO for color correction in which case we insert extra OCIO code.
    #ifdef PXR_OCIO_PLUGIN_ENABLED
        bool useOCIO = 
            _colorCorrectionMode == HdxColorCorrectionTokens->openColorIO;

        // Only use if $OCIO environment variable is set.
        // (Otherwise this option should be disabled.)
        if (TfGetenv("OCIO") == "") {
            useOCIO = false;
        }
    #else
        bool useOCIO = false;
    #endif

    _shaderProgram.reset(HdStResourceFactory::GetInstance()->NewProgram(
        _tokens->colorCorrectionShader));

    HioGlslfx glslfx(HdxPackageColorCorrectionShader());

    std::string commonCode;
    std::string vtxCode;
    std::string fragCode;

    if (_isOpenGL) {
        commonCode += "#version 120\n"
                      "#define ARCH_GFX_OPENGL\n";
    }
    else {
#if defined(ARCH_GFX_METAL)
        commonCode += HdSt_CodeGenMSL::GetComputeHeader();
#endif
    }

    if (useOCIO) {
        fragCode += "#define GLSLFX_USE_OCIO\n";
    }

    vtxCode += commonCode;
    vtxCode += glslfx.GetSource(_tokens->colorCorrectionVertex);

    fragCode += commonCode;
    fragCode += glslfx.GetSource(_tokens->colorCorrectionFragment);

    if (useOCIO) {
        std::string ocioGpuShaderText = _CreateOpenColorIOResources();
        fragCode += ocioGpuShaderText;
    }

    if (!_shaderProgram->CompileShader(GL_VERTEX_SHADER, vtxCode) ||
        !_shaderProgram->CompileShader(GL_FRAGMENT_SHADER, fragCode) ||
        !_shaderProgram->Link()) {
        TF_CODING_ERROR("Failed to load color correction shader");
        _shaderProgram.reset();
        return false;
    }

    if (!_isOpenGL) {
        _locations[COLOR_IN]  = 0;
        _locations[POSITION]  = 0;
        _locations[UV_IN]     = 1;
    }
    else {
#if defined(ARCH_GFX_OPENGL)
        GLuint programId = boost::dynamic_pointer_cast<HdStGLSLProgram>(_shaderProgram)->GetGLProgram();
        _locations[COLOR_IN]  = glGetUniformLocation(programId, "colorIn");
        _locations[POSITION] = glGetAttribLocation(programId, "position");
        _locations[UV_IN]     = glGetAttribLocation(programId, "uvIn");
        
        if (useOCIO) {
            _locations[LUT3D_IN] = glGetUniformLocation(programId, "LUT3dIn");
        }
#endif
    }

    GLF_POST_PENDING_GL_ERRORS();
    return true;
}

bool
HdxColorCorrectionTask::_CreateBufferResources()
{
    if (_vertexBuffer) {
        return true;
    }

    // A larger-than screen triangle with UVs made to fit the screen.
    //                                 positions          |   uvs
    static const float vertices[] = { -1,  3, -1, 1,        0, 2,
                                      -1, -1, -1, 1,        0, 0,
                                       3, -1, -1, 1,        2, 0 };
    
    HgiBufferDesc desc;
    desc.usage = HgiBufferUsageVertexData;
    desc.length = sizeof(vertices);
    _vertexBuffer = _hgi->CreateBuffer(desc);
    _vertexBuffer->Copy(&vertices[0], 0, sizeof(vertices));

    return true;
}

void
HdxColorCorrectionTask::_CopyTexture()
{
    if (_isOpenGL) {
#if defined(ARCH_GFX_OPENGL)
        GLint restoreReadFB, restoreDrawFB;
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &restoreReadFB);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &restoreDrawFB);

        if (_aovTexture) {
            // If we have an AOV we copy it so we can read from it while writing the
            // color corrected pixels back into the AOV.
            glBindFramebuffer(GL_READ_FRAMEBUFFER, _aovFramebuffer);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _copyFramebuffer);

        } else {
            // No AOV provided then make a copy of the default FB color attachment
            // so we can read from the copy and write back into it corrected pixels.
            glBindFramebuffer(GL_READ_FRAMEBUFFER, restoreDrawFB);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _copyFramebuffer);
        }

        int width = _textureSize[0];
        int height = _textureSize[1];

        glBlitFramebuffer(0, 0, width, height,
                          0, 0, width, height,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, restoreReadFB);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, restoreDrawFB);

        GLF_POST_PENDING_GL_ERRORS();
#endif
    }
}

bool
HdxColorCorrectionTask::_CreateFramebufferResources()
{
#if defined(ARCH_GFX_OPENGL)
    // If framebufferSize is not provided we use the viewport size.
    // This can be incorrect if the client/app has changed the viewport to
    // be different then the render window size. (E.g. UsdView CameraMask mode)
    GfVec2i fboSize = _framebufferSize;
    if (_isOpenGL) {
        if (fboSize[0] <= 0 || fboSize[1] <= 0) {
            GLint res[4] = {0};
            glGetIntegerv(GL_VIEWPORT, res);
            fboSize = GfVec2i(res[2], res[3]);
            _framebufferSize = fboSize;
        }
    }

    bool createTexture = (_texture == 0 || fboSize != _textureSize);

    if (createTexture) {
        if (_texture != 0) {
            delete _texture;
            _texture = NULL;
        }

        _textureSize = fboSize;

        GLint restoreTexture = 0;
        if (_isOpenGL) {
#if defined(ARCH_GFX_OPENGL)
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &restoreTexture);
#endif
        }

        HgiTextureDesc desc;
        desc.format = HgiFormatFloat16Vec4;

        desc.dimensions[0] = _textureSize[0];
        desc.dimensions[1] = _textureSize[1];
        desc.dimensions[2];

        _texture = new HgiTexture(desc);
    
        if (_isOpenGL) {
#if defined(ARCH_GFX_OPENGL)
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            // XXX For now we assume we always want R16F. We could perhaps expose
            //     this as client-API in HdxColorCorrectionTaskParams.
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, _textureSize[0],
                         _textureSize[1], 0, GL_RGBA, GL_FLOAT, 0);

            glBindTexture(GL_TEXTURE_2D, restoreTexture);
#endif
        }
    }

      
    // XXX: Removed due to slowness in the IsCurrent() call when multiple
    //      gl contexts are registered in GlfGLContextRegistry. This code is
    //      relevant only when there is a possibility of having context
    //      switching between the creation of the render pass and the execution
    //      of this task on each frame.
    //
    // bool switchedGLContext = !_owningContext || !_owningContext->IsCurrent();
    // 
    // if (switchedGLContext) {
    //     // If we're rendering with a different context than the render pass
    //     // was created with, recreate the FBO because FB is not shared.
    //     // XXX we need this since we use a FBO in _CopyTexture(). Ideally we
    //     // use HdxFullscreenShader to do the copy, but for that we need to know
    //     // the textureId currently bound to the default framebuffer. However
    //     // glGetFramebufferAttachmentParameteriv will return and error when
    //     // trying to query the texture name bound to GL_BACK_LEFT.
    //     if (_owningContext && _owningContext->IsValid()) {
    //         GlfGLContextScopeHolder contextHolder(_owningContext);
    //         glDeleteFramebuffers(1, &_copyFramebuffer);
    //         glDeleteFramebuffers(1, &_aovFramebuffer);
    //     }
    // 
    //     _owningContext = GlfGLContext::GetCurrentGLContext();
    //     if (!TF_VERIFY(_owningContext, "No valid GL context")) {
    //         return false;
    //     }
    // 
    
        if (_copyFramebuffer == 0) {
            if (_isOpenGL) {
#if defined(ARCH_GFX_OPENGL)
                glGenFramebuffers(1, &_copyFramebuffer);
#endif
            }
        }
        if (_aovFramebuffer == 0) {
            if (_isOpenGL) {
#if defined(ARCH_GFX_OPENGL)
                glGenFramebuffers(1, &_aovFramebuffer);
#endif
            }
        }

    // }
    //

    HgiTextureHandle texHandle = _aovBuffer ? 
        _aovBuffer->GetHgiTextureHandle(/*ms*/false) : nullptr;

    HgiTexture* aovTexture = static_cast<HgiTexture*>(texHandle);

    // XXX: see code comment above. Here we remove switchedGLContext from the
    //      if statement.
    // if (createTexture || switchedGLContext || aovTexture!=_aovTexture) {

    if (createTexture || aovTexture!=_aovTexture) {
   
        _aovTexture = aovTexture;
        if (_isOpenGL) {
#if defined(ARCH_GFX_OPENGL)
            GLint restoreReadFB, restoreDrawFB;
            glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &restoreReadFB);
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &restoreDrawFB);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _copyFramebuffer);

            HgiGLTexture* aovTextureGL = static_cast<HgiGLTexture*>(aovTexture);
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_2D, aovTextureGL->GetTextureId(), 0);

            // If an AOV is provided we'll use its texture on the read FB during
            // CopyTexture.
            if (aovTexture) {
                glBindFramebuffer(GL_READ_FRAMEBUFFER, _aovFramebuffer);
                glFramebufferTexture2D(
                    GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                    GL_TEXTURE_2D, aovTextureGL->GetTextureId(), 0);
            }

            glBindFramebuffer(GL_READ_FRAMEBUFFER, restoreReadFB);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, restoreDrawFB);
#endif
        }
    }

    GLF_POST_PENDING_GL_ERRORS();
#endif
    return true;
}

void
HdxColorCorrectionTask::_ApplyColorCorrection()
{
    // Client can choose to use Hydra's built-in sRGB color correction or use
    // OpenColorIO for color correction in which case we insert extra OCIO code.
    #ifdef PXR_OCIO_PLUGIN_ENABLED
        bool useOCIO = 
            _colorCorrectionMode == HdxColorCorrectionTokens->openColorIO;

        // Only use if $OCIO environment variable is set.
        // (Otherwise this option should be disabled.)
        if (TfGetenv("OCIO") == "") {
            useOCIO = false;
        }
    #else
        bool useOCIO = false;
    #endif

    // A note here: colorCorrection is used for all of our plugins and has to be
    // robust to poor GL support.  OSX compatibility profile provides a
    // GL 2.1 API, slightly restricting our choice of API and heavily
    // restricting our shader syntax. See also HdxFullscreenShader.

    // Read from the texture-copy we made of the clients FBO and output the
    // color-corrected pixels into the clients FBO.

    _shaderProgram->SetProgram();
    
    if (_isOpenGL) {
#if defined(ARCH_GFX_OPENGL)
        glActiveTexture(GL_TEXTURE0);
        HgiGLTexture* hgiGLTexture = static_cast<HgiGLTexture*>(_texture);
        glBindTexture(GL_TEXTURE_2D, hgiGLTexture->GetTextureId());
        glUniform1i(_locations[COLOR_IN], 0);

        if (useOCIO) {
            glEnable(GL_TEXTURE_3D);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_3D, _texture3dLUT);
            glUniform1i(_locations[LUT3D_IN], 1);
        }

        HgiGLBuffer* hgiGLBuffer = static_cast<HgiGLBuffer*>(_vertexBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, hgiGLBuffer->GetBufferId());
        glVertexAttribPointer(_locations[POSITION], 4, GL_FLOAT, GL_FALSE,
                              sizeof(float)*6, 0);
        glEnableVertexAttribArray(_locations[POSITION]);
        glVertexAttribPointer(_locations[UV_IN], 2, GL_FLOAT, GL_FALSE,
                sizeof(float)*6, reinterpret_cast<void*>(sizeof(float)*4));
        glEnableVertexAttribArray(_locations[UV_IN]);

        // We are rendering a full-screen triangle, which would render to depth.
        // Instead we want to preserve the original depth, so disable depth
        // writes.
        GLboolean restoreDepthWriteMask;
        GLboolean restoreStencilWriteMask;
        glGetBooleanv(GL_DEPTH_WRITEMASK, &restoreDepthWriteMask);
        glGetBooleanv(GL_STENCIL_WRITEMASK, &restoreStencilWriteMask);
        glDepthMask(GL_FALSE);
        glStencilMask(GL_FALSE);

        // Depth test must be ALWAYS instead of disabling the depth_test because
        // we still want to write to the depth buffer. Disabling depth_test
        // disables
        // depth_buffer writes.
        GLint restoreDepthFunc;
        glGetIntegerv(GL_DEPTH_FUNC, &restoreDepthFunc);
        glDepthFunc(GL_ALWAYS);

        GLint restoreViewport[4] = {0};
        glGetIntegerv(GL_VIEWPORT, restoreViewport);
        glViewport(0, 0, _framebufferSize[0], _framebufferSize[1]);

        // The app may have alpha blending enabled.
        // We want to pass-through the alpha values, not alpha-blend on top of
        // dest.
        GLboolean restoreblendEnabled;
        glGetBooleanv(GL_BLEND, &restoreblendEnabled);
        glDisable(GL_BLEND);

        // Alpha to coverage would prevent any pixels that have an alpha of 0.0
        // from being written. We want to color correct all pixels. Even
        // background pixels that were set with a clearColor alpha of 0.0
        GLboolean restoreAlphaToCoverage;
        glGetBooleanv(GL_SAMPLE_ALPHA_TO_COVERAGE, &restoreAlphaToCoverage);
        glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);

        glDrawArrays(GL_TRIANGLES, 0, 3);

        if (restoreAlphaToCoverage) {
            glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
        }

        if (restoreblendEnabled) {
            glEnable(GL_BLEND);
        }

        glViewport(restoreViewport[0], restoreViewport[1],
                   restoreViewport[2], restoreViewport[3]);

        glDepthFunc(restoreDepthFunc);
        glDepthMask(restoreDepthWriteMask);
        glStencilMask(restoreStencilWriteMask);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glDisableVertexAttribArray(_locations[POSITION]);
        glDisableVertexAttribArray(_locations[UV_IN]);

        _shaderProgram->UnsetProgram();

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);

        if (useOCIO) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_3D, 0);
            glDisable(GL_TEXTURE_3D);
        }

        GLF_POST_PENDING_GL_ERRORS();
#endif
    }
    
    _shaderProgram->UnsetProgram();
}

void
HdxColorCorrectionTask::Sync(HdSceneDelegate* delegate,
                             HdTaskContext* ctx,
                             HdDirtyBits* dirtyBits)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    if ((*dirtyBits) & HdChangeTracker::DirtyParams) {
        HdxColorCorrectionTaskParams params;

        if (_GetTaskParams(delegate, &params)) {
            _framebufferSize = params.framebufferSize;
            _colorCorrectionMode = params.colorCorrectionMode;
            _displayOCIO = params.displayOCIO;
            _viewOCIO = params.viewOCIO;
            _colorspaceOCIO = params.colorspaceOCIO;
            _looksOCIO = params.looksOCIO;
            _lut3dSizeOCIO = params.lut3dSizeOCIO;
            _aovName = params.aovName;
            _aovBufferPath = params.aovBufferPath;
            // Rebuild shader with new OCIO settings / shader-code.
            _shaderProgram.reset();
        }
    }

    *dirtyBits = HdChangeTracker::Clean;
}

void
HdxColorCorrectionTask::Prepare(HdTaskContext* ctx,
                                HdRenderIndex* renderIndex)
{
    // Aov path may change when visualizing a different aov (usdview)
    if (!_aovBufferPath.IsEmpty()) {
        _aovBuffer = static_cast<HdRenderBuffer*>(renderIndex->GetBprim(
                HdPrimTypeTokens->renderBuffer, _aovBufferPath));
    } else {
        _aovBuffer = nullptr;
    }
}

void
HdxColorCorrectionTask::Execute(HdTaskContext* ctx)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();
    GLF_GROUP_FUNCTION();

    // We currently only color correct the color aov. Depth aov currently won't
    // work well due to how we use glBlitFramebuffer. Other aovs may work, if
    // they are color buffers, but it isn't currently clear if we want to
    // color correct those or leave them as their raw values for debugging.
    if (!_aovName.IsEmpty() && _aovName != HdAovTokens->color) {
        return;
    }

    if (!_CreateBufferResources()) {
        return;
    }

    if (!_CreateShaderResources()) {
        return;
    }

    _CreateFramebufferResources();

    _CopyTexture();

    // If an Aov is provided, we render the color corrected pixels in the aov.
    // Otherwise, we render the color corrected pixels into bound FB.
    GLint restoreReadFB, restoreDrawFB;
    if (_aovTexture) {
        if (_isOpenGL) {
#if defined(ARCH_GFX_OPENGL)
            glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &restoreReadFB);
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &restoreDrawFB);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _aovFramebuffer);
#endif
        }
    }

    _ApplyColorCorrection();

    if (_aovTexture) {
        if (_isOpenGL) {
#if defined(ARCH_GFX_OPENGL)
            glBindFramebuffer(GL_READ_FRAMEBUFFER, restoreReadFB);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, restoreDrawFB);
#endif
        }
    }
}


// -------------------------------------------------------------------------- //
// VtValue Requirements
// -------------------------------------------------------------------------- //

std::ostream& operator<<(
    std::ostream& out, 
    const HdxColorCorrectionTaskParams& pv)
{
    out << "ColorCorrectionTask Params: (...) "
        << pv.framebufferSize << " "
        << pv.colorCorrectionMode << " "
        << pv.displayOCIO << " "
        << pv.viewOCIO << " "
        << pv.colorspaceOCIO << " "
        << pv.looksOCIO << " "
        << pv.lut3dSizeOCIO << " "
        << pv.aovName << " "
        << pv.aovBufferPath
    ;
    return out;
}

bool operator==(const HdxColorCorrectionTaskParams& lhs,
                const HdxColorCorrectionTaskParams& rhs)
{
    return lhs.framebufferSize == rhs. framebufferSize &&
           lhs.colorCorrectionMode == rhs.colorCorrectionMode &&
           lhs.displayOCIO == rhs.displayOCIO &&
           lhs.viewOCIO == rhs.viewOCIO &&
           lhs.colorspaceOCIO == rhs.colorspaceOCIO &&
           lhs.looksOCIO == rhs.looksOCIO &&
           lhs.lut3dSizeOCIO == rhs.lut3dSizeOCIO &&
           lhs.aovName == rhs.aovName &&
           lhs.aovBufferPath == rhs.aovBufferPath;
}

bool operator!=(const HdxColorCorrectionTaskParams& lhs,
                const HdxColorCorrectionTaskParams& rhs)
{
    return !(lhs == rhs);
}

PXR_NAMESPACE_CLOSE_SCOPE
