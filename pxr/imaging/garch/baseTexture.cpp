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
/// \file baseTexture.cpp
#include "pxr/imaging/garch/baseTexture.h"
#include "pxr/imaging/garch/baseTextureData.h"
#include "pxr/imaging/garch/utils.h"

#include "pxr/base/tf/registryManager.h"
#include "pxr/base/tf/type.h"
#include "pxr/base/trace/trace.h"

PXR_NAMESPACE_OPEN_SCOPE


TF_REGISTRY_FUNCTION(TfType)
{
    TfType::Define<GarchBaseTexture, TfType::Bases<GarchTexture> >();
}

GarchBaseTexture::GarchBaseTexture()
  : _loaded(false),
    _currentWidth(0),
    _currentHeight(0),
    // 1 since a 2d-texture can be thought of as x*y*1 3d-texture
    _currentDepth(1),
    _format(GL_RGBA),
    _hasWrapModeS(false),
    _hasWrapModeT(false),
    _hasWrapModeR(false),
    _wrapModeS(GL_REPEAT),
    _wrapModeT(GL_REPEAT),
    _wrapModeR(GL_REPEAT)
{
    /* nothing */
}

GarchBaseTexture::GarchBaseTexture(HioImage::ImageOriginLocation originLocation)
  : GarchTexture(originLocation),
    _loaded(false),
    _currentWidth(0),
    _currentHeight(0),
    // 1 since a 2d-texture can be thought of as x*y*1 3d-texture
    _currentDepth(1),
    _format(GL_RGBA),
    _hasWrapModeS(false),
    _hasWrapModeT(false),
    _hasWrapModeR(false),
    _wrapModeS(GL_REPEAT),
    _wrapModeT(GL_REPEAT),
    _wrapModeR(GL_REPEAT)
{
    /* nothing */
}

GarchBaseTexture::~GarchBaseTexture()
{
}

void
GarchBaseTexture::_ReadTextureIfNotLoaded()
{
    if (!_loaded) {
        _ReadTexture();
    }
}

GarchTextureGPUHandle
GarchBaseTexture::GetAPITextureName()
{
    _ReadTextureIfNotLoaded();

    return _textureName;
}

int
GarchBaseTexture::GetWidth()
{
    _ReadTextureIfNotLoaded();
    
    return _currentWidth;
}

int
GarchBaseTexture::GetHeight()
{
    _ReadTextureIfNotLoaded();
    
    return _currentHeight;
}

int
GarchBaseTexture::GetDepth()
{
    _ReadTextureIfNotLoaded();
    
    return _currentDepth;
}

int
GarchBaseTexture::GetFormat()
{
    _ReadTextureIfNotLoaded();
    
    return _format;
}

GarchTextureGPUHandle GarchBaseTexture::GetTextureName()
{
    _ReadTextureIfNotLoaded();
    
    return _textureName;
}

VtDictionary
GarchBaseTexture::GetTextureInfo(bool forceLoad)
{
    VtDictionary info;
    
    if (forceLoad) {
        _ReadTextureIfNotLoaded();
    }
    
    if (_loaded) {
        info["memoryUsed"] = GetMemoryUsed();
        info["width"] = _currentWidth;
        info["height"] = _currentHeight;
        info["depth"] = _currentDepth;
        info["format"] = _format;
        
        if (_hasWrapModeS) {
            info["wrapModeS"] = _wrapModeS;
        }
        
        if (_hasWrapModeT) {
            info["wrapModeT"] = _wrapModeT;
        }
        
        if (_hasWrapModeR) {
            info["wrapModeR"] = _wrapModeR;
        }
    } else {
        info["memoryUsed"] = (size_t)0;
        info["width"] = 0;
        info["height"] = 0;
        info["depth"] = 1;
        info["format"] = _format;
    }
    info["referenceCount"] = GetCurrentCount();
    
    return info;
}

void
GarchBaseTexture::_OnMemoryRequestedDirty()
{
    _loaded = false;
}

GARCH_API
void GarchBaseTexture::_SetLoaded()
{
    _loaded = true;
}

PXR_NAMESPACE_CLOSE_SCOPE

