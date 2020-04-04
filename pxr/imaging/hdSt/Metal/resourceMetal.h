//
// Copyright 2017 Pixar
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
#ifndef HDST_RESOURCE_METAL_H
#define HDST_RESOURCE_METAL_H

#include "pxr/pxr.h"
#include "pxr/imaging/hdSt/api.h"
#include "pxr/imaging/hd/resource.h"
#include "pxr/imaging/mtlf/mtlDevice.h"
#include "pxr/base/tf/token.h"

#include <boost/shared_ptr.hpp>

#include <cstddef>

PXR_NAMESPACE_OPEN_SCOPE


using HdStResourceMetalSharedPtr = std::shared_ptr<class HdStResourceMetal>;

/// \class HdStResourceMetal
///
/// Base class for simple Metal resource objects.
///
class HdStResourceMetal : public HdResource {
public:
    HDST_API
    HdStResourceMetal(TfToken const & role);
    HDST_API
    virtual ~HdStResourceMetal();

    /// The Metal object for this resource and its size
    HDST_API
    virtual void SetAllocation(HdResourceGPUHandle resId, size_t size);

    /// Returns the id of the GPU resource
    HDST_API
    virtual HdResourceGPUHandle GetId() const { return _id; }

private:
    HdResourceGPUHandle _id;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDST_RESOURCE_METAL_H
