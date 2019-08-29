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

#include "pxr/imaging/hdSt/debugCodes.h"
#include "pxr/imaging/hdSt/package.h"
#include "pxr/imaging/hdSt/surfaceShader.h"
#include "pxr/imaging/hdSt/textureResource.h"
#include "pxr/imaging/hdSt/textureResourceHandle.h"
#include "pxr/imaging/hdSt/GL/glslProgram.h"
#include "pxr/imaging/hdSt/GL/glUtils.h"

#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/resourceRegistry.h"
#include "pxr/imaging/hd/tokens.h"

#include "pxr/imaging/glf/bindingMap.h"
#include "pxr/imaging/glf/diagnostic.h"

#include "pxr/imaging/hio/glslfx.h"

#include "pxr/base/tf/diagnostic.h"
#include "pxr/base/tf/envSetting.h"

#include <string>
#include <fstream>
#include <iostream>

PXR_NAMESPACE_OPEN_SCOPE


TF_DEFINE_ENV_SETTING(HD_ENABLE_SHARED_CONTEXT_CHECK, 0,
    "Enable GL context sharing validation");

// Get the line number from the compilation error message, and return a boolean
// indicating success/failure of parsing.
// Note: This has been tested only on nVidia.
static bool
_ParseLineNumberOfError(std::string const &error, unsigned int *lineNum)
{
    if (!lineNum) {
        return false;
    }
    // sample error on nVidia:
    // 0(279) : error C1031: swizzle mask element not present in operand "xyz"
    // 279 is the line number here.
    std::string::size_type start = error.find('(');
    std::string::size_type end = error.find(')');
    if (start != std::string::npos && end != std::string::npos) {
        std::string lineNumStr = error.substr(start+1, end-1);
        unsigned long num = strtoul(lineNumStr.c_str(), nullptr, 10);
        *lineNum = (unsigned int) num;
        if (num == ULONG_MAX || num == 0) {
            // Out of range, or no valid conversion could be performed.
            return false;
        }
        return true;
    } else {
        // Error message isn't formatted as expected.
        return false;
    }
}

// Return the substring for the inclusive range given the start and end indices.
static std::string
_GetSubstring(std::string const& str,
              std::string::size_type startPos,
              std::string::size_type endPos)
{
    if (endPos == std::string::npos) {
        return str.substr(startPos, endPos);
    }
    return str.substr(startPos, endPos - startPos + 1);
}

// It's helpful to have a few more lines around the erroring line when logging
// compiler error messages. This function returns this contextual info
// as a string.
static std::string
_GetCompileErrorCodeContext(std::string const &shader,
                            unsigned int lineNum,
                            unsigned int contextSize)
{
    unsigned int numLinesToSkip =
    std::max<unsigned int>(0, lineNum - contextSize - 1);
    std::string::size_type i = 0;
    for (unsigned int line = 0; line < numLinesToSkip && i != std::string::npos;
         line++) {
        i = shader.find('\n', i+1); // find the next occurrance
    }
    
    if (i == std::string::npos) return std::string();
    
    // Copy context before the error line.
    std::string::size_type start = i;
    for (unsigned int line = 0; line < contextSize && i != std::string::npos;
         line++) {
        i = shader.find('\n', i+1);
    }
    
    std::string context = _GetSubstring(shader, start, i);
    
    // Copy error line with annotation.
    start = i+1;
    i = shader.find('\n', start);
    context += _GetSubstring(shader, start, i-1) + " <<< ERROR!\n";
    
    // Copy context after error line.
    start = i+1;
    for (unsigned int line = 0; line < contextSize && i != std::string::npos;
         line++) {
        i = shader.find('\n', i+1);
    }
    context += _GetSubstring(shader, start, i);
    
    return context;
}

static void
_DumpShaderSource(const char *shaderType, std::string const &shaderSource)
{
    std::cout << "--------- " << shaderType << " ----------\n";
    std::cout << shaderSource;
    std::cout << "---------------------------\n";
    std::cout << std::flush;
}

HdStGLSLProgram::HdStGLSLProgram(TfToken const &role)
    : HdStProgram(role), _program(0), _programSize(0), _uniformBuffer(role)
{
    static size_t globalDebugID = 0;
    _debugID = globalDebugID++;
}

