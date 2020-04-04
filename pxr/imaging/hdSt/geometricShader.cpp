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
#include "pxr/imaging/glf/glew.h"

#include "pxr/imaging/hdSt/geometricShader.h"

#include "pxr/imaging/hdSt/debugCodes.h"
#include "pxr/imaging/hdSt/shaderKey.h"
#include "pxr/imaging/hdSt/resourceBinder.h"

#include "pxr/imaging/hd/binding.h"
#include "pxr/imaging/hd/engine.h"
#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/tokens.h"

#include "pxr/imaging/hio/glslfx.h"

#include <boost/functional/hash.hpp>

#include <iostream>
#include <string>

PXR_NAMESPACE_OPEN_SCOPE


HdSt_GeometricShader::HdSt_GeometricShader(std::string const &glslfxString,
                                       PrimitiveType primType,
                                       HdCullStyle cullStyle,
                                       HdPolygonMode polygonMode,
                                       bool cullingPass,
                                       SdfPath const &debugId,
                                       float lineWidth)
    : HdStShaderCode()
    , _primType(primType)
    , _cullStyle(cullStyle)
    , _polygonMode(polygonMode)
    , _lineWidth(lineWidth)
    , _frustumCullingPass(cullingPass)
    , _hash(0)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    // XXX
    // we will likely move this (the constructor or the entire class) into
    // the base class (HdStShaderCode) at the end of refactoring, to be able to
    // use same machinery other than geometric shaders.

    if (TfDebug::IsEnabled(HDST_DUMP_GLSLFX_CONFIG)) {
        std::cout << debugId << "\n"
                  << glslfxString << "\n";
    }

    std::stringstream ss(glslfxString);
    _glslfx.reset(new HioGlslfx(ss));
    boost::hash_combine(_hash, _glslfx->GetHash());
    boost::hash_combine(_hash, cullingPass);
    boost::hash_combine(_hash, primType);
    //
    // note: Don't include cullStyle and polygonMode into the hash.
    //      They are independent from the program.
    //
}

HdSt_GeometricShader::~HdSt_GeometricShader()
{
    // nothing
}

/* virtual */
HdStShaderCode::ID
HdSt_GeometricShader::ComputeHash() const
{
    return _hash;
}

/* virtual */
std::string
HdSt_GeometricShader::GetSource(TfToken const &shaderStageKey) const
{
    return _glslfx->GetSource(shaderStageKey);
}

void
HdSt_GeometricShader::BindResources(HdStProgram const &program,
                                    HdSt_ResourceBinder const &binder,
                                    HdRenderPassState const &state)
{
    if (_cullStyle != HdCullStyleDontCare) {
        unsigned int cullStyle = _cullStyle;
        binder.BindUniformui(HdShaderTokens->cullStyle, 1, &cullStyle);
    } else {
        // don't care -- use renderPass's fallback
    }
#if defined(PXR_OPENGL_SUPPORT_ENABLED)
//    if (GetPrimitiveMode() == GL_PATCHES) {
//        glPatchParameteri(GL_PATCH_VERTICES, GetPrimitiveIndexSize());
//    }
//
//    if (_polygonMode == HdPolygonModeLine) {
//        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
//        if (_lineWidth > 0) {
//            glLineWidth(_lineWidth);
//        }
//    }
#else
//    TF_FATAL_CODING_ERROR("Not Implemented!"); //MTL_FIXME
#endif
}

void
HdSt_GeometricShader::UnbindResources(HdStProgram const &program,
                                      HdSt_ResourceBinder const &binder,
                                      HdRenderPassState const &state)
{
#if defined(PXR_OPENGL_SUPPORT_ENABLED)
//    if (_polygonMode == HdPolygonModeLine) {
//        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
//    }
#else
//    TF_FATAL_CODING_ERROR("Not Implemented!"); //MTL_FIXME
#endif
}

/*virtual*/
void
HdSt_GeometricShader::AddBindings(HdBindingRequestVector *customBindings)
{
    // no-op
}

