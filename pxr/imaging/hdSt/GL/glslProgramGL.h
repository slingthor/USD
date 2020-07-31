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
#ifndef HDST_GLSL_PROGRAM_H
#define HDST_GLSL_PROGRAM_H

#include "pxr/pxr.h"
#include "pxr/imaging/hdSt/api.h"

#include "pxr/imaging/hdSt/glslProgram.h"
#include "pxr/imaging/hdSt/GL/resourceGL.h"

#include <boost/shared_ptr.hpp>

PXR_NAMESPACE_OPEN_SCOPE

class HdResourceRegistry;
using HdStGLSLProgramSharedPtr =
    std::shared_ptr<class HdStGLSLProgram>;

/// \class HdGLSLProgram
///
/// An instance of a glsl program.
///
// XXX: this design is transitional and will be revised soon.
class HdStglslProgramGLSL: public HdStGLSLProgram
{
public:
    HDST_API
    HdStglslProgramGLSL(TfToken const &role, HdStResourceRegistry *const registry);
    HDST_API
    ~HdStglslProgramGLSL() override;

    /// Compile shader source of type
    HDST_API
    bool CompileShader(GLenum type, std::string const & source) override;

    /// Link the compiled shaders together.
    HDST_API
    bool Link() override;

    /// Validate if this program is a valid progam in the current context.
    HDST_API
    bool Validate() const override;
    
    /// Returns true if the program has been successfully linked.
    /// if not, returns false and fills the error log into reason.
    HDST_API
    bool GetProgramLinkStatus(std::string * reason) const override;

    /// Returns the binary size of the program (if available)
    HDST_API
    uint32_t GetProgramSize() const { return _programSize; }
    
    HDST_API
    void AssignUniformBindings(GarchBindingMapRefPtr bindingMap) const override;
    
    HDST_API
    void AssignSamplerUnits(GarchBindingMapRefPtr bindingMap) const override;
    
    HDST_API
    void AddCustomBindings(GarchBindingMapRefPtr bindingMap) const override;
    
    HDST_API
    void BindResources(HdStSurfaceShader* surfaceShader, HdSt_ResourceBinder const &binder) const override;
    
    HDST_API
    void UnbindResources(HdStSurfaceShader* surfaceShader, HdSt_ResourceBinder const &binder) const override;

    HDST_API
    void SetProgram(char const* const label) override;
    
    HDST_API
    void UnsetProgram() override;
    
    HDST_API
    void DrawElementsInstancedBaseVertex(int primitiveMode,
                                         int indexCount,
                                         int indexType,
                                         int firstIndex,
                                         int instanceCount,
                                         int baseVertex) const override;

    HDST_API
    void DrawArraysInstanced(int primitiveMode,
                             int baseVertex,
                             int vertexCount,
                             int instanceCount) const override;
    
    HDST_API
    void DrawArrays(int primitiveMode,
                    int baseVertex,
                    int vertexCount) const override;

    /// Returns the GL program object.
    HDST_API
    GLuint GetGLProgram() const { return _program->GetRawResource(); }
    
protected:
    HDST_API
    virtual std::string GetComputeHeader() const override;

private:
    size_t _programSize;
    // An identifier for uniquely identifying the program, for debugging
    // purposes - programs that fail to compile for one reason or another
    // will get deleted, and their GL program IDs reused, so we can't use
    // that to identify it uniquely
    size_t _debugID;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif  // HDST_COMPUTE_SHADER_H
