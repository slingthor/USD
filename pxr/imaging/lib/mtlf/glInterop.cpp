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

#include "pxr/imaging/garch/gl.h"

#include "pxr/imaging/mtlf/glInterop.h"
#include "pxr/imaging/mtlf/package.h"

#include "pxr/imaging/garch/glPlatformContext.h"


PXR_NAMESPACE_OPEN_SCOPE

static GLuint _compileShader(GLchar const* const shaderSource, GLuint shaderType)
{
    GLint status;
    
    // Determine if GLSL version 140 is supported by this context.
    //  We'll use this info to generate a GLSL shader source string
    //  with the proper version preprocessor string prepended
    float  glLanguageVersion;
    
    sscanf((char *)glGetString(GL_SHADING_LANGUAGE_VERSION), "%f", &glLanguageVersion);
    GLchar const * const versionTemplate = "#version %d\n";
    
    // GL_SHADING_LANGUAGE_VERSION returns the version standard version form
    //  with decimals, but the GLSL version preprocessor directive simply
    //  uses integers (thus 1.10 should 110 and 1.40 should be 140, etc.)
    //  We multiply the floating point number by 100 to get a proper
    //  number for the GLSL preprocessor directive
    GLuint version = 100 * glLanguageVersion;
    
    // Prepend our vertex shader source string with the supported GLSL version so
    //  the shader will work on ES, Legacy, and OpenGL 3.2 Core Profile contexts
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
        
        NSLog(@"%s", errorLog);
        free(errorLog);
        
        assert(0);
    }
    
    return s;
}

MtlfGlInterop::StaticGLState MtlfGlInterop::staticState;

