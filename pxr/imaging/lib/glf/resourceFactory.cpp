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
// utils.cpp
//
#include <pxr/imaging/glf/resourceFactory.h>

#include <pxr/imaging/glf/bindingMap.h>
#include <pxr/imaging/glf/drawTarget.h>
#include <pxr/imaging/glf/glslfx.h>
#include <pxr/imaging/glf/simpleLightingContext.h>
#include <pxr/imaging/glf/simpleShadowArray.h>
#include <pxr/imaging/glf/uniformBlock.h>

#include <pxr/base/tf/diagnostic.h>

PXR_NAMESPACE_OPEN_SCOPE

GlfResourceFactory::GlfResourceFactory()
{
    // Empty
}

GlfResourceFactory::~GlfResourceFactory()
{
    // Empty
}

GLSLFX *GlfResourceFactory::NewGLSLFX()
{
    return new GlfGLSLFX();
}

GLSLFX *GlfResourceFactory::NewGLSLFX(std::string const & filePath)
{
    return new GlfGLSLFX(filePath);
}

GLSLFX *GlfResourceFactory::NewGLSLFX(std::istream &is)
{
    return new GlfGLSLFX(is);
}

GarchSimpleLightingContext *GlfResourceFactory::NewSimpleLightingContext()
{
    return new GlfSimpleLightingContext();
}

GarchSimpleShadowArray *GlfResourceFactory::NewSimpleShadowArray(GfVec2i const & size, size_t numLayers)
{
    return new GlfSimpleShadowArray(size, numLayers);
}

GarchBindingMap *GlfResourceFactory::NewBindingMap()
{
    return new GlfBindingMap();
}

GarchDrawTarget *GlfResourceFactory::NewDrawTarget(GfVec2i const & size, bool requestMSAA)
{
    return new GlfDrawTarget(size, requestMSAA);
}

GarchDrawTarget *GlfResourceFactory::NewDrawTarget(GarchDrawTargetPtr const & drawtarget)
{
    return new GlfDrawTarget(drawtarget);
}

GarchUniformBlockRefPtr GlfResourceFactory::NewUniformBlock()
{
    return TfCreateRefPtr(new GlfUniformBlock());
}

PXR_NAMESPACE_CLOSE_SCOPE

