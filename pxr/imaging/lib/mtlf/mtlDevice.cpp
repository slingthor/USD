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

//
// MtlfMetalContext
//

MtlfMetalContext::MtlfMetalContext()
{
    NSArray<id<MTLDevice>> *devices = MTLCopyAllDevices();
    
    device = devices.firstObject;

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
    if (!pipelineStateDescriptor) {
        pipelineStateDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
        
        pipelineStateDescriptor.label = @"Gathered State";
        pipelineStateDescriptor.sampleCount = 1;
        pipelineStateDescriptor.colorAttachments[0].blendingEnabled = YES;
        pipelineStateDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
        pipelineStateDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
        pipelineStateDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        pipelineStateDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        pipelineStateDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
    }
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
                                          uint32_t offset)
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

    vertexDescriptor.layouts[index].stride = stride;
}

void MtlfMetalContext::SetBuffer(int index, id<MTLBuffer> buffer)
{
    vertexBuffers.insert(std::make_pair(index, buffer));
}

void MtlfMetalContext::SetIndexBuffer(id<MTLBuffer> buffer)
{
    indexBuffer = buffer;
}

void MtlfMetalContext::SetSampler(int index, id<MTLSamplerState> sampler)
{
    samplers.insert(std::make_pair(index, sampler));
}

void MtlfMetalContext::SetTexture(int index, id<MTLTexture> texture)
{
    textures.insert(std::make_pair(index, texture));
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

    pipelineStateDescriptor.vertexDescriptor = vertexDescriptor;

    NSError *error = NULL;
    id<MTLRenderPipelineState> _pipelineState = [device newRenderPipelineStateWithDescriptor:pipelineStateDescriptor error:&error];
    if (!_pipelineState) {
        NSLog(@"Failed to created pipeline state, error %@", error);
        return;
    }

    [renderEncoder setRenderPipelineState:_pipelineState];

    for(auto buffer : vertexBuffers) {
        if (buffer.first != 65535)
            [renderEncoder setVertexBuffer:buffer.second offset:0 atIndex:buffer.first];
    }
    for(auto texture : textures) {
        [renderEncoder setFragmentTexture:texture.second atIndex:texture.first];
    }
    for(auto sampler : samplers) {
        [renderEncoder setFragmentSamplerState:sampler.second atIndex:sampler.first];
    }
}

void MtlfMetalContext::ClearState()
{
    pipelineStateDescriptor = nil;
    vertexDescriptor = nil;
    indexBuffer = nil;
    numVertexComponents = 0;
    
    vertexBuffers.clear();
    textures.clear();
    samplers.clear();

}

PXR_NAMESPACE_CLOSE_SCOPE

