//
// Copyright 2019 Pixar
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
#ifndef PXR_IMAGING_HGIINTEROP_HGIINTEROP_H
#define PXR_IMAGING_HGIINTEROP_HGIINTEROP_H

#include "pxr/pxr.h"
#include "pxr/base/tf/type.h"

#include "pxr/imaging/hgiInterop/api.h"
#include "pxr/imaging/hgi/texture.h"

PXR_NAMESPACE_OPEN_SCOPE

class Hgi;
class HgiInteropMetal;

/// \class HgiInterop
///
/// Hydra Graphics Interface Interop.
///
/// HgiInterop provides functionality to transfer render targets between
/// supported APIs as efficiently as possible.
///
class HgiInterop 
{
public:
    HGIINTEROP_API
    HgiInterop();

    HGIINTEROP_API
    virtual ~HgiInterop();
    
    void SetFlipOnBlit(bool flipY);
    
    void TransferToApp(
        Hgi *hgi,
        HgiTextureHandle const &color,
        HgiTextureHandle const &depth);

private:
    HgiInterop & operator=(const HgiInterop&) = delete;
    HgiInterop(const HgiInterop&) = delete;
    
    bool _flipImage;
    std::unique_ptr<HgiInteropMetal> _metalToOpenGL;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif
