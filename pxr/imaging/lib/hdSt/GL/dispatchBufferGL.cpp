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

#include "pxr/imaging/hdSt/GL/dispatchBufferGL.h"
#include "pxr/imaging/hd/engine.h"
#include "pxr/imaging/hd/perfLog.h"

#include "pxr/imaging/hf/perfLog.h"

using namespace boost;

PXR_NAMESPACE_OPEN_SCOPE


HdStDispatchBufferGL::HdStDispatchBufferGL(TfToken const &role, int count,
                                           unsigned int commandNumUints)
    : HdStDispatchBuffer(role, count, commandNumUints)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    GarchContextCaps const &caps = GarchResourceFactory::GetInstance()->GetContextCaps();

    size_t stride = commandNumUints * sizeof(GLuint);
    size_t dataSize = count * stride;
    
    HdResourceGPUHandle newId;

    GLuint nid = 0;
    glGenBuffers(1, &nid);
    // just allocate uninitialized
    if (caps.directStateAccessEnabled) {
        glNamedBufferDataEXT(nid, dataSize, NULL, GL_STATIC_DRAW);
    } else {
        glBindBuffer(GL_ARRAY_BUFFER, nid);
        glBufferData(GL_ARRAY_BUFFER, dataSize, NULL, GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    newId = nid;

    _entireResource->SetAllocation(newId, dataSize);
}

HdStDispatchBufferGL::~HdStDispatchBufferGL()
{
    HdResourceGPUHandle _id = _entireResource->GetId();
    GLuint oid = _id;
    glDeleteBuffers(1, &oid);
    _entireResource->SetAllocation(HdResourceGPUHandle(), 0);
}

void
HdStDispatchBufferGL::CopyData(std::vector<GLuint> const &data)
{
    if (!TF_VERIFY(data.size()*sizeof(GLuint) == static_cast<size_t>(_entireResource->GetSize())))
        return;

    GarchContextCaps const &caps = GarchResourceFactory::GetInstance()->GetContextCaps();

    if (caps.directStateAccessEnabled) {
        glNamedBufferSubDataEXT(_entireResource->GetId(),
                                0,
                                _entireResource->GetSize(),
                                &data[0]);
    } else {
        glBindBuffer(GL_ARRAY_BUFFER, _entireResource->GetId());
        glBufferSubData(GL_ARRAY_BUFFER, 0,
                        _entireResource->GetSize(),
                        &data[0]);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}

PXR_NAMESPACE_CLOSE_SCOPE

