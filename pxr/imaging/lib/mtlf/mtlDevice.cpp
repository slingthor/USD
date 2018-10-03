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
    NSArray *preferredDeviceList = _discreteGPUs;
    
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
            preferredDeviceList = _deviceList;
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
    if (preferredDeviceList.count == 0) {
        NSLog(@"Preferred device not found, returning default GPU");
        preferredDeviceList = _deviceList;
    }
    return preferredDeviceList.firstObject;
}


//
// MtlfMetalContext
//

MtlfMetalContext::MtlfMetalContext() : enableMVA(false), enableComputeGS(false)
{
    // Select Intel GPU if possible due to current issues on AMD. Revert when fixed - MTL_FIXME
	device = MtlfMetalContext::GetMetalDevice(PREFER_INTEGRATED_GPU);

    NSLog(@"Selected %@ for Metal Device", device.name);
    
    // Create a new command queue
    commandQueue = [device newCommandQueue];

    // Reset dependency tracking state
    queueSyncEvent        = [device newEvent];
    queueSyncEventCounter = 1;
    outstandingDependency = METALWORKQUEUE_INVALID;
    
    NSOperatingSystemVersion minimumSupportedOSVersion = { .majorVersion = 10, .minorVersion = 14, .patchVersion = 0 };
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
    computeDepthCopyPipelineState = [device newComputePipelineStateWithFunction:computeDepthCopyProgram error:&error];
    if (!computeDepthCopyPipelineState) {
        NSLog(@"Failed to created pipeline state, error %@", error);
    }
    computeDepthCopyProgramExecutionWidth = [computeDepthCopyPipelineState threadExecutionWidth];
    
    MTLDepthStencilDescriptor *depthStateDesc = [[MTLDepthStencilDescriptor alloc] init];
    depthStateDesc.depthWriteEnabled = YES;
    depthStateDesc.depthCompareFunction = MTLCompareFunctionLessEqual;
    depthState = [device newDepthStencilStateWithDescriptor:depthStateDesc];
   
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
    glShaderProgram = glCreateProgram();
    glAttachShader(glShaderProgram, fs);
    glAttachShader(glShaderProgram, vs);
    glBindFragDataLocation(glShaderProgram, 0, "fragColour");
    glLinkProgram(glShaderProgram);
    
    // Release the local instance of the fragment shader. The shader program maintains a reference.
    glDeleteShader(vs);
    glDeleteShader(fs);
    
    GLint maxLength = 0;
    if (maxLength)
    {
        glGetProgramiv(glShaderProgram, GL_INFO_LOG_LENGTH, &maxLength);
        
        // The maxLength includes the NULL character
        GLchar *errorLog = (GLchar*)malloc(maxLength);
        glGetProgramInfoLog(glShaderProgram, maxLength, &maxLength, errorLog);
        
        NSLog(@"%s", errorLog);
        free(errorLog);
    }
    
    glUseProgram(glShaderProgram);
    
    glVAO = 0;
    glGenVertexArrays(1, &glVAO);
    glBindVertexArray(glVAO);
    
    glVBO = 0;
    glGenBuffers(1, &glVBO);
    
    glBindBuffer(GL_ARRAY_BUFFER, glVBO);

    // Set up the vertex structure description
    GLint posAttrib = glGetAttribLocation(glShaderProgram, "inPosition");
    GLint texAttrib = glGetAttribLocation(glShaderProgram, "inTexCoord");
    glEnableVertexAttribArray(posAttrib);
    glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(offsetof(Vertex, position)));
    glEnableVertexAttribArray(texAttrib);
    glVertexAttribPointer(texAttrib, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(offsetof(Vertex, uv)));

    GLint samplerColorLoc = glGetUniformLocation(glShaderProgram, "interopTexture");
    GLint samplerDepthLoc = glGetUniformLocation(glShaderProgram, "depthTexture");
    
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

    cvglTextureCache = nil;
    cvmtlTextureCache = nil;

    CVReturn cvret = CVMetalTextureCacheCreate(kCFAllocatorDefault, nil, device, nil, &cvmtlTextureCache);
    assert(cvret == kCVReturnSuccess);
    
    CGLContextObj glctx = [[NSOpenGLContext currentContext] CGLContextObj];
    CGLPixelFormatObj glPixelFormat = [[[NSOpenGLContext currentContext] pixelFormat] CGLPixelFormatObj];
    cvret = CVOpenGLTextureCacheCreate(kCFAllocatorDefault, nil, (__bridge CGLContextObj _Nonnull)(glctx), glPixelFormat, nil, &cvglTextureCache);
    assert(cvret == kCVReturnSuccess);
    
    glColorTexture = 0;
    glDepthTexture = 0;
    AllocateAttachments(256, 256);

    renderPipelineStateDescriptor = nil;
    computePipelineStateDescriptor = nil;
    vertexDescriptor = nil;
    indexBuffer = nil;
    remappedQuadIndexBuffer = nil;
    numVertexComponents = 0;
    
    drawTarget = NULL;
    
    currentWorkQueueType = METALWORKQUEUE_DEFAULT;
    currentWorkQueue     = &workQueues[METALWORKQUEUE_DEFAULT];
    
    for (int i = 0; i < METALWORKQUEUE_MAX; i ++) {
        ResetEncoders((MetalWorkQueueType)i);
    }
}

