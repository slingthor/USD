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
    glGetError();
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

MtlfGlInterop::MtlfGlInterop(id<MTLDevice> _interopDevice, NSMutableArray<id<MTLDevice>> *_renderDevices)
: interopDevice(_interopDevice)
, renderDevices(_renderDevices)
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
    
    memset(gpus, 0x00, sizeof(gpus));
    
    for(int i = 0; i < renderDevices.count; i++) {
        gpus[i].defaultLibrary = [renderDevices[i] newLibraryWithSource:shaderSource options:options error:&error];
        
        if (!gpus[i].defaultLibrary) {
            NSLog(@"Failed to created pipeline state, error %@", error);
        }
        
        // Load the fragment program into the library
        gpus[i].computeDepthCopyProgram = [gpus[i].defaultLibrary newFunctionWithName:@"copyDepth"];
        gpus[i].computeDepthCopyMultisampleProgram = [gpus[i].defaultLibrary newFunctionWithName:@"copyDepthMultisample"];
        gpus[i].computeColourCopyProgram = [gpus[i].defaultLibrary newFunctionWithName:@"copyColour"];
        gpus[i].computeColourCopyMultisampleProgram = [gpus[i].defaultLibrary newFunctionWithName:@"copyColourMultisample"];

        mtlLocalColorTexture[i] = nil;
        mtlLocalDepthTexture[i] = nil;
    }
    [options release];
    options = nil;

    CVReturn cvret;

    // Create the texture caches
    for (int i = 0; i < renderDevices.count; i++) {
        cvret = CVMetalTextureCacheCreate(kCFAllocatorDefault, nil, renderDevices[i], nil, &cvmtlTextureCache[i]);
        assert(cvret == kCVReturnSuccess);
        
        cvmtlColorTexture[i] = nil;
        cvmtlDepthTexture[i] = nil;
        
        mtlAliasedColorTexture[i] = nil;
        mtlAliasedDepthRegularFloatTexture[i] = nil;
    }
    
    CGLContextObj glctx = [[NSOpenGLContext currentContext] CGLContextObj];
    CGLPixelFormatObj glPixelFormat = [[[NSOpenGLContext currentContext] pixelFormat] CGLPixelFormatObj];
    cvret = CVOpenGLTextureCacheCreate(kCFAllocatorDefault, nil, (__bridge CGLContextObj _Nonnull)(glctx), glPixelFormat, nil, &cvglTextureCache);
    assert(cvret == kCVReturnSuccess);

    pixelBuffer = nil;
    depthBuffer = nil;
    cvglColorTexture = nil;
    cvglDepthTexture = nil;
    glColorTexture = 0;
    glDepthTexture = 0;
    
    mtlSampleCount = 1;

    AllocateAttachments(256, 256);
}

