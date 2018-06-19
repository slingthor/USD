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
    return preferredDeviceList.firstObject;
}


//
// MtlfMetalContext
//

MtlfMetalContext::MtlfMetalContext()
{
    device = MtlfMetalContext::GetMetalDevice(PREFER_DEFAULT_GPU);

    NSLog(@"Selected %@ for Metal Device", device.name);
    
    // Create a new command queue
    commandQueue = [device newCommandQueue];
    commandBuffer = nil;
    
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
    
    // Load the fragment program into the library
    id <MTLFunction> fragmentProgram = [defaultLibrary newFunctionWithName:@"tex_fs"];
    id <MTLFunction> vertexProgram = [defaultLibrary newFunctionWithName:@"quad_vs"];
    
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
    
    MTLDepthStencilDescriptor *depthStateDesc = [[MTLDepthStencilDescriptor alloc] init];
    depthStateDesc.depthCompareFunction = MTLCompareFunctionAlways;
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

    GLint samplerLoc = glGetUniformLocation(glShaderProgram, "interopTexture");
    
    // Indicate that the diffuse texture will be bound to texture unit 0
    GLint unit = 0;
    glUniform1i(samplerLoc, unit);
    
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
    
    
    
    //  allocate a CVPixelBuffer
    CVOpenGLTextureRef cvglTexture;
    CVPixelBufferRef pixelBuffer;
    CVMetalTextureRef cvmtlTexture;
    
    cvglTextureCache = nil;
    cvmtlTextureCache = nil;

    NSDictionary* cvBufferProperties = @{
        (__bridge NSString*)kCVPixelBufferOpenGLCompatibilityKey : @(TRUE),
        (__bridge NSString*)kCVPixelBufferMetalCompatibilityKey : @(TRUE),
    };

    CVPixelBufferCreate(kCFAllocatorDefault, 1024, 1024,
                                kCVPixelFormatType_32BGRA,
                                (__bridge CFDictionaryRef)cvBufferProperties,
                                &pixelBuffer);
    
    CVReturn cvret = CVMetalTextureCacheCreate(kCFAllocatorDefault, nil, device, nil, &cvmtlTextureCache);
    assert(cvret == kCVReturnSuccess);
    
    CGLContextObj glctx = [[NSOpenGLContext currentContext] CGLContextObj];
    CGLPixelFormatObj glPixelFormat = [[[NSOpenGLContext currentContext] pixelFormat] CGLPixelFormatObj];
    cvret = CVOpenGLTextureCacheCreate(kCFAllocatorDefault, nil, (__bridge CGLContextObj _Nonnull)(glctx), glPixelFormat, nil, &cvglTextureCache);

    assert(cvret == kCVReturnSuccess);

    cvret = CVOpenGLTextureCacheCreateTextureFromImage(kCFAllocatorDefault, cvglTextureCache,
                                                                pixelBuffer,
                                                                nil, &cvglTexture);
    glTexture = CVOpenGLTextureGetName(cvglTexture);
    
    cvret = CVMetalTextureCacheCreateTextureFromImage(kCFAllocatorDefault, cvmtlTextureCache,
                                                               pixelBuffer, nil, MTLPixelFormatBGRA8Unorm,
                                                               1024, 1024, 0, &cvmtlTexture);
    
    mtlTexture = CVMetalTextureGetTexture(cvmtlTexture);
    
    pipelineStateDescriptor = nil;
    vertexDescriptor = nil;
    indexBuffer = nil;
    numVertexComponents = 0;
}