MtlfMetalContext::~MtlfMetalContext()
{
    if (glColorTexture) {
        glDeleteTextures(1, &glColorTexture);
    }
    if (glDepthTexture) {
        glDeleteTextures(1, &glDepthTexture);
    }
}

void MtlfMetalContext::AllocateAttachments(int width, int height)
{
    CVOpenGLTextureRef cvglTexture;
    CVMetalTextureRef cvmtlTexture;

    NSDictionary* cvBufferProperties = @{
        (__bridge NSString*)kCVPixelBufferOpenGLCompatibilityKey : @(TRUE),
        (__bridge NSString*)kCVPixelBufferMetalCompatibilityKey : @(TRUE),
    };

    pixelBuffer = nil;
    depthBuffer = nil;
    CVPixelBufferCreate(kCFAllocatorDefault, width, height,
                        kCVPixelFormatType_32BGRA,
                        (__bridge CFDictionaryRef)cvBufferProperties,
                        &pixelBuffer);
    
    CVPixelBufferCreate(kCFAllocatorDefault, width, height,
                        kCVPixelFormatType_DepthFloat32,
                        (__bridge CFDictionaryRef)cvBufferProperties,
                        &depthBuffer);
    
    CVReturn cvret = CVOpenGLTextureCacheCreateTextureFromImage(kCFAllocatorDefault, cvglTextureCache,
                                                                pixelBuffer,
                                                                nil, &cvglTexture);
    assert(cvret == kCVReturnSuccess);

    if (glColorTexture) {
        glDeleteTextures(1, &glColorTexture);
    }
    glColorTexture = CVOpenGLTextureGetName(cvglTexture);
    
    cvret = CVOpenGLTextureCacheCreateTextureFromImage(kCFAllocatorDefault, cvglTextureCache,
                                                       depthBuffer,
                                                       nil, &cvglTexture);
    assert(cvret == kCVReturnSuccess);
    
    if (glDepthTexture) {
        glDeleteTextures(1, &glDepthTexture);
    }
    glDepthTexture = CVOpenGLTextureGetName(cvglTexture);
    
    cvret = CVMetalTextureCacheCreateTextureFromImage(kCFAllocatorDefault, cvmtlTextureCache,
                                                      pixelBuffer, nil, MTLPixelFormatBGRA8Unorm,
                                                      width, height, 0, &cvmtlTexture);
    assert(cvret == kCVReturnSuccess);
    
    mtlColorTexture = CVMetalTextureGetTexture(cvmtlTexture);
    
    cvret = CVMetalTextureCacheCreateTextureFromImage(kCFAllocatorDefault, cvmtlTextureCache,
                                                      depthBuffer, nil, MTLPixelFormatR32Float,
                                                      width, height, 0, &cvmtlTexture);
    assert(cvret == kCVReturnSuccess);

    mtlDepthRegularFloatTexture = CVMetalTextureGetTexture(cvmtlTexture);
    
    {
        MTLTextureDescriptor *depthTexDescriptor = [MTLTextureDescriptor
            texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                                    width:width height:height
                                                    mipmapped:false];
        depthTexDescriptor.usage = MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;
        depthTexDescriptor.resourceOptions = MTLResourceCPUCacheModeDefaultCache | MTLResourceStorageModePrivate;
        mtlDepthTexture = [device newTextureWithDescriptor:depthTexDescriptor];
    }
}

