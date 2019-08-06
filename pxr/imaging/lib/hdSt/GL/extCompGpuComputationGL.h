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
#ifndef HDST_EXT_COMP_GPU_COMPUTATION_GL_H
#define HDST_EXT_COMP_GPU_COMPUTATION_GL_H

#include "pxr/pxr.h"
#include "pxr/imaging/hdSt/api.h"
#include "pxr/imaging/hdSt/extCompGpuComputation.h"

PXR_NAMESPACE_OPEN_SCOPE

/// \class HdStExtCompGpuComputationGL
/// A Computation that represents a GPU implementation of a ExtComputation.
///
/// The computation implements the basic:
///    input HdBufferArrayRange -> processing -> output HdBufferArrayRange
/// model of HdComputations where processing happens in Execute during the
/// Execute phase of HdResourceRegistry::Commit.
///
/// The computation is performed in three stages by three companion classes:
/// 
/// 1. HdStExtCompGpuComputationBufferSource is responsible for loading
/// input HdBuffersources into the input HdBufferArrayRange during the Resolve
/// phase of the HdResourceRegistry::Commit processing.
///
/// 2. HdStExtCompGpuComputationResource holds the committed GPU resident
/// resources along with the compiled compute shading kernel to execute.
/// The values of the HdBufferArrayRanges for the inputs are stored in this
/// object. The resource can store heterogenous sources with differing number
/// of elements as may be required by computations.
///
/// 3. HdStExtCompGpuComputation executes the kernel using the committed GPU
/// resident resources and stores the results to the destination
/// HdBufferArrayRange given in Execute. The destination HdBufferArrayRange is
/// allocated by the owning HdRprim that registers the computation with the
/// HdResourceRegistry by calling HdResourceRegistry::AddComputation.
/// 
/// \see HdStExtCompGpuComputationBufferSource
/// \see HdStExtCompGpuComputationResource
/// \see HdRprim
/// \see HdComputation
/// \see HdResourceRegistry
/// \see HdExtComputation
/// \see HdBufferArrayRange
class HdStExtCompGpuComputationGL final : public HdStExtCompGpuComputation {
public:
    /// Constructs a new GPU ExtComputation computation.
    /// resource provides the set of input data and kernel to execute this
    /// computation.
    /// compPrimvars identifies the primvar data being computed
    ///
    /// dispatchCount specifies the number of kernel invocations to execute.
    /// elementCount specifies the number of elements to allocate for output.
    HdStExtCompGpuComputationGL(
            SdfPath const &id,
            HdStExtCompGpuComputationResourceSharedPtr const &resource,
            HdExtComputationPrimvarDescriptorVector const &compPrimvars,
            int dispatchCount,
            int elementCount);

    /// Creates a GPU computation implementing the given abstract computation.
    /// When created this allocates HdStExtCompGpuComputationResource to be
    /// shared with the HdStExtCompGpuComputationBufferSource. Nothing
    /// is assigned GPU resources unless the source is subsequently added to 
    /// the hdResourceRegistry and the registry is committed.
    /// 
    /// This delayed allocation allow Rprims to share computed primvar data and
    /// avoid duplicate allocations GPU resources for computation inputs and
    /// outputs.
    ///
    /// \param[in] sceneDelegate the delegate to pull scene inputs from.
    /// \param[in] sourceComp the abstract computation in the HdRenderIndex
    /// this instance actually implements.
    /// \param[in] compPrimvars identifies the primvar data being computed.
    /// \see HdExtComputation
    HDST_API
    static HdStExtCompGpuComputationSharedPtr
    CreateGpuComputationGL(
        HdSceneDelegate *sceneDelegate,
        HdExtComputation const *sourceComp,
        HdExtComputationPrimvarDescriptorVector const &compPrimvars);

    HDST_API
    virtual ~HdStExtCompGpuComputationGL() = default;

private:
    HDST_API
    virtual void _Execute(HdStProgramSharedPtr const &computeProgram,
                          std::vector<int32_t> const &_uniforms,
                          HdBufferArrayRangeSharedPtr outputBar) override;

    HdStExtCompGpuComputationGL()                                      = delete;
    HdStExtCompGpuComputationGL(const HdStExtCompGpuComputation &)     = delete;
    HdStExtCompGpuComputationGL &operator = (const HdStExtCompGpuComputationGL&)
                                                                       = delete;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDST_EXT_COMP_GPU_COMPUTATION_GL_H
