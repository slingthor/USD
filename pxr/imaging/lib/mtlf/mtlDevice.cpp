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

#include "pxr/imaging/mtlf/mtlDevice.h"
#include "pxr/imaging/garch/glPlatformContext.h"

#include "pxr/imaging/mtlf/drawTarget.h"
#include "pxr/imaging/mtlf/package.h"

#import <simd/simd.h>

#define METAL_TESSELLATION_SUPPORT 0
#define METAL_STATE_OPTIMISATION 1
#define CACHE_GSCOMPUTE 1

#define DIRTY_METALRENDERSTATE_OLD_STYLE_VERTEX_UNIFORM   0x001
#define DIRTY_METALRENDERSTATE_OLD_STYLE_FRAGMENT_UNIFORM 0x002
#define DIRTY_METALRENDERSTATE_VERTEX_UNIFORM_BUFFER      0x004
#define DIRTY_METALRENDERSTATE_FRAGMENT_UNIFORM_BUFFER    0x008
#define DIRTY_METALRENDERSTATE_INDEX_BUFFER               0x010
#define DIRTY_METALRENDERSTATE_VERTEX_BUFFER              0x020
#define DIRTY_METALRENDERSTATE_SAMPLER                    0x040
#define DIRTY_METALRENDERSTATE_TEXTURE                    0x080
#define DIRTY_METALRENDERSTATE_DRAW_TARGET                0x100
#define DIRTY_METALRENDERSTATE_VERTEX_DESCRIPTOR          0x200
#define DIRTY_METALRENDERSTATE_CULLMODE_WINDINGORDER      0x400

#define DIRTY_METALRENDERSTATE_ALL                      0xFFFFFFFF

PXR_NAMESPACE_OPEN_SCOPE
MtlfMetalContextSharedPtr MtlfMetalContext::context = NULL;

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

// Called when the window is dragged to another display
void MtlfMetalContext::handleDisplayChange()
{
    NSLog(@"Detected display change - but not doing about it");
}

// Called when an eGPU is either removed or added
void MtlfMetalContext::handleGPUHotPlug(id<MTLDevice> device, MTLDeviceNotificationName notifier)
{
    // Device plugged in
    if (notifier == MTLDeviceWasAddedNotification) {
        NSLog(@"New Device was added");
    }
    // Device Removal Requested. Cleanup and switch to preferred device
    else if (notifier == MTLDeviceRemovalRequestedNotification) {
        NSLog(@"Device removal request was notified");
    }
    // additional handling of surprise removal
    else if (notifier == MTLDeviceWasRemovedNotification) {
        NSLog(@"Device was removed");
    }
}


id<MTLDevice> MtlfMetalContext::GetMetalDevice(PREFERRED_GPU_TYPE preferredGPUType)
{
    // Get a list of all devices and register an obsever for eGPU events
    id <NSObject> metalDeviceObserver = nil;
    
    // Get a list of all devices and register an obsever for eGPU events
    NSArray<id<MTLDevice>> *_deviceList = MTLCopyAllDevicesWithObserver(&metalDeviceObserver,
                                                ^(id<MTLDevice> device, MTLDeviceNotificationName name) {
                                                    MtlfMetalContext::handleGPUHotPlug(device, name);
                                                });
    NSMutableArray<id<MTLDevice>> *_eGPUs          = [NSMutableArray array];
    NSMutableArray<id<MTLDevice>> *_integratedGPUs = [NSMutableArray array];
    NSMutableArray<id<MTLDevice>> *_discreteGPUs   = [NSMutableArray array];
    id<MTLDevice>                  _defaultDevice  = MTLCreateSystemDefaultDevice();
    NSArray *preferredDeviceList = _discreteGPUs;
    
    if (preferredGPUType == PREFER_DEFAULT_GPU) {
        return _defaultDevice;
    }
    
    // Put the device into the appropriate device list
    for (id<MTLDevice>dev in _deviceList) {
        if (dev.removable)
        	[_eGPUs addObject:dev];
        else if (dev.lowPower)
        	[_integratedGPUs addObject:dev];
        else
        	[_discreteGPUs addObject:dev];
    }
    
    switch (preferredGPUType) {
        case PREFER_DISPLAY_GPU:
            NSLog(@"Display device selection not supported yet, returning default GPU");
        case PREFER_DEFAULT_GPU:
            break;
        case PREFER_EGPU:
            preferredDeviceList = _eGPUs;
            break;
       case PREFER_DISCRETE_GPU:
            preferredDeviceList = _discreteGPUs;
            break;
        case PREFER_INTEGRATED_GPU:
            preferredDeviceList = _integratedGPUs;
            break;
    }
    // If no device matching the requested one was found then get the default device
    if (preferredDeviceList.count != 0) {
        return preferredDeviceList.firstObject;
    }
    else {
        NSLog(@"Preferred device not found, returning default GPU");
        return _defaultDevice;
    }
}

MtlfMetalContext::GLInterop MtlfMetalContext::staticGlInterop;