MtlfMetalContextSharedPtr
MtlfMetalContext::GetMetalContext()
{
    if (!context)
        context = MtlfMetalContextSharedPtr(new MtlfMetalContext());

    return MtlfMetalContextSharedPtr(context);
}

bool
MtlfMetalContext::IsInitialized()
{
    if (!context)
        context = MtlfMetalContextSharedPtr(new MtlfMetalContext());

    return context->device != nil;
}

void
MtlfMetalContext::BlitColorTargetToOpenGL()
{
    glPushAttrib(GL_ENABLE_BIT | GL_POLYGON_BIT | GL_DEPTH_BUFFER_BIT);
    
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glFrontFace(GL_CCW);
    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    
    glUseProgram(glShaderProgram);
    
    glBindBuffer(GL_ARRAY_BUFFER, glVBO);
    
    // Set up the vertex structure description
    GLint posAttrib = glGetAttribLocation(glShaderProgram, "inPosition");
    GLint texAttrib = glGetAttribLocation(glShaderProgram, "inTexCoord");
    glEnableVertexAttribArray(posAttrib);
    glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, sizeof(MtlfMetalContext::Vertex), (void*)(offsetof(Vertex, position)));
    glEnableVertexAttribArray(texAttrib);
    glVertexAttribPointer(texAttrib, 2, GL_FLOAT, GL_FALSE, sizeof(MtlfMetalContext::Vertex), (void*)(offsetof(Vertex, uv)));
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_RECTANGLE, glColorTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_RECTANGLE, glDepthTexture);
    
    GLuint blitTexSizeUniform = glGetUniformLocation(glShaderProgram, "texSize");
    
    glUniform2f(blitTexSizeUniform, mtlColorTexture.width, mtlColorTexture.height);
    
    glDrawArrays(GL_TRIANGLES, 0, 6);
    
    glFlush();
    
    glBindTexture(GL_TEXTURE_RECTANGLE, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_RECTANGLE, 0);
    
    glDisableVertexAttribArray(posAttrib);
    glDisableVertexAttribArray(texAttrib);
    glUseProgram(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    glPopAttrib();
}

void
MtlfMetalContext::CopyDepthTextureToOpenGL()
{
    NSUInteger exeWidth = computeDepthCopyProgramExecutionWidth;
    MTLSize threadGroupCount = MTLSizeMake(16, exeWidth / 32, 1);
    MTLSize threadGroups     = MTLSizeMake(mtlDepthTexture.width / threadGroupCount.width + 1,
                                           mtlDepthTexture.height / threadGroupCount.height + 1, 1);
    id <MTLComputeCommandEncoder> computeEncoder = GetComputeEncoder();
    
    computeEncoder.label = @"Depth buffer copy";
    
    context->SetComputeEncoderState(computeDepthCopyProgram, 0, @"Depth copy pipeline state");
    
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
        wq->commandBuffer = [context->commandQueue commandBuffer];
    } else {
        TF_CODING_WARNING("Command buffer already exists");
    }
    
    wq->currentRenderPipelineState = nil;
}

void MtlfMetalContext::LabelCommandBuffer(NSString *label, MetalWorkQueueType workQueueType)
{
    MetalWorkQueue *wq = &workQueues[workQueueType];
    
     if (wq->commandBuffer == nil) {
        TF_FATAL_CODING_ERROR("No command buffer to label");
    }
    wq->commandBuffer.label = label;
}


