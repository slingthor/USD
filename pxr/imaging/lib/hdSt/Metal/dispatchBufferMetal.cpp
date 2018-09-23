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
#include "pxr/imaging/glf/glew.h"
#include "pxr/imaging/garch/contextCaps.h"
#include "pxr/imaging/garch/resourceFactory.h"
#include "pxr/imaging/mtlf/mtlDevice.h"

#include "pxr/imaging/hdSt/Metal/dispatchBufferMetal.h"
#include "pxr/imaging/hd/engine.h"
#include "pxr/imaging/hd/perfLog.h"

#include "pxr/imaging/hf/perfLog.h"

using namespace boost;

PXR_NAMESPACE_OPEN_SCOPE

HdStDispatchBufferMetal::HdStDispatchBufferMetal(TfToken const &role, int count,
                                                 unsigned int commandNumUints)
    : HdStDispatchBuffer(role, count, commandNumUints)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    GarchContextCaps const &caps = GarchResourceFactory::GetInstance()->GetContextCaps();

    size_t stride = commandNumUints * sizeof(GLuint);
    size_t dataSize = count * stride;
    
    HdResourceGPUHandle newId;
    newId = [MtlfMetalContext::GetMetalContext()->device newBufferWithLength:dataSize options:MTLResourceStorageModeManaged];

    _entireResource->SetAllocation(newId, dataSize);
}

HdStDispatchBufferMetal::~HdStDispatchBufferMetal()
{
    HdResourceGPUHandle _id = _entireResource->GetId();
    id<MTLBuffer> oid = _id;
    [oid release];
    _entireResource->SetAllocation(HdResourceGPUHandle(), 0);
}

void
HdStDispatchBufferMetal::CopyData(std::vector<GLuint> const &data)
{
    if (!TF_VERIFY(data.size()*sizeof(GLuint) == static_cast<size_t>(_entireResource->GetSize())))
        return;

    id<MTLBuffer> buffer = _entireResource->GetId();
    memcpy([buffer contents], &data[0], _entireResource->GetSize());
}

PXR_NAMESPACE_CLOSE_SCOPE

