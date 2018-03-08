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

#include "pxr/imaging/hd/engine.h"

#include "pxr/imaging/hdSt/persistentBuffer.h"
#include "pxr/imaging/hdSt/renderContextCaps.h"
#include "pxr/imaging/hdSt/GL/persistentBufferGL.h"
#if defined(ARCH_GFX_METAL)
#include "pxr/imaging/hdSt/Metal/persistentBufferMetal.h"
#endif

PXR_NAMESPACE_OPEN_SCOPE

HdStPersistentBuffer *HdStPersistentBuffer::New(TfToken const &role, size_t dataSize, void* data)
{
    HdEngine::RenderAPI api = HdEngine::GetRenderAPI();
    switch(api)
    {
        case HdEngine::OpenGL:
            return new HdStPersistentBufferGL(role, dataSize, data);
#if defined(ARCH_GFX_METAL)
        case HdEngine::Metal:
            return new HdStPersistentBufferMetal(role, dataSize, data);
#endif
        default:
            TF_FATAL_CODING_ERROR("No HdStBufferResource for this API");
    }
    return NULL;
}

HdStPersistentBuffer::HdStPersistentBuffer(HdResourceSharedPtr resource):
    _resource(resource)
{
    /*NOTHING*/
}

HdStPersistentBuffer::~HdStPersistentBuffer()
{
    /*NOTHING*/
}

PXR_NAMESPACE_CLOSE_SCOPE

