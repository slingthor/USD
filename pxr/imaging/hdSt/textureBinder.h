//
// Copyright 2020 Pixar
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
#ifndef PXR_IMAGING_HD_ST_TEXTURE_BINDER_H
#define PXR_IMAGING_HD_ST_TEXTURE_BINDER_H

#include "pxr/pxr.h"
#include "pxr/imaging/hdSt/api.h"
#include "pxr/imaging/hdSt/shaderCode.h"

PXR_NAMESPACE_OPEN_SCOPE

using HdBufferSpecVector = std::vector<struct HdBufferSpec>;
class HdStProgram;

/// \class HdSt_TextureBinder
///
/// A class helping HdStShaderCode with binding textures.
///
/// This class helps binding GL texture names or populating the shader
/// bar with texture sampler handles if bindless textures are used. It
/// also includes writing texture metadata such as the sampling
/// transform to the shader bar.
///
class HdSt_TextureBinder {
public:
    using NamedTextureHandle =
        HdStShaderCode::NamedTextureHandle;
    using NamedTextureHandleVector =
        HdStShaderCode::NamedTextureHandleVector;

    /// Add buffer specs necessary for the textures (e.g., for
    /// bindless texture sampler handles or sampling transform).
    ///
    /// Specify whether to use the texture by binding it or by
    /// using bindless handles with useBindlessHandles.
    ///
    static void
    GetBufferSpecs(
        const NamedTextureHandleVector &textures,
        bool useBindlessHandles,
        HdBufferSpecVector * specs);

    /// Compute buffer sources for shader bar.
    ///
    /// This works in conjunction with GetBufferSpecs, but unlike
    /// GetBufferSpecs is extracting information from the texture
    /// handles and thus can only be called after the textures have
    /// been committed in
    /// HdStShaderCode::AddResourcesFromTextures().
    ///
    /// Specify whether to use the texture by binding it or by
    /// using bindless handles with useBindlessHandles.
    ///
    static void
    ComputeBufferSources(
        const NamedTextureHandleVector &textures,
        bool useBindlessHandles,
        HdBufferSourceSharedPtrVector * sources);

    /// Bind textures.
    ///
    /// Specify whether to use the texture by binding it or by
    /// using bindless handles with useBindlessHandles.
    ///
    static void
    BindResources(
        HdSt_ResourceBinder const &binder,
        HdStProgram const &program,
        bool useBindlessHandles,
        const NamedTextureHandleVector &textures);

    /// Unbind textures.
    ///
    /// Specify whether to use the texture by binding it or by
    /// using bindless handles with useBindlessHandles.
    ///
    static void
    UnbindResources(
        HdSt_ResourceBinder const &binder,
        HdStProgram const &program,
        bool useBindlessHandles,
        const NamedTextureHandleVector &textures);
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif  // PXR_IMAGING_HD_ST_TEXTURE_BINDER_H
