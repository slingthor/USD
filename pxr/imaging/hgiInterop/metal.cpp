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

#include "pxr/imaging/hgiInterop/metal.h"

#include "pxr/imaging/hgiMetal/diagnostic.h"
#include "pxr/imaging/hgiMetal/hgi.h"
#include "pxr/imaging/hgiMetal/immediateCommandBuffer.h"

#include "pxr/base/tf/diagnostic.h"

PXR_NAMESPACE_OPEN_SCOPE

struct Vertex {
    float position[2];
    float uv[2];
};

static GLuint _compileShader(
    GLchar const* const shaderSource, GLuint shaderType)
{
    GLint status;
    
    // Determine if GLSL version 140 is supported by this context.
    //  We'll use this info to generate a GLSL shader source string
    //  with the proper version preprocessor string prepended
    float  glLanguageVersion;
    
    sscanf((char *)glGetString(GL_SHADING_LANGUAGE_VERSION), "%f",
        &glLanguageVersion);
    GLchar const * const versionTemplate = "#version %d\n";
    
    // GL_SHADING_LANGUAGE_VERSION returns the version standard version form
    //  with decimals, but the GLSL version preprocessor directive simply
    //  uses integers (thus 1.10 should 110 and 1.40 should be 140, etc.)
    //  We multiply the floating point number by 100 to get a proper
    //  number for the GLSL preprocessor directive
    GLuint version = 100 * glLanguageVersion;
    
    // Prepend our vertex shader source string with the supported GLSL version
    // so the shader will work on ES, Legacy, and OpenGL 3.2 Core Profile
    // contexts
    GLchar versionString[sizeof(versionTemplate) + 8];
    snprintf(versionString, sizeof(versionString), versionTemplate, version);
    
    GLuint s = glCreateShader(shaderType);
    GLchar const* const sourceArray[2] = { versionString, shaderSource };
    glShaderSource(s, 2, sourceArray, NULL);
    glCompileShader(s);
    
    glGetShaderiv(s, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        GLint maxLength = 0;
        
        glGetShaderiv(s, GL_INFO_LOG_LENGTH, &maxLength);
        
        // The maxLength includes the NULL character
        GLchar *errorLog = (GLchar*)malloc(maxLength);
        glGetShaderInfoLog(s, maxLength, &maxLength, errorLog);
        
        TF_WARN("%s", errorLog);
        free(errorLog);
        
        assert(0);
    }
    
    return s;
}

HgiInteropMetal::HgiInteropMetal(
    id<MTLDevice> interopDevice)
