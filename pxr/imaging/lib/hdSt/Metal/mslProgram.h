#line 1 "/Volumes/Data/USDMetal/pxr/imaging/lib/hdSt/Metal/mslProgram.h"
#line 1 "/Volumes/Data/USDMetal/pxr/imaging/lib/hdSt/Metal/mslProgram.h"
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

#define GENERATE_METAL_DEBUG_SOURCE_CODE 1

#include "pxr/pxr.h"
#include "pxr/imaging/hdSt/api.h"
#include "pxr/imaging/hdSt/program.h"
#include "pxr/imaging/hdSt/Metal/resourceMetal.h"
#include "pxr/imaging/mtlf/mtlDevice.h"

#include <boost/shared_ptr.hpp>

PXR_NAMESPACE_OPEN_SCOPE

class HdResourceRegistry;
typedef boost::shared_ptr<class HdStMSLProgram> HdStMSLProgramSharedPtr;

enum MSL_BindingType
{
    kMSL_BindingType_VertexAttribute = (1 << 0),
    kMSL_BindingType_IndexBuffer     = (1 << 1),
    kMSL_BindingType_Texture         = (1 << 2),
    kMSL_BindingType_Sampler         = (1 << 3),
    kMSL_BindingType_Uniform         = (1 << 4),
    kMSL_BindingType_UniformBuffer   = (1 << 5),
};

struct MSL_ShaderBinding
{
    MSL_BindingType  _type;
    MSL_ProgramStage _stage;
    int              _index;
    std::string      _name;
    int              _offsetWithinResource;
	int              _uniformBufferSize;
};

typedef std::vector<MSL_ShaderBinding> MSL_ShaderBindings;

const MSL_ShaderBinding& MSL_FindBinding(const MSL_ShaderBindings& bindings, const std::string& name, bool& outFound, uint bindingTypeMask = 0xFFFFFFFF, uint programStageMask = 0xFFFFFFFF, uint skipCount = 0);

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
    MSL_ShaderBindings const &GetBindings() const {
        return _bindings;
    }
    
    HDST_API
    void AddBinding(std::string const &name, int index, MSL_BindingType bindingType, MSL_ProgramStage programStage, int offsetWithinResource = 0, int uniformBufferSize = 0) {
        _locationMap.insert(make_pair(name, index));
        _bindings.push_back({ bindingType, programStage, index, name, offsetWithinResource, uniformBufferSize });
    }

    HDST_API
    void UpdateUniformBinding(std::string const &name, int index = -1, int offsetWithinResource = -1) {
        for(auto it = _bindings.begin(); it != _bindings.end(); ++it) {
            if(it->_name != name || it->_type != kMSL_BindingType_Uniform)
                continue;
            if(index != -1)
                it->_index = index;
            if(offsetWithinResource != -1)
                it->_offsetWithinResource = offsetWithinResource;
            break;
        }
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
	
    uint32 _vertexFunctionIdx;
    uint32 _fragmentFunctionIdx;
    uint32 _computeFunctionIdx;

    bool _valid;
    HdStResourceMetal  _uniformBuffer;
    MSL_ShaderBindings _bindings;
    BindingLocationMap _locationMap;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif  // HDST_MSL_PROGRAM_H