MtlfMetalContext::~MtlfMetalContext()
{
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

void MtlfMetalContext::CheckNewStateGather()
{
    // Lazily create a new state object
    if (!pipelineStateDescriptor)
        pipelineStateDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
    
    [pipelineStateDescriptor reset];
}

void MtlfMetalContext::SetShadingPrograms(id<MTLFunction> vertexFunction, id<MTLFunction> fragmentFunction)
{
    CheckNewStateGather();
    
    pipelineStateDescriptor.vertexFunction = vertexFunction;
    pipelineStateDescriptor.fragmentFunction = fragmentFunction;
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

        //cullStyle?
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
}

void MtlfMetalContext::SetUniform(const void* _data, uint32 _dataSize, const TfToken& _name, uint32 _index, MSL_ProgramStage _stage)
{
    OldStyleUniformData newUniform = { _index, 0, 0, _name, _stage };
    newUniform.alloc(_data, _dataSize);
    oldStyleUniforms.push_back(newUniform);
}

void MtlfMetalContext::SetUniformBuffer(int index, id<MTLBuffer> buffer, const TfToken& name, MSL_ProgramStage stage, bool oldStyleBacker)
{
    if(stage == 0)
        TF_FATAL_CODING_ERROR("Not allowed!");
        
    uniformBuffers.push_back({index, buffer, name, stage});
    
    if(oldStyleBacker)
    {
        if(stage == kMSL_ProgramStage_Vertex) {
            vtxUniformBackingBuffer = buffer;
        }
        else if(stage == kMSL_ProgramStage_Fragment) {
            fragUniformBackingBuffer = buffer;
        }
    }
}

void MtlfMetalContext::SetBuffer(int index, id<MTLBuffer> buffer, const TfToken& name)
{
    vertexBuffers.push_back({index, buffer, name});
}

void MtlfMetalContext::SetIndexBuffer(id<MTLBuffer> buffer)
{
    indexBuffer = buffer;
}

void MtlfMetalContext::SetSampler(int index, id<MTLSamplerState> sampler, const TfToken& name, MSL_ProgramStage stage)
{
    samplers.push_back({index, sampler, name, stage});
}

void MtlfMetalContext::SetTexture(int index, id<MTLTexture> texture, const TfToken& name, MSL_ProgramStage stage)
{
    textures.push_back({index, texture, name, stage});
}

void MtlfMetalContext::SetDrawTarget(MtlfDrawTarget *dt)
{
    drawTarget = dt;
}

void MtlfMetalContext::BakeState()
{
    if (pipelineStateDescriptor == nil) {
        // This is temporary
        return;
    }

    pipelineStateDescriptor.label = @"Bake State";
    pipelineStateDescriptor.sampleCount = 1;
    pipelineStateDescriptor.vertexDescriptor = vertexDescriptor;
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
        }
    }
    else {
        //METAL TODO: Why does this need to be hardcoded? There is no matching drawTarget? Can we get this info from somewhere?
        pipelineStateDescriptor.colorAttachments[0].blendingEnabled = YES;
        pipelineStateDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
        pipelineStateDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
        pipelineStateDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        pipelineStateDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        pipelineStateDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
    }
    
    NSError *error = NULL;
    id<MTLRenderPipelineState> _pipelineState = [device newRenderPipelineStateWithDescriptor:pipelineStateDescriptor error:&error];
    if (!_pipelineState) {
        NSLog(@"Failed to created pipeline state, error %@", error);
        return;
    }
    
    [renderEncoder setRenderPipelineState:_pipelineState];
    
    for(auto uniform : oldStyleUniforms)
    {
        if(uniform.stage == kMSL_ProgramStage_Vertex) {
            if(!vtxUniformBackingBuffer)
                TF_FATAL_CODING_ERROR("No vertex uniform backing buffer assigned!");
            uint8 * data = (uint8*)(vtxUniformBackingBuffer.contents);
            memcpy(data + uniform.index, uniform.data, uniform.dataSize);
            [vtxUniformBackingBuffer didModifyRange:NSMakeRange(uniform.index, uniform.dataSize)];
        }
        else if(uniform.stage == kMSL_ProgramStage_Fragment) {
            if(!fragUniformBackingBuffer)
                TF_FATAL_CODING_ERROR("No fragment uniform backing buffer assigned!");
            uint8 * data = (uint8*)(fragUniformBackingBuffer.contents);
            memcpy(data + uniform.index, uniform.data, uniform.dataSize);
            [fragUniformBackingBuffer didModifyRange:NSMakeRange(uniform.index, uniform.dataSize)];
        }
        else
            TF_FATAL_CODING_ERROR("Not implemented!"); //Compute case
    }
    
    for(auto buffer : uniformBuffers)
    {
        if(buffer.stage == kMSL_ProgramStage_Vertex)
            [renderEncoder setVertexBuffer:buffer.buffer offset:0 atIndex:buffer.idx];
        else if(buffer.stage == kMSL_ProgramStage_Fragment)
            [renderEncoder setFragmentBuffer:buffer.buffer offset:0 atIndex:buffer.idx];
        else
            TF_FATAL_CODING_ERROR("Not implemented!"); //Compute case
    }

    for(auto buffer : vertexBuffers) {
        [renderEncoder setVertexBuffer:buffer.buffer offset:0 atIndex:buffer.idx];
    } 
    for(auto texture : textures) {
        if(texture.stage == kMSL_ProgramStage_Vertex)
            [renderEncoder setVertexTexture:texture.texture atIndex:texture.idx];
        else if(texture.stage == kMSL_ProgramStage_Fragment)
            [renderEncoder setFragmentTexture:texture.texture atIndex:texture.idx];
        else
            TF_FATAL_CODING_ERROR("Not implemented!"); //Compute case
    }
    for(auto sampler : samplers) {
        if(sampler.stage == kMSL_ProgramStage_Vertex)
            [renderEncoder setVertexSamplerState:sampler.sampler atIndex:sampler.idx];
        else if(sampler.stage == kMSL_ProgramStage_Fragment)
            [renderEncoder setFragmentSamplerState:sampler.sampler atIndex:sampler.idx];
        else
            TF_FATAL_CODING_ERROR("Not implemented!"); //Compute case
    }
}

void MtlfMetalContext::ClearState()
{
    pipelineStateDescriptor = nil;
    vertexDescriptor = nil;
    indexBuffer = nil;
    numVertexComponents = 0;
    
    for(auto it = oldStyleUniforms.begin(); it != oldStyleUniforms.end(); ++it)
        it->release();
    oldStyleUniforms.clear();
    vtxUniformBackingBuffer = 0;
    fragUniformBackingBuffer = 0;

    vertexBuffers.clear();
    uniformBuffers.clear();
    textures.clear();
    samplers.clear();
}

PXR_NAMESPACE_CLOSE_SCOPE

