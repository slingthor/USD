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
#ifndef HD_GLSLFX_H
#define HD_GLSLFX_H

/// \file hd/glslfx.h

#include "pxr/pxr.h"
#include "pxr/imaging/garch/glslfxConfig.h"

#include "pxr/base/tf/token.h"

#include <boost/scoped_ptr.hpp>

#include <string>
#include <vector>
#include <set>
#include <map>

PXR_NAMESPACE_OPEN_SCOPE


/// \class GLSLFX
///
/// A class representing the config and shader source of a glslfx file.
///
/// a GLSLFX object is constructed by providing the path of a file whose
/// contents look something like this:
///
/// \code
/// -- glslfx version 0.1
/// 
/// -- configuration
/// 
/// {
///
///     'textures' : {
///         'texture_1':{
///             'documentation' : 'a useful texture.',
///         },
///         'texture_2':{
///             'documentation' : 'another useful texture.',
///         },
///     },
///     'parameters': {
///         'param_1' : {
///             'default' : 1.0,
///             'documentation' : 'the first parameter'
///         },
///         'param_2' : {
///             'default' : [1.0, 1.0, 1.0],
///             'documentation' : 'a vec3f parameter'
///         },
///         'param_3' : {
///             'default' : 2.0
///         },
///         'param_4' : {
///             'default' : True
///         },
///         'param_5' : {
///             'default' : [1.0, 1.0, 1.0],
///             'role' : 'color'
///             'documentation' : 'specifies a color for use in the shader'
///         },
///     },
///     'parameterOrder': ['param_1',
///                        'param_2',
///                        'param_3',
///                        'param_4',
///                        'param_5'],
/// 
///     'techniques': {
///         'default': {
///             'fragmentShader': {
///                 'source': [ 'MyFragment' ]
///             }
///         }
///     }
/// }
/// 
/// -- glsl MyFragment
/// 
/// uniform float param_1;
/// uniform float param_2;
/// uniform float param_3;
/// uniform float param_4;
/// uniform float param_5;
/// 
/// void main()
/// {
///     // ...
///     // glsl code which consumes the various uniforms, and perhaps sets
///     // gl_FragColor = someOutputColor;
///     // ...
/// }
/// \endcode
///
class GLSLFX
{
public:
    /// Create an invalid glslfx object
    GARCH_API
    GLSLFX() {}
    
    GARCH_API
    virtual ~GLSLFX() {};

    /// Return the parameters specified in the configuration
    GARCH_API
    virtual GLSLFXConfig::Parameters GetParameters() const = 0;

    /// Return the textures specified in the configuration
    GARCH_API
    virtual GLSLFXConfig::Textures GetTextures() const = 0;

    /// Return the attributes specified in the configuration
    GARCH_API
    virtual GLSLFXConfig::Attributes GetAttributes() const = 0;

    /// Return the metadata specified in the configuration
    GARCH_API
    virtual GLSLFXConfig::MetadataDictionary GetMetadata() const = 0;

    /// Returns true if this is a valid glslfx file
    GARCH_API
    virtual bool IsValid(std::string *reason=NULL) const = 0;

    /// \name Compatible shader sources
    /// @{

    /// Get the vertex source string
    GARCH_API
    virtual std::string GetVertexSource() const = 0;

    /// Get the tess control source string
    GARCH_API
    virtual std::string GetTessControlSource() const = 0;

    /// Get the tess eval source string
    GARCH_API
    virtual std::string GetTessEvalSource() const = 0;

    /// Get the geometry source string
    GARCH_API
    virtual std::string GetGeometrySource() const = 0;

    /// Get the fragment source string
    GARCH_API
    virtual std::string GetFragmentSource() const = 0;

    /// @}

    /// \name OpenSubdiv composable shader sources
    /// @{

    /// Get the preamble (osd uniform definitions)
    GARCH_API
    virtual std::string GetPreambleSource() const = 0;

    /// Get the surface source string
    GARCH_API
    virtual std::string GetSurfaceSource() const = 0;

    /// Get the displacement source string
    GARCH_API
    virtual std::string GetDisplacementSource() const = 0;

    /// Get the vertex injection source string
    GARCH_API
    virtual std::string GetVertexInjectionSource() const = 0;

    /// Get the geometry injection source string
    GARCH_API
    virtual std::string GetGeometryInjectionSource() const = 0;

    /// @}

    /// Get the shader source associated with given key
    GARCH_API
    virtual std::string GetSource(const TfToken &shaderStageKey) const = 0;

    /// Get the original file name passed to the constructor
    virtual std::string const &GetFilePath() const = 0;

    /// Return set of all files processed for this glslfx object.
    /// This includes the original file given to the constructor
    /// as well as any other files that were imported. This set
    /// will only contain files that exist.
    virtual std::set<std::string> const& GetFiles() const = 0;

    /// Return the computed hash value based on the string
    virtual size_t GetHash() const = 0;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif // HD_GLSLFX_H

