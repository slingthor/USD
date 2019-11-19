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
#ifndef PXR_IMAGING_HD_ST_CODE_GEN_H
#define PXR_IMAGING_HD_ST_CODE_GEN_H


#include "pxr/pxr.h"
#include "pxr/imaging/hdSt/api.h"
#include "pxr/imaging/hdSt/resourceBinder.h"
#include "pxr/imaging/hdSt/program.h"

#include "pxr/imaging/hd/version.h"

#include <boost/shared_ptr.hpp>

#include <map>
#include <vector>
#include <sstream>

PXR_NAMESPACE_OPEN_SCOPE


typedef boost::shared_ptr<class HdStShaderCode> HdStShaderCodeSharedPtr;
typedef boost::shared_ptr<class HdSt_GeometricShader> HdSt_GeometricShaderPtr;
typedef std::vector<HdStShaderCodeSharedPtr> HdStShaderCodeSharedPtrVector;

/// \class HdSt_CodeGen
///
/// A utility class to compose glsl shader sources and compile them
/// upon request of HdShaderSpec.
///
class HdSt_CodeGen
{
public:
    typedef size_t ID;
    
    HDST_API
    virtual ~HdSt_CodeGen() {};

    /// Return the hash value of glsl shader to be generated.
    HDST_API
    virtual ID ComputeHash() const = 0;

    /// Generate shader source and compile it.
    HDST_API
    virtual HdStProgramSharedPtr Compile() = 0;

    /// Generate compute shader source and compile it.
    /// It uses the compute information in the meta data to determine
    /// layouts needed for a compute program.
    /// The caller should have populated the meta data before calling this
    /// using a method like HdSt_ResourceBinder::ResolveBindings.
    ///
    /// The layout and binding information is combined with the compute stage
    /// shader code from the shader vector to form a resolved shader for
    /// compilation.
    ///
    /// The generated code that is compiled is available for diagnostic
    /// purposes from GetComputeShaderSource.
    ///
    /// \see GetComputeShaderSource
    /// \see HdSt_ResourceBinder::ResolveBindings
    HDST_API
    virtual HdStProgramSharedPtr CompileComputeProgram() = 0;
    
    /// Return the generated vertex shader source
    virtual const std::string &GetVertexShaderSource() const = 0;

    /// Return the generated tess control shader source
    virtual const std::string &GetTessControlShaderSource() const = 0;

    /// Return the generated tess eval shader source
    virtual const std::string &GetTessEvalShaderSource() const = 0;

    /// Return the generated geometry shader source
    virtual const std::string &GetGeometryShaderSource() const = 0;

    /// Return the generated fragment shader source
    virtual const std::string &GetFragmentShaderSource() const = 0;

    /// Return the generated compute shader source
    virtual const std::string &GetComputeShaderSource() const = 0;
    
    /// Return the pointer of metadata to be populated by resource binder.
    virtual HdSt_ResourceBinder::MetaData *GetMetaData() = 0;

protected:
    /// Constructor.
    HDST_API
    HdSt_CodeGen() {}
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif  // PXR_IMAGING_HD_ST_CODE_GEN_H
