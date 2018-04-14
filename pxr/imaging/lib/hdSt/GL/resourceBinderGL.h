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
#ifndef HDST_RESOURCE_BINDER_GL_H
#define HDST_RESOURCE_BINDER_GL_H

#include "pxr/pxr.h"
#include "pxr/imaging/hdSt/api.h"

#include "pxr/imaging/hdSt/resourceBinder.h"

PXR_NAMESPACE_OPEN_SCOPE

class HdSt_ResourceBinderGL : public HdSt_ResourceBinder {
public:
    
    /// Constructor
    HDST_API
    HdSt_ResourceBinderGL();
    
    /// Destructor
    HDST_API
    virtual ~HdSt_ResourceBinderGL() {}

    /// call introspection APIs and fix up binding locations
    HDST_API
    virtual void IntrospectBindings(HdResource const & programResource) override;
    
    /// bind/unbind shader parameters and textures
    HDST_API
    virtual void BindShaderResources(HdStShaderCode const *shader) const override;
    HDST_API
    virtual void UnbindShaderResources(HdStShaderCode const *shader) const override;
    
    /// piecewise buffer binding utility
    /// (to be used for frustum culling, draw indirect result)
    HDST_API
    virtual void BindBuffer(TfToken const &name,
                            HdBufferResourceSharedPtr const &resource,
                            int offset, int level=-1) const override;
    HDST_API
    virtual void UnbindBuffer(TfToken const &name,
                              HdBufferResourceSharedPtr const &resource,
                              int level=-1) const override;
    
    /// bind(update) a standalone uniform (unsigned int)
    HDST_API
    virtual void BindUniformui(TfToken const &name, int count,
                               const unsigned int *value) const override;
    
    /// bind a standalone uniform (signed int, ivec2, ivec3, ivec4)
    HDST_API
    virtual void BindUniformi(TfToken const &name, int count, const int *value) const override;
    
    /// bind a standalone uniform array (int[N])
    HDST_API
    virtual void BindUniformArrayi(TfToken const &name, int count, const int *value) const override;
    
    /// bind a standalone uniform (float, vec2, vec3, vec4, mat4)
    HDST_API
    virtual void BindUniformf(TfToken const &name, int count, const float *value) const override;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif  // HDST_RESOURCE_BINDER_GL_H

