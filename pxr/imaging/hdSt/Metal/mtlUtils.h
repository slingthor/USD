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
#ifndef HD_METAL_UTILS_H
#define HD_METAL_UTILS_H

#include "pxr/pxr.h"
#include "pxr/imaging/hd/api.h"
#include "pxr/imaging/hd/version.h"
#include "pxr/base/vt/value.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

PXR_NAMESPACE_OPEN_SCOPE

class HdMetalUtils {
public:
    /// Returns true if the shader has been successfully compiled.
    /// if not, returns false and fills the error log into reason.
    HD_API
    static bool GetShaderCompileStatus(uint32_t shader,
                                       std::string * reason = NULL);
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // HD_METAL_UTILS_H