: _device(interopDevice)
{
    NSError *error = NULL;

    // Load our common vertex shader. This is used by both the fragment shaders
    // below
    GLchar const* const vertexShader =
        "attribute vec2 inPosition;\n"
        "attribute vec2 inTexCoord;\n"
        "varying vec2 texCoord;\n"
        "\n"
        "void main()\n"
        "{\n"
        "    texCoord = inTexCoord;\n"
        "    gl_Position = vec4(inPosition, 0.0, 1.0);\n"
        "}\n";
    
    GLuint vs = _compileShader(vertexShader, GL_VERTEX_SHADER);
    
    GLchar const* const fragmentShader =
        "varying vec2    texCoord;\n"
        "\n"
        // A GL_TEXTURE_RECTANGLE
        "uniform sampler2DRect interopTexture;\n"
        "uniform sampler2DRect depthTexture;\n"
        "\n"
        // The dimensions of the source texture. The sampler coordinates for
        // a GL_TEXTURE_RECTANGLE are in pixels,
        // rather than the usual normalised 0..1 range.
        "uniform vec2 texSize;\n"
        "\n"
        "void main(void)\n"
        "{\n"
        "    vec2 uv = vec2(texCoord.x, 1.0 - texCoord.y) * texSize;\n"
        "    gl_FragColor = texture2DRect(interopTexture, uv.st);\n"
        "    gl_FragDepth = texture2DRect(depthTexture, uv.st).r;\n"
        "}\n";

    GLuint fs = _compileShader(fragmentShader, GL_FRAGMENT_SHADER);
    
    // Create and link our GL_TEXTURE_2D compatible program
    _glShaderProgram = glCreateProgram();
    glAttachShader(_glShaderProgram, fs);
    glAttachShader(_glShaderProgram, vs);
    glLinkProgram(_glShaderProgram);
    
    GLint maxLength = 2048;
    glGetProgramiv(_glShaderProgram, GL_INFO_LOG_LENGTH, &maxLength);
    if (maxLength)
    {
        // The maxLength includes the NULL character
        GLchar *errorLog = (GLchar*)malloc(maxLength);
        glGetProgramInfoLog(_glShaderProgram, maxLength, &maxLength, errorLog);
        
        TF_FATAL_CODING_ERROR("%s", errorLog);
        free(errorLog);
    }
    
    // Release the local instance of the fragment shader. The shader program
    // maintains a reference.
    glDeleteShader(vs);
    glDeleteShader(fs);
    
    glUseProgram(_glShaderProgram);
    
    glGenVertexArrays(1, &_glVAO);
    glBindVertexArray(_glVAO);
    
    glGenBuffers(1, &_glVBO);
    glBindBuffer(GL_ARRAY_BUFFER, _glVBO);
    
    // Set up the vertex structure description
    _posAttrib = glGetAttribLocation(_glShaderProgram, "inPosition");
    _texAttrib = glGetAttribLocation(_glShaderProgram, "inTexCoord");
    
    glEnableVertexAttribArray(_posAttrib);
    glVertexAttribPointer(_posAttrib,
                          2,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(Vertex),
                          (void*)(offsetof(Vertex, position)));
    glEnableVertexAttribArray(_texAttrib);
    glVertexAttribPointer(_texAttrib,
                          2,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(Vertex),
                          (void*)(offsetof(Vertex, uv)));
    
    GLint samplerColorLoc =
        glGetUniformLocation(_glShaderProgram, "interopTexture");
    GLint samplerDepthLoc =
        glGetUniformLocation(_glShaderProgram, "depthTexture");
    
    _blitTexSizeUniform = glGetUniformLocation(_glShaderProgram, "texSize");
    
    // Indicate that the diffuse texture will be bound to texture unit 0
    // and depth to unit 1
    GLint unit = 0;
    glUniform1i(samplerColorLoc, unit++);
    glUniform1i(samplerDepthLoc, unit);
    
    Vertex v[12] = {
        { {-1, -1}, {0, 0} },
        { { 1, -1}, {1, 0} },
        { {-1,  1}, {0, 1} },
        
        { {-1, 1}, {0, 1} },
        { {1, -1}, {1, 0} },
        { {1,  1}, {1, 1} },
        
        // Second set have flipped v coord
        { {-1, -1}, {0, 1} },
        { { 1, -1}, {1, 1} },
        { {-1,  1}, {0, 0} },
        
        { {-1, 1}, {0, 0} },
        { {1, -1}, {1, 1} },
        { {1,  1}, {1, 0} }
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    error = NULL;

    // Load all the default shader files
    NSString *shaderSource =
        @"#include <metal_stdlib>\n"
        "#include <simd/simd.h>\n"
        "\n"
        "using namespace metal;\n"
        "\n"
        // Depth buffer copy function
        "kernel void copyDepth(depth2d<float, access::read> texIn,\n"
        "                      texture2d<float, access::write> texOut,\n"
        "                      uint2 gid [[thread_position_in_grid]])\n"
        "{\n"
        "    if(gid.x >= texOut.get_width() || gid.y >= texOut.get_height())\n"
        "        return;\n"
        "    texOut.write(float(texIn.read(gid)), gid);\n"
        "}\n"
        "\n"
        "kernel void copyColour(\n"
        "    texture2d<float, access::read> texIn,\n"
        "    texture2d<float, access::write> texOut,\n"
        "    uint2 gid [[thread_position_in_grid]])\n"
        "{\n"
        "    if(gid.x >= texOut.get_width() || gid.y >= texOut.get_height())\n"
        "        return;\n"
        "    texOut.write(texIn.read(gid), gid);\n"
        "}\n";

    MTLCompileOptions *options = [[MTLCompileOptions alloc] init];
    options.fastMathEnabled = YES;
        
    _defaultLibrary = [_device newLibraryWithSource:shaderSource
                                            options:options
                                              error:&error];
    
    if (!_defaultLibrary) {
        NSString *errStr = [error localizedDescription];
        TF_FATAL_CODING_ERROR(
            "Failed to create interop pipeline state: %s",
            [errStr UTF8String]);
        [errStr release];
    }
    
    // Load the fragment program into the library
    _computeDepthCopyProgram =
        [_defaultLibrary newFunctionWithName:@"copyDepth"];
    _computeColorCopyProgram =
        [_defaultLibrary newFunctionWithName:@"copyColour"];
    
    [options release];
    options = nil;

    
    MTLAutoreleasedComputePipelineReflection* reflData = 0;
    
    MTLComputePipelineDescriptor *computePipelineStateDescriptor =
        [[MTLComputePipelineDescriptor alloc] init];
    
    computePipelineStateDescriptor.computeFunction = _computeDepthCopyProgram;
    HGIMETAL_DEBUG_LABEL(computePipelineStateDescriptor, "Interop depth blit");
    
    // Create a new Compute pipeline state object
    _computePipelineStateDepth = [_device
        newComputePipelineStateWithDescriptor:computePipelineStateDescriptor
                                      options:MTLPipelineOptionNone
                                   reflection:reflData
                                        error:&error];

    if (!_computePipelineStateDepth) {
        NSString *errStr = [error localizedDescription];
        TF_FATAL_CODING_ERROR(
            "Failed to create compute pipeline state, error %s",
            [errStr UTF8String]);
        [errStr release];
        [error release];
    }
        
    computePipelineStateDescriptor.computeFunction = _computeColorCopyProgram;
    HGIMETAL_DEBUG_LABEL(computePipelineStateDescriptor, "Interop color blit");
    
    // Create a new Compute pipeline state object
    _computePipelineStateColor = [_device
        newComputePipelineStateWithDescriptor:computePipelineStateDescriptor
                                      options:MTLPipelineOptionNone
                                   reflection:reflData
                                        error:&error];
    [computePipelineStateDescriptor release];

    if (!_computePipelineStateColor) {
        NSString *errStr = [error localizedDescription];
        TF_FATAL_CODING_ERROR(
            "Failed to create compute pipeline state, error %s",
            [errStr UTF8String]);
        [errStr release];
        [error release];
    }

    CVReturn cvret;

    // Create the texture caches
    cvret = CVMetalTextureCacheCreate(
        kCFAllocatorDefault, nil, _device, nil, &_cvmtlTextureCache);
    assert(cvret == kCVReturnSuccess);

    _cvmtlColorTexture = nil;
    _cvmtlDepthTexture = nil;
    
    _mtlAliasedColorTexture = nil;
    _mtlAliasedDepthRegularFloatTexture = nil;
    
    CGLContextObj glctx = [[NSOpenGLContext currentContext] CGLContextObj];
    CGLPixelFormatObj glPixelFormat =
        [[[NSOpenGLContext currentContext] pixelFormat] CGLPixelFormatObj];
    cvret = CVOpenGLTextureCacheCreate(
        kCFAllocatorDefault, nil, (__bridge CGLContextObj _Nonnull)(glctx),
        glPixelFormat, nil, &_cvglTextureCache);
    assert(cvret == kCVReturnSuccess);
    
    _glInteropCtx = [[NSOpenGLContext alloc]
                      initWithFormat:[[NSOpenGLContext currentContext]
                         pixelFormat]
                      shareContext:[NSOpenGLContext currentContext]
                     ];

    _pixelBuffer = nil;
    _depthBuffer = nil;
    _cvglColorTexture = nil;
    _cvglDepthTexture = nil;
    _glColorTexture = 0;
    _glDepthTexture = 0;
    
    AllocateAttachments(256, 256);
}

HgiInteropMetal::~HgiInteropMetal()
{
	_FreeTransientTextureCacheRefs();

    if (_cvglTextureCache) {
        CFRelease(_cvglTextureCache);
        _cvglTextureCache = nil;
    }
    if (_cvmtlTextureCache) {
        CFRelease(_cvmtlTextureCache);
        _cvmtlTextureCache = nil;
    }
}

void HgiInteropMetal::_FreeTransientTextureCacheRefs()
{
    if (_glColorTexture) {
        glDeleteTextures(1, &_glColorTexture);
        _glColorTexture = 0;
    }
    if (_glDepthTexture) {
        glDeleteTextures(1, &_glDepthTexture);
        _glDepthTexture = 0;
    }
    
    if (_mtlAliasedColorTexture) {
        [_mtlAliasedColorTexture release];
        _mtlAliasedColorTexture = nil;
    }
    if (_mtlAliasedDepthRegularFloatTexture) {
        [_mtlAliasedDepthRegularFloatTexture release];
        _mtlAliasedDepthRegularFloatTexture = nil;
    }

    _cvmtlColorTexture = nil;
    _cvmtlDepthTexture = nil;

    _cvglColorTexture = nil;
    _cvglDepthTexture = nil;

    if (_pixelBuffer) {
        CFRelease(_pixelBuffer);
        _pixelBuffer = nil;
    }
    if (_depthBuffer) {
        CFRelease(_depthBuffer);
        _depthBuffer = nil;
    }
}

void HgiInteropMetal::AllocateAttachments(int width, int height)
{
    NSDictionary* cvBufferProperties = @{
        (__bridge NSString*)kCVPixelBufferOpenGLCompatibilityKey : @(TRUE),
        (__bridge NSString*)kCVPixelBufferMetalCompatibilityKey : @(TRUE),
    };

    _FreeTransientTextureCacheRefs();
    
    CVReturn cvret;

    // Create the IOSurface backed pixel buffers to hold the color and depth
    // data in OpenGL
    CVPixelBufferCreate(
        kCFAllocatorDefault,
        width,
        height,
        //kCVPixelFormatType_32BGRA,
        kCVPixelFormatType_64RGBAHalf,
        (__bridge CFDictionaryRef)cvBufferProperties,
        &_pixelBuffer);
    
    CVPixelBufferCreate(
        kCFAllocatorDefault,
        width,
        height,
        kCVPixelFormatType_DepthFloat32,
        (__bridge CFDictionaryRef)cvBufferProperties,
        &_depthBuffer);
    
    // Create the OpenGL texture for the color buffer
    cvret = CVOpenGLTextureCacheCreateTextureFromImage(
        kCFAllocatorDefault,
        _cvglTextureCache,
        _pixelBuffer,
        nil,
        &_cvglColorTexture);
    assert(cvret == kCVReturnSuccess);
    _glColorTexture = CVOpenGLTextureGetName(_cvglColorTexture);
    
    // Create the OpenGL texture for the depth buffer
    cvret = CVOpenGLTextureCacheCreateTextureFromImage(kCFAllocatorDefault,
                                                       _cvglTextureCache,
                                                       _depthBuffer,
                                                       nil,
                                                       &_cvglDepthTexture);
    assert(cvret == kCVReturnSuccess);
    _glDepthTexture = CVOpenGLTextureGetName(_cvglDepthTexture);
    
    // Create the metal texture for the color buffer
    NSDictionary* metalTextureProperties = @{
        (__bridge NSString*)kCVMetalTextureCacheMaximumTextureAgeKey : @0,
    };
    cvret = CVMetalTextureCacheCreateTextureFromImage(
        kCFAllocatorDefault,
        _cvmtlTextureCache,
        _pixelBuffer,
        (__bridge CFDictionaryRef)metalTextureProperties,
        MTLPixelFormatRGBA16Float,
        width,
        height,
        0,
        &_cvmtlColorTexture);
    assert(cvret == kCVReturnSuccess);
    _mtlAliasedColorTexture = CVMetalTextureGetTexture(_cvmtlColorTexture);
    
    // Create the Metal texture for the depth buffer
    cvret = CVMetalTextureCacheCreateTextureFromImage(
        kCFAllocatorDefault,
        _cvmtlTextureCache,
        _depthBuffer,
        (__bridge CFDictionaryRef)metalTextureProperties,
        MTLPixelFormatR32Float,
        width,
        height,
        0,
        &_cvmtlDepthTexture);
    assert(cvret == kCVReturnSuccess);
    _mtlAliasedDepthRegularFloatTexture =
        CVMetalTextureGetTexture(_cvmtlDepthTexture);

    MTLTextureDescriptor *depthTexDescriptor =
        [MTLTextureDescriptor
         texture2DDescriptorWithPixelFormat:MTLPixelFormatR32Float
         width:width
         height:height
         mipmapped:false];
    depthTexDescriptor.usage =
        MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite;
    depthTexDescriptor.resourceOptions =
        MTLResourceCPUCacheModeDefaultCache | MTLResourceStorageModePrivate;
    
    // Flush the caches
    CVOpenGLTextureCacheFlush(_cvglTextureCache, 0);
    CVMetalTextureCacheFlush(_cvmtlTextureCache, 0);
}

void
HgiInteropMetal::_CaptureOpenGlState()
{
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &_restoreVao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &_restoreVbo);
    glGetBooleanv(GL_DEPTH_TEST, (GLboolean*)&_restoreDepthTest);
    glGetBooleanv(GL_DEPTH_WRITEMASK, (GLboolean*)&_restoreDepthWriteMask);
    glGetBooleanv(GL_STENCIL_WRITEMASK, (GLboolean*)&_restoreStencilWriteMask);
    glGetIntegerv(GL_DEPTH_FUNC, &_restoreDepthFunc);
    glGetIntegerv(GL_VIEWPORT, _restoreViewport);
    glGetBooleanv(GL_BLEND, (GLboolean*)&_restoreblendEnabled);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &_restoreColorOp);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &_restoreAlphaOp);
    glGetIntegerv(GL_BLEND_SRC_RGB, &_restoreColorSrcFnOp);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &_restoreAlphaSrcFnOp);
    glGetIntegerv(GL_BLEND_DST_RGB, &_restoreColorDstFnOp);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &_restoreAlphaDstFnOp);
    glGetBooleanv(
        GL_SAMPLE_ALPHA_TO_COVERAGE,
        (GLboolean*)&_restoreAlphaToCoverage);
}