HdStGLSLProgram::~HdStGLSLProgram()
{
    if (_program != 0) {
        if (glDeleteProgram)
            glDeleteProgram(_program);
    }
    GLuint uniformBuffer = _uniformBuffer.GetId();
    if (uniformBuffer) {
        if (glDeleteBuffers)
            glDeleteBuffers(1, &uniformBuffer);
        _uniformBuffer.SetAllocation((GLuint)0, 0);
    }
}

bool
HdStGLSLProgram::CompileShader(GLenum type,
                             std::string const &shaderSource)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    // early out for empty source.
    // this may not be an error, since glslfx gives empty string
    // for undefined shader stages (i.e. null geometry shader)
    if (shaderSource.empty()) return false;

    const char *shaderType = NULL;
    if (type == GL_VERTEX_SHADER) {
        shaderType = "GL_VERTEX_SHADER";
    } else if (type == GL_TESS_CONTROL_SHADER) {
        shaderType = "GL_TESS_CONTROL_SHADER";
    } else if (type == GL_TESS_EVALUATION_SHADER) {
        shaderType = "GL_TESS_EVALUATION_SHADER";
    } else if (type == GL_GEOMETRY_SHADER) {
        shaderType = "GL_GEOMETRY_SHADER";
    } else if (type == GL_FRAGMENT_SHADER) {
        shaderType = "GL_FRAGMENT_SHADER";
    } else if (type == GL_COMPUTE_SHADER) {
        shaderType = "GL_COMPUTE_SHADER";
    } else {
        TF_CODING_ERROR("Invalid shader type %d\n", type);
        return false;
    }

    if (TfDebug::IsEnabled(HDST_DUMP_SHADER_SOURCE)) {
        _DumpShaderSource(shaderType, shaderSource);
    }

    // glew has to be initialized
    if (!glCreateProgram)
        return false;

    // create a program if not exists
    if (_program == 0) {
        _program = glCreateProgram();
    }

    // create a shader, compile it
    const char *shaderSources[1];
    shaderSources[0] = shaderSource.c_str();
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, sizeof(shaderSources)/sizeof(const char *), shaderSources, NULL);
    glCompileShader(shader);

    std::string fname;
    if (TfDebug::IsEnabled(HDST_DUMP_SHADER_SOURCEFILE)) {
        std::stringstream fnameStream;
        static size_t debugShaderID = 0;
        fnameStream << "program" << _debugID << "_shader" << debugShaderID++
                << "_" << shaderType << ".glsl";
        fname = fnameStream.str();
        std::fstream output(fname.c_str(), std::ios::out);
        output << shaderSource;
        output.close();

        std::cout << "Write " << fname << " (size=" << shaderSource.size() << ")\n";
    }

    std::string logString;
    if (!HdStGLUtils::GetShaderCompileStatus(shader, &logString)) {
        unsigned int lineNum = 0;
        if (_ParseLineNumberOfError(logString, &lineNum)) {
            // Get lines surrounding the erroring line for context.
            std::string errorContext =
            _GetCompileErrorCodeContext(shaderSource, lineNum, 3);
            if (!errorContext.empty()) {
                // erase the \0 if present.
                if (logString.back() == '\0') {
                    logString.erase(logString.end() - 1, logString.end());
                }
                logString.append("\nError Context:\n");
                logString.append(errorContext);
            }
        }
        
        const char* programName = fname.empty() ? shaderType : fname.c_str();
        TF_WARN("Failed to compile shader (%s): %s",
                programName, logString.c_str());
        
        if (TfDebug::IsEnabled(HDST_DUMP_FAILING_SHADER_SOURCE)) {
            _DumpShaderSource(shaderType, shaderSource);
        }
        
        // shader is no longer needed.
        glDeleteShader(shader);
        
        return false;
    }

    // attach the shader to the program
    glAttachShader(_program, shader);

    // shader is no longer needed.
    glDeleteShader(shader);

    return true;
}