void MtlfGlInterop::_InitializeStaticState()
{
    NSError *error = NULL;
    
    // Load our common vertex shader. This is used by both the fragment shaders below
    TfToken vtxShaderToken(MtlfPackageInteropVtxShader());
    GLchar const* const vertexShader =
    (GLchar const*)[[NSString stringWithContentsOfFile:[NSString stringWithUTF8String:vtxShaderToken.GetText()]
                                              encoding:NSUTF8StringEncoding
                                                 error:&error] cStringUsingEncoding:NSUTF8StringEncoding];
    
    GLuint vs = _compileShader(vertexShader, GL_VERTEX_SHADER);
    
    TfToken fragShaderToken(MtlfPackageInteropFragShader());
    GLchar const* const fragmentShader =
    (GLchar const*)[[NSString stringWithContentsOfFile:[NSString stringWithUTF8String:fragShaderToken.GetText()]
                                              encoding:NSUTF8StringEncoding
                                                 error:&error] cStringUsingEncoding:NSUTF8StringEncoding];
    
    GLuint fs = _compileShader(fragmentShader, GL_FRAGMENT_SHADER);
    
    // Create and link our GL_TEXTURE_2D compatible program
    staticState.glShaderProgram = glCreateProgram();
    glAttachShader(staticState.glShaderProgram, fs);
    glAttachShader(staticState.glShaderProgram, vs);
    glBindFragDataLocation(staticState.glShaderProgram, 0, "fragColor");
    glLinkProgram(staticState.glShaderProgram);
    
    GLint maxLength = 2048;
    if (maxLength)
    {
        glGetProgramiv(staticState.glShaderProgram, GL_INFO_LOG_LENGTH, &maxLength);
        
        // The maxLength includes the NULL character
        GLchar *errorLog = (GLchar*)malloc(maxLength);
        glGetProgramInfoLog(staticState.glShaderProgram, maxLength, &maxLength, errorLog);
        
        NSLog(@"%s", errorLog);
        free(errorLog);
    }
    
    // Release the local instance of the fragment shader. The shader program maintains a reference.
    glDeleteShader(vs);
    glDeleteShader(fs);
    
    glUseProgram(staticState.glShaderProgram);
    
    glGenVertexArrays(1, &staticState.glVAO);
    glBindVertexArray(staticState.glVAO);
    
    glGenBuffers(1, &staticState.glVBO);
    glBindBuffer(GL_ARRAY_BUFFER, staticState.glVBO);
    
    // Set up the vertex structure description
    staticState.posAttrib = glGetAttribLocation(staticState.glShaderProgram, "inPosition");
    staticState.texAttrib = glGetAttribLocation(staticState.glShaderProgram, "inTexCoord");
    
    glEnableVertexAttribArray(staticState.posAttrib);
    glVertexAttribPointer(staticState.posAttrib, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(offsetof(Vertex, position)));
    glEnableVertexAttribArray(staticState.texAttrib);
    glVertexAttribPointer(staticState.texAttrib, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(offsetof(Vertex, uv)));
    
    GLint samplerColorLoc = glGetUniformLocation(staticState.glShaderProgram, "interopTexture");
    GLint samplerDepthLoc = glGetUniformLocation(staticState.glShaderProgram, "depthTexture");
    
    staticState.blitTexSizeUniform = glGetUniformLocation(staticState.glShaderProgram, "texSize");
    
    // Indicate that the diffuse texture will be bound to texture unit 0, and depth to unit 1
    GLint unit = 0;
    glUniform1i(samplerColorLoc, unit++);
    glUniform1i(samplerDepthLoc, unit);
    
    Vertex v[6] = {
        { {-1, -1}, {0, 0} },
        { { 1, -1}, {1, 0} },
        { {-1,  1}, {0, 1} },
        
        { {-1, 1}, {0, 1} },
        { {1, -1}, {1, 0} },
        { {1,  1}, {1, 1} }
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

MtlfGlInterop::MtlfGlInterop(id<MTLDevice> _device)
: device(_device)
{
    static std::once_flag once;
    std::call_once(once, [](){
        _InitializeStaticState();
    });

    NSError *error = NULL;

    // Load all the default shader files
    TfToken shaderToken(MtlfPackageDefaultMetalShaders());
    NSString *shaderSource = [NSString stringWithContentsOfFile:[NSString stringWithUTF8String:shaderToken.GetText()]
                                                       encoding:NSUTF8StringEncoding
                                                          error:&error];
    MTLCompileOptions *options = [[MTLCompileOptions alloc] init];
    options.fastMathEnabled = YES;
    
    defaultLibrary = [device newLibraryWithSource:shaderSource options:options error:&error];
    [options release];
    options = nil;
    
    if (!defaultLibrary) {
        NSLog(@"Failed to created pipeline state, error %@", error);
    }
    
    // Load the fragment program into the library
    computeDepthCopyProgram = [defaultLibrary newFunctionWithName:@"copyDepth"];

    // Create the texture caches
    CVReturn cvret = CVMetalTextureCacheCreate(kCFAllocatorDefault, nil, device, nil, &cvmtlTextureCache);
    assert(cvret == kCVReturnSuccess);
    
    CGLContextObj glctx = [[NSOpenGLContext currentContext] CGLContextObj];
    CGLPixelFormatObj glPixelFormat = [[[NSOpenGLContext currentContext] pixelFormat] CGLPixelFormatObj];
    cvret = CVOpenGLTextureCacheCreate(kCFAllocatorDefault, nil, (__bridge CGLContextObj _Nonnull)(glctx), glPixelFormat, nil, &cvglTextureCache);
    assert(cvret == kCVReturnSuccess);

    pixelBuffer = nil;
    depthBuffer = nil;
    cvglColorTexture = nil;
    cvglDepthTexture = nil;
    cvmtlColorTexture = nil;
    cvmtlDepthTexture = nil;
    glColorTexture = 0;
    glDepthTexture = 0;
    mtlColorTexture = nil;
    mtlDepthTexture = nil;
    mtlDepthRegularFloatTexture = nil;

    AllocateAttachments(256, 256);
}

MtlfGlInterop::~MtlfGlInterop()
{
	FreeTransientTextureCacheRefs();

    if (cvglTextureCache) {
        CFRelease(cvglTextureCache);
        cvglTextureCache = nil;
    }
    if (cvmtlTextureCache) {
        CFRelease(cvmtlTextureCache);
        cvmtlTextureCache = nil;
    }
}

void MtlfGlInterop::FreeTransientTextureCacheRefs()
{
    if (glColorTexture) {
        glDeleteTextures(1, &glColorTexture);
        glColorTexture = 0;
    }
    if (glDepthTexture) {
        glDeleteTextures(1, &glDepthTexture);
        glDepthTexture = 0;
    }

    if (mtlColorTexture) {
        [mtlColorTexture release];
        mtlColorTexture = nil;
    }
    if (mtlDepthRegularFloatTexture) {
        [mtlDepthRegularFloatTexture release];
        mtlDepthRegularFloatTexture = nil;
    }
    if (mtlDepthTexture) {
        [mtlDepthTexture release];
        mtlDepthTexture = nil;
    }
    
    cvglColorTexture = nil;
    cvglDepthTexture = nil;
    cvmtlColorTexture = nil;
    cvmtlDepthTexture = nil;

    if (pixelBuffer) {
        CFRelease(pixelBuffer);
        pixelBuffer = nil;
    }
    if (depthBuffer) {
        CFRelease(depthBuffer);
        depthBuffer = nil;
    }
}

void MtlfGlInterop::AllocateAttachments(int width, int height)
{
    NSDictionary* cvBufferProperties = @{
        (__bridge NSString*)kCVPixelBufferOpenGLCompatibilityKey : @(TRUE),
        (__bridge NSString*)kCVPixelBufferMetalCompatibilityKey : @(TRUE),
    };

    FreeTransientTextureCacheRefs();
    
    CVReturn cvret;

    //  Create the IOSurface backed pixel buffers to hold the color and depth data in Open
    CVPixelBufferCreate(kCFAllocatorDefault,
                        width,
                        height,
                        kCVPixelFormatType_32BGRA,
                        (__bridge CFDictionaryRef)cvBufferProperties,
                        &pixelBuffer);
    
    CVPixelBufferCreate(kCFAllocatorDefault,
                        width,
                        height,
                        kCVPixelFormatType_DepthFloat32,
                        (__bridge CFDictionaryRef)cvBufferProperties,
                        &depthBuffer);
    
    // Create the OpenGL texture for the color buffer
    cvret = CVOpenGLTextureCacheCreateTextureFromImage(kCFAllocatorDefault,
                                                       cvglTextureCache,
                                                       pixelBuffer,
                                                       nil,
                                                       &cvglColorTexture);
    assert(cvret == kCVReturnSuccess);
    glColorTexture = CVOpenGLTextureGetName(cvglColorTexture);
    
    // Create the OpenGL texture for the depth buffer
    cvret = CVOpenGLTextureCacheCreateTextureFromImage(kCFAllocatorDefault,
                                                       cvglTextureCache,
                                                       depthBuffer,
                                                       nil,
                                                       &cvglDepthTexture);
    assert(cvret == kCVReturnSuccess);
    glDepthTexture = CVOpenGLTextureGetName(cvglDepthTexture);
    
    // Create the metal texture for the color buffer
    NSDictionary* metalTextureProperties = @{
        (__bridge NSString*)kCVMetalTextureCacheMaximumTextureAgeKey : @0,
    };
    cvret = CVMetalTextureCacheCreateTextureFromImage(kCFAllocatorDefault,
                                                      cvmtlTextureCache,
                                                      pixelBuffer,
                                                      (__bridge CFDictionaryRef)metalTextureProperties,
                                                      MTLPixelFormatBGRA8Unorm,
                                                      width,
                                                      height,
                                                      0,
                                                      &cvmtlColorTexture);
    assert(cvret == kCVReturnSuccess);
    mtlColorTexture = CVMetalTextureGetTexture(cvmtlColorTexture);
    
    // Create the Metal texture for the depth buffer
    cvret = CVMetalTextureCacheCreateTextureFromImage(kCFAllocatorDefault,
                                                      cvmtlTextureCache,
                                                      depthBuffer,
                                                      (__bridge CFDictionaryRef)metalTextureProperties,
                                                      MTLPixelFormatR32Float,
                                                      width,
                                                      height,
                                                      0,
                                                      &cvmtlDepthTexture);
    assert(cvret == kCVReturnSuccess);
    mtlDepthRegularFloatTexture = CVMetalTextureGetTexture(cvmtlDepthTexture);
    
    // Create a Metal texture of type Depth32Float that we can actually use as a depth attachment
    MTLTextureDescriptor *depthTexDescriptor =
                            [MTLTextureDescriptor
                             texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                             width:width
                             height:height
                             mipmapped:false];
    depthTexDescriptor.usage = MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;
    depthTexDescriptor.resourceOptions = MTLResourceCPUCacheModeDefaultCache | MTLResourceStorageModePrivate;
    mtlDepthTexture = [device newTextureWithDescriptor:depthTexDescriptor];
    
    // Flush the caches
    CVOpenGLTextureCacheFlush(cvglTextureCache, 0);
    CVMetalTextureCacheFlush(cvmtlTextureCache, 0);
}

void
MtlfGlInterop::BlitColorTargetToOpenGL()
{
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
    glDisable(GL_BLEND);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    
    glUseProgram(staticState.glShaderProgram);
    
    glBindBuffer(GL_ARRAY_BUFFER, staticState.glVBO);
    
    // Set up the vertex structure description
    if (core) {
        glBindVertexArray(staticState.glVAO);
    }
    glEnableVertexAttribArray(staticState.posAttrib);
    glVertexAttribPointer(staticState.posAttrib, 2, GL_FLOAT, GL_FALSE, sizeof(MtlfMetalContext::Vertex), (void*)(offsetof(Vertex, position)));
    glEnableVertexAttribArray(staticState.texAttrib);
    glVertexAttribPointer(staticState.texAttrib, 2, GL_FLOAT, GL_FALSE, sizeof(MtlfMetalContext::Vertex), (void*)(offsetof(Vertex, uv)));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_RECTANGLE, glColorTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_RECTANGLE, glDepthTexture);

    glUniform2f(staticState.blitTexSizeUniform, mtlColorTexture.width, mtlColorTexture.height);
    
    glDrawArrays(GL_TRIANGLES, 0, 6);
    
    glFlush();
    
    glBindTexture(GL_TEXTURE_RECTANGLE, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_RECTANGLE, 0);
    
    glDisableVertexAttribArray(staticState.posAttrib);
    glDisableVertexAttribArray(staticState.texAttrib);
    glUseProgram(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    if (core) {
        glBindVertexArray(0);
    }
    else {
        glPopAttrib();
    }
}

void
MtlfGlInterop::CopyDepthTextureToOpenGL(id<MTLComputeCommandEncoder> computeEncoder)
{
    MtlfMetalContextSharedPtr context(MtlfMetalContext::GetMetalContext());

    NSUInteger exeWidth = context->SetComputeEncoderState(computeDepthCopyProgram, 0, 0, @"Depth copy pipeline state");
    NSUInteger maxThreadsPerThreadgroup = context->GetMaxThreadsPerThreadgroup();

    MTLSize threadgroupCount = MTLSizeMake(exeWidth, maxThreadsPerThreadgroup / exeWidth, 1);
    MTLSize threadsPerGrid   = MTLSizeMake((mtlDepthTexture.width + (threadgroupCount.width - 1)) / threadgroupCount.width,
                                           (mtlDepthTexture.height + (threadgroupCount.height - 1)) / threadgroupCount.height,
                                           1);

    [computeEncoder setTexture:mtlDepthTexture atIndex:0];
    [computeEncoder setTexture:mtlDepthRegularFloatTexture atIndex:1];
    
    [computeEncoder dispatchThreadgroups:threadsPerGrid threadsPerThreadgroup:threadgroupCount];
}

PXR_NAMESPACE_CLOSE_SCOPE