void
HgiInteropMetal::_RestoreOpenGlState()
{
    if (_restoreAlphaToCoverage) {
        glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    } else {
        glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    }

    glBlendFuncSeparate(_restoreColorSrcFnOp, _restoreColorDstFnOp,
                        _restoreAlphaSrcFnOp, _restoreAlphaDstFnOp);
    glBlendEquationSeparate(_restoreColorOp, _restoreAlphaOp);

    if (_restoreblendEnabled) {
        glEnable(GL_BLEND);
    } else {
        glDisable(GL_BLEND);
    }

    glViewport(_restoreViewport[0], _restoreViewport[1],
               _restoreViewport[2], _restoreViewport[3]);
    glDepthFunc(_restoreDepthFunc);
    glDepthMask(_restoreDepthWriteMask);
    glStencilMask(_restoreStencilWriteMask);

    if (_restoreDepthTest) {
        glEnable(GL_DEPTH_TEST);
    } else {
        glDisable(GL_DEPTH_TEST);
    }

    glBindVertexArray(_restoreVao);
    glBindBuffer(GL_ARRAY_BUFFER, _restoreVbo);
}

void
HgiInteropMetal::_BlitToOpenGL(bool flipY)
{
    _CaptureOpenGlState();

    GLint core;
    glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &core);
    core &= GL_CONTEXT_CORE_PROFILE_BIT;

    if (!core) {
        glPushAttrib(GL_ENABLE_BIT | GL_POLYGON_BIT | GL_DEPTH_BUFFER_BIT);
    }
    
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glFrontFace(GL_CCW);
    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
    glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
    
    glUseProgram(_glShaderProgram);
    
    glBindBuffer(GL_ARRAY_BUFFER, _glVBO);
    
    // Set up the vertex structure description
    if (core) {
        glBindVertexArray(_glVAO);
    }
    glEnableVertexAttribArray(_posAttrib);
    glVertexAttribPointer(
        _posAttrib, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
        (void*)(offsetof(Vertex, position)));
    glEnableVertexAttribArray(_texAttrib);
    glVertexAttribPointer(
        _texAttrib, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
        (void*)(offsetof(Vertex, uv)));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_RECTANGLE, _glColorTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_RECTANGLE, _glDepthTexture);

    glUniform2f(_blitTexSizeUniform,
                _mtlAliasedColorTexture.width,
                _mtlAliasedColorTexture.height);
    
    if (flipY) {
        glDrawArrays(GL_TRIANGLES, 6, 12);
    }
    else {
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
    
    glBindTexture(GL_TEXTURE_RECTANGLE, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_RECTANGLE, 0);
    
    glDisableVertexAttribArray(_posAttrib);
    glDisableVertexAttribArray(_texAttrib);
    glUseProgram(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    if (core) {
        glBindVertexArray(0);
    }
    else {
        glPopAttrib();
    }

    glFlush();
    
    _RestoreOpenGlState();
}

void
HgiInteropMetal::CopyToInterop(
    Hgi* hgi,
    id<MTLTexture> sourceColorTexture,
    id<MTLTexture> sourceDepthTexture,
    bool flipImage)
{
    HgiMetal* metalHgi = static_cast<HgiMetal*>(hgi);
    HgiMetalImmediateCommandBuffer* metalIcb =
        static_cast<HgiMetalImmediateCommandBuffer*>(
            &hgi->GetImmediateCommandBuffer());
    id<MTLCommandBuffer> commandBuffer = metalIcb->GetCommandBuffer();
    
    id<MTLComputeCommandEncoder> computeEncoder;
    
    if (metalHgi->GetConcurrentDispatch()) {
        computeEncoder = [commandBuffer
         computeCommandEncoderWithDispatchType:MTLDispatchTypeConcurrent];
    }
    else {
        computeEncoder = [commandBuffer computeCommandEncoder];
    }

    //
    // Depth
    //
    if (sourceDepthTexture) {
        NSUInteger exeWidth = [_computePipelineStateDepth threadExecutionWidth];
        NSUInteger maxThreadsPerThreadgroup =
            [_computePipelineStateDepth maxTotalThreadsPerThreadgroup];

        MTLSize threadgroupCount = MTLSizeMake(exeWidth,
            maxThreadsPerThreadgroup / exeWidth, 1);
        MTLSize threadsPerGrid   = MTLSizeMake(
            (_mtlAliasedDepthRegularFloatTexture.width +
                (threadgroupCount.width - 1)) / threadgroupCount.width,
            (_mtlAliasedDepthRegularFloatTexture.height +
                (threadgroupCount.height - 1)) / threadgroupCount.height,
            1);

        [computeEncoder setComputePipelineState:_computePipelineStateDepth];
        [computeEncoder setTexture:sourceDepthTexture atIndex:0];
        [computeEncoder setTexture:_mtlAliasedDepthRegularFloatTexture
                           atIndex:1];
        
        [computeEncoder dispatchThreadgroups:threadsPerGrid
                       threadsPerThreadgroup:threadgroupCount];
    }
    
    //
    // Color
    //
    if (sourceColorTexture) {
        NSUInteger exeWidth = [_computePipelineStateColor threadExecutionWidth];
        NSUInteger maxThreadsPerThreadgroup =
            [_computePipelineStateColor maxTotalThreadsPerThreadgroup];

        MTLSize threadgroupCount = MTLSizeMake(exeWidth,
            maxThreadsPerThreadgroup / exeWidth, 1);
        MTLSize threadsPerGrid   = MTLSizeMake(
            (_mtlAliasedColorTexture.width +
                (threadgroupCount.width - 1)) / threadgroupCount.width,
            (_mtlAliasedColorTexture.height +
                (threadgroupCount.height - 1)) / threadgroupCount.height,
            1);

        [computeEncoder setComputePipelineState:_computePipelineStateColor];
        [computeEncoder setTexture:sourceColorTexture atIndex:0];
        [computeEncoder setTexture:_mtlAliasedColorTexture atIndex:1];
        
        [computeEncoder dispatchThreadgroups:threadsPerGrid
                       threadsPerThreadgroup:threadgroupCount];
    }

    [computeEncoder endEncoding];
    
    // We wait until the work is scheduled for execution so that future OpenGL
    // calls are guaranteed to happen after the Metal work encoded above
    metalIcb->BlockUntilSubmitted();

    _BlitToOpenGL(flipImage);
    glGetError();
}

PXR_NAMESPACE_CLOSE_SCOPE

