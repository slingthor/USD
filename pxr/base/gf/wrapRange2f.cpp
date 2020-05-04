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
////////////////////////////////////////////////////////////////////////
// This file is generated by a script.  Do not edit directly.  Edit the
// wrapRange.template.cpp file to make changes.

#include "pxr/pxr.h"
#include "pxr/base/gf/range2f.h"
#include "pxr/base/gf/range2d.h"

#include "pxr/base/tf/pyUtils.h"
#include "pxr/base/tf/wrapTypeHelpers.h"
#include "pxr/base/tf/pyContainerConversions.h"

#include <boost/python/class.hpp>
#include <boost/python/copy_const_reference.hpp>
#include <boost/python/operators.hpp>
#include <boost/python/return_arg.hpp>

#include <string>

using namespace boost::python;

using std::string;

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

static const int _dimension = 2;

static string _Repr(GfRange2f const &self) {
    return TF_PY_REPR_PREFIX + "Range2f(" +
        TfPyRepr(self.GetMin()) + ", " + TfPyRepr(self.GetMax()) + ")";
}

#if PY_MAJOR_VERSION == 2
static GfRange2f __truediv__(const GfRange2f &self, double value)
{
    return self / value;
}

static GfRange2f __itruediv__(GfRange2f &self, double value)
{
    return self /= value;
}
#endif

static size_t __hash__(GfRange2f const &r) { return hash_value(r); }

} // anonymous namespace 

void wrapRange2f()
{    
    object getMin = make_function(&GfRange2f::GetMin,
                                  return_value_policy<return_by_value>());

    object getMax = make_function(&GfRange2f::GetMax,
                                  return_value_policy<return_by_value>());

    class_<GfRange2f>("Range2f", init<>())
        .def(init<GfRange2f>())
        .def(init<const GfVec2f &, const GfVec2f &>())
        
        .def(TfTypePythonClass())

        .def_readonly("dimension", _dimension)
        
        .add_property("min", getMin, &GfRange2f::SetMin)
        .add_property("max", getMax, &GfRange2f::SetMax)

        .def("GetMin", getMin)
        .def("GetMax", getMax)

        .def("GetSize", &GfRange2f::GetSize)
        .def("GetMidpoint", &GfRange2f::GetMidpoint)
    
        .def("SetMin", &GfRange2f::SetMin)
        .def("SetMax", &GfRange2f::SetMax)
    
        .def("IsEmpty", &GfRange2f::IsEmpty)
    
        .def("SetEmpty", &GfRange2f::SetEmpty)

        .def("Contains", (bool (GfRange2f::*)(const GfVec2f &) const)
             &GfRange2f::Contains)
        .def("Contains", (bool (GfRange2f::*)(const GfRange2f &) const)
             &GfRange2f::Contains)
    
        .def("GetUnion", &GfRange2f::GetUnion)
        .staticmethod("GetUnion")
    
        .def("UnionWith", (const GfRange2f & (GfRange2f::*)(const GfVec2f &))
             &GfRange2f::UnionWith, return_self<>())
        .def("UnionWith", (const GfRange2f & (GfRange2f::*)(const GfRange2f &))
             &GfRange2f::UnionWith, return_self<>())
    
        .def("GetIntersection", &GfRange2f::GetIntersection)
        .staticmethod("GetIntersection")
    
        .def("IntersectWith", (const GfRange2f & (GfRange2f::*)(const GfRange2f &))
             &GfRange2f::IntersectWith, return_self<>())
    
        .def("GetDistanceSquared", &GfRange2f::GetDistanceSquared)
    
        .def(str(self))
        .def(self += self)
        .def(self -= self)
        .def(self *= double())
        .def(self /= double())
        .def(self + self)
        .def(self - self)
        .def(double() * self)
        .def(self * double())
        .def(self / double())
        .def(self == GfRange2d())
        .def(self != GfRange2d())
        .def(self == self)
        .def(self != self)
    
#if PY_MAJOR_VERSION == 2
        // Needed only to support "from __future__ import division" in
        // python 2. In python 3 builds boost::python adds this for us.
        .def("__truediv__", __truediv__ )
        .def("__itruediv__", __itruediv__ )
#endif

        .def("__repr__", _Repr)
        .def("__hash__", __hash__)

        .def("GetCorner", &GfRange2f::GetCorner)
        .def("GetQuadrant", &GfRange2f::GetQuadrant)
        .def_readonly("unitSquare", &GfRange2f::UnitSquare)
        
        ;
    to_python_converter<std::vector<GfRange2f>,
        TfPySequenceToPython<std::vector<GfRange2f> > >();
    
}
