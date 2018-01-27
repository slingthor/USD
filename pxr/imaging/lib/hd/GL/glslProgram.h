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
#ifndef HD_GLSL_PROGRAM_H
#define HD_GLSL_PROGRAM_H

#include "pxr/pxr.h"
#include "pxr/imaging/hd/api.h"
#include "pxr/imaging/hd/program.h"
#include "pxr/imaging/hd/version.h"
#include "pxr/imaging/hd/GL/resourceGL.h"

#include <boost/shared_ptr.hpp>

PXR_NAMESPACE_OPEN_SCOPE

class HdResourceRegistry;
typedef boost::shared_ptr<class HdGLSLProgram> HdGLSLProgramSharedPtr;

/// \class HdGLSLProgram
///
/// An instance of a glsl program.
///
// XXX: this design is transitional and will be revised soon.
class HdGLSLProgram: public HdProgram
{
public:
    HD_API
    HdGLSLProgram(TfToken const &role);
    HD_API
    virtual ~HdGLSLProgram();

    /// Compile shader source of type
    HD_API
    virtual bool CompileShader(GLenum type, std::string const & source);

    /// Link the compiled shaders together.
    HD_API
    virtual bool Link();

    /// Validate if this program is a valid progam in the current context.
    HD_API
    virtual bool Validate() const;

    /// Returns HdResource of the program object.
    HD_API
    virtual HdResource const &GetProgram() const { return _program; }

    /// Returns HdResource of the global uniform buffer object for this program.
    HD_API
    virtual HdResource const &GetGlobalUniformBuffer() const {
        return _uniformBuffer;
    }
    
    /// Returns true if the program has been successfully linked.
    /// if not, returns false and fills the error log into reason.
    HD_API
    virtual bool GetProgramLinkStatus(std::string * reason);

    /// Convenience method to get a shared compute shader program
    HD_API
    static HdProgramSharedPtr GetComputeProgram(TfToken const &shaderToken,
        HdResourceRegistry *resourceRegistry);

private:
    HdResourceGL _program;
    HdResourceGL _uniformBuffer;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif  // HD_COMPUTE_SHADER_H
