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


#include "pxr/imaging/hdSt/codeGen.h"
#include "pxr/imaging/hd/engine.h"

PXR_NAMESPACE_OPEN_SCOPE

PXR_NAMESPACE_CLOSE_SCOPE
            str << "  return " << _GetPackedTypeAccessor(type, true) << "("
            accessors 
                << "#ifdef HD_HAS_" << it->second.name << "_" 
                << HdStTokens->scale << "\n"
                << "vec4 HdGet_" << it->second.name << "_" 
                << HdStTokens->scale << "();\n"
                << "#endif\n"
                << "#ifdef HD_HAS_" << it->second.name << "_" 
                << HdStTokens->bias << "\n"
                << "vec4 HdGet_" << it->second.name << "_" 
                << HdStTokens->bias << "();\n"
                << "#endif\n";
                
                    << "  }\n";
                << "if (c.z < -0.5) { return vec4(0, 0, 0, 0)" << swizzle
                << "; } else { \n"
                << "  return texture(sampler2DArray(shaderData[shaderCoord]."
                << it->second.name << "), c)" << swizzle << ";}\n}\n";
                << "  return (ret\n"
                << "#ifdef HD_HAS_" << it->second.name << "_" 
                << HdStTokens->scale << "\n"
                << "    * HdGet_" << it->second.name << "_" 
                << HdStTokens->scale << "()\n"
                << "#endif\n" 
                << "#ifdef HD_HAS_" << it->second.name << "_" 
                << HdStTokens->bias << "\n"
                << "    + HdGet_" << it->second.name << "_" 
                << HdStTokens->bias  << "()\n"
                << "#endif\n"
                << "  )" << swizzle << ";\n}\n";

            // Emit pre-multiplication by alpha indicator
            if (it->second.isPremultiplied) {
                accessors 
                    << "#define " << it->second.name << "_IS_PREMULTIPLIED 1\n";
            }      
            accessors 
                << "#ifdef HD_HAS_" << it->second.name << "_" 
                << HdStTokens->scale << "\n"
                << "vec4 HdGet_" << it->second.name << "_" 
                << HdStTokens->scale << "();\n"
                << "#endif\n"
                << "#ifdef HD_HAS_" << it->second.name << "_" 
                << HdStTokens->bias << "\n"
                << "vec4 HdGet_" << it->second.name << "_" 
                << HdStTokens->bias << "();\n"
                << "#endif\n";

            // if (c.z < -0.5) { return vec4(0, 0, 0, 0).xyz; } else {
            // return texture(sampler2dArray_name, c).xyz;}}
            // return (ret
            // #ifdef HD_HAS_name_scale
            //   * HdGet_name_scale()
            // #endif
            // #ifdef HD_HAS_name_bias
            //   + HdGet_name_bias()
            // #endif
            // ).xyz; }
                << "if (c.z < -0.5) { return vec4(0, 0, 0, 0)"
                << "  return texture(sampler2dArray_"
                << it->second.name << ", c); }\n  return (ret\n"
                << "#ifdef HD_HAS_" << it->second.name << "_"
                << HdStTokens->scale << "\n"
                << "    * HdGet_" << it->second.name << "_" 
                << HdStTokens->scale << "()\n"
                << "#endif\n" 
                << "#ifdef HD_HAS_" << it->second.name << "_" 
                << HdStTokens->bias << "\n"
                << "    + HdGet_" << it->second.name << "_" 
                << HdStTokens->bias  << "()\n"
                << "#endif\n"
                << it->second.name << ", c)" << swizzle << ";}}\n";


            // Emit pre-multiplication by alpha indicator
            if (it->second.isPremultiplied) {
                accessors 
                    << "#define " << it->second.name << "_IS_PREMULTIPLIED 1\n";
            }  

            // Emit pre-multiplication by alpha indicator
            if (it->second.isPremultiplied) {
                accessors 
                    << "#define " << it->second.name << "_IS_PREMULTIPLIED 1\n";
            }     

            // Emit pre-multiplication by alpha indicator
            if (it->second.isPremultiplied) {
                accessors 
                    << "#define " << it->second.name << "_IS_PREMULTIPLIED 1\n";
            }    
