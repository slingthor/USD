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
#include "pxr/pxr.h"
#include "usdMaya/primWriterContext.h"

PXR_NAMESPACE_OPEN_SCOPE


PxrUsdMayaPrimWriterContext::PxrUsdMayaPrimWriterContext(
        const UsdTimeCode& timeCode,
        const SdfPath& authorPath, 
        const UsdStageRefPtr& stage) : 
    _timeCode(timeCode),
    _authorPath(authorPath),
    _stage(stage),
    _exportsGprims(false),
    _pruneChildren(false)
{
}

const UsdTimeCode&
PxrUsdMayaPrimWriterContext::GetTimeCode() const 
{
    return _timeCode;
}

const SdfPath&
PxrUsdMayaPrimWriterContext::GetAuthorPath() const
{
    return _authorPath;
}

UsdStageRefPtr
PxrUsdMayaPrimWriterContext::GetUsdStage() const 
{
    return _stage;
}

bool 
PxrUsdMayaPrimWriterContext::GetExportsGprims() const
{
    return _exportsGprims;
}

void
PxrUsdMayaPrimWriterContext::SetExportsGprims(bool exportsGprims)
{
    _exportsGprims = exportsGprims;
}

void
PxrUsdMayaPrimWriterContext::SetPruneChildren(bool pruneChildren)
{
    _pruneChildren = pruneChildren;
}

bool
PxrUsdMayaPrimWriterContext::GetPruneChildren() const
{
    return _pruneChildren;
}

const SdfPathVector&
PxrUsdMayaPrimWriterContext::GetModelPaths() const
{
    return _modelPaths;
}

void
PxrUsdMayaPrimWriterContext::SetModelPaths(
    const SdfPathVector& modelPaths)
{
    _modelPaths = modelPaths;
}

void
PxrUsdMayaPrimWriterContext::SetModelPaths(
    SdfPathVector&& modelPaths)
{
    _modelPaths = std::move(modelPaths);
}

PXR_NAMESPACE_CLOSE_SCOPE

