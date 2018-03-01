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
#include "pxr/imaging/glf/glew.h"
#include "pxr/imaging/hd/GL/resourceGL.h"

PXR_NAMESPACE_OPEN_SCOPE


HdResourceGL::HdResourceGL(TfToken const & role) 
    : HdResource(role)
    , _id(0)
{
    /*NOTHING*/
}

HdResourceGL::~HdResourceGL()
{
    /*NOTHING*/
}

void
HdResourceGL::SetAllocation(HdBufferResourceGPUHandle id, size_t size)
{
    _id = (GLuint)(uint64_t)id;
    HdResource::SetSize(size);
}

void
HdResourceGL::SetAllocation(GLuint id, size_t size)
{
    _id = id;
    HdResource::SetSize(size);
}

HdBufferResourceGPUHandle
GetId()
{
    TF_FATAL_CODING_ERROR("Not a valid call - HdResourceGL was instantiated directly");
    return NULL;
}

PXR_NAMESPACE_CLOSE_SCOPE