void MtlfMetalContext::SetEventDependency(MetalWorkQueueType workQueueType, uint32_t eventValue)
{
    MetalWorkQueue *wq = &workQueues[workQueueType];
    
    if (outstandingDependency != METALWORKQUEUE_INVALID) {
        TF_FATAL_CODING_ERROR("Currently only support one outstanding dependency");
    }
    
    if (wq->encoderHasWork) {
        if (wq->encoderInUse) {
            TF_FATAL_CODING_ERROR("Can't set an event dependency if encoder is still in use");
        }
        // If the last used encoder wasn't ended then we need to end it now
        if (!wq->encoderEnded) {
            wq->encoderInUse = true;
            ReleaseEncoder(true, workQueueType);
        }
    }
    // Make this command buffer wait for the event to be resolved
    [wq->commandBuffer encodeWaitForEvent:queueSyncEvent value:((eventValue == 0) ? queueSyncEventCounter : eventValue)];
    
    // Record that we have an oustanding dependency on this work queue
    outstandingDependency = workQueueType;
}

uint32_t MtlfMetalContext::GenerateEvent(MetalWorkQueueType workQueueType)
{
    MetalWorkQueue *wq = &workQueues[workQueueType];
    
    if (outstandingDependency == METALWORKQUEUE_INVALID) {
        TF_FATAL_CODING_ERROR("No outstanding dependency to generate event for");
    }
    if (outstandingDependency == workQueueType) {
        TF_FATAL_CODING_ERROR("Cicrular event dependency - can't resolve event on same queue that is waiting for it");
    }
     if (wq->encoderHasWork) {
        if (wq->encoderInUse) {
            TF_FATAL_CODING_ERROR("Can't generate an event if encoder is still in use");
        }
        // If the last used encoder wasn't ended then we need to end it now
        if (!wq->encoderEnded) {
            wq->encoderInUse = true;
            ReleaseEncoder(true, workQueueType);
        }
    }
    // Generate event
    [wq->commandBuffer encodeSignalEvent:queueSyncEvent value:queueSyncEventCounter];
    
    // Remove the indication of an outstanding event
    outstandingDependency = METALWORKQUEUE_INVALID;
    
    return queueSyncEventCounter++;
}

