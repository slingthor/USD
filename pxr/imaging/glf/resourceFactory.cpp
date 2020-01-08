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
#include <pxr/imaging/glf/contextCaps.h>
#include <pxr/imaging/glf/drawTarget.h>
#include <pxr/imaging/glf/ptexTexture.h>
#include <pxr/imaging/glf/simpleLightingContext.h>
#include <pxr/imaging/glf/simpleShadowArray.h>
#include <pxr/imaging/glf/udimTexture.h>
#include <pxr/imaging/glf/uniformBlock.h>

#include <pxr/imaging/garch/vdbTexture.h>

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

GarchSimpleShadowArray *GlfResourceFactory::NewSimpleShadowArray() const
{
    return new GlfSimpleShadowArray();
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

GarchUniformBlockRefPtr GlfResourceFactory::NewUniformBlock(char const *label) const
{
    return TfCreateRefPtr(new GlfUniformBlock(label));
}

GarchArrayTextureRefPtr GlfResourceFactory::NewArrayTexture(TfTokenVector const &imageFilePaths,
                                                            unsigned int arraySize,
                                                            unsigned int cropTop,
                                                            unsigned int cropBottom,
                                                            unsigned int cropLeft,
                                                            unsigned int cropRight,
                                                            GarchImage::ImageOriginLocation originLocation) const
{
    return TfCreateRefPtr(new GlfArrayTexture(imageFilePaths, arraySize,
                                              cropTop, cropBottom,
                                              cropLeft, cropRight,
                                              originLocation));
}

GarchBaseTexture *GlfResourceFactory::NewBaseTexture() const
{
    return new GlfBaseTexture();
}

#ifdef PXR_PTEX_SUPPORT_ENABLED
GarchPtexTextureRefPtr GlfResourceFactory::NewPtexTexture(
                            const TfToken &imageFilePath) const
{
    return TfCreateRefPtr(new GlfPtexTexture(imageFilePath));
}
#endif

GarchUdimTextureRefPtr GlfResourceFactory::NewUdimTexture(
                            TfToken const& imageFilePath,
                            GarchImage::ImageOriginLocation originLocation,
                            std::vector<std::tuple<int, TfToken>>&& tiles) const
{
    return TfCreateRefPtr(new GlfUdimTexture(imageFilePath, originLocation, std::move(tiles)));
}

GarchVdbTextureRefPtr GlfResourceFactory::NewVdbTexture(
                            const TfToken &imageFilePath) const
{
    return TfCreateRefPtr(new GarchVdbTexture(NewBaseTexture(), imageFilePath));
}

PXR_NAMESPACE_CLOSE_SCOPE