static std::string
_GetShaderType(GLint type)
{
    switch(type) {
        case GL_VERTEX_SHADER:
            return "--------GL_VERTEX_SHADER--------\n";
        case GL_FRAGMENT_SHADER:
            return "--------GL_FRAGMENT_SHADER--------\n";
        case GL_GEOMETRY_SHADER:
            return "--------GL_GEOMETRY_SHADER--------\n";
        case GL_TESS_CONTROL_SHADER:
            return "--------GL_TESS_CONTROL_SHADER--------\n";
        case GL_TESS_EVALUATION_SHADER:
            return "--------GL_TESS_EVALUATION_SHADER--------\n";
            
        default:
            return "--------UNKNOWN_SHADER_STAGE--------\n";
    }
}

static void
_DebugAppendShaderSource(GLuint shader, std::string * result)
{
    GLint sourceType = 0;
    glGetShaderiv(shader, GL_SHADER_TYPE, &sourceType);
    
    GLint sourceLength = 0;
    glGetShaderiv(shader, GL_SHADER_SOURCE_LENGTH, &sourceLength);
    if (sourceLength > 0) {
        char *shaderSource = new char[sourceLength];
        glGetShaderSource(shader, sourceLength, NULL, shaderSource);
        // don't copy in the null terminator from the char*
        result->append(_GetShaderType(sourceType));
        result->append(shaderSource, sourceLength-1);
        delete[] shaderSource;
    }
}

static std::string
_DebugLinkSource(GLuint program)
{
    std::string result;
    result = TfStringPrintf("==== Source Program ID=%d\nBEGIN_DUMP\n", program);
    
    GLint numAttachedShaders = 0;
    glGetProgramiv(program, GL_ATTACHED_SHADERS, &numAttachedShaders);
    if (numAttachedShaders > 0) {
        GLuint * attachedShaders = new GLuint[numAttachedShaders];
        glGetAttachedShaders(program,
                             numAttachedShaders, NULL, attachedShaders);
        for (int i=0; i<numAttachedShaders; ++i) {
            _DebugAppendShaderSource(attachedShaders[i], &result);
        }
    }
    result += "END DUMP\n";
    
    return result;
}

bool
HdStGLSLProgram::Link()
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    if (!glLinkProgram) return false; // glew initialized

    if (_program == 0) {
        TF_CODING_ERROR("At least one shader has to be compiled before linking.");
        return false;
    }

    bool dumpShaderBinary = TfDebug::IsEnabled(HDST_DUMP_SHADER_BINARY);
    
    if (dumpShaderBinary) {
        // set RETRIEVABLE_HINT to true for getting program binary length.
        // note: Actually the GL driver may recompile the program dynamically on
        // some state changes, so the size of program could be inaccurate.
        glProgramParameteri(_program, GL_PROGRAM_BINARY_RETRIEVABLE_HINT,
                            GL_TRUE);
    }

    // link
    glLinkProgram(_program);

    std::string logString;
    bool success = true;
    if (!HdStGLUtils::GetProgramLinkStatus(_program, &logString)) {
        // XXX:validation
        TF_WARN("Failed to link shader: %s", logString.c_str());
        success = false;
        
        if (TfDebug::IsEnabled(HDST_DUMP_FAILING_SHADER_SOURCE)) {
            _DebugLinkSource(_program);
        }
    }

    // initial program size
    GLint size;
    glGetProgramiv(_program, GL_PROGRAM_BINARY_LENGTH, &size);

    // update the program resource allocation
    _programSize = size;

    // create an uniform buffer
    GLuint uniformBuffer = _uniformBuffer.GetId();
    if (uniformBuffer == 0) {
        glGenBuffers(1, &uniformBuffer);
        _uniformBuffer.SetAllocation(uniformBuffer, 0);
    }

    // binary dump out
    if (dumpShaderBinary) {
        std::vector<char> bin(size);
        GLsizei len;
        GLenum format;
        glGetProgramBinary(_program, size, &len, &format, &bin[0]);
        std::stringstream fname;
        fname << "program" << _debugID << ".bin";
        
        std::fstream output(fname.str().c_str(), std::ios::out|std::ios::binary);
        output.write(&bin[0], size);
        output.close();
        
        std::cout << "Write " << fname.str() << " (size=" << size << ")\n";
    }

    return success;
}