void MtlfMetalContext::_InitialiseGL()
{
    if (staticGlInterop.glShaderProgram != 0) {
        return;
    }
    
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
    staticGlInterop.glShaderProgram = glCreateProgram();
    glAttachShader(staticGlInterop.glShaderProgram, fs);
    glAttachShader(staticGlInterop.glShaderProgram, vs);
    glBindFragDataLocation(staticGlInterop.glShaderProgram, 0, "fragColour");
    glLinkProgram(staticGlInterop.glShaderProgram);
    
    // Release the local instance of the fragment shader. The shader program maintains a reference.
    glDeleteShader(vs);
    glDeleteShader(fs);
    
    GLint maxLength = 0;
    if (maxLength)
    {
        glGetProgramiv(staticGlInterop.glShaderProgram, GL_INFO_LOG_LENGTH, &maxLength);
        
        // The maxLength includes the NULL character
        GLchar *errorLog = (GLchar*)malloc(maxLength);
        glGetProgramInfoLog(staticGlInterop.glShaderProgram, maxLength, &maxLength, errorLog);
        
        NSLog(@"%s", errorLog);
        free(errorLog);
    }
    
    glUseProgram(staticGlInterop.glShaderProgram);
    
    glGenVertexArrays(1, &staticGlInterop.glVAO);
    glBindVertexArray(staticGlInterop.glVAO);
    
    glGenBuffers(1, &staticGlInterop.glVBO);
    
    glBindBuffer(GL_ARRAY_BUFFER, staticGlInterop.glVBO);
    
    // Set up the vertex structure description
    staticGlInterop.posAttrib = glGetAttribLocation(staticGlInterop.glShaderProgram, "inPosition");
    staticGlInterop.texAttrib = glGetAttribLocation(staticGlInterop.glShaderProgram, "inTexCoord");
    glEnableVertexAttribArray(staticGlInterop.posAttrib);
    glVertexAttribPointer(staticGlInterop.posAttrib, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(offsetof(Vertex, position)));
    glEnableVertexAttribArray(staticGlInterop.texAttrib);
    glVertexAttribPointer(staticGlInterop.texAttrib, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(offsetof(Vertex, uv)));
    
    GLint samplerColorLoc = glGetUniformLocation(staticGlInterop.glShaderProgram, "interopTexture");
    GLint samplerDepthLoc = glGetUniformLocation(staticGlInterop.glShaderProgram, "depthTexture");

    staticGlInterop.blitTexSizeUniform = glGetUniformLocation(staticGlInterop.glShaderProgram, "texSize");
    
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

//
// MtlfMetalContext
//

MtlfMetalContext::MtlfMetalContext(id<MTLDevice> _device)
: enableMVA(false)
, enableComputeGS(false)
{
    if (_device == nil) {
        // Select Intel GPU if possible due to current issues on AMD. Revert when fixed - MTL_FIXME
        //device = MtlfMetalContext::GetMetalDevice(PREFER_INTEGRATED_GPU);
        device = MtlfMetalContext::GetMetalDevice(PREFER_DEFAULT_GPU);
    }
    else
        device = _device;
    NSLog(@"Selected %@ for Metal Device", device.name);
#if defined(METAL_EVENTS_AVAILABLE)
    enableMultiQueue = [device supportsFeatureSet:MTLFeatureSet_macOS_GPUFamily2_v1];
#endif
    // Create a new command queue
    commandQueue = [device newCommandQueue];
    if(enableMultiQueue) {
        NSLog(@"Device %@ supports Metal 2, enabling multi-queue codepath.", device.name);
        commandQueueGS = [device newCommandQueue];
    }
    else {
        NSLog(@"Device %@ does not support Metal 2, using fallback path, performance may be sub-optimal.", device.name);
    }
    
    NSOperatingSystemVersion minimumSupportedOSVersion = { .majorVersion = 10, .minorVersion = 14, .patchVersion = 0 };
    
    // MTL_FIXME - Disabling concurrent dispatch until appropriate fencing is in place
    concurrentDispatchSupported = [NSProcessInfo.processInfo isOperatingSystemAtLeastVersion:minimumSupportedOSVersion];

    // Load all the default shader files
    NSError *error = NULL;
    
    // Load our common vertex shader. This is used by both the fragment shaders below
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
    id <MTLFunction> fragmentProgram = [defaultLibrary newFunctionWithName:@"tex_fs"];
    id <MTLFunction> vertexProgram = [defaultLibrary newFunctionWithName:@"quad_vs"];
    
    computeDepthCopyProgram = [defaultLibrary newFunctionWithName:@"copyDepth"];
 
    MTLDepthStencilDescriptor *depthStateDesc = [[MTLDepthStencilDescriptor alloc] init];
    depthStateDesc.depthWriteEnabled = YES;
    depthStateDesc.depthCompareFunction = MTLCompareFunctionLessEqual;
    depthState = [device newDepthStencilStateWithDescriptor:depthStateDesc];
   
    _InitialiseGL();

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

    renderPipelineStateDescriptor = nil;
    computePipelineStateDescriptor = nil;
    vertexDescriptor = nil;
    indexBuffer = nil;
    remappedQuadIndexBuffer = nil;
    numVertexComponents = 0;
    vtxUniformBackingBuffer  = NULL;
    fragUniformBackingBuffer = NULL;
    
    drawTarget = NULL;
    
    currentWorkQueueType = METALWORKQUEUE_DEFAULT;
    currentWorkQueue     = &workQueues[currentWorkQueueType];
    
    for (int i = 0; i < METALWORKQUEUE_MAX; i ++) {
        ResetEncoders((MetalWorkQueueType)i, true);
    }
    
#if METAL_ENABLE_STATS
    resourceStats.commandBuffersCreated   = 0;
    resourceStats.commandBuffersCommitted = 0;
    resourceStats.buffersCreated          = 0;
    resourceStats.buffersReused           = 0;
    resourceStats.renderEncodersCreated   = 0;
    resourceStats.computeEncodersCreated  = 0;
    resourceStats.blitEncodersCreated     = 0;
    resourceStats.renderEncodersRequested = 0;
    resourceStats.computeEncodersRequested= 0;
    resourceStats.blitEncodersRequested   = 0;
    resourceStats.renderPipelineStates    = 0;
    resourceStats.computePipelineStates   = 0;
#endif
    
    frameCount = 0;
}

MtlfMetalContext::~MtlfMetalContext()
{
	_FreeTransientTextureCacheRefs();

    if (cvglTextureCache) {
        CFRelease(cvglTextureCache);
        cvglTextureCache = nil;
    }
    if (cvmtlTextureCache) {
        CFRelease(cvmtlTextureCache);
        cvmtlTextureCache = nil;
    }

    [commandQueue release];
    if(enableMultiQueue)
        [commandQueueGS release];

	CleanupUnusedBuffers();
	bufferFreeList.clear();
   
#if METAL_ENABLE_STATS
    NSLog(@"--- METAL Resource Stats (average per frame / total) ----");
    NSLog(@"Frame count:                %7lu", frameCount);
    NSLog(@"Command Buffers created:    %7lu / %7lu", resourceStats.commandBuffersCreated   / frameCount, resourceStats.commandBuffersCreated);
    NSLog(@"Command Buffers committed:  %7lu / %7lu", resourceStats.commandBuffersCommitted / frameCount, resourceStats.commandBuffersCommitted);
    NSLog(@"Metal   Buffers created:    %7lu / %7lu", resourceStats.buffersCreated          / frameCount, resourceStats.buffersCreated);
    NSLog(@"Metal   Buffers reused:     %7lu / %7lu", resourceStats.buffersReused           / frameCount, resourceStats.buffersReused);
    NSLog(@"Render  Encoders requested: %7lu / %7lu", resourceStats.renderEncodersRequested / frameCount, resourceStats.renderEncodersRequested);
    NSLog(@"Render  Encoders created:   %7lu / %7lu", resourceStats.renderEncodersCreated   / frameCount, resourceStats.renderEncodersCreated);
    NSLog(@"Render  Pipeline States:    %7lu / %7lu", resourceStats.renderPipelineStates    / frameCount, resourceStats.renderPipelineStates);
    NSLog(@"Compute Encoders requested: %7lu / %7lu", resourceStats.computeEncodersRequested/ frameCount, resourceStats.computeEncodersRequested);
    NSLog(@"Compute Encoders created:   %7lu / %7lu", resourceStats.computeEncodersCreated  / frameCount, resourceStats.computeEncodersCreated);
    NSLog(@"Compute Pipeline States:    %7lu / %7lu", resourceStats.computePipelineStates   / frameCount, resourceStats.computePipelineStates);
    NSLog(@"Blit    Encoders requested: %7lu / %7lu", resourceStats.blitEncodersRequested   / frameCount, resourceStats.blitEncodersRequested);
    NSLog(@"Blit    Encoders created:   %7lu / %7lu", resourceStats.blitEncodersCreated     / frameCount, resourceStats.blitEncodersCreated);
#endif
}

void MtlfMetalContext::RecreateInstance(id<MTLDevice> device)
{
    context = NULL;
    context = MtlfMetalContextSharedPtr(new MtlfMetalContext(device));
}

void MtlfMetalContext::_FreeTransientTextureCacheRefs()
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

void MtlfMetalContext::AllocateAttachments(int width, int height)
{
    NSDictionary* cvBufferProperties = @{
        (__bridge NSString*)kCVPixelBufferOpenGLCompatibilityKey : @(TRUE),
        (__bridge NSString*)kCVPixelBufferMetalCompatibilityKey : @(TRUE),
    };

    _FreeTransientTextureCacheRefs();
    
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

MtlfMetalContextSharedPtr
MtlfMetalContext::GetMetalContext()
{
    if (!context)
        context = MtlfMetalContextSharedPtr(new MtlfMetalContext(nil));

    return context;
}

bool
MtlfMetalContext::IsInitialized()
{
    if (!context)
        context = MtlfMetalContextSharedPtr(new MtlfMetalContext(nil));

    return context->device != nil;
}

void
MtlfMetalContext::BlitColorTargetToOpenGL()
{
    currentWorkQueueType = METALWORKQUEUE_DEFAULT;
    currentWorkQueue     = &workQueues[currentWorkQueueType];
    
    glPushAttrib(GL_ENABLE_BIT | GL_POLYGON_BIT | GL_DEPTH_BUFFER_BIT);
    
    glDepthMask(GL_TRUE);
    glFrontFace(GL_CCW);
    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    
    glUseProgram(staticGlInterop.glShaderProgram);
    
    glBindBuffer(GL_ARRAY_BUFFER, staticGlInterop.glVBO);
    
    // Set up the vertex structure description
    glEnableVertexAttribArray(staticGlInterop.posAttrib);
    glVertexAttribPointer(staticGlInterop.posAttrib, 2, GL_FLOAT, GL_FALSE, sizeof(MtlfMetalContext::Vertex), (void*)(offsetof(Vertex, position)));
    glEnableVertexAttribArray(staticGlInterop.texAttrib);
    glVertexAttribPointer(staticGlInterop.texAttrib, 2, GL_FLOAT, GL_FALSE, sizeof(MtlfMetalContext::Vertex), (void*)(offsetof(Vertex, uv)));
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_RECTANGLE, glColorTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_RECTANGLE, glDepthTexture);

    glUniform2f(staticGlInterop.blitTexSizeUniform, mtlColorTexture.width, mtlColorTexture.height);
    
    glDrawArrays(GL_TRIANGLES, 0, 6);
    
    glFlush();
    
    glBindTexture(GL_TEXTURE_RECTANGLE, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_RECTANGLE, 0);
    
    glDisableVertexAttribArray(staticGlInterop.posAttrib);
    glDisableVertexAttribArray(staticGlInterop.texAttrib);
    glUseProgram(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    glPopAttrib();
}

void
MtlfMetalContext::CopyDepthTextureToOpenGL()
{
    id <MTLComputeCommandEncoder> computeEncoder = GetComputeEncoder();
    computeEncoder.label = @"Depth buffer copy";
    
    NSUInteger exeWidth = context->SetComputeEncoderState(computeDepthCopyProgram, 0, 0, @"Depth copy pipeline state");
    
    MTLSize threadGroupCount = MTLSizeMake(16, exeWidth / 32, 1);
    MTLSize threadGroups     = MTLSizeMake(mtlDepthTexture.width / threadGroupCount.width + 1,
                                           mtlDepthTexture.height / threadGroupCount.height + 1, 1);
    
    [computeEncoder setTexture:mtlDepthTexture atIndex:0];
    [computeEncoder setTexture:mtlDepthRegularFloatTexture atIndex:1];
    [computeEncoder dispatchThreadgroups:threadGroups threadsPerThreadgroup: threadGroupCount];
    
    ReleaseEncoder(true);
}

id<MTLBuffer>
MtlfMetalContext::GetQuadIndexBuffer(MTLIndexType indexTypeMetal) {
    // Each 4 vertices will require 6 remapped one
    uint32 remappedIndexBufferSize = (indexBuffer.length / 4) * 6;
    
    // Since remapping is expensive check if the buffer we created this from originally has changed  - MTL_FIXME - these checks are not robust
    if (remappedQuadIndexBuffer) {
        if ((remappedQuadIndexBufferSource != indexBuffer) ||
            (remappedQuadIndexBuffer.length != remappedIndexBufferSize)) {
            [remappedQuadIndexBuffer release];
            remappedQuadIndexBuffer = nil;
        }
    }
    // Remap the quad indices into two sets of triangle indices
    if (!remappedQuadIndexBuffer) {
        if (indexTypeMetal != MTLIndexTypeUInt32) {
            TF_FATAL_CODING_ERROR("Only 32 bit indices currently supported for quads");
        }
        NSLog(@"Recreating quad remapped index buffer");
        
        remappedQuadIndexBufferSource = indexBuffer;
        remappedQuadIndexBuffer = [device newBufferWithLength:remappedIndexBufferSize  options:MTLResourceStorageModeManaged];
        
        uint32 *srcData =  (uint32 *)indexBuffer.contents;
        uint32 *destData = (uint32 *)remappedQuadIndexBuffer.contents;
        for (int i= 0; i < (indexBuffer.length / 4) ; i+=4)
        {
            destData[0] = srcData[0];
            destData[1] = srcData[1];
            destData[2] = srcData[2];
            destData[3] = srcData[0];
            destData[4] = srcData[2];
            destData[5] = srcData[3];
            srcData  += 4;
            destData += 6;
        }
        [remappedQuadIndexBuffer didModifyRange:(NSMakeRange(0, remappedQuadIndexBuffer.length))];
    }
    return remappedQuadIndexBuffer;
}

void MtlfMetalContext::CheckNewStateGather()
{
    // Lazily create a new state object
    if (!renderPipelineStateDescriptor)
        renderPipelineStateDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
    if (!computePipelineStateDescriptor)
        computePipelineStateDescriptor = [[MTLComputePipelineDescriptor alloc] init];
    
    [renderPipelineStateDescriptor reset];
    [computePipelineStateDescriptor reset];
}

void MtlfMetalContext::CreateCommandBuffer(MetalWorkQueueType workQueueType) {
    MetalWorkQueue *wq = &workQueues[workQueueType];
    
    //NSLog(@"Creating command buffer %d", (int)workQueueType);

    if (wq->commandBuffer == nil) {
        if (enableMultiQueue && workQueueType == METALWORKQUEUE_GEOMETRY_SHADER)
            wq->commandBuffer = [context->commandQueueGS commandBuffer];
        else
            wq->commandBuffer = [context->commandQueue commandBuffer];
#if defined(METAL_EVENTS_AVAILABLE)
        wq->event = [device newEvent];
#endif
    }
    // We'll reuse an existing buffer silently if it's empty, otherwise emit warning
    else if (wq->encoderHasWork) {
        TF_CODING_WARNING("Command buffer already exists");
    }
    METAL_INC_STAT(resourceStats.commandBuffersCreated);
}

void MtlfMetalContext::LabelCommandBuffer(NSString *label, MetalWorkQueueType workQueueType)
{
    MetalWorkQueue *wq = &workQueues[workQueueType];
    
     if (wq->commandBuffer == nil) {
        TF_FATAL_CODING_ERROR("No command buffer to label");
    }
    wq->commandBuffer.label = label;
}

void MtlfMetalContext::EncodeWaitForEvent(MetalWorkQueueType waitQueue, MetalWorkQueueType signalQueue, uint64_t eventValue)
{
    MetalWorkQueue *wait_wq   = &workQueues[waitQueue];
    MetalWorkQueue *signal_wq = &workQueues[signalQueue];
    
    // Check both work queues have been set up 
    if (!wait_wq->commandBuffer || !signal_wq->commandBuffer) {
        TF_FATAL_CODING_ERROR("One of the work queue has no command buffer associated with it");
    }
    
    if (wait_wq->encoderHasWork) {
        if (wait_wq->encoderInUse) {
            TF_FATAL_CODING_ERROR("Can't set an event dependency if encoder is still in use");
        }
        // If the last used encoder wasn't ended then we need to end it now
        if (!wait_wq->encoderEnded) {
            wait_wq->encoderInUse = true;
            ReleaseEncoder(true, waitQueue);
        }
    }
    // Make this command buffer wait for the event to be resolved
    signal_wq->currentHighestWaitValue = (eventValue != 0) ? eventValue : signal_wq->currentEventValue;
#if defined(METAL_EVENTS_AVAILABLE)
    [wait_wq->commandBuffer encodeWaitForEvent:signal_wq->event value:signal_wq->currentHighestWaitValue];
#endif
}

void MtlfMetalContext::EncodeWaitForQueue(MetalWorkQueueType waitQueue, MetalWorkQueueType signalQueue)
{
    MetalWorkQueue *signal_wq = &workQueues[signalQueue];
    signal_wq->generatesEndOfQueueEvent = true;

    EncodeWaitForEvent(waitQueue, signalQueue, endOfQueueEventValue);
}

uint64_t MtlfMetalContext::EncodeSignalEvent(MetalWorkQueueType signalQueue)
{
    MetalWorkQueue *wq = &workQueues[signalQueue];

    if (!wq->commandBuffer) {
        TF_FATAL_CODING_ERROR("Signal work queue has no command buffer associated with it");
    }
    
     if (wq->encoderHasWork) {
        if (wq->encoderInUse) {
            TF_FATAL_CODING_ERROR("Can't generate an event if encoder is still in use");
        }
        // If the last used encoder wasn't ended then we need to end it now
        if (!wq->encoderEnded) {
            wq->encoderInUse = true;
            ReleaseEncoder(true, signalQueue);
        }
    }
#if defined(METAL_EVENTS_AVAILABLE)
    // Generate event
    [wq->commandBuffer encodeSignalEvent:wq->event value:wq->currentEventValue];
#endif
    return wq->currentEventValue++;
}

MTLRenderPassDescriptor* MtlfMetalContext::GetRenderPassDescriptor()
{
    MetalWorkQueue *wq = &workQueues[METALWORKQUEUE_DEFAULT];
    return (wq == nil) ? nil : wq->currentRenderPassDescriptor;
}

void MtlfMetalContext::setFrontFaceWinding(MTLWinding _windingOrder)
{
    windingOrder = _windingOrder;
    dirtyRenderState |= DIRTY_METALRENDERSTATE_CULLMODE_WINDINGORDER;
}

void MtlfMetalContext::setCullMode(MTLCullMode _cullMode)
{
    cullMode = _cullMode;
    dirtyRenderState |= DIRTY_METALRENDERSTATE_CULLMODE_WINDINGORDER;
}

void MtlfMetalContext::SetShadingPrograms(id<MTLFunction> vertexFunction, id<MTLFunction> fragmentFunction, bool _enableMVA)
{
    CheckNewStateGather();
    
#if CACHE_GSCOMPUTE
    renderVertexFunction     = vertexFunction;
    renderFragmentFunction   = fragmentFunction;
    enableMVA                = _enableMVA;
    // Assume there is no GS associated with these shaders, they must be linked by calling SetGSPrograms after this
    renderComputeGSFunction  = nil;
    enableComputeGS          = false;
#else
    renderPipelineStateDescriptor.vertexFunction   = vertexFunction;
    renderPipelineStateDescriptor.fragmentFunction = fragmentFunction;
    
    if (fragmentFunction == nil) {
        renderPipelineStateDescriptor.rasterizationEnabled = false;
    }
    else {
        renderPipelineStateDescriptor.rasterizationEnabled = true;
    }
#endif
    
}

void MtlfMetalContext::SetGSProgram(id<MTLFunction> computeFunction)
{
    if (!computeFunction || !renderVertexFunction) {
         TF_FATAL_CODING_ERROR("Compute and Vertex functions must be set when using a Compute Geometry Shader!");
    }
    if(!enableMVA)
    {
        TF_FATAL_CODING_ERROR("Manual Vertex Assembly must be enabled when using a Compute Geometry Shader!");
    }
    renderComputeGSFunction = computeFunction;
    enableComputeGS = true;
 }

void MtlfMetalContext::SetVertexAttribute(uint32_t index,
                                          int size,
                                          int type,
                                          size_t stride,
                                          uint32_t offset,
                                          const TfToken& name)
{
    if (enableMVA)  //Setting vertex attributes means nothing when Manual Vertex Assembly is enabled.
        return;

    if (!vertexDescriptor)
    {
        vertexDescriptor = [[MTLVertexDescriptor alloc] init];

        vertexDescriptor.layouts[0].stepFunction = MTLVertexStepFunctionConstant;
        vertexDescriptor.layouts[0].stepRate = 0;
        vertexDescriptor.layouts[0].stride = stride;
        vertexDescriptor.attributes[0].format = MTLVertexFormatUInt;
        numVertexComponents = 1;
    }

    vertexDescriptor.attributes[index].bufferIndex = index;
    vertexDescriptor.attributes[index].offset = offset;
    vertexDescriptor.layouts[index].stepFunction = MTLVertexStepFunctionPerVertex;
    vertexDescriptor.layouts[index].stepRate = 1;
    vertexDescriptor.layouts[index].stride = stride;
    
    switch (type) {
        case GL_INT:
            vertexDescriptor.attributes[index].format = MTLVertexFormat(MTLVertexFormatInt + (size - 1));
            break;
        case GL_UNSIGNED_INT:
            vertexDescriptor.attributes[index].format = MTLVertexFormat(MTLVertexFormatUInt + (size - 1));
            break;
        case GL_FLOAT:
            vertexDescriptor.attributes[index].format = MTLVertexFormat(MTLVertexFormatFloat + (size - 1));
            break;
        case GL_INT_2_10_10_10_REV:
            vertexDescriptor.attributes[index].format = MTLVertexFormatInt1010102Normalized;
            break;
        default:
            TF_CODING_ERROR("Unsupported data type");
            break;
    }
    
    if (index + 1 > numVertexComponents) {
        numVertexComponents = index + 1;
    }
    
    dirtyRenderState |= DIRTY_METALRENDERSTATE_VERTEX_DESCRIPTOR;
}

// I think this can be removed didn't seem to make too much difference to speeds
void copyUniform(uint8 *dest, uint8 *src, uint32 size)
{
    switch (size) {
        case 4: {
            *(uint32*)dest = *(uint32*)src;
            break; }
        case 8: {
            *(uint64*)dest = *(uint64*)src;
            break; }
        case 12: {
            *(((uint32*)dest) + 0) = *(((uint32*)src) + 0);
            *(((uint32*)dest) + 1) = *(((uint32*)src) + 1);
            *(((uint32*)dest) + 2) = *(((uint32*)src) + 2);
            break; }
        case 16: {
            *(((uint64*)dest) + 0) = *(((uint64*)src) + 0);
            *(((uint64*)dest) + 1) = *(((uint64*)src) + 1);
            break; }
        default:
            memcpy(dest, src, size);
    }
}

void MtlfMetalContext::SetUniform(const void* _data, uint32 _dataSize, const TfToken& _name, uint32 _index, MSL_ProgramStage _stage)
{
    BufferBinding *OSBuffer = NULL;
    
    if (!_dataSize) {
        return;
    }
    
    if(_stage == kMSL_ProgramStage_Vertex) {
        OSBuffer = vtxUniformBackingBuffer;
        dirtyRenderState |= DIRTY_METALRENDERSTATE_OLD_STYLE_VERTEX_UNIFORM;
    }
    else if (_stage == kMSL_ProgramStage_Fragment) {
        OSBuffer = fragUniformBackingBuffer;
        dirtyRenderState |= DIRTY_METALRENDERSTATE_OLD_STYLE_FRAGMENT_UNIFORM;
    }
    else {
        TF_FATAL_CODING_ERROR("Unsupported stage");
    }
    
    if(!OSBuffer || !OSBuffer->buffer) {
        TF_FATAL_CODING_ERROR("Uniform Backing buffer not allocated");
    }
    
    uint8* bufferContents  = (OSBuffer->contents)  + OSBuffer->offset;
    
    uint32 uniformEnd = (_index + _dataSize);
    copyUniform(bufferContents + _index, (uint8*)_data, _dataSize);
    
    OSBuffer->modified = true;
}

void MtlfMetalContext::SetUniformBuffer(int index, id<MTLBuffer> buffer, const TfToken& name, MSL_ProgramStage stage, int offset, int oldStyleUniformSize)
{
    if(stage == 0)
        TF_FATAL_CODING_ERROR("Not allowed!");
    
    if(oldStyleUniformSize && offset != 0) {
            TF_FATAL_CODING_ERROR("Expected zero offset!");
    }
    // Allocate a binding for this buffer
    BufferBinding *bufferInfo = new BufferBinding{index, buffer, name, stage, offset, true, (uint32)oldStyleUniformSize, (uint8 *)(buffer.contents)};

    boundBuffers.push_back(bufferInfo);
    
    if(stage == kMSL_ProgramStage_Vertex) {
        dirtyRenderState |= DIRTY_METALRENDERSTATE_VERTEX_UNIFORM_BUFFER;
        if (oldStyleUniformSize) {
            if (vtxUniformBackingBuffer) {
                NSLog(@"Overwriting existing backing buffer, possible issue?");
            }
            vtxUniformBackingBuffer = bufferInfo;
        }
    }
    if(stage == kMSL_ProgramStage_Fragment) {
        dirtyRenderState |= DIRTY_METALRENDERSTATE_FRAGMENT_UNIFORM_BUFFER;
        if (oldStyleUniformSize) {
            if (fragUniformBackingBuffer) {
                NSLog(@"Overwriting existing backing buffer, possible issue?");
            }
            fragUniformBackingBuffer = bufferInfo;
        }
    }
}

void MtlfMetalContext::SetBuffer(int index, id<MTLBuffer> buffer, const TfToken& name)
{
    BufferBinding *bufferInfo = new BufferBinding{index, buffer, name, kMSL_ProgramStage_Vertex, 0, true};
    boundBuffers.push_back(bufferInfo);
    dirtyRenderState |= DIRTY_METALRENDERSTATE_VERTEX_BUFFER;
}

void MtlfMetalContext::SetIndexBuffer(id<MTLBuffer> buffer)
{
    indexBuffer = buffer;
    //remappedQuadIndexBuffer = nil;
    dirtyRenderState |= DIRTY_METALRENDERSTATE_INDEX_BUFFER;
}

void MtlfMetalContext::SetSampler(int index, id<MTLSamplerState> sampler, const TfToken& name, MSL_ProgramStage stage)
{
    samplers.push_back({index, sampler, name, stage});
    dirtyRenderState |= DIRTY_METALRENDERSTATE_SAMPLER;
}

void MtlfMetalContext::SetTexture(int index, id<MTLTexture> texture, const TfToken& name, MSL_ProgramStage stage)
{
    textures.push_back({index, texture, name, stage});
    dirtyRenderState |= DIRTY_METALRENDERSTATE_TEXTURE;
}

void MtlfMetalContext::SetDrawTarget(MtlfDrawTarget *dt)
{
    drawTarget = dt;
    dirtyRenderState |= DIRTY_METALRENDERSTATE_DRAW_TARGET;
}

size_t MtlfMetalContext::HashVertexDescriptor()
{
    size_t hashVal = 0;
    for (int i = 0; i < numVertexComponents; i++) {
        boost::hash_combine(hashVal, vertexDescriptor.layouts[i].stepFunction);
        boost::hash_combine(hashVal, vertexDescriptor.layouts[i].stepRate);
        boost::hash_combine(hashVal, vertexDescriptor.layouts[i].stride);
        boost::hash_combine(hashVal, vertexDescriptor.attributes[i].bufferIndex);
        boost::hash_combine(hashVal, vertexDescriptor.attributes[i].offset);
        boost::hash_combine(hashVal, vertexDescriptor.attributes[i].format);
    }
    return hashVal;
}

size_t MtlfMetalContext::HashColourAttachments(uint32_t numColourAttachments)
{
    size_t hashVal = 0;
    MTLRenderPipelineColorAttachmentDescriptorArray *colourAttachments = renderPipelineStateDescriptor.colorAttachments;
    for (int i = 0; i < numColourAttachments; i++) {
        boost::hash_combine(hashVal, colourAttachments[i].pixelFormat);
        boost::hash_combine(hashVal, colourAttachments[i].blendingEnabled);
        boost::hash_combine(hashVal, colourAttachments[i].sourceRGBBlendFactor);
        boost::hash_combine(hashVal, colourAttachments[i].destinationRGBBlendFactor);
        boost::hash_combine(hashVal, colourAttachments[i].rgbBlendOperation);
        boost::hash_combine(hashVal, colourAttachments[i].sourceAlphaBlendFactor);
        boost::hash_combine(hashVal, colourAttachments[i].destinationAlphaBlendFactor);
        boost::hash_combine(hashVal, colourAttachments[i].alphaBlendOperation);
    }
    return hashVal;
}

size_t MtlfMetalContext::HashPipelineDescriptor()
{
    MetalWorkQueue *wq = currentWorkQueue;
    
    size_t hashVal = 0;
    boost::hash_combine(hashVal, renderPipelineStateDescriptor.vertexFunction);
    boost::hash_combine(hashVal, renderPipelineStateDescriptor.fragmentFunction);
    boost::hash_combine(hashVal, renderPipelineStateDescriptor.sampleCount);
    boost::hash_combine(hashVal, renderPipelineStateDescriptor.rasterSampleCount);
    boost::hash_combine(hashVal, renderPipelineStateDescriptor.alphaToCoverageEnabled);
    boost::hash_combine(hashVal, renderPipelineStateDescriptor.alphaToOneEnabled);
    boost::hash_combine(hashVal, renderPipelineStateDescriptor.rasterizationEnabled);
    boost::hash_combine(hashVal, renderPipelineStateDescriptor.depthAttachmentPixelFormat);
    boost::hash_combine(hashVal, renderPipelineStateDescriptor.stencilAttachmentPixelFormat);
#if METAL_TESSELLATION_SUPPORT
    // Add here...
#endif
    boost::hash_combine(hashVal, wq->currentVertexDescriptorHash);
    boost::hash_combine(hashVal, wq->currentColourAttachmentsHash);
    return hashVal;
}

void MtlfMetalContext::SetRenderPipelineState()
{
    MetalWorkQueue *wq = currentWorkQueue;
    
    id<MTLRenderPipelineState> pipelineState;
    
    if (renderPipelineStateDescriptor == nil) {
         TF_FATAL_CODING_ERROR("No pipeline state descriptor allocated!");
    }
    
    if (wq->currentEncoderType != MTLENCODERTYPE_RENDER || !wq->encoderInUse || !wq->currentRenderEncoder) {
        TF_FATAL_CODING_ERROR("Not valid to call SetPipelineState() without an active render encoder");
    }
    
    renderPipelineStateDescriptor.label = @"SetRenderEncoderState";
    renderPipelineStateDescriptor.sampleCount = 1;
    renderPipelineStateDescriptor.inputPrimitiveTopology = MTLPrimitiveTopologyClassUnspecified;
    
#if CACHE_GSCOMPUTE
    renderPipelineStateDescriptor.vertexFunction   = renderVertexFunction;
    renderPipelineStateDescriptor.fragmentFunction = renderFragmentFunction;
    
    if (renderFragmentFunction == nil) {
        renderPipelineStateDescriptor.rasterizationEnabled = false;
    }
    else {
        renderPipelineStateDescriptor.rasterizationEnabled = true;
    }
#endif

#if METAL_TESSELLATION_SUPPORT
    renderPipelineStateDescriptor.maxTessellationFactor             = 1;
    renderPipelineStateDescriptor.tessellationFactorScaleEnabled    = NO;
    renderPipelineStateDescriptor.tessellationFactorFormat          = MTLTessellationFactorFormatHalf;
    renderPipelineStateDescriptor.tessellationControlPointIndexType = MTLTessellationControlPointIndexTypeNone;
    renderPipelineStateDescriptor.tessellationFactorStepFunction    = MTLTessellationFactorStepFunctionConstant;
    renderPipelineStateDescriptor.tessellationOutputWindingOrder    = MTLWindingCounterClockwise;
    renderPipelineStateDescriptor.tessellationPartitionMode         = MTLTessellationPartitionModePow2;
#endif
    if (enableMVA)
        renderPipelineStateDescriptor.vertexDescriptor = nil;
    else {
        if (dirtyRenderState & DIRTY_METALRENDERSTATE_VERTEX_DESCRIPTOR || renderPipelineStateDescriptor.vertexDescriptor == NULL) {
            // This assignment can be expensive as the vertexdescriptor will be copied (due to interface property)
            renderPipelineStateDescriptor.vertexDescriptor = vertexDescriptor;
            // Update vertex descriptor hash
            wq->currentVertexDescriptorHash = HashVertexDescriptor();
        }
    }
    
    if (dirtyRenderState & DIRTY_METALRENDERSTATE_DRAW_TARGET) {
        uint32_t numColourAttachments = 0;
    
        if (drawTarget) {
            auto& attachments = drawTarget->GetAttachments();
            for(auto it : attachments) {
                MtlfDrawTarget::MtlfAttachment* attachment = ((MtlfDrawTarget::MtlfAttachment*)&(*it.second));
                MTLPixelFormat depthFormat = [attachment->GetTextureName() pixelFormat];
                if(attachment->GetFormat() == GL_DEPTH_COMPONENT || attachment->GetFormat() == GL_DEPTH_STENCIL) {
                    renderPipelineStateDescriptor.depthAttachmentPixelFormat = depthFormat;
                    if(attachment->GetFormat() == GL_DEPTH_STENCIL)
                        renderPipelineStateDescriptor.stencilAttachmentPixelFormat = depthFormat; //Do not use the stencil pixel format (X32_S8)
                }
                else {
                    id<MTLTexture> texture = attachment->GetTextureName();
                    int idx = attachment->GetAttach();
                    
                    renderPipelineStateDescriptor.colorAttachments[idx].blendingEnabled = NO;
                    renderPipelineStateDescriptor.colorAttachments[idx].pixelFormat = [texture pixelFormat];
                }
                numColourAttachments++;
            }
        }
        else {
            //METAL TODO: Why does this need to be hardcoded? There is no matching drawTarget? Can we get this info from somewhere?
            renderPipelineStateDescriptor.colorAttachments[0].blendingEnabled = YES;
            renderPipelineStateDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
            renderPipelineStateDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
            renderPipelineStateDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
            renderPipelineStateDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
            renderPipelineStateDescriptor.colorAttachments[0].pixelFormat = mtlColorTexture.pixelFormat;
            numColourAttachments++;
            
            renderPipelineStateDescriptor.depthAttachmentPixelFormat = mtlDepthTexture.pixelFormat;
        }
        [wq->currentRenderEncoder setDepthStencilState:depthState];
        // Update colour attachments hash
        wq->currentColourAttachmentsHash = HashColourAttachments(numColourAttachments);
    }
    
    // Unset the state tracking flags
    dirtyRenderState &= ~(DIRTY_METALRENDERSTATE_VERTEX_DESCRIPTOR | DIRTY_METALRENDERSTATE_DRAW_TARGET);
    
#if METAL_STATE_OPTIMISATION
    // Always call this because currently we're not tracking changes to its state
    size_t hashVal = HashPipelineDescriptor();
    
    // If this matches the current pipeline state then we should already have the correct pipeline bound
    if (hashVal == wq->currentRenderPipelineDescriptorHash && wq->currentRenderPipelineState != nil) {
        return;
    }
    wq->currentRenderPipelineDescriptorHash = hashVal;
    
    boost::unordered_map<size_t, id<MTLRenderPipelineState>>::const_iterator pipelineStateIt = renderPipelineStateMap.find(wq->currentRenderPipelineDescriptorHash);
    
    if (pipelineStateIt != renderPipelineStateMap.end()) {
        pipelineState = pipelineStateIt->second;
    }
    else
#endif
    {
        NSError *error = NULL;
        pipelineState = [device newRenderPipelineStateWithDescriptor:renderPipelineStateDescriptor error:&error];
        if (!pipelineState) {
            NSLog(@"Failed to created pipeline state, error %@", error);
            return;
        }
#if METAL_STATE_OPTIMISATION
        renderPipelineStateMap.emplace(wq->currentRenderPipelineDescriptorHash, pipelineState);
        METAL_INC_STAT(resourceStats.renderPipelineStates);
#endif
        
    }
  
#if METAL_STATE_OPTIMISATION
    if (pipelineState != wq->currentRenderPipelineState)
#endif
    {
        [wq->currentRenderEncoder setRenderPipelineState:pipelineState];
        wq->currentRenderPipelineState = pipelineState;
    }
}

void MtlfMetalContext::UpdateOldStyleUniformBlock(BufferBinding *uniformBuffer, MSL_ProgramStage stage)
{
    if(!uniformBuffer->buffer) {
        TF_FATAL_CODING_ERROR("No vertex uniform backing buffer assigned!");
    }
    
    // Update vertex uniform buffer and move block along
    [uniformBuffer->buffer  didModifyRange:NSMakeRange(uniformBuffer->offset, uniformBuffer->blockSize)];
    
    // Copy existing data to the new blocks
    uint8 *data = (uint8 *)((uint8 *)(uniformBuffer->contents) + uniformBuffer->offset);
    copyUniform(data + uniformBuffer->blockSize, data, uniformBuffer->blockSize);
    
    // Move the offset along
    uniformBuffer->offset += uniformBuffer->blockSize;
    
    // Check for wrapping
    if (uniformBuffer->offset > uniformBuffer->buffer.length) {
        NSLog(@"Old style uniform buffer wrapped - expect strangeness"); // MTL_FIXME
        uniformBuffer->offset  = 0;
    }
}

void MtlfMetalContext::SetRenderEncoderState()
{
#if !METAL_STATE_OPTIMISATION
    dirtyRenderState = DIRTY_METALRENDERSTATE_ALL;
#endif
    MetalWorkQueue *wq = currentWorkQueue;
    MetalWorkQueue *gswq = &workQueues[METALWORKQUEUE_GEOMETRY_SHADER];
    id <MTLComputeCommandEncoder> computeEncoder;
    
#if CACHE_GSCOMPUTE
    // Default is all buffers writable
    unsigned long immutableBufferMask = 0;
#else
    id<MTLComputePipelineState> computePipelineState;
#endif
    
    // Get a compute encoder on the Geometry Shader work queue
    if(enableComputeGS) {
        MetalWorkQueueType oldWorkQueueType = currentWorkQueueType;
        computeEncoder = GetComputeEncoder(METALWORKQUEUE_GEOMETRY_SHADER);
        currentWorkQueueType = oldWorkQueueType;
        currentWorkQueue     = &workQueues[currentWorkQueueType];
    }
    
    if (wq->currentEncoderType != MTLENCODERTYPE_RENDER || !wq->encoderInUse || !wq->currentRenderEncoder) {
        TF_FATAL_CODING_ERROR("Not valid to call SetRenderEncoderState() without an active render encoder");
    }
    
    // Create and set a new pipelinestate if required
    SetRenderPipelineState();
    
    if (dirtyRenderState & DIRTY_METALRENDERSTATE_CULLMODE_WINDINGORDER) {
        [wq->currentRenderEncoder setFrontFacingWinding:windingOrder];
        [wq->currentRenderEncoder setCullMode:cullMode];
        dirtyRenderState &= ~DIRTY_METALRENDERSTATE_CULLMODE_WINDINGORDER;
    }
 
    // Any buffers modified
    if (dirtyRenderState & (DIRTY_METALRENDERSTATE_VERTEX_UNIFORM_BUFFER     |
                       DIRTY_METALRENDERSTATE_FRAGMENT_UNIFORM_BUFFER  |
                       DIRTY_METALRENDERSTATE_VERTEX_BUFFER            |
                       DIRTY_METALRENDERSTATE_OLD_STYLE_VERTEX_UNIFORM |
                       DIRTY_METALRENDERSTATE_OLD_STYLE_FRAGMENT_UNIFORM)) {
        
        for(auto buffer : boundBuffers)
        {
            // Only output if this buffer was modified
            if (buffer->modified) {
                if(buffer->stage == kMSL_ProgramStage_Vertex){
                    if(enableComputeGS) {
                        [computeEncoder setBuffer:buffer->buffer offset:buffer->offset atIndex:buffer->index];
#if CACHE_GSCOMPUTE
                        // Remove writable status
                        immutableBufferMask |= (1 << buffer->index);
#else
                        computePipelineStateDescriptor.buffers[buffer->index].mutability = MTLMutabilityImmutable;
#endif
                    }
                    [wq->currentRenderEncoder setVertexBuffer:buffer->buffer offset:buffer->offset atIndex:buffer->index];
                }
                else if(buffer->stage == kMSL_ProgramStage_Fragment) {
                    [wq->currentRenderEncoder setFragmentBuffer:buffer->buffer offset:buffer->offset atIndex:buffer->index];
                }
                else{
                    if(enableComputeGS) {
                        [computeEncoder setBuffer:buffer->buffer offset:buffer->offset atIndex:buffer->index];
#if CACHE_GSCOMPUTE
                        // Remove writable status
                        immutableBufferMask |= (1 << buffer->index);
#else
                        computePipelineStateDescriptor.buffers[buffer->index].mutability = MTLMutabilityImmutable;
#endif
                    }
                    else
                        TF_FATAL_CODING_ERROR("Compute Geometry Shader should be enabled when modifying Compute buffers!");
                }
                buffer->modified = false;
            }
        }
         
         if (dirtyRenderState & DIRTY_METALRENDERSTATE_OLD_STYLE_VERTEX_UNIFORM) {
             UpdateOldStyleUniformBlock(vtxUniformBackingBuffer, kMSL_ProgramStage_Vertex);
         }
         if (dirtyRenderState & DIRTY_METALRENDERSTATE_OLD_STYLE_FRAGMENT_UNIFORM) {
             UpdateOldStyleUniformBlock(fragUniformBackingBuffer, kMSL_ProgramStage_Fragment);
         }
         
         dirtyRenderState &= ~(DIRTY_METALRENDERSTATE_VERTEX_UNIFORM_BUFFER    |
                         DIRTY_METALRENDERSTATE_FRAGMENT_UNIFORM_BUFFER  |
                         DIRTY_METALRENDERSTATE_VERTEX_BUFFER            |
                         DIRTY_METALRENDERSTATE_OLD_STYLE_VERTEX_UNIFORM |
                         DIRTY_METALRENDERSTATE_OLD_STYLE_FRAGMENT_UNIFORM);
    }

 
    if (dirtyRenderState & DIRTY_METALRENDERSTATE_TEXTURE) {
        for(auto texture : textures) {
            if(texture.stage == kMSL_ProgramStage_Vertex) {
                if(enableComputeGS) {
                    [computeEncoder setTexture:texture.texture atIndex:texture.index];
                }
                [wq->currentRenderEncoder setVertexTexture:texture.texture atIndex:texture.index];
            }
            else if(texture.stage == kMSL_ProgramStage_Fragment)
                [wq->currentRenderEncoder setFragmentTexture:texture.texture atIndex:texture.index];
            else
                TF_FATAL_CODING_ERROR("Not implemented!"); //Compute case
        }
        dirtyRenderState &= ~DIRTY_METALRENDERSTATE_TEXTURE;
    }
    if (dirtyRenderState & DIRTY_METALRENDERSTATE_SAMPLER) {
        for(auto sampler : samplers) {
            if(sampler.stage == kMSL_ProgramStage_Vertex) {
                if(enableComputeGS) {
                    [computeEncoder setSamplerState:sampler.sampler atIndex:sampler.index];
                }
                [wq->currentRenderEncoder setVertexSamplerState:sampler.sampler atIndex:sampler.index];
            }
            else if(sampler.stage == kMSL_ProgramStage_Fragment)
                [wq->currentRenderEncoder setFragmentSamplerState:sampler.sampler atIndex:sampler.index];
            else
                TF_FATAL_CODING_ERROR("Not implemented!"); //Compute case
        }
        dirtyRenderState &= ~DIRTY_METALRENDERSTATE_SAMPLER;
    }
    
    if(enableComputeGS) {
#if CACHE_GSCOMPUTE
        SetComputeEncoderState(renderComputeGSFunction, boundBuffers.size(), immutableBufferMask, @"GS Compute phase", METALWORKQUEUE_GEOMETRY_SHADER);
#else
        //MTL_FIXME: We should cache compute pipelines like we cache render pipelines.
        NSError *error = NULL;
        MTLAutoreleasedComputePipelineReflection* reflData = 0;
        computePipelineState = [device newComputePipelineStateWithDescriptor:computePipelineStateDescriptor options:MTLPipelineOptionNone reflection:reflData error:&error];
        [computeEncoder setComputePipelineState:computePipelineState];
#endif
        // Release the geometry shader encoder
        ReleaseEncoder(false, METALWORKQUEUE_GEOMETRY_SHADER);
    }
}

void MtlfMetalContext::ClearRenderEncoderState()
{
    MetalWorkQueue *wq = currentWorkQueue;
    
    // Release owned resources
    [renderPipelineStateDescriptor  release];
    [computePipelineStateDescriptor release];
    [vertexDescriptor               release];
    
    renderPipelineStateDescriptor  = nil;
    computePipelineStateDescriptor = nil;
    vertexDescriptor               = nil;
    
    wq->currentRenderPipelineDescriptorHash  = 0;
    wq->currentRenderPipelineState           = nil;
    
    // clear referenced resources
    indexBuffer         = nil;
    numVertexComponents = 0;
    dirtyRenderState    = DIRTY_METALRENDERSTATE_ALL;
    
    // Free all state associated with the buffers
    for(auto buffer : boundBuffers) {
        delete buffer;
    }
    boundBuffers.clear();
    textures.clear();
    samplers.clear();
    
    vtxUniformBackingBuffer  = NULL;
    fragUniformBackingBuffer = NULL;
}

size_t MtlfMetalContext::HashComputePipelineDescriptor(unsigned int bufferCount)
{
    size_t hashVal = 0;
    boost::hash_combine(hashVal, computePipelineStateDescriptor.computeFunction);
    boost::hash_combine(hashVal, computePipelineStateDescriptor.label);
    boost::hash_combine(hashVal, computePipelineStateDescriptor.threadGroupSizeIsMultipleOfThreadExecutionWidth);
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 101400 /* __MAC_10_14 */
    boost::hash_combine(hashVal, computePipelineStateDescriptor.maxTotalThreadsPerThreadgroup);
#endif
    for (int i = 0; i < bufferCount; i++) {
        boost::hash_combine(hashVal, computePipelineStateDescriptor.buffers[i].mutability);
    }
    //boost::hash_combine(hashVal, computePipelineStateDescriptor.stageInputDescriptor); MTL_FIXME
    return hashVal;
}

// Using this function instead of setting the pipeline state directly allows caching
NSUInteger MtlfMetalContext::SetComputeEncoderState(id<MTLFunction>     computeFunction,
                                                    unsigned int        bufferCount,
                                                    unsigned long       immutableBufferMask,
                                                    NSString           *label,
                                                    MetalWorkQueueType  workQueueType)
{
    MetalWorkQueue *wq = &workQueues[workQueueType];
    
    id<MTLComputePipelineState> computePipelineState;
    
    if (wq->currentComputeEncoder == nil || wq->currentEncoderType != MTLENCODERTYPE_COMPUTE
        || !wq->encoderInUse) {
        TF_FATAL_CODING_ERROR("Compute encoder must be set and active to set the pipeline state");
    }
    
    if (computePipelineStateDescriptor == nil) {
        computePipelineStateDescriptor = [[MTLComputePipelineDescriptor alloc] init];
    }
    
    [computePipelineStateDescriptor reset];
    computePipelineStateDescriptor.computeFunction = computeFunction;
    computePipelineStateDescriptor.label = label;
    
    // Setup buffer mutability
    while (immutableBufferMask) {
        if (immutableBufferMask & 0x1) {
            computePipelineStateDescriptor.buffers[bufferCount].mutability = MTLMutabilityImmutable;
        }
        immutableBufferMask >>= 1;
    }
    
    // Always call this because currently we're not tracking changes to its state
    size_t hashVal = HashComputePipelineDescriptor(bufferCount);
    
    // If this matches the currently bound pipeline state (assuming one is bound) then carry on using it
    if (wq->currentComputePipelineState != nil && hashVal == wq->currentComputePipelineDescriptorHash) {
        return wq->currentComputeThreadExecutionWidth;
    }
    // Update the hash
    wq->currentComputePipelineDescriptorHash = hashVal;
    
    // Search map to see if we've created a pipeline state object for this already
    boost::unordered_map<size_t, id<MTLComputePipelineState>>::const_iterator computePipelineStateIt = computePipelineStateMap.find(wq->currentComputePipelineDescriptorHash);
    
    if (computePipelineStateIt != computePipelineStateMap.end()) {
        // Retrieve pre generated state
        computePipelineState = computePipelineStateIt->second;
    }
    else
    {
        NSError *error = NULL;
        MTLAutoreleasedComputePipelineReflection* reflData = 0;
        
        // Create a new Compute pipeline state object
        computePipelineState = [device newComputePipelineStateWithDescriptor:computePipelineStateDescriptor options:MTLPipelineOptionNone reflection:reflData error:&error];
        if (!computePipelineState) {
            NSLog(@"Failed to create compute pipeline state, error %@", error);
            return 0;
        }
        computePipelineStateMap.emplace(wq->currentComputePipelineDescriptorHash, computePipelineState);
        METAL_INC_STAT(resourceStats.computePipelineStates);
    }
    
    if (computePipelineState != wq->currentComputePipelineState)
    {
        [wq->currentComputeEncoder setComputePipelineState:computePipelineState];
        wq->currentComputePipelineState = computePipelineState;
        wq->currentComputeThreadExecutionWidth = [wq->currentComputePipelineState threadExecutionWidth];
    }
    
    return wq->currentComputeThreadExecutionWidth;
}

void MtlfMetalContext::ResetEncoders(MetalWorkQueueType workQueueType, bool isInitializing)
{
    MetalWorkQueue *wq = &workQueues[workQueueType];
 
    if(!isInitializing) {
        if(wq->currentHighestWaitValue != endOfQueueEventValue && wq->currentHighestWaitValue >= wq->currentEventValue) {
            TF_FATAL_CODING_ERROR("There is a WaitForEvent which is never going to get Signalled!");
		}
#if defined(METAL_EVENTS_AVAILABLE)
        if(wq->event != nil) {
            [wq->event release];
		}
#endif
    }
   
    wq->commandBuffer         = nil;
#if defined(METAL_EVENTS_AVAILABLE)
    wq->event                 = nil;
#endif
    
    wq->encoderInUse             = false;
    wq->encoderEnded             = false;
    wq->encoderHasWork           = false;
    wq->generatesEndOfQueueEvent = false;
    wq->currentEncoderType       = MTLENCODERTYPE_NONE;
    wq->currentBlitEncoder       = nil;
    wq->currentRenderEncoder     = nil;
    wq->currentComputeEncoder    = nil;
    
    wq->currentEventValue                    = 1;
    wq->currentHighestWaitValue              = 0;
    wq->currentVertexDescriptorHash          = 0;
    wq->currentColourAttachmentsHash         = 0;
    wq->currentRenderPipelineDescriptorHash  = 0;
    wq->currentRenderPipelineState           = nil;
    wq->currentComputePipelineDescriptorHash = 0;
    wq->currentComputePipelineState          = nil;
}

void MtlfMetalContext::CommitCommandBuffer(bool waituntilScheduled, bool waitUntilCompleted, MetalWorkQueueType workQueueType)
{
    MetalWorkQueue *wq = &workQueues[workQueueType];
    
    //NSLog(@"Committing command buffer %d %@", (int)workQueueType, wq->commandBuffer.label);
    
    if (waituntilScheduled && waitUntilCompleted) {
        TF_FATAL_CODING_ERROR("Just pick one please!");
    }
    
    // Check if there was any work to submit on this queue
    if (wq->commandBuffer == nil) {
        TF_FATAL_CODING_ERROR("Can't commit command buffer if it was never created");
    }

    // Check that we actually encoded something to commit..
    if (wq->encoderHasWork) {
         if (wq->encoderInUse) {
            TF_FATAL_CODING_ERROR("Can't commit command buffer if encoder is still in use");
        }
        
        // If the last used encoder wasn't ended then we need to end it now
        if (!wq->encoderEnded) {
            wq->encoderInUse = true;
            ReleaseEncoder(true, workQueueType);
        }
        //NSLog(@"Committing command buffer: %@",  wq->commandBuffer.label);
    }
    else {
        /*
         There may be cases where we speculatively create a command buffer (geometry shaders) but dont
         actually use it. In this case we've nothing to commit so we'll return but not destroy the buffer
         so we don't have the overhead of recreating it every time. If it's required to generate an event
         we have to kick it regardless
         */
        if (!wq->generatesEndOfQueueEvent) {
            //NSLog(@"No work in this command buffer: %@", wq->commandBuffer.label);
            // Have to disable reuse as the command buffer seems to dissapear if not used (garbage collected?), need to investigate
            // Just reset it for now so a new one will get created.
            ResetEncoders(workQueueType);
            return;
        }
     }
    
    if(wq->generatesEndOfQueueEvent) {
        wq->currentEventValue = endOfQueueEventValue;
#if defined(METAL_EVENTS_AVAILABLE)
        [wq->commandBuffer encodeSignalEvent:wq->event value:wq->currentEventValue];
#endif
    }
    
    __block unsigned long thisFrameNumber = frameCount;

     [wq->commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> commandBuffer) {
        GPUTimerEndTimer(thisFrameNumber);
      }];

    GPUTimerStartTimer(thisFrameNumber);

    [wq->commandBuffer commit];
    
    if (waitUntilCompleted) {
        [wq->commandBuffer waitUntilCompleted];
    }
    else if (waituntilScheduled && wq->encoderHasWork) {
        [wq->commandBuffer waitUntilScheduled];
    }

    ResetEncoders(workQueueType);
    METAL_INC_STAT(resourceStats.commandBuffersCommitted);
}


void MtlfMetalContext::SetRenderPassDescriptor(MTLRenderPassDescriptor *renderPassDescriptor)
{
    MetalWorkQueue *wq = currentWorkQueue;
    
    // Could relax this check to only include render encoders but probably not worthwhile
    if (wq->encoderInUse) {
        TF_FATAL_CODING_ERROR("Dont set a new renderpass descriptor whilst an encoder is active");
    }
    
    // Release the current render encoder if it's active to force a new encoder to be created with the new descriptor
    if (wq->currentEncoderType == MTLENCODERTYPE_RENDER) {
        // Mark encoder in use so we can do release+end
        wq->encoderInUse = true;
        ReleaseEncoder(true);
    }
    
    wq->currentRenderPassDescriptor = renderPassDescriptor;
}


void MtlfMetalContext::ReleaseEncoder(bool endEncoding, MetalWorkQueueType workQueueType)
{
    MetalWorkQueue *wq = &workQueues[workQueueType];
    
    if (!wq->encoderInUse) {
        TF_FATAL_CODING_ERROR("No encoder to release");
    }
    if (!wq->commandBuffer) {
        TF_FATAL_CODING_ERROR("Shouldn't be able to get here without having a command buffer created");
    }
   
    if (endEncoding) {
        switch (wq->currentEncoderType) {
            case MTLENCODERTYPE_RENDER:
            {
                [wq->currentRenderEncoder endEncoding];
                wq->currentRenderEncoder      = nil;
                wq->currentRenderPipelineState = nil;
                break;
            }
            case MTLENCODERTYPE_COMPUTE:
            {
                [wq->currentComputeEncoder endEncoding];
                wq->currentComputePipelineState = nil;
                wq->currentComputeEncoder       = nil;
                break;
            }
            case MTLENCODERTYPE_BLIT:
            {
                [wq->currentBlitEncoder endEncoding];
                wq->currentBlitEncoder = nil;
                break;
            }
            case MTLENCODERTYPE_NONE:
            default:
            {
                TF_FATAL_CODING_ERROR("Unsupported encoder type to flush");
                break;
            }
        }
        wq->currentEncoderType = MTLENCODERTYPE_NONE;
        wq->encoderEnded       = true;
    }
    wq->encoderInUse       = false;
}

void MtlfMetalContext::SetCurrentEncoder(MetalEncoderType encoderType, MetalWorkQueueType workQueueType)
{
    MetalWorkQueue *wq = &workQueues[workQueueType];
    
    if (wq->encoderInUse) {
        TF_FATAL_CODING_ERROR("Need to release the current encoder before getting a new one");
    }
    if (!wq->commandBuffer) {
        NSLog(@"Creating a command buffer on demand, try and avoid this!");
        CreateCommandBuffer(workQueueType);
        LabelCommandBuffer(@"Default label - fix!");
        //TF_FATAL_CODING_ERROR("Shouldn't be able to get here without having a command buffer created");
    }

    // Check if we have a cuurently active encoder
    if (wq->currentEncoderType != MTLENCODERTYPE_NONE) {
        // If the encoder types match then we can carry on using the current encoder otherwise we need to create a new one
        if(wq->currentEncoderType == encoderType) {
            // Mark that this curent encoder is in use and return
            wq->encoderInUse = true;
            return;
        }
        // If we're switching encoder types then we should end the current one
        else if(wq->currentEncoderType != encoderType && !wq->encoderEnded) {
            // Mark encoder in use so we can do release+end
            wq->encoderInUse = true;
            ReleaseEncoder(true);
        }
    }
   
    // Create a new encoder
    switch (encoderType) {
        case MTLENCODERTYPE_RENDER: {
            // Check we have a valid render pass descriptor
            if (!wq->currentRenderPassDescriptor) {
                TF_FATAL_CODING_ERROR("Can ony pass null renderPassDescriptor if the render encoder is currently active");
            }
            wq->currentRenderEncoder = [wq->commandBuffer renderCommandEncoderWithDescriptor: wq->currentRenderPassDescriptor];
            // Since the encoder is new we'll need to emit all the state again
            dirtyRenderState     = DIRTY_METALRENDERSTATE_ALL;
            for(auto buffer : boundBuffers) { buffer->modified = true; }
            METAL_INC_STAT(resourceStats.renderEncodersCreated);
            break;
        }
        case MTLENCODERTYPE_COMPUTE: {
#if defined(METAL_EVENTS_AVAILABLE)
            if (concurrentDispatchSupported) {
                wq->currentComputeEncoder = [wq->commandBuffer computeCommandEncoderWithDispatchType:MTLDispatchTypeConcurrent];
            }
            else
#endif
            {
                wq->currentComputeEncoder = [wq->commandBuffer computeCommandEncoder];
            }
            
            dirtyRenderState     = DIRTY_METALRENDERSTATE_ALL;
            for(auto buffer : boundBuffers) { buffer->modified = true; }
            METAL_INC_STAT(resourceStats.computeEncodersCreated);
            break;
        }
        case MTLENCODERTYPE_BLIT: {
            wq->currentBlitEncoder = [wq->commandBuffer blitCommandEncoder];
            METAL_INC_STAT(resourceStats.blitEncodersCreated);
            break;
        }
        case MTLENCODERTYPE_NONE:
        default: {
            TF_FATAL_CODING_ERROR("Invalid encoder type!");
        }
    }
    // Mark that this curent encoder is in use
    wq->currentEncoderType = encoderType;
    wq->encoderInUse       = true;
    wq->encoderEnded       = false;
    wq->encoderHasWork     = true;
    
    currentWorkQueueType = workQueueType;
    currentWorkQueue     = &workQueues[currentWorkQueueType];
}

id<MTLBlitCommandEncoder> MtlfMetalContext::GetBlitEncoder(MetalWorkQueueType workQueueType)
{
    MetalWorkQueue *wq = &workQueues[workQueueType];
    SetCurrentEncoder(MTLENCODERTYPE_BLIT, workQueueType);
    METAL_INC_STAT(resourceStats.blitEncodersRequested);
    return wq->currentBlitEncoder;
}

id<MTLComputeCommandEncoder> MtlfMetalContext::GetComputeEncoder(MetalWorkQueueType workQueueType)
{
    MetalWorkQueue *wq = &workQueues[workQueueType];
    SetCurrentEncoder(MTLENCODERTYPE_COMPUTE, workQueueType);
    METAL_INC_STAT(resourceStats.computeEncodersRequested);
    return wq->currentComputeEncoder;
    
}

// If a renderpass descriptor is provided a new render encoder will be created otherwise we'll use the current one
id<MTLRenderCommandEncoder>  MtlfMetalContext::GetRenderEncoder(MetalWorkQueueType workQueueType)
{
    MetalWorkQueue *wq = &workQueues[workQueueType];
    SetCurrentEncoder(MTLENCODERTYPE_RENDER, workQueueType);
    METAL_INC_STAT(resourceStats.renderEncodersRequested);
    return currentWorkQueue->currentRenderEncoder;
}

id<MTLBuffer> MtlfMetalContext::GetMetalBuffer(NSUInteger length, MTLResourceOptions options, const void *pointer)
{
    id<MTLBuffer> buffer;
    
#if METAL_REUSE_BUFFERS
    for (auto entry = bufferFreeList.begin(); entry != bufferFreeList.end(); entry++) {
        MetalBufferListEntry bufferEntry = *entry;
        buffer = bufferEntry.buffer;
        MTLStorageMode  storageMode  =  MTLStorageMode((options & MTLResourceStorageModeMask)  >> MTLResourceStorageModeShift);
        MTLCPUCacheMode cpuCacheMode = MTLCPUCacheMode((options & MTLResourceCPUCacheModeMask) >> MTLResourceCPUCacheModeShift);
        
        // Check if buffer matches size and storage mode and is old enough to reuse
        if (buffer.length == length              &&
            storageMode   == buffer.storageMode  &&
            cpuCacheMode  == buffer.cpuCacheMode &&
            frameCount > (bufferEntry.releasedOnFrame + METAL_SAFE_BUFFER_AGE_IN_FRAMES) ) {
            //NSLog(@"Reusing buffer of length %lu", length);
            
            // Copy over data
            if (pointer) {
                memcpy(buffer.contents, pointer, length);
                [buffer didModifyRange:(NSMakeRange(0, length))];
            }
            
            bufferFreeList.erase(entry);
            METAL_INC_STAT(resourceStats.buffersReused);
            return buffer;
        }
    }
#endif
    //NSLog(@"Creating buffer of length %lu", length);
    if (pointer) {
        buffer  =  [device newBufferWithBytes:pointer length:length options:options];
    } else {
        buffer  =  [device newBufferWithLength:length options:options];
    }
    METAL_INC_STAT(resourceStats.buffersCreated);
    return buffer;
}

void MtlfMetalContext::ReleaseMetalBuffer(id<MTLBuffer> buffer)
{
 #if METAL_REUSE_BUFFERS
    MetalBufferListEntry bufferEntry;
    bufferEntry.buffer = buffer;
    bufferEntry.releasedOnFrame = frameCount;
    bufferFreeList.push_back(bufferEntry);
#else
    [buffer release];
#endif
}

void MtlfMetalContext::CleanupUnusedBuffers()
{
    id<MTLBuffer> buffer;
    
    for (auto entry = bufferFreeList.begin(); entry != bufferFreeList.end(); entry++) {
        MetalBufferListEntry bufferEntry = *entry;
        buffer = bufferEntry.buffer;
        
        if (frameCount > (bufferEntry.releasedOnFrame + METAL_MAX_BUFFER_AGE_IN_FRAMES) ) {
            [buffer release];
            bufferFreeList.erase(entry);
        }
    }
}

void MtlfMetalContext::StartFrame() {
    numPrimsDrawn = 0;
    GPUTImerResetTimer(frameCount);
}

void MtlfMetalContext::EndFrame() {
    GPUTimerFinish(frameCount);
    
    //NSLog(@"Time: %3.3f (%lu)", GetGPUTimeInMs(), frameCount);
    
    frameCount++;
    CleanupUnusedBuffers();
}

void  MtlfMetalContext::GPUTImerResetTimer(unsigned long frameNumber) {
    GPUFrameTime *timer = &gpuFrameTimes[frameNumber % METAL_NUM_GPU_FRAME_TIMES];
    
    timer->startingFrame        = frameNumber;
    timer->timingEventsIssued   = 0;
    timer->timingEventsReceived = 0;
    timer->timingCompleted      = false;
}


// Starts the GPU frame timer, only the first call per frame will start the timer
void MtlfMetalContext::GPUTimerStartTimer(unsigned long frameNumber)
{
    GPUFrameTime *timer = &gpuFrameTimes[frameNumber % METAL_NUM_GPU_FRAME_TIMES];
    // Just start the timer on the first call
    if (!timer->timingEventsIssued) {
        gettimeofday(&timer->frameStartTime, 0);
    }
    timer->timingEventsIssued++;
}

// Records a GPU end of frame timer, if multiple are received only the last is recorded
void MtlfMetalContext::GPUTimerEndTimer(unsigned long frameNumber)
{
    GPUFrameTime *timer = &gpuFrameTimes[frameNumber % METAL_NUM_GPU_FRAME_TIMES];
    gettimeofday(&timer->frameEndTime, 0);
    timer->timingEventsReceived++;
}

// Indicates that a timer has finished receiving all of the events to be issued
void  MtlfMetalContext::GPUTimerFinish(unsigned long frameNumber) {
    GPUFrameTime *timer = &gpuFrameTimes[frameNumber % METAL_NUM_GPU_FRAME_TIMES];
    timer->timingCompleted = true;
}

// Returns the most recently valid frame time
float MtlfMetalContext::GetGPUTimeInMs() {
    GPUFrameTime *validTimer = NULL;
    unsigned long highestFrameNumber = 0;
    
    for (int i = 0; i < METAL_NUM_GPU_FRAME_TIMES; i++) {
        GPUFrameTime *timer = &gpuFrameTimes[i];
        // To be a valid time it must have received all timing events back and have it's frame marked as finished
        if (timer->startingFrame >= highestFrameNumber &&
            timer->timingCompleted                   &&
            timer->timingEventsIssued == timer->timingEventsReceived) {
            validTimer = timer;
            highestFrameNumber = timer->startingFrame;
        }
    }
    if (!validTimer) {
        return 0.0f;
    }
    
    struct timeval diff;
    timersub(&validTimer->frameEndTime, &validTimer->frameStartTime, &diff);
    return (float) ((diff.tv_sec + diff.tv_usec) / 1000.0f);
}


PXR_NAMESPACE_CLOSE_SCOPE

