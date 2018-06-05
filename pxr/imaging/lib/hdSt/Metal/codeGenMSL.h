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
#ifndef HDST_CODE_GEN_MSL_H
#define HDST_CODE_GEN_MSL_H

#include "pxr/pxr.h"
#include "pxr/imaging/hdSt/api.h"
#include "pxr/imaging/hdSt/resourceBinder.h"
#include "pxr/imaging/hdSt/codeGen.h"

#include "pxr/imaging/hdSt/Metal/mslProgram.h"

#include <boost/shared_ptr.hpp>

#include <map>
#include <vector>
#include <sstream>

PXR_NAMESPACE_OPEN_SCOPE


/// \class Hd_CodeGenMSL
///
/// A utility class to compose glsl shader sources and compile them
/// upon request of HdShaderSpec.
///
class HdSt_CodeGenMSL : public HdSt_CodeGen
{
public:
    struct TParam {
        enum Usage {
            Unspecified = 0,
            Texture,
            Sampler,
            
            // The following are bit flags | with one of the above
            EntryFuncArgument   = 1 << 4,
            ProgramScope        = 1 << 5,
            VertexShaderOnly    = 1 << 6
        };
        static Usage const maskShaderUsage = Usage(EntryFuncArgument - 1);
        
        TParam()
        : usage(Unspecified) {}

        TParam(TfToken   const &_name,
               TfToken   const &_dataType,
               TfToken   const &_accessorStr,
               TfToken   const &_attribute,
               Usage     const _usage,
               HdBinding const &_binding = HdBinding(HdBinding::UNKNOWN, 0))
        : name(_name)
        , dataType(_dataType)
        , accessorStr(_accessorStr)
        , attribute(_attribute)
        , usage(_usage)
        , binding(_binding)
        {}
          
        TfToken const name;
        TfToken const dataType;
        TfToken const accessorStr;
        TfToken const attribute;
        Usage usage;
        HdBinding const binding;
    };
    
    typedef std::vector<TParam> InOutParams;

    /// Constructor.
    HD_API
    HdSt_CodeGenMSL(HdSt_GeometricShaderPtr const &geometricShader,
                    HdStShaderCodeSharedPtrVector const &shaders);

    /// Constructor for non-geometric use cases.
    /// Don't call compile when constructed this way.
    /// Call CompileComputeProgram instead.
    HD_API
    HdSt_CodeGenMSL(HdStShaderCodeSharedPtrVector const &shaders);
    
    /// Destructor
    HD_API
    virtual ~HdSt_CodeGenMSL() {};
    
    /// Return the hash value of glsl shader to be generated.
    HD_API
    virtual ID ComputeHash() const;

    /// Generate shader source and compile it.
    HD_API
    virtual HdStProgramSharedPtr Compile();

    /// Generate compute shader source and compile it.
    /// It uses the compute information in the meta data to determine
    /// layouts needed for a compute program.
    /// The caller should have populated the meta data before calling this
    /// using a method like Hd_ResourceBinder::ResolveBindings.
    ///
    /// The layout and binding information is combined with the compute stage
    /// shader code from the shader vector to form a resolved shader for
    /// compilation.
    ///
    /// The generated code that is compiled is available for diagnostic
    /// purposes from GetComputeShaderSource.
    ///
    /// \see GetComputeShaderSource
    /// \see Hd_ResourceBinder::ResolveBindings
    HD_API
    virtual HdStProgramSharedPtr CompileComputeProgram();
    
    /// Return the generated vertex shader source
    virtual const std::string &GetVertexShaderSource() const { return _vsSource; }

    /// Return the generated tess control shader source
    virtual const std::string &GetTessControlShaderSource() const { return _tcsSource; }

    /// Return the generated tess eval shader source
    virtual const std::string &GetTessEvalShaderSource() const { return _tesSource; }

    /// Return the generated geometry shader source
    virtual const std::string &GetGeometryShaderSource() const { return _gsSource; }

    /// Return the generated fragment shader source
    virtual const std::string &GetFragmentShaderSource() const { return _fsSource; }

    /// Return the generated compute shader source
    virtual const std::string &GetComputeShaderSource() const { return _csSource; }
    
    /// Return the pointer of metadata to be populated by resource binder.
    virtual HdSt_ResourceBinder::MetaData *GetMetaData() { return &_metaData; }

private:
    void _GenerateDrawingCoord();
    void _GenerateConstantPrimVar();
    void _GenerateInstancePrimVar();
    void _GenerateElementPrimVar();
    void _GenerateVertexPrimVar();
    void _GenerateShaderParameters();
    
    void _ParseGLSL(std::stringstream &source, InOutParams& inParams, InOutParams& outParams);
    void _GenerateGlue(std::stringstream& glueVS, std::stringstream& gluePS, HdStMSLProgramSharedPtr mslProgram);

    HdSt_ResourceBinder::MetaData _metaData;
    HdSt_GeometricShaderPtr _geometricShader;
    HdStShaderCodeSharedPtrVector _shaders;

    // source buckets
    std::stringstream _genCommon, _genVS, _genTCS, _genTES;
    std::stringstream _genGS, _genFS, _genCS;
    std::stringstream _procVS, _procTCS, _procTES, _procGS;

    // generated sources (for diagnostics)
    std::string _vsSource;
    std::string _tcsSource;
    std::string _tesSource;
    std::string _gsSource;
    std::string _fsSource;
    std::string _csSource;
    
    InOutParams _mslVSInputParams;
    InOutParams _mslVSOutputParams;
    InOutParams _mslPSInputParams;
    InOutParams _mslPSOutputParams;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif  // HDST_CODE_GEN_H
