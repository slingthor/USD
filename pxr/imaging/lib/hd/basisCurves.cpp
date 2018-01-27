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
#include "pxr/imaging/hd/basisCurves.h"
#include "pxr/base/tf/envSetting.h"

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_ENV_SETTING(HD_ENABLE_REFINED_CURVES, 0, 
                      "Force curves to always be refined.");

HdBasisCurves::HdBasisCurves(SdfPath const& id,
                 SdfPath const& instancerId)
    : HdRprim(id, instancerId)
{
    /*NOTHING*/
}

HdBasisCurves::~HdBasisCurves()
{
    /*NOTHING*/
}

// static repr configuration
HdBasisCurves::_BasisCurvesReprConfig HdBasisCurves::_reprDescConfig;

/* static */
bool
HdBasisCurves::IsEnabledForceRefinedCurves()
{
    return TfGetEnvSetting(HD_ENABLE_REFINED_CURVES) == 1;
}


/* static */
void
HdBasisCurves::ConfigureRepr(TfToken const &reprName,
                             HdBasisCurvesReprDesc desc)
{
    HD_TRACE_FUNCTION();

    if (IsEnabledForceRefinedCurves()) {
        desc.geomStyle = HdBasisCurvesGeomStyleRefined;
    }

    _reprDescConfig.Append(reprName, _BasisCurvesReprConfig::DescArray{{desc}});
}

/* static */
HdBasisCurves::_BasisCurvesReprConfig::DescArray
HdBasisCurves::_GetReprDesc(TfToken const &reprName)
{
    return _reprDescConfig.Find(reprName);
}

PXR_NAMESPACE_CLOSE_SCOPE

