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

#ifndef GARCH_TEXTURE_HANDLE_H
#define GARCH_TEXTURE_HANDLE_H

/// \file garch/textureHandle.h

#include "pxr/pxr.h"
#include "pxr/imaging/garch/api.h"
#include "pxr/imaging/garch/texture.h"

#include "pxr/imaging/garch/glApi.h"

#include "pxr/base/tf/declarePtrs.h"
#include "pxr/base/tf/refPtr.h"
#include "pxr/base/tf/weakPtr.h"

#include <string>
#include <map>

PXR_NAMESPACE_OPEN_SCOPE


TF_DECLARE_WEAK_AND_REF_PTRS(GarchTextureHandle);

class GarchTextureHandle : public TfRefBase, public TfWeakBase {
public:
    GARCH_API
    static GarchTextureHandleRefPtr New(GarchTextureRefPtr texture);

    GARCH_API
    virtual ~GarchTextureHandle();

    GarchTexturePtr GetTexture() {
        return _texture;
    }

    GARCH_API
    void AddMemoryRequest(size_t targetMemory);

    GARCH_API
    void DeleteMemoryRequest(size_t targetMemory);

protected:
    GARCH_API
    GarchTextureHandle(GarchTextureRefPtr texture);

    GarchTextureRefPtr _texture;

    GARCH_API
    void _ComputeMemoryRequirement();

    // requested memory map
    std::map<size_t, size_t> _requestedMemories;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif // GARCH_TEXTURE_HANDLE_H
