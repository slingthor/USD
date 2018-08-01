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

#define DIRTY_METAL_STATE_OLD_STYLE_VERTEX_UNIFORM   0x001
#define DIRTY_METAL_STATE_OLD_STYLE_FRAGMENT_UNIFORM 0x002
#define DIRTY_METAL_STATE_VERTEX_UNIFORM_BUFFER      0x004
#define DIRTY_METAL_STATE_FRAGMENT_UNIFORM_BUFFER    0x008
#define DIRTY_METAL_STATE_INDEX_BUFFER               0x010
#define DIRTY_METAL_STATE_VERTEX_BUFFER              0x020
#define DIRTY_METAL_STATE_SAMPLER                    0x040
#define DIRTY_METAL_STATE_TEXTURE                    0x080
#define DIRTY_METAL_STATE_DRAW_TARGET                0x100
#define DIRTY_METAL_STATE_VERTEX_DESCRIPTOR          0x200

#define DIRTY_METAL_STATE_ALL                      0xFFFFFFFF

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

MtlfMetalContext::MtlfMetalContext() : queueSyncEventCounter(0), computeVSOutputCurrentIdx(0), computeVSOutputCurrentOffset(0), usingComputeVS(false), isEncoding(false)
{
    // Select Intel GPU if possible due to current issues on AMD. Revert when fixed - MTL_FIXME
	device = MtlfMetalContext::GetMetalDevice(PREFER_INTEGRATED_GPU);

    NSLog(@"Selected %@ for Metal Device", device.name);
    
    // Create a new command queue
    commandQueue = [device newCommandQueue];
    commandBuffer = nil;
    computeCommandQueue = [device newCommandQueue];
    computeCommandBuffer = nil;
    queueSyncEvent = [device newEvent];
    
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
    if (!defaultLibrary) {
        NSLog(@"Failed to created pipeline state, error %@", error);
    }

    // Load the fragment program into the library
    id <MTLFunction> fragmentProgram = [defaultLibrary newFunctionWithName:@"tex_fs"];
    id <MTLFunction> vertexProgram = [defaultLibrary newFunctionWithName:@"quad_vs"];
    id <MTLFunction> computeDepthCopyProgram = [defaultLibrary newFunctionWithName:@"copyDepth"];
    
    computePipelineState = [device newComputePipelineStateWithFunction:computeDepthCopyProgram error:&error];
    if (!computePipelineState) {
        NSLog(@"Failed to created pipeline state, error %@", error);
    }

    // Create the vertex description
    MTLVertexDescriptor *vtxDescriptor = [[MTLVertexDescriptor alloc] init];
    vtxDescriptor.attributes[0].format = MTLVertexFormatFloat2;
    vtxDescriptor.attributes[0].bufferIndex = 0;
    vtxDescriptor.attributes[0].offset = 0;
    
    vtxDescriptor.attributes[1].format = MTLVertexFormatFloat2;
    vtxDescriptor.attributes[1].bufferIndex = 0;
    vtxDescriptor.attributes[1].offset = sizeof(float) * 2;
    
    vtxDescriptor.layouts[0].stride = sizeof(Vertex);
    vtxDescriptor.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
    
    // Create a reusable pipeline state
    MTLRenderPipelineDescriptor *pipelineStateDesc = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineStateDesc.label = @"Metal/GL interop";
    pipelineStateDesc.sampleCount = 1;
    pipelineStateDesc.vertexFunction = vertexProgram;
    pipelineStateDesc.fragmentFunction = fragmentProgram;
    pipelineStateDesc.vertexDescriptor = vtxDescriptor;
    pipelineStateDesc.colorAttachments[0].blendingEnabled = YES;
    pipelineStateDesc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    pipelineStateDesc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
    pipelineStateDesc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    pipelineStateDesc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    pipelineStateDesc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
    
    pipelineState = [device newRenderPipelineStateWithDescriptor:pipelineStateDesc error:&error];
    if (!pipelineState) {
        NSLog(@"Failed to created pipeline state, error %@", error);
    }
    currentVertexDescriptorHash   = 0;
    currentColourAttachmentsHash  = 0;
    currentPipelineDescriptorHash = 0;
    currentPipelineState          = nil;
    windingOrder                  = MTLWindingCounterClockwise;
    cullMode                      = MTLCullModeNone;
    
    MTLDepthStencilDescriptor *depthStateDesc = [[MTLDepthStencilDescriptor alloc] init];
    depthStateDesc.depthWriteEnabled = YES;
    depthStateDesc.depthCompareFunction = MTLCompareFunctionLessEqual;
    depthState = [device newDepthStencilStateWithDescriptor:depthStateDesc];
    
    //vtxUniformBackingBuffer  = NULL;
    //fragUniformBackingBuffer = NULL;
    
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

    pipelineStateDescriptor = nil;
    computePipelineStateDescriptor = nil;
    vertexDescriptor = nil;
    indexBuffer = nil;
    remappedQuadIndexBuffer = nil;
    numVertexComponents = 0;
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

id<MTLBuffer>
MtlfMetalContext::GetQuadIndexBuffer(MTLIndexType indexTypeMetal) {
    // Each 4 vertices will require 6 remapped one
    uint32 remappedIndexBufferSize = (indexBuffer.length / 4) * 6;
    
    // Since remapping is expensive check if the buffer we created this from originally has changed  - MTL_FIXME - these checks are not robust
    if (remappedQuadIndexBuffer) {
        if ((remappedQuadIndexBufferSource != indexBuffer) ||
            (remappedQuadIndexBuffer.length != remappedIndexBufferSize)) {
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
    if (!pipelineStateDescriptor)
        pipelineStateDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
    if (!computePipelineStateDescriptor)
        computePipelineStateDescriptor = [[MTLComputePipelineDescriptor alloc] init];
    
    [pipelineStateDescriptor reset];
    [computePipelineStateDescriptor reset];
}

id<MTLCommandBuffer> MtlfMetalContext::CreateCommandBuffer() {
    commandBuffer        = [context->commandQueue commandBuffer];
    computeCommandBuffer = [context->computeCommandQueue commandBuffer];
    currentPipelineState = NULL;
    dirtyState           = DIRTY_METAL_STATE_ALL;
    return commandBuffer;
}

id<MTLRenderCommandEncoder> MtlfMetalContext::CreateRenderEncoder(MTLRenderPassDescriptor *renderPassDescriptor) {
    [commandBuffer encodeWaitForEvent:queueSyncEvent value:++queueSyncEventCounter];
    renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
    computeEncoder = [computeCommandBuffer computeCommandEncoderWithDispatchType:MTLDispatchTypeSerial];

    [renderEncoder setFrontFacingWinding:windingOrder];
    [renderEncoder setCullMode:cullMode];

    currentPipelineState = NULL;
    dirtyState           = DIRTY_METAL_STATE_ALL;
    isEncoding           = true;
    
    computeVSOutputCurrentIdx = computeVSOutputCurrentOffset = 0;
    
    return renderEncoder;
}

void MtlfMetalContext::EndEncoding()
{
    if(!isEncoding)
        TF_FATAL_CODING_ERROR("EndEncoding called while not actually encoding!");
    
    [computeEncoder endEncoding];
    [renderEncoder endEncoding];
    [computeCommandBuffer encodeSignalEvent:queueSyncEvent value:queueSyncEventCounter];
    
    isEncoding = false;
}

void MtlfMetalContext::Commit()
{
    if(isEncoding)
        TF_FATAL_CODING_ERROR("Commit called while encoders are still open!");
    [computeCommandBuffer commit];
    [commandBuffer commit];
}

void MtlfMetalContext::setFrontFaceWinding(MTLWinding _windingOrder)
{
    windingOrder = _windingOrder;
}

void MtlfMetalContext::setCullMode(MTLCullMode _cullMode)
{
    cullMode = _cullMode;
}

void MtlfMetalContext::SetShadingPrograms(id<MTLFunction> vertexFunction, id<MTLFunction> fragmentFunction, id<MTLFunction> computeVSFunction)
{
    CheckNewStateGather();
    
    pipelineStateDescriptor.vertexFunction   = vertexFunction;
    pipelineStateDescriptor.fragmentFunction = fragmentFunction;
    if(computeVSFunction != NULL) {
        computePipelineStateDescriptor.computeFunction = computeVSFunction;
        usingComputeVS = true;
    }
    else
        usingComputeVS = false;
}

void MtlfMetalContext::SetVertexAttribute(uint32_t index,
                                          int size,
                                          int type,
                                          size_t stride,
                                          uint32_t offset,
                                          const TfToken& name)
{
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
    
    dirtyState |= DIRTY_METAL_STATE_VERTEX_DESCRIPTOR;
}

void MtlfMetalContext::SetupComputeVS( UInt32 indexBufferSlot, id<MTLBuffer> indexBuffer, UInt32 indexCount, UInt32 startIndex, UInt32 baseVertex,
                                       UInt32 vertexOutputStructSize, UInt32 argumentBufferSlot, UInt32 outputBufferSlot)
{
    if(!usingComputeVS)
        TF_FATAL_CODING_ERROR("SetupComputeVS being called without having supplied a CS");

    [computeEncoder setBuffer:indexBuffer offset:(startIndex * sizeof(UInt32)) atIndex:indexBufferSlot];
    computePipelineStateDescriptor.buffers[indexBufferSlot].mutability = MTLMutabilityImmutable;

    struct { UInt32 _indexCount, _baseVertex; } arguments = { indexCount, baseVertex };
    [computeEncoder setBytes:(const void*)&arguments length:sizeof(arguments) atIndex:argumentBufferSlot];
    computePipelineStateDescriptor.buffers[argumentBufferSlot].mutability = MTLMutabilityImmutable;
    
    const UInt32 bufferSize = 100 * 1024 * 1024; //100 MiB
    UInt32 requiredSize = vertexOutputStructSize * indexCount;
    if(requiredSize > bufferSize)
        TF_FATAL_CODING_ERROR("Too large!");
    if(bufferSize - computeVSOutputCurrentOffset < requiredSize) {
        computeVSOutputCurrentIdx++;
        computeVSOutputCurrentOffset = 0;
    }
    if(computeVSOutputCurrentIdx >= computeVSOutputBuffers.size())
        computeVSOutputBuffers.push_back([device newBufferWithLength:bufferSize options:MTLResourceStorageModePrivate|MTLResourceOptionCPUCacheModeDefault]);
    id<MTLBuffer> outputBuffer = computeVSOutputBuffers[computeVSOutputCurrentIdx];
    [computeEncoder setBuffer:outputBuffer offset:computeVSOutputCurrentOffset atIndex:outputBufferSlot];
    computePipelineStateDescriptor.buffers[outputBufferSlot].mutability = MTLMutabilityMutable;
    [renderEncoder setVertexBuffer:indexBuffer offset:(startIndex * sizeof(UInt32)) atIndex:0];
    [renderEncoder setVertexBuffer:outputBuffer offset:computeVSOutputCurrentOffset atIndex:1];
    computeVSOutputCurrentOffset += vertexOutputStructSize * indexCount;
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
        dirtyState |= DIRTY_METAL_STATE_OLD_STYLE_VERTEX_UNIFORM;
    }
    else if (_stage == kMSL_ProgramStage_Fragment) {
        OSBuffer = fragUniformBackingBuffer;
        dirtyState |= DIRTY_METAL_STATE_OLD_STYLE_FRAGMENT_UNIFORM;
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
        dirtyState |= DIRTY_METAL_STATE_VERTEX_UNIFORM_BUFFER;
        if (oldStyleUniformSize) {
            if (vtxUniformBackingBuffer) {
                NSLog(@"Overwriting existing backing buffer, possible issue?");
            }
            vtxUniformBackingBuffer = bufferInfo;
        }
    }
    if(stage == kMSL_ProgramStage_Fragment) {
        dirtyState |= DIRTY_METAL_STATE_FRAGMENT_UNIFORM_BUFFER;
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
    dirtyState |= DIRTY_METAL_STATE_VERTEX_BUFFER;
}

void MtlfMetalContext::SetIndexBuffer(id<MTLBuffer> buffer)
{
    indexBuffer = buffer;
    //remappedQuadIndexBuffer = nil;
    dirtyState |= DIRTY_METAL_STATE_INDEX_BUFFER;
}

void MtlfMetalContext::SetSampler(int index, id<MTLSamplerState> sampler, const TfToken& name, MSL_ProgramStage stage)
{
    samplers.push_back({index, sampler, name, stage});
    dirtyState |= DIRTY_METAL_STATE_SAMPLER;
}

void MtlfMetalContext::SetTexture(int index, id<MTLTexture> texture, const TfToken& name, MSL_ProgramStage stage)
{
    textures.push_back({index, texture, name, stage});
    dirtyState |= DIRTY_METAL_STATE_TEXTURE;
}

void MtlfMetalContext::SetDrawTarget(MtlfDrawTarget *dt)
{
    drawTarget = dt;
    dirtyState |= DIRTY_METAL_STATE_DRAW_TARGET;
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

size_t MtlfMetalContext::HashColourAttachments()
{
    size_t hashVal = 0;
    MTLRenderPipelineColorAttachmentDescriptorArray *colourAttachments = pipelineStateDescriptor.colorAttachments;
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

size_t MtlfMetalContext::HashPipeLineDescriptor()
{
    size_t hashVal = 0;
    boost::hash_combine(hashVal, pipelineStateDescriptor.vertexFunction);
    boost::hash_combine(hashVal, pipelineStateDescriptor.fragmentFunction);
    boost::hash_combine(hashVal, pipelineStateDescriptor.sampleCount);
    boost::hash_combine(hashVal, pipelineStateDescriptor.rasterSampleCount);
    boost::hash_combine(hashVal, pipelineStateDescriptor.alphaToCoverageEnabled);
    boost::hash_combine(hashVal, pipelineStateDescriptor.alphaToOneEnabled);
    boost::hash_combine(hashVal, pipelineStateDescriptor.rasterizationEnabled);
    boost::hash_combine(hashVal, pipelineStateDescriptor.depthAttachmentPixelFormat);
    boost::hash_combine(hashVal, pipelineStateDescriptor.stencilAttachmentPixelFormat);
#if METAL_TESSELLATION_SUPPORT
    // Add here...
#endif
    boost::hash_combine(hashVal, currentVertexDescriptorHash);
    boost::hash_combine(hashVal, currentColourAttachmentsHash);
    return hashVal;
}

void MtlfMetalContext::SetPipelineState()
{
    id<MTLRenderPipelineState> pipelineState;
    
    if (pipelineStateDescriptor == nil) {
         TF_FATAL_CODING_ERROR("No pipeline state descriptor allocated!");
    }
    
    pipelineStateDescriptor.label = @"Bake State";
    pipelineStateDescriptor.sampleCount = 1;
    pipelineStateDescriptor.inputPrimitiveTopology = MTLPrimitiveTopologyClassUnspecified;

#if METAL_TESSELLATION_SUPPORT
    pipelineStateDescriptor.maxTessellationFactor             = 1;
    pipelineStateDescriptor.tessellationFactorScaleEnabled    = NO;
    pipelineStateDescriptor.tessellationFactorFormat          = MTLTessellationFactorFormatHalf;
    pipelineStateDescriptor.tessellationControlPointIndexType = MTLTessellationControlPointIndexTypeNone;
    pipelineStateDescriptor.tessellationFactorStepFunction    = MTLTessellationFactorStepFunctionConstant;
    pipelineStateDescriptor.tessellationOutputWindingOrder    = MTLWindingCounterClockwise;
    pipelineStateDescriptor.tessellationPartitionMode         = MTLTessellationPartitionModePow2;
#endif
    
    if (dirtyState & DIRTY_METAL_STATE_VERTEX_DESCRIPTOR || pipelineStateDescriptor.vertexDescriptor == NULL) {
        // This assignment can be expensive as the vertexdescriptor will be copied (due to interface property)
        pipelineStateDescriptor.vertexDescriptor = vertexDescriptor;
        // Update vertex descriptor hash
        currentVertexDescriptorHash = HashVertexDescriptor();
    }
    
    if (dirtyState & DIRTY_METAL_STATE_DRAW_TARGET) {
        numColourAttachments = 0;
    
        if (drawTarget) {
            auto& attachments = drawTarget->GetAttachments();
            for(auto it : attachments) {
                MtlfDrawTarget::MtlfAttachment* attachment = ((MtlfDrawTarget::MtlfAttachment*)&(*it.second));
                MTLPixelFormat depthFormat = [attachment->GetTextureName() pixelFormat];
                if(attachment->GetFormat() == GL_DEPTH_COMPONENT || attachment->GetFormat() == GL_DEPTH_STENCIL) {
                    pipelineStateDescriptor.depthAttachmentPixelFormat = depthFormat;
                    if(attachment->GetFormat() == GL_DEPTH_STENCIL)
                        pipelineStateDescriptor.stencilAttachmentPixelFormat = depthFormat; //Do not use the stencil pixel format (X32_S8)
                }
                else {
                    id<MTLTexture> texture = attachment->GetTextureName();
                    int idx = attachment->GetAttach();
                    
                    pipelineStateDescriptor.colorAttachments[idx].blendingEnabled = NO;
                    pipelineStateDescriptor.colorAttachments[idx].pixelFormat = [texture pixelFormat];
                }
                numColourAttachments++;
            }
        }
        else {
            //METAL TODO: Why does this need to be hardcoded? There is no matching drawTarget? Can we get this info from somewhere?
            pipelineStateDescriptor.colorAttachments[0].blendingEnabled = YES;
            pipelineStateDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
            pipelineStateDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
            pipelineStateDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
            pipelineStateDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
            pipelineStateDescriptor.colorAttachments[0].pixelFormat = mtlColorTexture.pixelFormat;
            numColourAttachments++;
            
            pipelineStateDescriptor.depthAttachmentPixelFormat = mtlDepthTexture.pixelFormat;
        }
        [renderEncoder setDepthStencilState:depthState];
        // Update colour attachments hash
        currentColourAttachmentsHash = HashColourAttachments();
    }
    
    // Unset the state tracking flags
    dirtyState &= ~(DIRTY_METAL_STATE_VERTEX_DESCRIPTOR | DIRTY_METAL_STATE_DRAW_TARGET);
    
#if METAL_STATE_OPTIMISATION
    // Always call this because currently we're not tracking changes to its state
    size_t hashVal = HashPipeLineDescriptor();
    
    // If this matches the current pipeline state then we should already have the correct pipeline bound
    if (hashVal == currentPipelineDescriptorHash && currentPipelineState != nil) {
        return;
    }
    currentPipelineDescriptorHash = hashVal;
    
    boost::unordered_map<size_t, id<MTLRenderPipelineState>>::const_iterator pipelineStateIt = pipelineStateMap.find(currentPipelineDescriptorHash);
    
    if (pipelineStateIt != pipelineStateMap.end()) {
        pipelineState = pipelineStateIt->second;
    }
    else
#endif
    {
        NSError *error = NULL;
        pipelineState = [device newRenderPipelineStateWithDescriptor:pipelineStateDescriptor error:&error];
        if (!pipelineState) {
            NSLog(@"Failed to created pipeline state, error %@", error);
            return;
        }
#if METAL_STATE_OPTIMISATION
        pipelineStateMap.emplace(currentPipelineDescriptorHash, pipelineState);
        NSLog(@"Unique pipeline states %lu", pipelineStateMap.size());
#endif
        
    }
  
#if METAL_STATE_OPTIMISATION
    if (pipelineState != currentPipelineState)
#endif
    {
        [renderEncoder setRenderPipelineState:pipelineState];
        currentPipelineState = pipelineState;
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

void MtlfMetalContext::BakeState()
{
#if !METAL_STATE_OPTIMISATION
    dirtyState = DIRTY_METAL_STATE_ALL;
#endif
    
    id<MTLComputePipelineState> computePipelineState;

    // Create and set a new pipelinestate if required
    SetPipelineState();
 
    // Any buffers modified
    if (dirtyState & (DIRTY_METAL_STATE_VERTEX_UNIFORM_BUFFER     |
                       DIRTY_METAL_STATE_FRAGMENT_UNIFORM_BUFFER  |
                       DIRTY_METAL_STATE_VERTEX_BUFFER            |
                       DIRTY_METAL_STATE_OLD_STYLE_VERTEX_UNIFORM |
                       DIRTY_METAL_STATE_OLD_STYLE_FRAGMENT_UNIFORM)) {
        
        for(auto buffer : boundBuffers)
        {
            // Only output if this buffer was modified
            if (buffer->modified) {
                if(buffer->stage == kMSL_ProgramStage_Vertex){
                    if(usingComputeVS) {
                        [computeEncoder setBuffer:buffer->buffer offset:buffer->offset atIndex:buffer->index];
                        computePipelineStateDescriptor.buffers[buffer->index].mutability = MTLMutabilityImmutable;
                    }
                    else
                        [renderEncoder setVertexBuffer:buffer->buffer offset:buffer->offset atIndex:buffer->index];
                }
                else if(buffer->stage == kMSL_ProgramStage_Fragment) {
                    [renderEncoder setFragmentBuffer:buffer->buffer offset:buffer->offset atIndex:buffer->index];
                }
                else{
                    TF_FATAL_CODING_ERROR("Not implemented!"); //Compute case
                }
                buffer->modified = false;
            }
        }
         
         if (dirtyState & DIRTY_METAL_STATE_OLD_STYLE_VERTEX_UNIFORM) {
             UpdateOldStyleUniformBlock(vtxUniformBackingBuffer, kMSL_ProgramStage_Vertex);
         }
         if (dirtyState & DIRTY_METAL_STATE_OLD_STYLE_FRAGMENT_UNIFORM) {
             UpdateOldStyleUniformBlock(fragUniformBackingBuffer, kMSL_ProgramStage_Fragment);
         }
         
         dirtyState &= ~(DIRTY_METAL_STATE_VERTEX_UNIFORM_BUFFER    |
                         DIRTY_METAL_STATE_FRAGMENT_UNIFORM_BUFFER  |
                         DIRTY_METAL_STATE_VERTEX_BUFFER            |
                         DIRTY_METAL_STATE_OLD_STYLE_VERTEX_UNIFORM |
                         DIRTY_METAL_STATE_OLD_STYLE_FRAGMENT_UNIFORM);
    }

 
    if (dirtyState & DIRTY_METAL_STATE_TEXTURE) {
        for(auto texture : textures) {
            if(texture.stage == kMSL_ProgramStage_Vertex) {
                if(usingComputeVS)
                    [computeEncoder setTexture:texture.texture atIndex:texture.index];
                else
                    [renderEncoder setVertexTexture:texture.texture atIndex:texture.index];
            }
            else if(texture.stage == kMSL_ProgramStage_Fragment)
                [renderEncoder setFragmentTexture:texture.texture atIndex:texture.index];
            else
                TF_FATAL_CODING_ERROR("Not implemented!"); //Compute case
        }
        dirtyState &= ~DIRTY_METAL_STATE_TEXTURE;
    }
    if (dirtyState & DIRTY_METAL_STATE_SAMPLER) {
        for(auto sampler : samplers) {
            if(sampler.stage == kMSL_ProgramStage_Vertex) {
                if(usingComputeVS)
                    [computeEncoder setSamplerState:sampler.sampler atIndex:sampler.index];
                else
                    [renderEncoder setVertexSamplerState:sampler.sampler atIndex:sampler.index];
            }
            else if(sampler.stage == kMSL_ProgramStage_Fragment)
                [renderEncoder setFragmentSamplerState:sampler.sampler atIndex:sampler.index];
            else
                TF_FATAL_CODING_ERROR("Not implemented!"); //Compute case
        }
        dirtyState &= ~DIRTY_METAL_STATE_SAMPLER;
    }
    
    if(usingComputeVS) {
        NSError *error = NULL;
        MTLAutoreleasedComputePipelineReflection* reflData = 0;
        computePipelineState = [device newComputePipelineStateWithDescriptor:computePipelineStateDescriptor options:MTLPipelineOptionNone reflection:reflData error:&error];
        [computeEncoder setComputePipelineState:computePipelineState];
    }
}

void MtlfMetalContext::ClearState()
{
    pipelineStateDescriptor = nil;
    vertexDescriptor = nil;
    currentPipelineState = nil;
    indexBuffer = nil;
    numVertexComponents = 0;
    dirtyState = DIRTY_METAL_STATE_ALL;
    
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

PXR_NAMESPACE_CLOSE_SCOPE

