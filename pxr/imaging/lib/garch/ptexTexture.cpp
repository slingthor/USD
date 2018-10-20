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
/// \file mtlf/ptexTexture.cpp

// 

#include "pxr/imaging/glf/glew.h"

#include "pxr/imaging/garch/ptexTexture.h"

#include "pxr/base/tf/stringUtils.h"

PXR_NAMESPACE_OPEN_SCOPE

bool GarchIsSupportedPtexTexture(std::string const & imageFilePath)
{
#ifdef PXR_PTEX_SUPPORT_ENABLED
    return (TfStringEndsWith(imageFilePath, ".ptx") ||
            TfStringEndsWith(imageFilePath, ".ptex"));
#else
    return false;
#endif
}

PXR_NAMESPACE_CLOSE_SCOPE

#ifdef PXR_PTEX_SUPPORT_ENABLED

#include "pxr/imaging/garch/ptexMipmapTextureLoader.h"
#include "pxr/imaging/garch/resourceFactory.h"

#include "pxr/base/tf/diagnostic.h"
#include "pxr/base/tf/registryManager.h"
#include "pxr/base/tf/type.h"
#include "pxr/base/trace/trace.h"

#include <Ptexture.h>
#include <PtexUtils.h>

#include <string>
#include <vector>
#include <list>
#include <algorithm>

using std::string;
using namespace boost;

PXR_NAMESPACE_OPEN_SCOPE

TF_REGISTRY_FUNCTION(TfType)
{
    typedef GarchPtexTexture Type;
    TfType t = TfType::Define<Type, TfType::Bases<GarchTexture> >();
    t.SetFactory< GarchTextureFactory<Type> >();
}

//------------------------------------------------------------------------------
GarchPtexTextureRefPtr
GarchPtexTexture::New(const TfToken &imageFilePath)
{
    return GarchResourceFactory::GetInstance()->NewPtexTexture(imageFilePath);
}

//------------------------------------------------------------------------------
GarchPtexTexture::GarchPtexTexture(const TfToken &imageFilePath) :
    _loaded(false),
    _width(0), _height(0), _depth(0),
    _imageFilePath(imageFilePath)
{
}

//------------------------------------------------------------------------------
GarchPtexTexture::~GarchPtexTexture()
{
}

//------------------------------------------------------------------------------
void
GarchPtexTexture::_OnMemoryRequestedDirty()
{
    _loaded = false;
}

//------------------------------------------------------------------------------

/* virtual */
GarchTexture::BindingVector
GarchPtexTexture::GetBindings(TfToken const & identifier,
                              GarchSamplerGPUHandle samplerId)
{
    if (!_loaded) {
        _ReadImage();
    }

    BindingVector result;
    result.reserve(2);

    result.push_back(Binding(
        TfToken(identifier.GetString() + "_Data"), GarchTextureTokens->texels,
        GL_TEXTURE_2D_ARRAY, _texels, samplerId));

    // packing buffer doesn't need external sampler
    result.push_back(Binding(
        TfToken(identifier.GetString() + "_Packing"), GarchTextureTokens->layout,
        GL_TEXTURE_BUFFER, _layout, nil));

    return result;
}

//------------------------------------------------------------------------------

VtDictionary
GarchPtexTexture::GetTextureInfo(bool forceLoad)
{
    if (!_loaded && forceLoad) {
        _ReadImage();
    }

    VtDictionary info;

    info["memoryUsed"] = GetMemoryUsed();
    info["width"] = (int)_width;
    info["height"] = (int)_height;
    info["depth"] = (int)_depth;
    info["format"] = (int)_format;
    info["imageFilePath"] = _imageFilePath;
    info["referenceCount"] = GetRefCount().Get();

    return info;
}

bool
GarchPtexTexture::IsMinFilterSupported(GLenum filter)
{
    switch(filter) {
    case GL_NEAREST:
    case GL_LINEAR:
        return true;
    default:
        return false;
    }
}

bool
GarchPtexTexture::IsMagFilterSupported(GLenum filter)
{
    switch(filter) {
    case GL_NEAREST:
    case GL_LINEAR:
        return true;
    default:
        return false;
    }
}
    
GarchTextureGPUHandle
GarchPtexTexture::GetLayoutTextureName()
{
    if (!_loaded) {
        _ReadImage();
    }
    
    return _layout;
}

GarchTextureGPUHandle
GarchPtexTexture::GetTexelsTextureName()
{
    if (!_loaded) {
        _ReadImage();
    }
    
    return _texels;
}

GarchTextureGPUHandle
GarchPtexTexture::GetTextureName()
{
    if (!_loaded) {
        _ReadImage();
    }

    return GarchTextureGPUHandle(_texels);    
}

void
GarchPtexTexture::_ReadTexture()
{
    TF_FATAL_CODING_ERROR("Should not get here!"); //MTL_FIXME
}

PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_PTEX_SUPPORT_ENABLED