uint32_t MtlfMetalContext::GetEventCounter()
{
    return queueSyncEventCounter;
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

void MtlfMetalContext::SetShadingPrograms(id<MTLFunction> vertexFunction, id<MTLFunction> fragmentFunction,  id<MTLFunction> computeFunction, bool _enableMVA, bool _enableComputeGS)
{
    CheckNewStateGather();
    
    renderPipelineStateDescriptor.vertexFunction   = vertexFunction;
    renderPipelineStateDescriptor.fragmentFunction = fragmentFunction;
    
    if (computeFunction) {
        computePipelineStateDescriptor.computeFunction = computeFunction;
    }
    else if (fragmentFunction == nil) {
        renderPipelineStateDescriptor.rasterizationEnabled = false;
    }
    else {
        renderPipelineStateDescriptor.rasterizationEnabled = true;
    }

    enableMVA = _enableMVA;
    enableComputeGS = _enableComputeGS;
    
    if(enableComputeGS && !enableMVA)
        TF_FATAL_CODING_ERROR("Manual Vertex Assembly must be enabled when using a Compute Geometry Shader!");
    if(enableComputeGS && (!computeFunction || !vertexFunction))
        TF_FATAL_CODING_ERROR("Compute and Vertex functions must be set when using a Compute Geometry Shader!");
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

void MtlfMetalContext::SetPipelineState()
{
    MetalWorkQueue *wq = currentWorkQueue;
    
    id<MTLRenderPipelineState> pipelineState;
    
    if (renderPipelineStateDescriptor == nil) {
         TF_FATAL_CODING_ERROR("No pipeline state descriptor allocated!");
    }
    
    if (wq->currentEncoderType != MTLENCODERTYPE_RENDER || !wq->encoderInUse || !wq->currentRenderEncoder) {
        TF_FATAL_CODING_ERROR("Not valid to call SetPipelineState() without an active render encoder");
    }
    
    renderPipelineStateDescriptor.label = @"Bake State";
    renderPipelineStateDescriptor.sampleCount = 1;
    renderPipelineStateDescriptor.inputPrimitiveTopology = MTLPrimitiveTopologyClassUnspecified;

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
        NSLog(@"Unique pipeline states %lu", renderPipelineStateMap.size());
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
    
    // Get a compute encoder on the Geometry Shader work queue
    if(enableComputeGS) {
        computeEncoder = GetComputeEncoder(METALWORKQUEUE_GEOMETRY_SHADER);
    }
    
    id<MTLComputePipelineState> computePipelineState;

    if (wq->currentEncoderType != MTLENCODERTYPE_RENDER || !wq->encoderInUse || !wq->currentRenderEncoder) {
        TF_FATAL_CODING_ERROR("Not valid to call BakeState() without an active render encoder");
    }
    
    // Create and set a new pipelinestate if required
    SetPipelineState();
    
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
                        computePipelineStateDescriptor.buffers[buffer->index].mutability = MTLMutabilityImmutable;
                    }
                    [wq->currentRenderEncoder setVertexBuffer:buffer->buffer offset:buffer->offset atIndex:buffer->index];
                }
                else if(buffer->stage == kMSL_ProgramStage_Fragment) {
                    [wq->currentRenderEncoder setFragmentBuffer:buffer->buffer offset:buffer->offset atIndex:buffer->index];
                }
                else{
                    if(enableComputeGS) {
                        [computeEncoder setBuffer:buffer->buffer offset:buffer->offset atIndex:buffer->index];
                        computePipelineStateDescriptor.buffers[buffer->index].mutability = MTLMutabilityImmutable;
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
    
    //MTL_FIXME: We should cache compute pipelines like we cache render pipelines.
    if(enableComputeGS) {
        NSError *error = NULL;
        MTLAutoreleasedComputePipelineReflection* reflData = 0;
        computePipelineState = [device newComputePipelineStateWithDescriptor:computePipelineStateDescriptor options:MTLPipelineOptionNone reflection:reflData error:&error];
        [computeEncoder setComputePipelineState:computePipelineState];
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
    boost::hash_combine(hashVal, computePipelineStateDescriptor.maxTotalThreadsPerThreadgroup);
    for (int i = 0; i < bufferCount; i++) {
        boost::hash_combine(hashVal, computePipelineStateDescriptor.buffers[i].mutability);
    }
    //boost::hash_combine(hashVal, computePipelineStateDescriptor.stageInputDescriptor); MTL_FIXME
    return hashVal;
}

// Using this function instead of setting the pipeline state directly allows caching
void MtlfMetalContext::SetComputeEncoderState(id<MTLFunction> computeFunction, unsigned long bufferWritableMask, NSString *label)
{
    MetalWorkQueue *wq = currentWorkQueue;
    
    id<MTLComputePipelineState> computePipelineState;
    
    if (wq->currentComputeEncoder == nil || wq->currentEncoderType != MTLENCODERTYPE_COMPUTE || !wq->encoderInUse) {
        TF_FATAL_CODING_ERROR("Compute encoder must be set and active to set the pipeline state");
    }
    
    if (computePipelineStateDescriptor == nil) {
        computePipelineStateDescriptor = [[MTLComputePipelineDescriptor alloc] init];
    }
    
    [computePipelineStateDescriptor reset];
    computePipelineStateDescriptor.computeFunction = computeFunction;
    computePipelineStateDescriptor.label = label;
    
    // Setup buffer mutability
    unsigned int bufferCount = 0;
    while (bufferWritableMask) {
        if (bufferWritableMask & 0x1) {
            computePipelineStateDescriptor.buffers[bufferCount].mutability = MTLMutabilityMutable;
        }
        else {
            computePipelineStateDescriptor.buffers[bufferCount].mutability = MTLMutabilityImmutable;
        }
        bufferCount++;
        bufferWritableMask >>= 1;
    }
    
    // Always call this because currently we're not tracking changes to its state
    size_t hashVal = HashComputePipelineDescriptor(bufferCount);
    
    // If this matches the currently bound pipeline state (assuming one is bound) then carry on using it
    if (wq->currentComputePipelineState != nil && hashVal == wq->currentComputePipelineDescriptorHash) {
        return;
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
            return;
        }
        computePipelineStateMap.emplace(wq->currentComputePipelineDescriptorHash, computePipelineState);
        NSLog(@"Unique compute pipeline states %lu", computePipelineStateMap.size());
    }
    
    if (computePipelineState != wq->currentComputePipelineState)
    {
        [wq->currentComputeEncoder setComputePipelineState:computePipelineState];
        wq->currentComputePipelineState = computePipelineState;
    }
}

void MtlfMetalContext::SetComputeBufferMutability(int index, bool isMutable)
{
    computePipelineStateDescriptor.buffers[index].mutability = isMutable ? MTLMutabilityMutable : MTLMutabilityImmutable;
}

void MtlfMetalContext::ResetEncoders(MetalWorkQueueType workQueueType)
{
    MetalWorkQueue *wq = &workQueues[workQueueType];
 
    wq->commandBuffer         = nil;
    
    wq->encoderInUse          = false;
    wq->encoderEnded          = false;
    wq->encoderHasWork        = false;
    wq->currentEncoderType    = MTLENCODERTYPE_NONE;
    wq->currentBlitEncoder    = nil;
    wq->currentRenderEncoder  = nil;
    wq->currentComputeEncoder = nil;
    
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
    
    //NSLog(@"Comitting command buffer %d %@", (int)workQueueType, wq->commandBuffer.label);
    
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
        // Raise a warning if there's no work here, still need to commit command buffer to free it though (check out why this is) MTL_FIXME
        NSLog(@"No work in this command buffer: %@", wq->commandBuffer.label);
    }
    
    [wq->commandBuffer commit];
    
    if (waitUntilCompleted) {
        [wq->commandBuffer waitUntilCompleted];
    }
    else if (waituntilScheduled) {
        [wq->commandBuffer waitUntilScheduled];
    }
  
    ResetEncoders(workQueueType);
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
        LabelCommandBuffer(@"Default lable - fix!");
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
            break;
        }
        case MTLENCODERTYPE_COMPUTE: {
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 101400 /* __MAC_10_14 */
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
            break;
        }
        case MTLENCODERTYPE_BLIT: {
            wq->currentBlitEncoder = [wq->commandBuffer blitCommandEncoder];
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
}

id<MTLBlitCommandEncoder> MtlfMetalContext::GetBlitEncoder(MetalWorkQueueType workQueueType)
{
    MetalWorkQueue *wq = &workQueues[workQueueType];
    SetCurrentEncoder(MTLENCODERTYPE_BLIT, workQueueType);
    return wq->currentBlitEncoder;
}

id<MTLComputeCommandEncoder> MtlfMetalContext::GetComputeEncoder(MetalWorkQueueType workQueueType)
{
    MetalWorkQueue *wq = &workQueues[workQueueType];
    SetCurrentEncoder(MTLENCODERTYPE_COMPUTE, workQueueType);
    return wq->currentComputeEncoder;
    
}

// If a renderpass descriptor is provided a new render encoder will be created otherwise we'll use the current one
id<MTLRenderCommandEncoder>  MtlfMetalContext::GetRenderEncoder(MetalWorkQueueType workQueueType)
{
    MetalWorkQueue *wq = &workQueues[workQueueType];
    SetCurrentEncoder(MTLENCODERTYPE_RENDER, workQueueType);
    return currentWorkQueue->currentRenderEncoder;
}


PXR_NAMESPACE_CLOSE_SCOPE