GLenum
HdSt_GeometricShader::GetPrimitiveMode() const 
{
    GLenum primMode = GL_POINTS;

    switch (_primType)
    {
        case PrimitiveType::PRIM_POINTS:
            primMode = GL_POINTS;
            break;
        case PrimitiveType::PRIM_BASIS_CURVES_LINES:
            primMode = GL_LINES;
            break;
        case PrimitiveType::PRIM_MESH_COARSE_TRIANGLES:
        case PrimitiveType::PRIM_MESH_REFINED_TRIANGLES:
        case PrimitiveType::PRIM_VOLUME:
            primMode = GL_TRIANGLES;
            break;
        case PrimitiveType::PRIM_MESH_COARSE_QUADS:
        case PrimitiveType::PRIM_MESH_REFINED_QUADS:
            primMode = GL_LINES_ADJACENCY;
            break;
        case PrimitiveType::PRIM_BASIS_CURVES_CUBIC_PATCHES:
        case PrimitiveType::PRIM_BASIS_CURVES_LINEAR_PATCHES:
        case PrimitiveType::PRIM_MESH_BSPLINE:
        case PrimitiveType::PRIM_MESH_BOXSPLINETRIANGLE:
            primMode = GL_PATCHES;
            break;
    }

    return primMode;
}

int
HdSt_GeometricShader::GetPrimitiveIndexSize() const
{
    int primIndexSize = 1;

    switch (_primType)
    {
        case PrimitiveType::PRIM_POINTS:
            primIndexSize = 1;
            break;
        case PrimitiveType::PRIM_BASIS_CURVES_LINES:
        case PrimitiveType::PRIM_BASIS_CURVES_LINEAR_PATCHES:
            primIndexSize = 2;
            break;
        case PrimitiveType::PRIM_MESH_COARSE_TRIANGLES:
        case PrimitiveType::PRIM_MESH_REFINED_TRIANGLES:
        case PrimitiveType::PRIM_VOLUME:
            primIndexSize = 3;
            break;
        case PrimitiveType::PRIM_BASIS_CURVES_CUBIC_PATCHES:
        case PrimitiveType::PRIM_MESH_COARSE_QUADS:
        case PrimitiveType::PRIM_MESH_REFINED_QUADS:
            primIndexSize = 4;
            break;
        case PrimitiveType::PRIM_MESH_BSPLINE:
            primIndexSize = 16;
            break;
        case PrimitiveType::PRIM_MESH_BOXSPLINETRIANGLE:
            primIndexSize = 12;
            break;
    }

    return primIndexSize;
}

int
HdSt_GeometricShader::GetNumPrimitiveVertsForGeometryShader() const
{
    int numPrimVerts = 1;

    switch (_primType)
    {
        case PrimitiveType::PRIM_POINTS:
            numPrimVerts = 1;
            break;
        case PrimitiveType::PRIM_BASIS_CURVES_LINES:
            numPrimVerts = 2;
            break;
        case PrimitiveType::PRIM_MESH_COARSE_TRIANGLES:
        case PrimitiveType::PRIM_MESH_REFINED_TRIANGLES:
        case PrimitiveType::PRIM_BASIS_CURVES_LINEAR_PATCHES:
        case PrimitiveType::PRIM_BASIS_CURVES_CUBIC_PATCHES:
        case PrimitiveType::PRIM_MESH_BSPLINE:
        case PrimitiveType::PRIM_MESH_BOXSPLINETRIANGLE:
        // for patches with tesselation, input to GS is still a series of tris
        case PrimitiveType::PRIM_VOLUME:
            numPrimVerts = 3;
            break;
        case PrimitiveType::PRIM_MESH_COARSE_QUADS:
        case PrimitiveType::PRIM_MESH_REFINED_QUADS:
            numPrimVerts = 4;
            break;
    }

    return numPrimVerts;
}

/*static*/
 HdSt_GeometricShaderSharedPtr
 HdSt_GeometricShader::Create(
     HdSt_ShaderKey const &shaderKey, 
    HdStResourceRegistrySharedPtr const &resourceRegistry)
{
    // Use the shaderKey hash to deduplicate geometric shaders.
    HdInstance<HdSt_GeometricShaderSharedPtr> geometricShaderInstance =
        resourceRegistry->RegisterGeometricShader(shaderKey.ComputeHash());

    if (geometricShaderInstance.IsFirstInstance()) {
        geometricShaderInstance.SetValue(
            HdSt_GeometricShaderSharedPtr(
                new HdSt_GeometricShader(
                    shaderKey.GetGlslfxString(),
                    shaderKey.GetPrimitiveType(),
                    shaderKey.GetCullStyle(),
                    shaderKey.GetPolygonMode(),
                    shaderKey.IsFrustumCullingPass(),
                    /*debugId=*/SdfPath(),
                    shaderKey.GetLineWidth())));
    }
    return geometricShaderInstance.GetValue();
}

PXR_NAMESPACE_CLOSE_SCOPE

