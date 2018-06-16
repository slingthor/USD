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
/// \file UVTextureStorage.cpp
//    

#include "pxr/imaging/garch/resourceFactory.h"
#include "pxr/imaging/garch/uvTextureStorage.h"
#include "pxr/imaging/garch/uvTextureStorageData.h"

PXR_NAMESPACE_OPEN_SCOPE


TF_REGISTRY_FUNCTION(TfType)
{
    TfType::Define<GarchUVTextureStorage, TfType::Bases<GarchBaseTexture> >();
}

GarchUVTextureStorageRefPtr 
GarchUVTextureStorage::New(
    unsigned int width,
    unsigned int height, 
    const VtValue &storageData)
{
    return TfCreateRefPtr(new GarchUVTextureStorage(
                                    GarchResourceFactory::GetInstance()->NewBaseTexture(),
                                    width, height, storageData));
}

GarchUVTextureStorage::GarchUVTextureStorage(
    GarchBaseTexture *baseTexture,
    unsigned int width,
    unsigned int height, 
    const VtValue &storageData)
    : _baseTexture(baseTexture)
    , _width(width)
    , _height(height)
    , _storageData(storageData)
{
    /* nothing */
}

GarchUVTextureStorage::~GarchUVTextureStorage()
{
    if (_baseTexture) {
        delete _baseTexture;
    }
}

void 
GarchUVTextureStorage::_OnSetMemoryRequested(size_t targetMemory)
{
    GarchUVTextureStorageDataRefPtr texData =
        GarchUVTextureStorageData::New(
            _width,
            _height,
            _storageData);
    texData->Read(0, false); 
    _UpdateTexture(texData);
    _CreateTexture(texData, _GenerateMipmap()); 
}

bool
GarchUVTextureStorage::_GenerateMipmap() const
{
    return false;
}

PXR_NAMESPACE_CLOSE_SCOPE

