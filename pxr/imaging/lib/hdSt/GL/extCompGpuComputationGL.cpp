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
#include "pxr/base/arch/defines.h"

#include "pxr/imaging/glf/glew.h"
#include "pxr/imaging/glf/diagnostic.h"

#include "pxr/imaging/garch/contextCaps.h"
#include "pxr/imaging/garch/resourceFactory.h"

#include "pxr/imaging/hdSt/GL/extCompGpuComputationGL.h"

#include "pxr/imaging/hdSt/bufferResource.h"
#include "pxr/imaging/hdSt/extCompGpuComputationBufferSource.h"
#include "pxr/imaging/hdSt/extCompGpuPrimvarBufferSource.h"
#include "pxr/imaging/hdSt/extComputation.h"
#include "pxr/imaging/hdSt/program.h"
#include "pxr/imaging/hdSt/resourceRegistry.h"
#include "pxr/imaging/hd/bufferArrayRange.h"
#include "pxr/imaging/hd/compExtCompInputSource.h"
#include "pxr/imaging/hd/extComputation.h"
#include "pxr/imaging/hd/extCompPrimvarBufferSource.h"
#include "pxr/imaging/hd/extCompCpuComputation.h"
#include "pxr/imaging/hd/sceneDelegate.h"
#include "pxr/imaging/hd/sceneExtCompInputSource.h"
#include "pxr/imaging/hd/vtBufferSource.h"

#include <limits>

using namespace boost;

PXR_NAMESPACE_OPEN_SCOPE


HdStExtCompGpuComputationGL::HdStExtCompGpuComputationGL(
        SdfPath const &id,
        HdStExtCompGpuComputationResourceSharedPtr const &resource,
        HdExtComputationPrimvarDescriptorVector const &compPrimvars,
        int dispatchCount,
        int elementCount)
 : HdStExtCompGpuComputation(
    id, resource, compPrimvars, dispatchCount, elementCount)
{
    
}

void
HdStExtCompGpuComputationGL::_Execute(
    HdStProgramSharedPtr const &computeProgram,
    std::vector<int32_t> const &_uniforms,
    HdBufferArrayRangeSharedPtr outputBar)
{
    HdSt_ResourceBinder const &binder = _resource->GetResourceBinder();

    // Prepare uniform buffer for GPU computation
    GLuint ubo = (GLuint)(uint64_t)computeProgram->GetGlobalUniformBuffer().GetId();
    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
    glBufferData(GL_UNIFORM_BUFFER,
            sizeof(int32_t) * _uniforms.size(),
            &_uniforms[0],
            GL_STATIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo);

    glDispatchCompute((GLuint)GetDispatchCount(), 1, 1);
    GLF_POST_PENDING_GL_ERRORS();

    // For now we make sure the computation finishes right away.
    // Figure out if sync or async is the way to go.
    // Assuming SSBOs for the output
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // Unbind.
    // XXX this should go away once we use a graphics abstraction
    // as that would take care of cleaning state.
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, 0);
    for (HdStBufferResourceNamedPair const & it: outputBar->GetResources()) {
        TfToken const &name = it.first;
        HdBufferResourceSharedPtr const &buffer = it.second;

        HdBinding const &binding = binder.GetBinding(name);
        // XXX we need a better way than this to pick
        // which buffers to bind on the output.
        // No guarantee that we are hiding buffers that
        // shouldn't be written to for example.
        if (binding.IsValid()) {
            binder.UnbindBuffer(name, buffer);
        }
    }
    for (HdBufferArrayRangeSharedPtr const & input: _resource->GetInputs()) {
        HdBufferArrayRangeSharedPtr const & inputBar =
            boost::static_pointer_cast<HdBufferArrayRange>(input);

        for (HdStBufferResourceNamedPair const & it:
                        inputBar->GetResources()) {
            TfToken const &name = it.first;
            HdBufferResourceSharedPtr const &buffer = it.second;

            HdBinding const &binding = binder.GetBinding(name);
            // These should all be valid as they are required inputs
            if (TF_VERIFY(binding.IsValid())) {
                binder.UnbindBuffer(name, buffer);
            }
        }
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
