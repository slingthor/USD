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

#include "pxr/imaging/hd/systemSchema.h"
#include "pxr/imaging/hd/retainedDataSource.h"

#include "pxr/base/trace/trace.h"

#include "pxr/imaging/hd/overlayContainerDataSource.h"
#include "pxr/imaging/hd/sceneIndex.h"


PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PUBLIC_TOKENS(HdSystemSchemaTokens,
    HDSYSTEM_SCHEMA_TOKENS);

// static
HdDataSourceBaseHandle
HdSystemSchema::GetFromPath(
    HdSceneIndexBaseRefPtr const& inputScene,
    SdfPath const& fromPath,
    TfToken const& key,
    SdfPath* foundAtPath)
{
    if (!inputScene) {
        return nullptr;
    }

    const HdDataSourceLocator locator(HdSystemSchemaTokens->system, key);

    for (SdfPath currPath = fromPath;
            !currPath.IsEmpty();
            currPath = currPath.GetParentPath()) {
        const HdSceneIndexPrim currPrim = inputScene->GetPrim(currPath);
        if (HdDataSourceBaseHandle dataSource
            = HdContainerDataSource::Get(currPrim.dataSource, locator)) {
            if (foundAtPath) {
                *foundAtPath = currPath;
            }
            return dataSource;
        }
    }

    return nullptr;
}

// static
HdContainerDataSourceHandle
HdSystemSchema::Compose(
    HdSceneIndexBaseRefPtr const& inputScene,
    SdfPath const& fromPath,
    SdfPath* foundAtPath)
{
    if (!inputScene) {
        return nullptr;
    }

    TfSmallVector<HdContainerDataSourceHandle, 4> systemContainers;

    SdfPath lastFound;
    for (SdfPath currPath = fromPath;
            !currPath.IsEmpty();
            currPath = currPath.GetParentPath()) {
        const HdSceneIndexPrim currPrim = inputScene->GetPrim(currPath);
        if (HdContainerDataSourceHandle systemContainer
            = HdSystemSchema::GetFromParent(currPrim.dataSource)
                  .GetContainer()) {
            systemContainers.push_back(systemContainer);
            lastFound = currPath;
        }
    }

    if (systemContainers.empty()) {
        return nullptr;
    }

    if (foundAtPath) {
        *foundAtPath = lastFound;
    }

    return HdOverlayContainerDataSource::New(
        systemContainers.size(), systemContainers.data());
}

// static
HdContainerDataSourceHandle
HdSystemSchema::ComposeAsPrimDataSource(
    HdSceneIndexBaseRefPtr const& inputScene,
    SdfPath const& fromPath,
    SdfPath* foundAtPath)
{
    if (HdContainerDataSourceHandle systemDs
        = Compose(inputScene, fromPath, foundAtPath)) {
        return HdRetainedContainerDataSource::New(
            HdSystemSchemaTokens->system, systemDs);
    }
    return nullptr;
}

/*static*/
HdSystemSchema
HdSystemSchema::GetFromParent(
        const HdContainerDataSourceHandle &fromParentContainer)
{
    return HdSystemSchema(
        fromParentContainer
        ? HdContainerDataSource::Cast(fromParentContainer->Get(
                HdSystemSchemaTokens->system))
        : nullptr);
}

/*static*/
const TfToken &
HdSystemSchema::GetSchemaToken()
{
    return HdSystemSchemaTokens->system;
} 
/*static*/
const HdDataSourceLocator &
HdSystemSchema::GetDefaultLocator()
{
    static const HdDataSourceLocator locator(
        HdSystemSchemaTokens->system
    );
    return locator;
} 
PXR_NAMESPACE_CLOSE_SCOPE