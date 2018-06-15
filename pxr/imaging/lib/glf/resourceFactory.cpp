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

#include <pxr/imaging/glf/arrayTexture.h>
#include <pxr/imaging/glf/baseTexture.h>
#include <pxr/imaging/glf/bindingMap.h>
#include <pxr/imaging/glf/drawTarget.h>
#include <pxr/imaging/glf/simpleLightingContext.h>
#include <pxr/imaging/glf/simpleShadowArray.h>
#include <pxr/imaging/glf/uniformBlock.h>

#include <pxr/imaging/garch/glslfx.h>

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

GarchSimpleLightingContext *GlfResourceFactory::NewSimpleLightingContext() const
{
    return new GlfSimpleLightingContext();
}

GarchSimpleShadowArray *GlfResourceFactory::NewSimpleShadowArray(GfVec2i const & size, size_t numLayers) const
{
    return new GlfSimpleShadowArray(size, numLayers);
}

GarchBindingMap *GlfResourceFactory::NewBindingMap() const
{
    return new GlfBindingMap();
}

GarchDrawTarget *GlfResourceFactory::NewDrawTarget(GfVec2i const & size, bool requestMSAA) const
{
    return new GlfDrawTarget(size, requestMSAA);
}

GarchDrawTarget *GlfResourceFactory::NewDrawTarget(GarchDrawTargetPtr const & drawtarget) const
{
    return new GlfDrawTarget(drawtarget);
}

GarchUniformBlockRefPtr GlfResourceFactory::NewUniformBlock() const
{
    return TfCreateRefPtr(new GlfUniformBlock());
}

GarchArrayTextureRefPtr GlfResourceFactory::NewArrayTexture(TfTokenVector const &imageFilePaths,
                                                            unsigned int arraySize,
                                                            unsigned int cropTop,
                                                            unsigned int cropBottom,
                                                            unsigned int cropLeft,
                                                            unsigned int cropRight) const
{
    return TfCreateRefPtr(new GlfArrayTexture(imageFilePaths, arraySize,
                                              cropTop, cropBottom,
                                              cropLeft, cropRight));
}

GarchBaseTexture *GlfResourceFactory::NewBaseTexture() const
{
    return new GlfBaseTexture();
}

PXR_NAMESPACE_CLOSE_SCOPE