bool
HdStGLSLProgram::Validate() const
{
    if (_program == 0) return false;

    if (TfDebug::IsEnabled(HD_SAFE_MODE) ||
        TfGetEnvSetting(HD_ENABLE_SHARED_CONTEXT_CHECK)) {

        HD_TRACE_FUNCTION();

        // make sure the binary size is same as when it's created.
        if (glIsProgram(_program) == GL_FALSE) return false;
        GLint size = 0;
        glGetProgramiv(_program, GL_PROGRAM_BINARY_LENGTH, &size);
        if (size == 0) {
            return false;
        }
        if (static_cast<size_t>(size) != _programSize) {
            return false;
        }
    }
    return true;
}

bool
HdStGLSLProgram::GetProgramLinkStatus(std::string * reason) const
{
    // glew has to be initialized
    if (!glGetProgramiv) return true;
    
    GLint status = 0;
    glGetProgramiv(_program, GL_LINK_STATUS, &status);
    if (reason) {
        GLint infoLength = 0;
        glGetProgramiv(_program, GL_INFO_LOG_LENGTH, &infoLength);
        if (infoLength > 0) {
            char *infoLog = new char[infoLength];;
            glGetProgramInfoLog(_program, infoLength, NULL, infoLog);
            reason->assign(infoLog, infoLength);
            delete[] infoLog;
        }
    }
    return (status == GL_TRUE);
}

void HdStGLSLProgram::AssignUniformBindings(GarchBindingMapRefPtr bindingMap) const
{
    GlfBindingMapRefPtr glfBindingMap(TfDynamic_cast<GlfBindingMapRefPtr>(bindingMap));
    
    glfBindingMap->AssignUniformBindingsToProgram(GetGLProgram());
}

void HdStGLSLProgram::AssignSamplerUnits(GarchBindingMapRefPtr bindingMap) const
{
    GlfBindingMapRefPtr glfBindingMap(TfDynamic_cast<GlfBindingMapRefPtr>(bindingMap));
    
    glfBindingMap->AssignSamplerUnitsToProgram(GetGLProgram());
}

void HdStGLSLProgram::AddCustomBindings(GarchBindingMapRefPtr bindingMap) const
{
    GlfBindingMapRefPtr glfBindingMap(TfDynamic_cast<GlfBindingMapRefPtr>(bindingMap));
    
    glfBindingMap->AddCustomBindings(GetGLProgram());
}

void HdStGLSLProgram::BindResources(HdStSurfaceShader* surfaceShader, HdSt_ResourceBinder const &binder) const
{
    for (auto const & it : surfaceShader->GetTextureDescriptors()) {
        HdBinding binding = binder.GetBinding(it.name);
        
        if (!TF_VERIFY(it.handle)) {
            continue;
        }
        HdStTextureResourceSharedPtr resource = it.handle->GetTextureResource();
        
        // XXX: put this into resource binder.
        if (binding.GetType() == HdBinding::TEXTURE_2D) {
            int samplerUnit = binding.GetTextureUnit();
            glActiveTexture(GL_TEXTURE0 + samplerUnit);
            glBindTexture(GL_TEXTURE_2D, resource->GetTexelsTextureId());
            glBindSampler(samplerUnit, resource->GetTexelsSamplerId());
            
            glProgramUniform1i(_program, binding.GetLocation(), samplerUnit);
        } else if (binding.GetType() == HdBinding::TEXTURE_UDIM_ARRAY) {
            int samplerUnit = binding.GetTextureUnit();
            glActiveTexture(GL_TEXTURE0 + samplerUnit);
            glBindTexture(GL_TEXTURE_2D_ARRAY, resource->GetTexelsTextureId());
            glBindSampler(samplerUnit, resource->GetTexelsSamplerId());
            
            glProgramUniform1i(_program, binding.GetLocation(), samplerUnit);
        } else if (binding.GetType() == HdBinding::TEXTURE_UDIM_LAYOUT) {
            int samplerUnit = binding.GetTextureUnit();
            glActiveTexture(GL_TEXTURE0 + samplerUnit);
            glBindTexture(GL_TEXTURE_1D, resource->GetLayoutTextureId());
            
            glProgramUniform1i(_program, binding.GetLocation(), samplerUnit);
        } else if (binding.GetType() == HdBinding::TEXTURE_PTEX_TEXEL) {
            int samplerUnit = binding.GetTextureUnit();
            glActiveTexture(GL_TEXTURE0 + samplerUnit);
            glBindTexture(GL_TEXTURE_2D_ARRAY, resource->GetTexelsTextureId());
            
            glProgramUniform1i(_program, binding.GetLocation(), samplerUnit);
        } else if (binding.GetType() == HdBinding::TEXTURE_PTEX_LAYOUT) {
            int samplerUnit = binding.GetTextureUnit();
            glActiveTexture(GL_TEXTURE0 + samplerUnit);
            glBindTexture(GL_TEXTURE_BUFFER, resource->GetLayoutTextureId());
            
            glProgramUniform1i(_program, binding.GetLocation(), samplerUnit);
        }
    }
    glActiveTexture(GL_TEXTURE0);
}

