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

//#define GENERATE_METAL_DEBUG_SOURCE_CODE

#include "pxr/pxr.h"
#include "pxr/imaging/hdSt/api.h"
#include "pxr/imaging/hdSt/program.h"
#include "pxr/imaging/hdSt/Metal/resourceMetal.h"
#include "pxr/imaging/mtlf/mtlDevice.h"

#include "pxr/imaging/hd/binding.h"

#include <boost/shared_ptr.hpp>

PXR_NAMESPACE_OPEN_SCOPE

class HdResourceRegistry;
using HdStMSLProgramSharedPtr = std::shared_ptr<class HdStMSLProgram>;

enum MSL_BindingType
{
    kMSL_BindingType_VertexAttribute = (1 << 0),
    kMSL_BindingType_IndexBuffer     = (1 << 1),
    kMSL_BindingType_Texture         = (1 << 2),
    kMSL_BindingType_Sampler         = (1 << 3),
    kMSL_BindingType_Uniform         = (1 << 4),
    kMSL_BindingType_UniformBuffer   = (1 << 5),
    kMSL_BindingType_GSVertOutput    = (1 << 6),
    kMSL_BindingType_GSPrimOutput    = (1 << 7),
    kMSL_BindingType_DrawArgs        = (1 << 8),
    kMSL_BindingType_FragExtras      = (1 << 9)
};

enum MSL_BuildTarget
{
    kMSL_BuildTarget_Regular = 0,
    kMSL_BuildTarget_MVA,               //Where MVA stands for: Manual Vertex Assembly. Vertex assembly is not done by the hardware, it is done in the vertex shader.
    kMSL_BuildTarget_MVA_ComputeGS,     //Adds a seperate Geometry Shader pass that is executed in Compute. Required for Geometry Shader support, requires MVA.
};

struct MSL_ShaderBinding
{
    MSL_ShaderBinding(MSL_BindingType type, MSL_ProgramStage stage, int index,
        const std::string& name, int offsetWithinResource,
        int uniformBufferSize, HdBinding const &binding) :
        _type(type),
        _stage(stage),
        _index(index),
        _name(name),
        _nameToken(name),
        _offsetWithinResource(offsetWithinResource),
        _uniformBufferSize(uniformBufferSize),
        _binding(binding) {}

    MSL_BindingType  _type;
    MSL_ProgramStage _stage;
    int              _index;
    std::string      _name;
    TfToken          _nameToken;
    int              _offsetWithinResource;
	int              _uniformBufferSize;
    HdBinding        _binding;
};

typedef std::multimap<size_t, MSL_ShaderBinding*> MSL_ShaderBindingMap;
typedef std::multimap<size_t, MSL_ShaderBinding*>::const_iterator MSL_ShaderBindingMapConstIterator;
typedef std::pair<MSL_ShaderBindingMapConstIterator,MSL_ShaderBindingMapConstIterator> MSL_ShaderBindingMapResults;

MSL_ShaderBinding const *MSL_FindBinding(
    MSL_ShaderBindingMap const& bindings,
    TfToken const& name,
    uint32_t bindingTypeMask = 0xFFFFFFFF,
    uint32_t programStageMask = 0xFFFFFFFF,
    uint32_t skipCount = 0,
    int32_t level = -1);

MSL_ShaderBindingMapResults MSL_FindBinding(
    MSL_ShaderBindingMap const& bindings,
    TfToken const& name,
    int32_t level = -1);

/// \class HdStMSLProgram
///
/// An instance of an MSL program.
///
class HdStMSLProgram: public HdStProgram
{
public:
    typedef std::map<std::string, int> BindingLocationMap;

    HDST_API
    HdStMSLProgram(TfToken const &role, HdStResourceRegistry *const registry);
    HDST_API
    ~HdStMSLProgram() override;

    /// Compile shader source of type
    HDST_API
    bool CompileShader(HgiShaderStage stage, std::string const & source) override;

    /// Link the compiled shaders together.
    HDST_API
    virtual bool Link();

    /// Validate if this program is a valid progam in the current context.
    HDST_API
    bool Validate() const override;

    /// Returns HdResource of the global uniform buffer object for this program.
    HDST_API
    HdResource const &GetGlobalUniformBuffer() const override {
        return _uniformBuffer;
    }

    /// Returns true if the program has been successfully linked.
    /// if not, returns false and fills the error log into reason.
    HDST_API
    bool GetProgramLinkStatus(std::string * reason) const override;
    
    /// Returns the binary size of the program (if available)
    HDST_API
    virtual uint32_t GetProgramSize() const { return 0; }
    
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
    virtual void UnsetProgram() override;
    
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
    
    HDST_API
    BindingLocationMap const &GetBindingLocations() const {
        return _locationMap;
    }
    
    HDST_API
    MSL_ShaderBindingMap const &GetBindingMap() const {
        return _bindingMap;
    }
    
    HDST_API
    void AddBinding(std::string const &name, int index,
        HdBinding const &binding, MSL_BindingType bindingType,
        MSL_ProgramStage programStage, int offsetWithinResource = 0,
        int uniformBufferSize = 0);
    
    HDST_API
    void BindTexture(
        const TfToken &name,
        id<MTLTexture> textureId) const;

    HDST_API
    void BindSampler(
        const TfToken &name,
        id<MTLSamplerState> samplerId) const;
        
    HDST_API
    void UpdateUniformBinding(std::string const &name, int index);

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
    
    HDST_API
    void SetBuildTarget(MSL_BuildTarget buildTarget) { _buildTarget = buildTarget; }
    
    HDST_API
    void SetGSOutStructsSize(int vertOutStructSize, int primOutStructSize) {
        _gsVertOutStructSize = vertOutStructSize;
        _gsPrimOutStructSize = primOutStructSize;
    }
protected:
    HDST_API
    void BakeState();
    
    HDST_API
    virtual std::string GetComputeHeader() const override;

private:
    TfToken const _role;
    
    id<MTLFunction> _vertexFunction;
    id<MTLFunction> _fragmentFunction;
    id<MTLFunction> _computeFunction;
    id<MTLFunction> _computeGeometryFunction;
    
    id<MTLRenderPipelineState> _pipelineState;

    bool _valid;
    HdStResourceMetal  _uniformBuffer;
    MSL_ShaderBindingMap _bindingMap;
    BindingLocationMap _locationMap;
    
    MSL_BuildTarget _buildTarget;
    int _gsVertOutBufferSlot;
    int _gsPrimOutBufferSlot;
    int _gsVertOutStructSize;
    int _gsPrimOutStructSize;
    int _drawArgsSlot;
    int _indicesSlot;
    int _fragExtrasSlot;
    
    bool _currentlySet;
    mutable bool _reapplyIndexBuffer;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif  // HDST_MSL_PROGRAM_H
