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
#ifndef HDST_PROGRAM_H
#define HDST_PROGRAM_H

#include "pxr/pxr.h"
#include "pxr/imaging/hdSt/api.h"

#include "pxr/base/tf/declarePtrs.h"
#include "pxr/base/tf/token.h"

#include "pxr/imaging/hgi/shaderProgram.h"
#include "pxr/imaging/hgi/enums.h"

#include <boost/shared_ptr.hpp>

PXR_NAMESPACE_OPEN_SCOPE

class HdStResourceRegistry;
class HdResource;
class HdStSurfaceShader;
class HdSt_ResourceBinder;

using HdStProgramSharedPtr =
    std::shared_ptr<class HdStProgram>;

using HgiShaderProgramHandle = HgiHandle<class HgiShaderProgram>;

TF_DECLARE_WEAK_AND_REF_PTRS(GarchBindingMap);

/// \class HdProgram
///
/// An instance of a shader language program.
///
// XXX: this design is transitional and will be revised soon.
class HdStProgram
{
public:
    typedef size_t ID;
    
    HDST_API
    virtual ~HdStProgram();
    
    /// Returns the role of the GPU data in this resource.
    TfToken const & GetRole() const { return _role; }

    /// Compile shader source for a shader stage.
    HDST_API
    virtual bool CompileShader(HgiShaderStage type, std::string const & source) = 0;

    /// Link the compiled shaders together.
    HDST_API
    virtual bool Link() = 0;

    /// Validate if this program is a valid progam in the current context.
    HDST_API
    virtual bool Validate() const = 0;

    /// Returns HdResource of the global uniform buffer object for this program.
    HDST_API
    virtual HdResource const &GetGlobalUniformBuffer() const = 0;

    /// Returns true if the program has been successfully linked.
    /// if not, returns false and fills the error log into reason.
    HDST_API
    virtual bool GetProgramLinkStatus(std::string * reason) const = 0;
    
    /// Returns the binary size of the program (if available)
    HDST_API
    virtual uint32_t GetProgramSize() const = 0;
    
    HDST_API
    virtual void AssignUniformBindings(GarchBindingMapRefPtr bindingMap) const = 0;
    
    HDST_API
    virtual void AssignSamplerUnits(GarchBindingMapRefPtr bindingMap) const = 0;
    
    HDST_API
    virtual void AddCustomBindings(GarchBindingMapRefPtr bindingMap) const = 0;
    
    HDST_API
    virtual void BindResources(HdStSurfaceShader* surfaceShader, HdSt_ResourceBinder const &binder) const = 0;

    HDST_API
    virtual void UnbindResources(HdStSurfaceShader* surfaceShader, HdSt_ResourceBinder const &binder) const = 0;
    
    HDST_API
    virtual void SetProgram(char const* const label = NULL) = 0;
    
    HDST_API
    virtual void UnsetProgram() = 0;
    
    HDST_API
    virtual void DrawElementsInstancedBaseVertex(int primitiveMode,
                                                 int indexCount,
                                                 int indexType,
                                                 int firstIndex,
                                                 int instanceCount,
                                                 int baseVertex) const = 0;

    HDST_API
    virtual void DrawArraysInstanced(int primitiveMode,
                                     int baseVertex,
                                     int vertexCount,
                                     int instanceCount) const = 0;
    
    HDST_API
    virtual void DrawArrays(int primitiveMode,
                            int baseVertex,
                            int vertexCount) const = 0;
    
    /// Returns the hash value of the program for \a sourceFile
    HDST_API
    static ID ComputeHash(TfToken const & sourceFile);

    /// Convenience method to get a shared compute shader program
    HDST_API
    static HdStProgramSharedPtr GetComputeProgram(TfToken const &shaderToken,
        HdStResourceRegistry *resourceRegistry);
    
    HDST_API
    static HdStProgramSharedPtr GetComputeProgram(
        TfToken const &shaderFileName,
        TfToken const &shaderToken,
        HdStResourceRegistry *resourceRegistry);

protected:
    HDST_API
    HdStProgram(TfToken const &role,
                HdStResourceRegistry* resourceRegistry);

    HDST_API
    virtual std::string GetComputeHeader() const = 0;
    
    TfToken const _role;
    HdStResourceRegistry* _registry;
    
    HgiShaderProgramDesc _programDesc;
    HgiShaderProgramHandle _program;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif  // HDST_PROGRAM_H
