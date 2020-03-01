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

#include "pxr/imaging/hdx/fullscreenShader.h"
#include "pxr/imaging/hdx/package.h"
#include "pxr/imaging/hdSt/program.h"
#include "pxr/imaging/hdSt/resourceFactory.h"
#include "pxr/imaging/hf/perfLog.h"
#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/glf/diagnostic.h"
#include "pxr/imaging/hio/glslfx.h"
#include "pxr/base/tf/staticTokens.h"

#include "pxr/imaging/hgi/buffer.h"

#if defined(ARCH_GFX_OPENGL)
#include "pxr/imaging/hdSt/GL/glslProgram.h"
#include "pxr/imaging/hgiGL/buffer.h"
#endif

#if defined(ARCH_GFX_METAL)
#include "pxr/imaging/hdSt/Metal/mslProgram.h"
#include "pxr/imaging/hdSt/Metal/codeGenMSL.h"
#include "pxr/imaging/hgiMetal/buffer.h"
#include "pxr/imaging/hgiMetal/immediateCommandBuffer.h"
#endif


PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PRIVATE_TOKENS(
    _tokens,
    ((fullscreenVertex,              "FullscreenVertex"))
    ((compositeFragmentNoDepth,      "CompositeFragmentNoDepth"))
    ((compositeFragmentWithDepth,    "CompositeFragmentWithDepth"))
    (fullscreenShader)
);

HdxFullscreenShader::HdxFullscreenShader(Hgi *hgi)
    : _hgi(hgi), _program(), _depthAware(false)
{
    _isOpenGL = HdStResourceFactory::GetInstance()->IsOpenGL();
}

HdxFullscreenShader::~HdxFullscreenShader()
{
    for (auto const& texture : _textures) {
        if (_isOpenGL) {
#if defined(ARCH_GFX_OPENGL)
            GLuint t = texture.second;
            glDeleteTextures(1, &t);
#endif
        }
    }
    if (_vertexBuffer) {
        _hgi->DestroyBuffer(&_vertexBuffer);
    }
    if (_program) {
        _program.reset();
    }
    GLF_POST_PENDING_GL_ERRORS();
}

void
HdxFullscreenShader::SetProgramToCompositor(bool depthAware) {
    SetProgram(HdxPackageFullscreenShader(),
        depthAware ? _tokens->compositeFragmentWithDepth
                   : _tokens->compositeFragmentNoDepth);
    _depthAware = depthAware;
}

void
HdxFullscreenShader::SetProgram(TfToken const& glslfx, TfToken const& technique) {
    if (_glslfx == glslfx && _technique == technique) {
        return;
    }

    _program.reset(HdStResourceFactory::GetInstance()->NewProgram(
        _tokens->fullscreenShader));

    HioGlslfx vsGlslfx(HdxPackageFullscreenShader());
    HioGlslfx fsGlslfx(glslfx);

    if (!_program->CompileShader(GL_VERTEX_SHADER,
            vsGlslfx.GetSource(_tokens->fullscreenVertex)) ||
        !_program->CompileShader(GL_FRAGMENT_SHADER,
            fsGlslfx.GetSource(technique)) ||
        !_program->Link()) {
        TF_CODING_ERROR("Failed to load shader: %s (%s)",
                glslfx.GetText(), technique.GetText());
        _program.reset();
        return;
    }
}

void
HdxFullscreenShader::SetUniform(TfToken const& name, VtValue const& data)
{
    if (data.IsEmpty()) {
        _uniforms.erase(name);
    } else {
        _uniforms[name] = data;
    }
}

