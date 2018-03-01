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
#ifndef HD_FALLBACK_LIGHTING_SHADER_H
#define HD_FALLBACK_LIGHTING_SHADER_H

#include "pxr/pxr.h"
#include "pxr/imaging/hd/api.h"
#include "pxr/imaging/hd/version.h"
#include "pxr/imaging/hd/lightingShader.h"
#include "pxr/imaging/garch/glslfx.h"

#include "pxr/base/gf/vec4d.h"

#include "pxr/base/tf/declarePtrs.h"
#include "pxr/base/tf/token.h"

#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>

#include <vector>

PXR_NAMESPACE_OPEN_SCOPE


/// \class Hd_FallbackLightingShader
///
/// A shader that provides fallback lighting behavior.
///
class Hd_FallbackLightingShader : public HdLightingShader {
public:
    HD_API
    Hd_FallbackLightingShader();
    HD_API
    virtual ~Hd_FallbackLightingShader();

    // HdShaderCode overrides
    HD_API
    virtual ID ComputeHash() const;
    HD_API
    virtual std::string GetSource(TfToken const &shaderStageKey) const;
    HD_API
    virtual void BindResources(Hd_ResourceBinder const &binder, HdBufferResourceGPUHandle program);
    HD_API
    virtual void UnbindResources(Hd_ResourceBinder const &binder, HdBufferResourceGPUHandle program);
    HD_API
    virtual void AddBindings(HdBindingRequestVector *customBindings);

    // HdLightingShader overrides
    HD_API
    virtual void SetCamera(GfMatrix4d const &worldToViewMatrix,
                           GfMatrix4d const &projectionMatrix);

private:
    boost::scoped_ptr<GLSLFX> _glslfx;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif // HD_FALLBACK_LIGHTING_SHADER_H
