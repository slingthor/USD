//
// Copyright 2023 Pixar
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
////////////////////////////////////////////////////////////////////////

/* ************************************************************************** */
/* ** This file is generated by a script.  Do not edit directly.  Edit     ** */
/* ** defs.py or the (*)Schema.template.h files to make changes.           ** */
/* ************************************************************************** */

#ifndef PXR_IMAGING_HD_INSTANCER_TOPOLOGY_SCHEMA_H
#define PXR_IMAGING_HD_INSTANCER_TOPOLOGY_SCHEMA_H

#include "pxr/imaging/hd/api.h"

#include "pxr/imaging/hd/schema.h" 

PXR_NAMESPACE_OPEN_SCOPE

//-----------------------------------------------------------------------------

#define HDINSTANCERTOPOLOGY_SCHEMA_TOKENS \
    (instancerTopology) \
    (prototypes) \
    (instanceIndices) \
    (mask) \
    (instanceLocations) \

TF_DECLARE_PUBLIC_TOKENS(HdInstancerTopologySchemaTokens, HD_API,
    HDINSTANCERTOPOLOGY_SCHEMA_TOKENS);

//-----------------------------------------------------------------------------

// Since the instancing schema is complicated:
//
// An instancer is a prim at a certain scenegraph location that causes other
// prims to be duplicated.  The instancer can also hold instance-varying data
// like constant primvars or material relationships.
//
// The important things an instancer has is:
// 1.) Instancer topology, describing how exactly the prims are duplicated;
// 2.) Instance-rate data, meaning data that varies per instance, such as
//     primvars or material bindings.
//
// If an instancer causes prims "/A" and "/B" to be duplicated, we encode that
// by setting prototypes = ["/A", "/B"].  Note that "/A" and "/B" can be
// subtrees, not direct gprims.  instanceIndices encodes both multiplicity
// and position in arrays of instance-rate data, per prototype path; if
// instanceIndices = { [0,2], [1] }, then we draw /A twice (with instance
// primvar indices 0 and 2); and /B once (with instance primvar index 1).
// Mask is an auxiliary parameter that can be used to deactivate certain
// instances; mask = [true, true, false] would disable the
// second copy of "/A".  An empty mask array is the same as all-true.
//
// Scenes generally specify instancing in one of two ways:
// 1.) Explicit instancing: prim /Instancer wants to draw its subtree at
//     an array of locations.  This is a data expansion form.
// 2.) Implicit instancing: prims /X and /Y are marked as being identical,
//     and scene load replaces them with a single prim and an instancer.
//     This is a data coalescing form.
//
// For implicit instancing, we want to know the original paths of /X and /Y,
// for doing things like resolving inheritance.  This is encoded in the
// "instanceLocations" path, while the prototype prims (e.g. /_Prototype/Cube,
// the deduplicated version of /X/Cube and /Y/Cube) is encoded in the
// "prototypes" path.
//
// For explicit instancing, the "instanceLocations" attribute is meaningless
// and should be left null.

class HdInstancerTopologySchema : public HdSchema
{
public:
    HdInstancerTopologySchema(HdContainerDataSourceHandle container)
    : HdSchema(container) {}

    //ACCESSORS

    HD_API
    HdPathArrayDataSourceHandle GetPrototypes();
    HD_API
    HdIntArrayVectorSchema GetInstanceIndices();
    HD_API
    HdBoolArrayDataSourceHandle GetMask();
    HD_API
    HdPathArrayDataSourceHandle GetInstanceLocations();

    // RETRIEVING AND CONSTRUCTING

    /// Builds a container data source which includes the provided child data
    /// sources. Parameters with nullptr values are excluded. This is a
    /// low-level interface. For cases in which it's desired to define
    /// the container with a sparse set of child fields, the Builder class
    /// is often more convenient and readable.
    HD_API
    static HdContainerDataSourceHandle
    BuildRetained(
        const HdPathArrayDataSourceHandle &prototypes,
        const HdVectorDataSourceHandle &instanceIndices,
        const HdBoolArrayDataSourceHandle &mask,
        const HdPathArrayDataSourceHandle &instanceLocations
    );

    /// \class HdInstancerTopologySchema::Builder
    /// 
    /// Utility class for setting sparse sets of child data source fields to be
    /// filled as arguments into BuildRetained. Because all setter methods
    /// return a reference to the instance, this can be used in the "builder
    /// pattern" form.
    class Builder
    {
    public:
        HD_API
        Builder &SetPrototypes(
            const HdPathArrayDataSourceHandle &prototypes);
        HD_API
        Builder &SetInstanceIndices(
            const HdVectorDataSourceHandle &instanceIndices);
        HD_API
        Builder &SetMask(
            const HdBoolArrayDataSourceHandle &mask);
        HD_API
        Builder &SetInstanceLocations(
            const HdPathArrayDataSourceHandle &instanceLocations);

        /// Returns a container data source containing the members set thus far.
        HD_API
        HdContainerDataSourceHandle Build();

    private:
        HdPathArrayDataSourceHandle _prototypes;
        HdVectorDataSourceHandle _instanceIndices;
        HdBoolArrayDataSourceHandle _mask;
        HdPathArrayDataSourceHandle _instanceLocations;
    };
    // HELPERS
    HD_API
    VtArray<int> ComputeInstanceIndicesForProto(SdfPath const &path);


    /// Retrieves a container data source with the schema's default name token
    /// "instancerTopology" from the parent container and constructs a
    /// HdInstancerTopologySchema instance.
    /// Because the requested container data source may not exist, the result
    /// should be checked with IsDefined() or a bool comparison before use.
    HD_API
    static HdInstancerTopologySchema GetFromParent(
        const HdContainerDataSourceHandle &fromParentContainer);

    /// Returns a token where the container representing this schema is found in
    /// a container by default.
    HD_API
    static const TfToken &GetSchemaToken();

    /// Returns an HdDataSourceLocator (relative to the prim-level data source)
    /// where the container representing this schema is found by default.
    HD_API
    static const HdDataSourceLocator &GetDefaultLocator();

};

PXR_NAMESPACE_CLOSE_SCOPE

#endif