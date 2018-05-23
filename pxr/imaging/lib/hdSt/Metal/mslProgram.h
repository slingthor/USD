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
#ifndef HDST_MSL_PROGRAM_H
#define HDST_MSL_PROGRAM_H

#include "pxr/pxr.h"
#include "pxr/imaging/hdSt/api.h"
#include "pxr/imaging/hdSt/program.h"
#include "pxr/imaging/hdSt/Metal/resourceMetal.h"

#include <boost/shared_ptr.hpp>

PXR_NAMESPACE_OPEN_SCOPE

class HdResourceRegistry;
typedef boost::shared_ptr<class HdStMSLProgram> HdStMSLProgramSharedPtr;

/// \class HdStMSLProgram
///
/// An instance of an MSL program.
///
class HdStMSLProgram: public HdStProgram
{
public:
    typedef std::map<std::string, int> BindingLocationMap;

    HDST_API
    HdStMSLProgram(TfToken const &role);
    HDST_API
    virtual ~HdStMSLProgram();

    /// Compile shader source of type
    HDST_API
    virtual bool CompileShader(GLenum type, std::string const & source) override;

    /// Link the compiled shaders together.
    HDST_API
    virtual bool Link();

    /// Validate if this program is a valid progam in the current context.
    HDST_API
    virtual bool Validate() const override;

    /// Returns HdResource of the global uniform buffer object for this program.
    HDST_API
    virtual HdResource const &GetGlobalUniformBuffer() const override {
        return _uniformBuffer;
    }

    /// Returns true if the program has been successfully linked.
    /// if not, returns false and fills the error log into reason.
    HDST_API
    virtual bool GetProgramLinkStatus(std::string * reason) const override;
    
    /// Returns the binary size of the program (if available)
    HDST_API
    virtual uint32_t GetProgramSize() const { return 0; }
    
    HDST_API
    virtual void AssignUniformBindings(GarchBindingMapRefPtr bindingMap) const override;
    
    HDST_API
    virtual void AssignSamplerUnits(GarchBindingMapRefPtr bindingMap) const override;
    
    HDST_API
    virtual void AddCustomBindings(GarchBindingMapRefPtr bindingMap) const override;
    
    HDST_API
    virtual void BindResources(HdStSurfaceShader* surfaceShader, HdSt_ResourceBinder const &binder) const override;
    
    HDST_API
    virtual void UnbindResources(HdStSurfaceShader* surfaceShader, HdSt_ResourceBinder const &binder) const override;

    HDST_API
    virtual void SetProgram() const override;
    
    HDST_API
    virtual void UnsetProgram() const override;
    
    HDST_API
    virtual void DrawElementsInstancedBaseVertex(GLenum primitiveMode,
                                                 int indexCount,
                                                 GLint indexType,
                                                 GLint firstIndex,
                                                 GLint instanceCount,
                                                 GLint baseVertex) const override;

    HDST_API
    virtual void DrawArraysInstanced(GLenum primitiveMode,
                                     GLint baseVertex,
                                     GLint vertexCount,
                                     GLint instanceCount) const override;
    
    HDST_API
    BindingLocationMap const &GetBindingLocations() const {
        return _locationMap;
    }
    
    HDST_API
    void AddBinding(std::string const &name, int location) {
        _locationMap.insert(make_pair(name, location));
    }

    HDST_API
    id<MTLFunction> GetVertexFunction() const {
        return _vertexFunction;
    }
    
    HDST_API
    id<MTLFunction> GetFragmentFunction() const {
        return _fragmentFunction;
    }
    
    HDST_API
    id<MTLFunction> GetComputeFunction() const {
        return _computeFunction;
    }
protected:
    HDST_API
    void BakeState();

private:
    TfToken const _role;

    id<MTLFunction> _vertexFunction;
    id<MTLFunction> _fragmentFunction;
    id<MTLFunction> _computeFunction;
    
    id<MTLRenderPipelineState> _pipelineState;

    bool _valid;
    HdStResourceMetal _uniformBuffer;
    BindingLocationMap  _locationMap;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif  // HDST_MSL_PROGRAM_H
