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
#ifndef GARCH_UNIFORM_BLOCK_H
#define GARCH_UNIFORM_BLOCK_H

/// \file garch/uniformBlock.h

#include "pxr/pxr.h"
#include "pxr/imaging/garch/api.h"
#include "pxr/base/tf/declarePtrs.h"
#include "pxr/base/tf/refBase.h"
#include "pxr/base/tf/weakBase.h"

PXR_NAMESPACE_OPEN_SCOPE


TF_DECLARE_WEAK_AND_REF_PTRS(GarchUniformBlock);
TF_DECLARE_WEAK_PTRS(GarchBindingMap);

/// \class GarchUniformBlock
///
/// Manages a uniform buffer object.
///
class GarchUniformBlock : public TfRefBase, public TfWeakBase {
public:
    GARCH_API
    virtual ~GarchUniformBlock();

    /// Binds the uniform buffer using a bindingMap and identifier.
    GARCH_API
    virtual void Bind(GarchBindingMapPtr const & bindingMap,
                      std::string const & identifier) = 0;

    /// Updates the content of the uniform buffer. If the size
    /// is different, the buffer will be reallocated.
    GARCH_API
    virtual void Update(const void *data, int size) = 0;

protected:
    GARCH_API
    GarchUniformBlock();
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif // GARCH_UNIFORM_BLOCK_H