void HdStGLSLProgram::UnbindResources(HdStSurfaceShader* surfaceShader, HdSt_ResourceBinder const &binder) const
{
    int samplerUnit = binder.GetNumReservedTextureUnits();
    for (auto const & it : surfaceShader->GetTextureDescriptors()) {
        HdBinding binding = binder.GetBinding(it.name);
        // XXX: put this into resource binder.
        if (binding.GetType() == HdBinding::TEXTURE_2D) {
            int samplerUnit = binding.GetTextureUnit();
            glActiveTexture(GL_TEXTURE0 + samplerUnit);
            glBindTexture(GL_TEXTURE_2D, 0);
            glBindSampler(samplerUnit, 0);
        } else if (binding.GetType() == HdBinding::TEXTURE_UDIM_ARRAY) {
            int samplerUnit = binding.GetTextureUnit();
            glActiveTexture(GL_TEXTURE0 + samplerUnit);
            glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
            glBindSampler(samplerUnit, 0);
        } else if (binding.GetType() == HdBinding::TEXTURE_UDIM_LAYOUT) {
            int samplerUnit = binding.GetTextureUnit();
            glActiveTexture(GL_TEXTURE0 + samplerUnit);
            glBindTexture(GL_TEXTURE_1D, 0);
        } else if (binding.GetType() == HdBinding::TEXTURE_PTEX_TEXEL) {
            int samplerUnit = binding.GetTextureUnit();
            glActiveTexture(GL_TEXTURE0 + samplerUnit);
            glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
        } else if (binding.GetType() == HdBinding::TEXTURE_PTEX_LAYOUT) {
            int samplerUnit = binding.GetTextureUnit();
            glActiveTexture(GL_TEXTURE0 + samplerUnit);
            glBindTexture(GL_TEXTURE_BUFFER, 0);
        }
    }

}

void HdStGLSLProgram::SetProgram(char const* const label) {
    if (label) {
        GlfDebugLabelProgram(_program, label);
    }
    glUseProgram(_program);
}

void HdStGLSLProgram::UnsetProgram() {
    glUseProgram(0);
}

void HdStGLSLProgram::DrawElementsInstancedBaseVertex(GLenum primitiveMode,
                                                      int indexCount,
                                                      GLint indexType,
                                                      GLint firstIndex,
                                                      GLint instanceCount,
                                                      GLint baseVertex) const {
    uint64_t size;
    
    switch(indexType) {
        case GL_UNSIGNED_BYTE:
            size = sizeof(GLubyte);
            break;
        case GL_UNSIGNED_SHORT:
            size = sizeof(GLushort);
            break;
        case GL_UNSIGNED_INT:
            size = sizeof(GLuint);
            break;
    }
    glDrawElementsInstancedBaseVertex(primitiveMode,
                                      indexCount,
                                      indexType,
                                      (void *)(firstIndex * size),
                                      instanceCount,
                                      baseVertex);
}

void HdStGLSLProgram::DrawArraysInstanced(GLenum primitiveMode,
                                          GLint baseVertex,
                                          GLint vertexCount,
                                          GLint instanceCount) const {
    glDrawArraysInstanced(primitiveMode, baseVertex, vertexCount, instanceCount);
}

void HdStGLSLProgram::DrawArrays(GLenum primitiveMode,
                                 GLint baseVertex,
                                 GLint vertexCount) const {
    glDrawArrays(primitiveMode, baseVertex, vertexCount);
}

std::string HdStGLSLProgram::GetComputeHeader() const
{
    return "#version 430\n";
}

PXR_NAMESPACE_CLOSE_SCOPE

