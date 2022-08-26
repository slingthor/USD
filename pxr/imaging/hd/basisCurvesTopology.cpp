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
#include "pxr/imaging/hd/basisCurvesTopology.h"
#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/tokens.h"

#include "pxr/base/arch/hash.h"

#include <algorithm>

PXR_NAMESPACE_OPEN_SCOPE

namespace {

static size_t
_ComputeNumPoints(
    VtIntArray const &curveVertexCounts,
    VtIntArray const &indices)
{
    // Make absolutely sure the iterator is constant
    // (so we don't detach the array while multi-threaded)
    if (indices.empty()) {
        size_t sum = 0;
        return std::accumulate(
            curveVertexCounts.cbegin(), curveVertexCounts.cend(), size_t {0} );
        return sum;
    } else {
        return 1 + *std::max_element(indices.cbegin(), indices.cend());
    }
}

}; // anon namespace

HdBasisCurvesTopology::HdBasisCurvesTopology()
  : HdTopology()
  , _curveType(HdTokens->linear)
  , _curveBasis(TfToken())
  , _curveWrap(HdTokens->nonperiodic)
  , _curveVertexCounts()
  , _curveIndices()
  , _invisiblePoints()
  , _invisibleCurves()
  , _numPoints()
{
    HD_PERF_COUNTER_INCR(HdPerfTokens->basisCurvesTopology);
}

HdBasisCurvesTopology::HdBasisCurvesTopology(const HdBasisCurvesTopology& src)
  : HdTopology(src)
  , _curveType(src._curveType)
  , _curveBasis(src._curveBasis)
  , _curveWrap(src._curveWrap)
  , _curveVertexCounts(src._curveVertexCounts)
  , _curveIndices(src._curveIndices)
  , _invisiblePoints(src._invisiblePoints)
  , _invisibleCurves(src._invisibleCurves)
{
    HD_PERF_COUNTER_INCR(HdPerfTokens->basisCurvesTopology);
    _numPoints = _ComputeNumPoints(_curveVertexCounts, _curveIndices);
}

HdBasisCurvesTopology::HdBasisCurvesTopology(const TfToken &curveType,
                                             const TfToken &curveBasis,
                                             const TfToken &curveWrap,
                                             const VtIntArray &curveVertexCounts,
                                             const VtIntArray &curveIndices)
  : HdTopology()
  , _curveType(curveType)
  , _curveBasis(curveBasis)
  , _curveWrap(curveWrap)
  , _curveVertexCounts(curveVertexCounts)
  , _curveIndices(curveIndices)
  , _invisiblePoints()
  , _invisibleCurves()
{
    if (_curveType != HdTokens->linear && _curveType != HdTokens->cubic){
        TF_WARN("Curve type must be 'linear' or 'cubic'.  Got: '%s'", _curveType.GetText());
        _curveType = HdTokens->linear;
        _curveBasis = TfToken();
    }
    if (curveBasis == HdTokens->linear && curveType == HdTokens->cubic){
        TF_WARN("Basis 'linear' passed in to 'cubic' curveType.  Converting 'curveType' to 'linear'.");
        _curveType = HdTokens->linear;
        _curveBasis = TfToken();
    }
    HD_PERF_COUNTER_INCR(HdPerfTokens->basisCurvesTopology);
    _numPoints = _ComputeNumPoints(_curveVertexCounts, _curveIndices);
}

HdBasisCurvesTopology::~HdBasisCurvesTopology()
{
    HD_PERF_COUNTER_DECR(HdPerfTokens->basisCurvesTopology);
}

bool
HdBasisCurvesTopology::operator==(HdBasisCurvesTopology const &other) const
{
    HD_TRACE_FUNCTION();

    // no need to compare _adajency and _quadInfo
    return (_curveType == other._curveType                  &&
            _curveBasis == other._curveBasis                &&
            _curveWrap == other._curveWrap                  &&
            _curveVertexCounts == other._curveVertexCounts  &&
            _curveIndices == other._curveIndices            &&
            _invisiblePoints == other._invisiblePoints      &&
            _invisibleCurves == other._invisibleCurves);
}

bool
HdBasisCurvesTopology::operator!=(HdBasisCurvesTopology const &other) const
{
    return !(*this == other);
}

HdTopology::ID
HdBasisCurvesTopology::ComputeHash() const
{
    HD_TRACE_FUNCTION();

    HdTopology::ID hash = 0;
    hash = ArchHash64((const char*)&_curveBasis, sizeof(TfToken), hash);
    hash = ArchHash64((const char*)&_curveType, sizeof(TfToken), hash);
    hash = ArchHash64((const char*)&_curveWrap, sizeof(TfToken), hash);
    hash = ArchHash64((const char*)_curveVertexCounts.cdata(),
                      _curveVertexCounts.size() * sizeof(int), hash);
    hash = ArchHash64((const char*)_curveIndices.cdata(),
                      _curveIndices.size() * sizeof(int), hash);
    
    // Note: We don't hash topological visibility, because it is treated as a
    // per-prim opinion, and hence, shouldn't break topology sharing.
    return hash;
}

std::ostream&
operator << (std::ostream &out, HdBasisCurvesTopology const &topo)
{
    out << "(" << topo.GetCurveBasis().GetString() << ", " <<
        topo.GetCurveType().GetString() << ", " <<
        topo.GetCurveWrap().GetString() << ", (" <<
        topo.GetCurveVertexCounts() << "), (" <<
        topo.GetCurveIndices() << "), (" <<
        topo.GetInvisiblePoints() << "), (" <<
        topo.GetInvisibleCurves() << "))";
    return out;
}

size_t
HdBasisCurvesTopology::CalculateNeededNumberOfControlPoints() const
{
    // This in computed on construction and accounts for authored indices.
    return _numPoints;
}

size_t
HdBasisCurvesTopology::CalculateNeededNumberOfVaryingControlPoints() const
{
    if (GetCurveType() == HdTokens->linear){
        // For linear curves, varying and vertex interpolation is identical.
        return CalculateNeededNumberOfControlPoints();
    }

    size_t numVarying = 0;
    int numSegs = 0, vStep = 0;
    bool wrap = GetCurveWrap() == HdTokens->periodic;
    
    if(GetCurveBasis() == HdTokens->bezier) {
        vStep = 3;
    }
    else {
        vStep = 1;
    }

    // Make absolutely sure the iterator is constant 
    // (so we don't detach the array while multi-threaded)
    for (VtIntArray::const_iterator itCounts = _curveVertexCounts.cbegin();
            itCounts != _curveVertexCounts.cend(); ++itCounts) {
        
        // Partial handling for the case of potentially incorrect vertex counts.
        // We don't validate the vertex count for each curve (which differs
        // based on the basis and wrap mode) since a renderer
        // may choose to handle underspecified vertices via e.g., repetition.
        if (*itCounts < 1) {
            continue;
        }

        // The number of segments is different if we have periodic vs 
        // non-periodic curves, check basisCurvesComputations.cpp for a diagram.
        if (wrap) {
            // For bezier curves, if the authored vertex count is less than the
            // minimum, treat it as 1 segment.
            numSegs = std::max<int>(*itCounts / vStep, 1);
        } else {
            numSegs = (std::max<int>(*itCounts - 4, 0) / vStep) + 1;
        }
        numVarying += numSegs + 1;
    }

    return numVarying;
}

PXR_NAMESPACE_CLOSE_SCOPE