MtlfGlInterop::~MtlfGlInterop()
{
	FreeTransientTextureCacheRefs();

    if (cvglTextureCache) {
        CFRelease(cvglTextureCache);
        cvglTextureCache = nil;
    }
    for (int i = 0; i < renderDevices.count; i++) {
        if (cvmtlTextureCache[i]) {
            CFRelease(cvmtlTextureCache[i]);
            cvmtlTextureCache[i] = nil;
        }
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
    
    for (int i = 0; i < renderDevices.count; i++) {
        if (mtlAliasedColorTexture[i]) {
            [mtlAliasedColorTexture[i] release];
            mtlAliasedColorTexture[i] = nil;
        }
        if (mtlAliasedDepthRegularFloatTexture[i]) {
            [mtlAliasedDepthRegularFloatTexture[i] release];
            mtlAliasedDepthRegularFloatTexture[i] = nil;
        }

        if (mtlLocalColorTexture[i]) {
            if (renderDevices[i] != interopDevice) {
//                [mtlLocalColorTexture[i] release];
            }
            mtlLocalColorTexture[i] = nil;
        }
        if (mtlLocalDepthTexture[i]) {
            [mtlLocalDepthTexture[i] release];
            mtlLocalDepthTexture[i] = nil;
        }
        
        cvmtlColorTexture[i] = nil;
        cvmtlDepthTexture[i] = nil;
    }

    cvglColorTexture = nil;
    cvglDepthTexture = nil;

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
    if (TF_DEV_BUILD) {
        NSLog(@"Resizing targets: %ix%i", width, height);
    }

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
    for (int i = 0; i < renderDevices.count; i++) {
        cvret = CVMetalTextureCacheCreateTextureFromImage(kCFAllocatorDefault,
                                                          cvmtlTextureCache[i],
                                                          pixelBuffer,
                                                          (__bridge CFDictionaryRef)metalTextureProperties,
                                                          MTLPixelFormatBGRA8Unorm,
                                                          width,
                                                          height,
                                                          0,
                                                          &cvmtlColorTexture[i]);
        assert(cvret == kCVReturnSuccess);
        mtlAliasedColorTexture[i] = CVMetalTextureGetTexture(cvmtlColorTexture[i]);
    }
    
    // Create the Metal texture for the depth buffer
    for (int i = 0; i < renderDevices.count; i++) {
        cvret = CVMetalTextureCacheCreateTextureFromImage(kCFAllocatorDefault,
                                                          cvmtlTextureCache[i],
                                                          depthBuffer,
                                                          (__bridge CFDictionaryRef)metalTextureProperties,
                                                          MTLPixelFormatR32Float,
                                                          width,
                                                          height,
                                                          0,
                                                          &cvmtlDepthTexture[i]);
        assert(cvret == kCVReturnSuccess);
        mtlAliasedDepthRegularFloatTexture[i] = CVMetalTextureGetTexture(cvmtlDepthTexture[i]);
    }
    
    // Create a Metal texture of type Depth32Float that we can actually use as a depth attachment
    MTLTextureDescriptor *depthTexDescriptor =
                            [MTLTextureDescriptor
                             texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                             width:width
                             height:height
                             mipmapped:false];

    depthTexDescriptor.usage = MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;
    depthTexDescriptor.resourceOptions = MTLResourceCPUCacheModeDefaultCache | MTLResourceStorageModePrivate;

    if (mtlSampleCount > 1)
    {
        depthTexDescriptor.sampleCount = mtlSampleCount;
        depthTexDescriptor.textureType = MTLTextureType2DMultisample;
    }

    MTLTextureDescriptor *colorTexDescriptor =
                            [MTLTextureDescriptor
                             texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                             width:width
                             height:height
                             mipmapped:false];

    colorTexDescriptor.usage = MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite;
    colorTexDescriptor.resourceOptions = MTLResourceStorageModeManaged;

    for(int i = 0; i < renderDevices.count; i++) {
        id<MTLDevice> dev = renderDevices[i];
        mtlLocalDepthTexture[i] = [dev newTextureWithDescriptor:depthTexDescriptor];

        if (dev == interopDevice) {
            mtlLocalColorTexture[i] = mtlAliasedColorTexture[i];
        }
        else {
            mtlLocalColorTexture[i] = mtlAliasedColorTexture[i];//[dev newTextureWithDescriptor:colorTexDescriptor];
        }
    }
    
    // Flush the caches
    CVOpenGLTextureCacheFlush(cvglTextureCache, 0);
    for(int i = 0; i < renderDevices.count; i++) {
        CVMetalTextureCacheFlush(cvmtlTextureCache[i], 0);
    }
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

    MtlfMetalContextSharedPtr context(MtlfMetalContext::GetMetalContext());
    glUniform2f(staticState.blitTexSizeUniform,
                mtlAliasedColorTexture[context->currentGPU].width,
                mtlAliasedColorTexture[context->currentGPU].height);
    
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

    if (true || context->currentDevice == interopDevice) {
        id<MTLFunction> computeProgram = gpus[context->currentGPU].computeDepthCopyProgram;
        if (mtlSampleCount > 1)
            computeProgram = gpus[context->currentGPU].computeDepthCopyMultisampleProgram;
        
        NSUInteger exeWidth = context->SetComputeEncoderState(computeProgram, 0, 0, @"Depth copy pipeline state");
        NSUInteger maxThreadsPerThreadgroup = context->GetMaxThreadsPerThreadgroup();

        MTLSize threadgroupCount = MTLSizeMake(exeWidth, maxThreadsPerThreadgroup / exeWidth, 1);
        MTLSize threadsPerGrid   = MTLSizeMake((mtlAliasedDepthRegularFloatTexture[context->currentGPU].width + (threadgroupCount.width - 1)) / threadgroupCount.width,
                                               (mtlAliasedDepthRegularFloatTexture[context->currentGPU].height + (threadgroupCount.height - 1)) / threadgroupCount.height,
                                               1);

        [computeEncoder setTexture:mtlLocalDepthTexture[context->currentGPU] atIndex:0];
        [computeEncoder setTexture:mtlAliasedDepthRegularFloatTexture[context->currentGPU] atIndex:1];
        
        [computeEncoder dispatchThreadgroups:threadsPerGrid threadsPerThreadgroup:threadgroupCount];
    }
    else {
        // Transfer to interop GPU
        if ([interopDevice peerGroupID] != 0 &&
            [interopDevice peerGroupID] == [context->currentDevice peerGroupID]) {
            
            // XGMI transfer
        }
        else {
            // Via system memory
            
        }
    }
}

void
MtlfGlInterop::ColourCorrectColourTexture(id<MTLComputeCommandEncoder> computeEncoder, id<MTLTexture> colourTexture)
{
    MtlfMetalContextSharedPtr context(MtlfMetalContext::GetMetalContext());
    
    if (true || context->currentDevice == interopDevice) {
        id<MTLFunction> computeProgram = gpus[context->currentGPU].computeColourCopyProgram;
        if (mtlSampleCount > 1)
            computeProgram = gpus[context->currentGPU].computeColourCopyMultisampleProgram;
        
        NSUInteger exeWidth = context->SetComputeEncoderState(computeProgram, 0, 0, @"Colour correction pipeline state");
        NSUInteger maxThreadsPerThreadgroup = context->GetMaxThreadsPerThreadgroup();
        
        MTLSize threadgroupCount = MTLSizeMake(exeWidth, maxThreadsPerThreadgroup / exeWidth, 1);
        MTLSize threadsPerGrid   = MTLSizeMake((colourTexture.width + (threadgroupCount.width - 1)) / threadgroupCount.width,
                                               (colourTexture.height + (threadgroupCount.height - 1)) / threadgroupCount.height,
                                               1);
        
        [computeEncoder setTexture:colourTexture atIndex:0];
        [computeEncoder setTexture:mtlAliasedColorTexture[context->currentGPU] atIndex:1];
        
        [computeEncoder dispatchThreadgroups:threadsPerGrid threadsPerThreadgroup:threadgroupCount];
    }
    else {
        // Transfer to interop GPU
        if ([interopDevice peerGroupID] != 0 &&
            [interopDevice peerGroupID] == [context->currentDevice peerGroupID]) {

            // XGMI transfer
        }
        else {
            // Via system memory
            
        }
    }
}

PXR_NAMESPACE_CLOSE_SCOPE