void
HdxFullscreenShader::_CreateBufferResources()
{
    /* For the fullscreen pass, we draw a triangle:
     *
     * |\
     * |_\
     * | |\
     * |_|_\
     *
     * The vertices are at (-1, 3) [top left]; (-1, -1) [bottom left];
     * and (3, -1) [bottom right]; UVs are assigned so that the bottom left
     * is (0,0) and the clipped vertices are 2 on their axis, so that:
     * x=-1 => s = 0; x = 3 => s = 2, which means x = 1 => s = 1.
     *
     * This maps the texture space [0,1]^2 to the clip space XY [-1,1]^2.
     * The parts of the triangle extending past NDC space are clipped before
     * rasterization.
     *
     * This has the advantage (over rendering a quad) that we don't render
     * the diagonal twice.
     *
     * Note that we're passing in NDC positions, and we don't expect the vertex
     * shader to transform them.  Also note: the fragment shader can optionally
     * read depth from a texture, but otherwise the depth is -1, meaning near
     * plane.
     */
    //                                 positions          |   uvs
    static const float vertices[] = { -1,  3, -1, 1,        0, 2,
                                      -1, -1, -1, 1,        0, 0,
                                       3, -1, -1, 1,        2, 0 };

    HgiBufferDesc desc;
    desc.usage = HgiBufferUsageVertex;
    desc.byteSize = sizeof(vertices);
    desc.initialData = vertices;
    _vertexBuffer = _hgi->CreateBuffer(desc);
}

void
HdxFullscreenShader::_CreateTextureResources(GarchTextureGPUHandle *texture)
{
    if (_isOpenGL) {
#if defined(ARCH_GFX_OPENGL)
        GLuint t;
        glGenTextures(1, &t);
        glBindTexture(GL_TEXTURE_2D, t);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        *texture = t;
#endif
    }
}

void
HdxFullscreenShader::SetTexture(TfToken const& name, int width, int height,
                             HdFormat format, void *data)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    if (width == 0 || height == 0 || data == nullptr) {
        auto it = _textures.find(name);
        if (it != _textures.end()) {
            if (_isOpenGL) {
#if defined(ARCH_GFX_OPENGL)
                GLuint t = it->second;
                glDeleteTextures(1, &t);
#endif
            }
            _textures.erase(it);
        }
        return;
    }

    auto it = _textures.find(name);
    if (it == _textures.end()) {
        GarchTextureGPUHandle tex;
        _CreateTextureResources(&tex);
        it = _textures.insert({name, tex}).first;
    }
    if (_isOpenGL) {
#if defined(ARCH_GFX_OPENGL)
        glBindTexture(GL_TEXTURE_2D, it->second);

        if (format == HdFormatFloat32Vec4) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA,
                         GL_FLOAT, data);
        } else if (format == HdFormatFloat16Vec4) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA,
                         GL_HALF_FLOAT, data);
        } else if (format == HdFormatUNorm8Vec4) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA,
                         GL_UNSIGNED_BYTE, data);
        } else if (format == HdFormatFloat32) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, width, height, 0, GL_RED,
                         GL_FLOAT, data);
        } else {
            TF_WARN("Unsupported texture format: %s (%s)",
                    name.GetText(), TfEnum::GetName(format).c_str());
        }
        glBindTexture(GL_TEXTURE_2D, 0);

        GLF_POST_PENDING_GL_ERRORS();
#endif
    }
}

void
HdxFullscreenShader::_SetUniform(TfToken const& name, VtValue const& value)
{
    if (_isOpenGL) {
#if defined(ARCH_GFX_OPENGL)
        GLuint programId = boost::dynamic_pointer_cast<HdStGLSLProgram>(_program)->GetGLProgram();

        GLint loc = glGetUniformLocation(programId, name.GetText());
        HdTupleType type = HdGetValueTupleType(value);
        const void* data = HdGetValueData(value);

        switch (type.type) {
            case HdTypeInt32:
                glUniform1iv(loc, type.count, static_cast<const GLint*>(data)); break;
            case HdTypeInt32Vec2:
                glUniform2iv(loc, type.count, static_cast<const GLint*>(data)); break;
            case HdTypeInt32Vec3:
                glUniform3iv(loc, type.count, static_cast<const GLint*>(data)); break;
            case HdTypeInt32Vec4:
                glUniform4iv(loc, type.count, static_cast<const GLint*>(data)); break;
            case HdTypeUInt32:
                glUniform1uiv(loc, type.count, static_cast<const GLuint*>(data)); break;
            case HdTypeUInt32Vec2:
                glUniform2uiv(loc, type.count, static_cast<const GLuint*>(data)); break;
            case HdTypeUInt32Vec3:
                glUniform3uiv(loc, type.count, static_cast<const GLuint*>(data)); break;
            case HdTypeUInt32Vec4:
                glUniform4uiv(loc, type.count, static_cast<const GLuint*>(data)); break;
            case HdTypeFloat:
                glUniform1fv(loc, type.count, static_cast<const GLfloat*>(data)); break;
            case HdTypeFloatVec2:
                glUniform2fv(loc, type.count, static_cast<const GLfloat*>(data)); break;
            case HdTypeFloatVec3:
                glUniform3fv(loc, type.count, static_cast<const GLfloat*>(data)); break;
            case HdTypeFloatVec4:
                glUniform4fv(loc, type.count, static_cast<const GLfloat*>(data)); break;
            case HdTypeFloatMat3:
                glUniformMatrix3fv(loc, type.count, GL_FALSE, static_cast<const GLfloat*>(data)); break;
            case HdTypeFloatMat4:
                glUniformMatrix4fv(loc, type.count, GL_FALSE, static_cast<const GLfloat*>(data)); break;
            default:
                TF_WARN("Unsupported uniform type: %s (%s)",
                    name.GetText(), value.GetTypeName().c_str());
                break;
        }
#endif
    }
}

void 
HdxFullscreenShader::Draw(TextureMap const& textures)
{
    // No-op if we haven't set a shader.
    if (!_program) {
        if (_glslfx.IsEmpty() || _technique.IsEmpty()) {
            TF_CODING_ERROR("HdxFullscreenShader: caller needs to set a program "
                            "before calling draw!");
        }
        return;
    }

    // Create draw buffers if they haven't been created yet.
    if (_vertexBuffer == 0) {
        _CreateBufferResources();
    }

    // A note here: HdxFullscreenShader is used for all of our plugins and has to be
    // robust to poor GL support.  OSX compatibility profile provides a
    // GL 2.1 API, slightly restricting our choice of API and heavily
    // restricting our shader syntax.

    _program->SetProgram();

    if (_isOpenGL) {
#if defined(ARCH_GFX_OPENGL)
        GLuint programId = boost::dynamic_pointer_cast<HdStGLSLProgram>(_program)->GetGLProgram();
        
        // Setup textures
        int textureIndex = 0;
        for (auto const& texture : textures) {
            glActiveTexture(GL_TEXTURE0 + textureIndex);
            glBindTexture(GL_TEXTURE_2D, texture.second);
            GLint loc = glGetUniformLocation(programId, texture.first.GetText());
            glUniform1i(loc, textureIndex);
            textureIndex++;
        }

        // Set up buffers
        GLint locPosition = glGetAttribLocation(programId, "position");
        GLuint bufferId = static_cast<HgiGLBuffer*>(_vertexBuffer.Get())->GetBufferId();
        glBindBuffer(GL_ARRAY_BUFFER, bufferId);
        glVertexAttribPointer(locPosition, 4, GL_FLOAT, GL_FALSE,
                sizeof(float)*6, 0);
        glEnableVertexAttribArray(locPosition);

        GLint locUv = glGetAttribLocation(programId, "uvIn");
        glVertexAttribPointer(locUv, 2, GL_FLOAT, GL_FALSE,
                sizeof(float)*6, reinterpret_cast<void*>(sizeof(float)*4));
        glEnableVertexAttribArray(locUv);

        // Set up uniforms
        for (auto const& uniform : _uniforms) {
            _SetUniform(uniform.first, uniform.second);
        }

        // Set up state
        GLboolean restoreAlphaToCoverage;
        glGetBooleanv(GL_SAMPLE_ALPHA_TO_COVERAGE, &restoreAlphaToCoverage);
        glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);

        glDrawArrays(GL_TRIANGLES, 0, 3);

        // Restore state
        if (restoreAlphaToCoverage) {
            glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
        }

        // Restore buffers
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glDisableVertexAttribArray(locPosition);
        glDisableVertexAttribArray(locUv);

        // Restore textures
        for (int i = textureIndex-1; i >= 0; --i) {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        GLF_POST_PENDING_GL_ERRORS();
#endif
    }
    else {
        id<MTLDevice> device = static_cast<HgiMetal*>(_hgi)->GetDevice();

        MTLRenderPassDescriptor *renderPassDescriptor = [[MTLRenderPassDescriptor alloc] init];
        MTLDepthStencilDescriptor *depthStateDesc = [[MTLDepthStencilDescriptor alloc] init];

        HgiMetalImmediateCommandBuffer &hgiCommandBuffer =
            static_cast<HgiMetalImmediateCommandBuffer&>(_hgi->GetImmediateCommandBuffer());
        id<MTLRenderCommandEncoder> renderEncoder =
            [hgiCommandBuffer.GetCommandBuffer() renderCommandEncoderWithDescriptor:renderPassDescriptor];
        // Setup textures
        int textureIndex = 0;
        for (auto const& texture : textures) {
            GLint loc = 0;//glGetUniformLocation(programId, texture.first.GetText());
            //glUniform1i(loc, textureIndex);
            [renderEncoder setFragmentTexture:texture.second.multiTexture.forCurrentGPU() atIndex:textureIndex];
            textureIndex++;
        }

        // Set up buffers
        MTLVertexDescriptor *_mtlVertexDescriptor;
        _mtlVertexDescriptor = [[MTLVertexDescriptor alloc] init];

        _mtlVertexDescriptor.attributes[0].format = MTLVertexFormatFloat4;
        _mtlVertexDescriptor.attributes[0].offset = 0;
        _mtlVertexDescriptor.attributes[0].bufferIndex = 0;

        _mtlVertexDescriptor.attributes[1].format = MTLVertexFormatFloat2;
        _mtlVertexDescriptor.attributes[1].offset = sizeof(float) * 4;
        _mtlVertexDescriptor.attributes[1].bufferIndex = 0;

        _mtlVertexDescriptor.layouts[0].stride = sizeof(float) * 6;
        _mtlVertexDescriptor.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;

        HgiMetalBuffer *metalBuffer = static_cast<HgiMetalBuffer*>(_vertexBuffer.Get());
        [renderEncoder setVertexBuffer:metalBuffer->GetBufferId()
                                offset:0
                               atIndex:0];

        // Set up uniforms
        for (auto const& uniform : _uniforms) {
            _SetUniform(uniform.first, uniform.second);
        }

        depthStateDesc.depthCompareFunction = MTLCompareFunctionAlways;
        depthStateDesc.depthWriteEnabled = _depthAware?YES:NO;
        id <MTLDepthStencilState> _depthState = [device newDepthStencilStateWithDescriptor:depthStateDesc];
        [renderEncoder setDepthStencilState:_depthState];

        id <MTLRenderPipelineState> _pipelineStateBlit;

        HdStMSLProgramSharedPtr mslProgram = boost::dynamic_pointer_cast<HdStMSLProgram>(_program);
        id <MTLFunction> vertexFunction = mslProgram->GetVertexFunction(0);
        id <MTLFunction> fragmentFunction = mslProgram->GetFragmentFunction(0);

        MTLRenderPipelineDescriptor *pipelineStateDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
        pipelineStateDescriptor.rasterSampleCount = 1;
        pipelineStateDescriptor.vertexFunction = vertexFunction;
        pipelineStateDescriptor.fragmentFunction = fragmentFunction;
        pipelineStateDescriptor.vertexDescriptor = _mtlVertexDescriptor;
        pipelineStateDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA32Float;
        if (_depthAware) {
            pipelineStateDescriptor.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
        }

        NSError *error = NULL;
        _pipelineStateBlit = [device newRenderPipelineStateWithDescriptor:pipelineStateDescriptor error:&error];
        if (!_pipelineStateBlit) {
            NSLog(@"Failed to created pipeline state, error %@", error);
        }

        [renderEncoder setRenderPipelineState:_pipelineStateBlit];

        [renderEncoder drawPrimitives: MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];

        [renderEncoder endEncoding];
        hgiCommandBuffer.FlushEncoders();
        
        [renderPassDescriptor release];
        [pipelineStateDescriptor release];
        [depthStateDesc release];
    }
    _program->UnsetProgram();
}

void
HdxFullscreenShader::Draw()
{
    Draw(_textures);
}

PXR_NAMESPACE_CLOSE_SCOPE
