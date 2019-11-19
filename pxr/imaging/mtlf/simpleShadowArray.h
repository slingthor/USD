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
#ifndef MTLF_SIMPLE_SHADOW_ARRAY_H
#define MTLF_SIMPLE_SHADOW_ARRAY_H

/// \file mtlf/simpleShadowArray.h

#include "pxr/pxr.h"
#include "pxr/imaging/mtlf/api.h"
#include "pxr/base/tf/declarePtrs.h"
#include "pxr/base/tf/refPtr.h"
#include "pxr/base/tf/weakPtr.h"
#include "pxr/base/gf/matrix4d.h"
#include "pxr/base/gf/vec2i.h"
#include "pxr/base/gf/vec4d.h"
#include "pxr/imaging/garch/simpleShadowArray.h"

#include <boost/noncopyable.hpp>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE


class MtlfSimpleShadowArray : public GarchSimpleShadowArray {
public:

    MTLF_API
    virtual void SetSize(GfVec2i const & size) override;

    MTLF_API
    virtual void SetNumLayers(size_t numLayers) override;

    MTLF_API
    virtual void InitCaptureEnvironment(bool   depthBiasEnable,
                                        float  depthBiasConstantFactor,
                                        float  depthBiasSlopeFactor,
                                        GLenum depthFunc) override;
    MTLF_API
    virtual void DisableCaptureEnvironment() override;

    MTLF_API
    virtual void BeginCapture(size_t index, bool clear) override;
    MTLF_API
    virtual void EndCapture(size_t index) override;

protected:
    MTLF_API
    MtlfSimpleShadowArray(GfVec2i const & size, size_t numLayers);
    MTLF_API
    virtual ~MtlfSimpleShadowArray();

    friend class MtlfResourceFactory;

private:
    void _AllocTextureArray();
    void _FreeTextureArray();
	
    void _AllocSamplers();
    void _FreeSamplers();

    void _BindFramebuffer(size_t index);
    void _UnbindFramebuffer();
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif  // MTLF_SIMPLE_SHADOW_ARRAY_H
