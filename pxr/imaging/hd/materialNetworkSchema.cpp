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
/* ** defs.py or the (*)Schema.template.cpp files to make changes.         ** */
/* ************************************************************************** */

#include "pxr/imaging/hd/materialNetworkSchema.h"
#include "pxr/imaging/hd/retainedDataSource.h"

#include "pxr/base/trace/trace.h"


PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PUBLIC_TOKENS(HdMaterialNetworkSchemaTokens,
    HDMATERIALNETWORK_SCHEMA_TOKENS);



HdContainerDataSourceHandle
HdMaterialNetworkSchema::GetNodes()
{
    return _GetTypedDataSource<HdContainerDataSource>(
        HdMaterialNetworkSchemaTokens->nodes);
}

HdContainerDataSourceHandle
HdMaterialNetworkSchema::GetTerminals()
{
    return _GetTypedDataSource<HdContainerDataSource>(
        HdMaterialNetworkSchemaTokens->terminals);
}

/*static*/
HdContainerDataSourceHandle
HdMaterialNetworkSchema::BuildRetained(
        const HdContainerDataSourceHandle &nodes,
        const HdContainerDataSourceHandle &terminals
)
{
    TfToken names[2];
    HdDataSourceBaseHandle values[2];

    size_t count = 0;
    if (nodes) {
        names[count] = HdMaterialNetworkSchemaTokens->nodes;
        values[count++] = nodes;
    }

    if (terminals) {
        names[count] = HdMaterialNetworkSchemaTokens->terminals;
        values[count++] = terminals;
    }

    return HdRetainedContainerDataSource::New(count, names, values);
}


HdMaterialNetworkSchema::Builder &
HdMaterialNetworkSchema::Builder::SetNodes(
    const HdContainerDataSourceHandle &nodes)
{
    _nodes = nodes;
    return *this;
}

HdMaterialNetworkSchema::Builder &
HdMaterialNetworkSchema::Builder::SetTerminals(
    const HdContainerDataSourceHandle &terminals)
{
    _terminals = terminals;
    return *this;
}

HdContainerDataSourceHandle
HdMaterialNetworkSchema::Builder::Build()
{
    return HdMaterialNetworkSchema::BuildRetained(
        _nodes,
        _terminals
    );
}


PXR_NAMESPACE_CLOSE_SCOPE