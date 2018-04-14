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
#ifndef MTLF_SIMPLE_LIGHTING_CONTEXT_H
#define MTLF_SIMPLE_LIGHTING_CONTEXT_H

/// \file mtlf/simpleLightingContext.h

#include "pxr/pxr.h"
#include "pxr/imaging/mtlf/api.h"

#include "pxr/imaging/garch/simpleLightingContext.h"

#include "pxr/base/gf/matrix4d.h"
#include "pxr/base/gf/vec4f.h"

#include "pxr/base/tf/declarePtrs.h"
#include "pxr/base/tf/refBase.h"
#include "pxr/base/tf/weakBase.h"

PXR_NAMESPACE_OPEN_SCOPE

class MtlfSimpleLightingContext : public GarchSimpleLightingContext {
public:

    MTLF_API
    virtual void BindUniformBlocks(GarchBindingMapPtr const &bindingMap) override;
    MTLF_API
    virtual void BindSamplers(GarchBindingMapPtr const &bindingMap) override;

    MTLF_API
    virtual void UnbindSamplers(GarchBindingMapPtr const &bindingMap) override;

    MTLF_API
    virtual void SetStateFromOpenGL() override;

protected:
    MTLF_API
    MtlfSimpleLightingContext();
    MTLF_API
    virtual ~MtlfSimpleLightingContext();
    
    friend class MtlfResourceFactory;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif
