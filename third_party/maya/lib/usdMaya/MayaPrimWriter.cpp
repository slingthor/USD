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
#include "usdMaya/MayaPrimWriter.h"

#include "usdMaya/adaptor.h"
#include "usdMaya/primWriterArgs.h"
#include "usdMaya/primWriterContext.h"
#include "usdMaya/translatorGprim.h"
#include "usdMaya/usdWriteJobCtx.h"
#include "usdMaya/util.h"
#include "usdMaya/writeUtil.h"

#include "pxr/base/gf/gamma.h"
#include "pxr/base/tf/staticTokens.h"
#include "pxr/usd/usdGeom/gprim.h"
#include "pxr/usd/usdGeom/imageable.h"
#include "pxr/usd/usd/stage.h"
#include "pxr/usd/usdGeom/scope.h"
#include "pxr/usd/usdGeom/gprim.h"
#include "pxr/usd/usd/inherits.h"

#include <maya/MFnDependencyNode.h>
#include <maya/MFnDagNode.h>
#include <maya/MObjectArray.h>
#include <maya/MColorArray.h>
#include <maya/MFloatArray.h>
#include <maya/MPlugArray.h>
#include <maya/MUintArray.h>
#include <maya/MColor.h>
#include <maya/MFnSet.h>

PXR_NAMESPACE_OPEN_SCOPE


PXRUSDMAYA_REGISTER_ADAPTOR_ATTRIBUTE_ALIAS(
        UsdGeomTokens->purpose, "USD_purpose");

TF_DEFINE_PRIVATE_TOKENS(
        _tokens, 

        (USD_inheritClassNames)
);

static bool
_IsAnimated(const PxrUsdMayaJobExportArgs& args, const MDagPath& dagPath)
{
    MObject obj = dagPath.node();
    if (!args.timeInterval.IsEmpty()) {
        return PxrUsdMayaUtil::isAnimated(obj);
    }
    return false;
}

MayaPrimWriter::MayaPrimWriter(const MDagPath& iDag,
                               const SdfPath& uPath,
                               usdWriteJobCtx& jobCtx) :
    _writeJobCtx(jobCtx),
    _dagPath(iDag),
    _usdPath(uPath),
    _baseDagToUsdPaths({{iDag, uPath}}),
    _exportVisibility(jobCtx.getArgs().exportVisibility),
    _hasAnimCurves(_IsAnimated(jobCtx.getArgs(), iDag))
{
    // Determine if shape is animated
    // note that we can't use hasTransform, because we need to test the original
    // dag, not the transform (if mergeTransformAndShape is on)!
    if (!GetDagPath().hasFn(MFn::kTransform)) { // if is a shape
        MObject obj = GetDagPath().node();
        if (!_GetExportArgs().timeInterval.IsEmpty()) {
            _isShapeAnimated = PxrUsdMayaUtil::isAnimated(obj);
        }
    }
}

MayaPrimWriter::~MayaPrimWriter()
{
}

bool
MayaPrimWriter::_IsMergedTransform() const
{
    return _writeJobCtx.IsMergedTransform(GetDagPath());
}

bool
MayaPrimWriter::_IsMergedShape() const
{
    MDagPath parentPath = GetDagPath();
    parentPath.pop();
    return parentPath.isValid() &&
            _writeJobCtx.IsMergedTransform(parentPath);
}

// In the future, we'd like to make this a plugin point.
static bool 
_GetClassNamesToWrite(
        MObject mObj,
        std::vector<std::string>* outClassNames)
{
    std::vector<std::string> ret;
    if (PxrUsdMayaWriteUtil::ReadMayaAttribute(
            MFnDependencyNode(mObj), 
            MString(_tokens->USD_inheritClassNames.GetText()),
            outClassNames)) {
        return true;
    }

    return false;
}

void
MayaPrimWriter::Write(const UsdTimeCode &usdTime)
{
    // We imagine that most prim writers will be writing Imageable prims
    // (all of the ones thus far do), but this might not be true in
    // generality, so it's OK to skip writing if this isn't Imageable.
    UsdGeomImageable primSchema(_usdPrim);
    if (!primSchema) {
        return;
    }

    MStatus status;
    MFnDependencyNode depFn(GetDagPath().node());

    // Visibility is unfortunately special when merging transforms and shapes in
    // that visibility is "pruning" and cannot be overriden by descendants.
    // Thus, we arbitrarily say that, when merging transforms and shapes, the
    // _shape_ writer always writes visibility.
    if (_exportVisibility && !_IsMergedTransform()) {
        bool isVisible  = true;   // if BOTH shape or xform is animated, then visible
        bool isAnimated = false;  // if either shape or xform is animated, then animated
        PxrUsdMayaUtil::getPlugValue(
                depFn, "visibility", &isVisible, &isAnimated);

        if (_IsMergedShape()) {
            MDagPath parentDagPath = GetDagPath();
            parentDagPath.pop();
            MFnDependencyNode parentDepFn(parentDagPath.node());

            bool parentVisible = true;
            bool parentAnimated = false;
            PxrUsdMayaUtil::getPlugValue(
                    parentDepFn, "visibility", &parentVisible, &parentAnimated);
            isVisible = isVisible && parentVisible;
            isAnimated = isAnimated || parentAnimated;
        }

        TfToken const &visibilityTok = (isVisible ? UsdGeomTokens->inherited : 
                                        UsdGeomTokens->invisible);
        if (usdTime.IsDefault() != isAnimated) {
            _SetAttribute(primSchema.CreateVisibilityAttr(VtValue(), true), 
                          visibilityTok,
                          usdTime);
        }
    }

    UsdPrim usdPrim = primSchema.GetPrim();
    if (usdTime.IsDefault()) {
        // There is no Gprim abstraction in this module, so process the few
        // Gprim attrs here.
        // Similar to the Imageable check above, we imagine that many, but not
        // all, prim writers will write Gprims, so it's OK to skip writing
        // if this isn't a Gprim.
        if (UsdGeomGprim gprim = UsdGeomGprim(usdPrim)) {
            PxrUsdMayaPrimWriterContext* unused = nullptr;
            PxrUsdMayaTranslatorGprim::Write(
                    GetDagPath().node(),
                    gprim,
                    unused);
        }

        // Only write class inherits once at default time.
        std::vector<std::string> classNames;
        if (_GetClassNamesToWrite(
                GetDagPath().node(),
                &classNames)) {
            PxrUsdMayaWriteUtil::WriteClassInherits(usdPrim, classNames);
        }

        // Write UsdGeomImageable typed schema attributes.
        // Currently only purpose, which is uniform, so only export at default
        // time.
        PxrUsdMayaWriteUtil::WriteSchemaAttributesToPrim<UsdGeomImageable>(
                GetDagPath().node(),
                usdPrim,
                {UsdGeomTokens->purpose},
                usdTime,
                &_valueWriter);

        // Write API schema attributes and strongly-typed metadata.
        // We currently only support these at default time.
        PxrUsdMayaWriteUtil::WriteMetadataToPrim(GetDagPath().node(), usdPrim);
        PxrUsdMayaWriteUtil::WriteAPISchemaAttributesToPrim(
                GetDagPath().node(), usdPrim, _GetSparseValueWriter());
    }

    // Write out user-tagged attributes, which are supported at default time and
    // at animated time-samples.
    PxrUsdMayaWriteUtil::WriteUserExportedAttributes(GetDagPath(), usdPrim, 
            usdTime, _GetSparseValueWriter());
}

bool
MayaPrimWriter::ExportsGprims() const
{
    return false;
}

bool
MayaPrimWriter::ShouldPruneChildren() const
{
    return false;
}

void
MayaPrimWriter::PostExport()
{ 
}

void
MayaPrimWriter::SetExportVisibility(bool exportVis)
{
    _exportVisibility = exportVis;
}

bool
MayaPrimWriter::GetExportVisibility() const
{
    return _exportVisibility;
}

const SdfPathVector&
MayaPrimWriter::GetModelPaths() const
{
    static const SdfPathVector empty;
    return empty;
}

const PxrUsdMayaUtil::MDagPathMap<SdfPath>&
MayaPrimWriter::GetDagToUsdPathMapping() const
{
    return _baseDagToUsdPaths;
}

const MDagPath&
MayaPrimWriter::GetDagPath() const
{
    return _dagPath;
}

const SdfPath&
MayaPrimWriter::GetUsdPath() const
{
    return _usdPath;
}

const UsdPrim&
MayaPrimWriter::GetUsdPrim() const
{
    return _usdPrim;
}

const UsdStageRefPtr&
MayaPrimWriter::GetUsdStage() const
{
    return _writeJobCtx.getUsdStage();
}

const PxrUsdMayaJobExportArgs&
MayaPrimWriter::_GetExportArgs() const
{
    return _writeJobCtx.getArgs();
}

UsdUtilsSparseValueWriter*
MayaPrimWriter::_GetSparseValueWriter()
{
    return &_valueWriter;
}

bool
MayaPrimWriter::_HasAnimCurves() const
{
    return _hasAnimCurves;
}

PXR_NAMESPACE_CLOSE_SCOPE

