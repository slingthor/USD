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
#include "pxr/imaging/hd/points.h"
#include "pxr/imaging/hd/tokens.h"

PXR_NAMESPACE_OPEN_SCOPE


HdPoints::HdPoints(SdfPath const& id)
    : HdRprim(id)
{
    /*NOTHING*/
}

HdPoints::~HdPoints()
{
    /*NOTHING*/
}

/* virtual */
TfTokenVector const &
HdPoints::GetBuiltinPrimvarNames() const
{
    static const TfTokenVector primvarNames = {
        HdTokens->points,
        HdTokens->normals,
        HdTokens->widths
    };
    return primvarNames;
}

// static repr configuration
HdPoints::_PointsReprConfig HdPoints::_reprDescConfig;

/* static */
void
HdPoints::ConfigureRepr(TfToken const &reprName,
                        const HdPointsReprDesc &desc)
{
    HD_TRACE_FUNCTION();

    _reprDescConfig.AddOrUpdate(reprName, _PointsReprConfig::DescArray{{desc}});
}

/* static */
HdPoints::_PointsReprConfig::DescArray
HdPoints::_GetReprDesc(TfToken const &reprName)
{
    return _reprDescConfig.Find(reprName);
}

PXR_NAMESPACE_CLOSE_SCOPE

