//
// Copyright 2018 Pixar
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
#ifndef HDST_FLAT_NORMALS_GL_H
#define HDST_FLAT_NORMALS_GL_H

#include "pxr/imaging/hdSt/flatNormals.h"

PXR_NAMESPACE_OPEN_SCOPE


/// flat normal computation GPU for GL
///
///
class HdSt_FlatNormalsComputationGL : public HdSt_FlatNormalsComputationGPU {
public:

    /// Constructor
    HDST_API
    HdSt_FlatNormalsComputationGL(HdBufferArrayRangeSharedPtr const
                                       &topologyRange,
                                  HdBufferArrayRangeSharedPtr const
                                       &vertexRange,
                                  int numFaces,
                                  TfToken const &srcName,
                                  TfToken const &dstName,
                                  HdType srcDataType,
                                  bool packed);

protected:
    
    HDST_API
    virtual void _Execute(
                      HdStProgramSharedPtr computeProgram,
                      Uniform const& uniform,
                      HdStBufferResourceGLSharedPtr points,
                      HdStBufferResourceGLSharedPtr normals,
                      HdStBufferResourceGLSharedPtr indices,
                      HdStBufferResourceGLSharedPtr primitiveParam,
                      int numPrims) override;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDST_FLAT_NORMALS_GL_H
