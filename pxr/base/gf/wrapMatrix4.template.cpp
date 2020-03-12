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
// This file is generated by a script.  Do not edit directly.  Edit the
// wrapMatrix4.template.cpp file to make changes.

#ifndef BOOST_PYTHON_MAX_ARITY
#define BOOST_PYTHON_MAX_ARITY 20
#endif

{% extends "wrapMatrix.template.cpp" %}

{% block customIncludes %}
#include "pxr/base/gf/matrix3{{ SCL[0] }}.h"
#include "pxr/base/gf/quat{{ SCL[0] }}.h"
#include "pxr/base/gf/rotation.h"
{% endblock customIncludes %}

{% block customFunctions %}
static tuple FactorWithEpsilon({{ MAT }} &self, double eps) {
    {{ MAT }} r, u, p;
    GfVec3{{ SCL[0] }} s, t;
    bool result = self.Factor(&r, &s, &u, &t, &p, eps);
    return boost::python::make_tuple(result, r, s, u, t, p);
}    

static tuple Factor({{ MAT }} &self) {
    {{ MAT }} r, u, p;
    GfVec3{{ SCL[0] }} s, t;
    bool result = self.Factor(&r, &s, &u, &t, &p);
    return boost::python::make_tuple(result, r, s, u, t, p);
}

static {{ MAT }} RemoveScaleShearWrapper( const {{ MAT }} &self ) {
    return self.RemoveScaleShear();
}
{% endblock customFunctions %}

{% block customInit %}
        .def(init< const vector<float>&,
                   const vector<float>&,
                   const vector<float>&,
                   const vector<float>& >())
        .def(init< const vector<double>&,
                   const vector<double>&,
                   const vector<double>&,
                   const vector<double>& >())
        .def(init< const GfMatrix3{{ SCL[0] }} &, const GfVec3{{ SCL[0] }} >())
        .def(init< const GfRotation &, const GfVec3{{ SCL[0] }} >())
{% endblock customInit %}

{% block customDefs %}
        .def("GetRow3", &This::GetRow3)
        .def("SetRow3", &This::SetRow3)
        .def("GetDeterminant3", &This::GetDeterminant3)
        .def("HasOrthogonalRows3", &This::HasOrthogonalRows3)

        .def("GetHandedness", &This::GetHandedness)
        .def("IsLeftHanded", &This::IsLeftHanded)
        .def("IsRightHanded", &This::IsRightHanded)

        .def("Orthonormalize", &This::Orthonormalize,
             (arg("issueWarning") = true))
        .def("GetOrthonormalized", &This::GetOrthonormalized,
             (arg("issueWarning") = true))
{% endblock customDefs %}

{% block customXformDefs %}
        .def("SetTransform",
	     (This & (This::*)( const GfRotation &,
				const GfVec3{{ SCL[0] }} & ))&This::SetTransform,
	     return_self<>())	
        .def("SetTransform",
	     (This & (This::*)( const GfMatrix3{{ SCL[0] }}&,
				const GfVec3{{ SCL[0] }} & ))&This::SetTransform,
	     return_self<>())

        .def("SetScale", (This & (This::*)( const GfVec3{{ SCL[0] }}& ))&This::SetScale,
	     return_self<>())

        .def("SetTranslate", &This::SetTranslate, return_self<>())
        .def("SetTranslateOnly", &This::SetTranslateOnly, return_self<>())

        .def("SetRotate",
	     (This & (This::*)( const GfQuat{{ SCL[0] }} & )) &This::SetRotate,
	     return_self<>())
        .def("SetRotateOnly",
	     (This & (This::*)( const GfQuat{{ SCL[0] }} & )) &This::SetRotateOnly,
	     return_self<>())

        .def("SetRotate",
	     (This & (This::*)( const GfRotation & )) &This::SetRotate,
	     return_self<>())
        .def("SetRotateOnly",
	     (This & (This::*)( const GfRotation & )) &This::SetRotateOnly,
	     return_self<>())

        .def("SetRotate",
	     (This & (This::*)( const GfMatrix3{{ SCL[0] }}& )) &This::SetRotate,
	     return_self<>())
        .def("SetRotateOnly",
	     (This & (This::*)( const GfMatrix3{{ SCL[0] }}& )) &This::SetRotateOnly,
	     return_self<>())

        .def("SetLookAt", (This & (This::*)( const GfVec3{{ SCL[0] }} &,
                                             const GfVec3{{ SCL[0] }} &,
                                             const GfVec3{{ SCL[0] }} & ))&This::SetLookAt,
	     return_self<>())

        .def("SetLookAt",
             (This & (This::*)( const GfVec3{{ SCL[0] }} &,
                                const GfRotation & ))&This::SetLookAt,
             return_self<>())

        .def("ExtractTranslation", &This::ExtractTranslation)
        .def("ExtractRotation", &This::ExtractRotation)
        .def("ExtractRotationMatrix", &This::ExtractRotationMatrix)
        .def("ExtractRotationQuat", &This::ExtractRotationQuat)

        .def("Factor", FactorWithEpsilon)
        .def("Factor", Factor)
        .def("RemoveScaleShear", RemoveScaleShearWrapper)
        
        .def("Transform",
	     (GfVec3f (This::*)(const GfVec3f &) const)&This::Transform)
        .def("Transform",
	     (GfVec3d (This::*)(const GfVec3d &) const)&This::Transform)

        .def("TransformDir",
	     (GfVec3f (This::*)(const GfVec3f &) const)&This::TransformDir)
        .def("TransformDir",
	     (GfVec3d (This::*)(const GfVec3d &) const)&This::TransformDir)

        .def("TransformAffine",
	     (GfVec3f (This::*)(const GfVec3f &) const)&This::TransformAffine)
        .def("TransformAffine",
	     (GfVec3d (This::*)(const GfVec3d &) const)&This::TransformAffine)
        .def("SetScale", (This & (This::*)( {{ SCL }} ))&This::SetScale,
	     return_self<>())

{% endblock customXformDefs %}
