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
#ifndef HDST_VBO_MEMORY_BUFFER_METAL_H
#define HDST_VBO_MEMORY_BUFFER_METAL_H

#include "pxr/pxr.h"
#include "pxr/imaging/hdSt/api.h"
#include "pxr/imaging/hdSt/vboMemoryManager.h"

PXR_NAMESPACE_OPEN_SCOPE

/// \class HdStVBOMemoryBufferMetal
///
/// VBO memory buffer for Metal
///
class HdStVBOMemoryBufferMetal : public HdStVBOMemoryManager::_StripedBufferArray {
public:
    /// Constructor.
    HDST_API
    HdStVBOMemoryBufferMetal(TfToken const &role,
                             HdBufferSpecVector const &bufferSpecs,
                             HdBufferArrayUsageHint usageHint);
    
    /// Destructor. It invalidates _rangeList
    HDST_API
    virtual ~HdStVBOMemoryBufferMetal() {}

    /// Performs reallocation.
    /// GLX context has to be set when calling this function.
    HDST_API
    virtual void Reallocate(
        std::vector<HdBufferArrayRangeSharedPtr> const &ranges,
        HdBufferArraySharedPtr const &curRangeOwner) override;
    
protected:
    HDST_API
    virtual void _DeallocateResources() override;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif  // HDST_VBO_MEMORY_BUFFER_METAL_H
