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
/// \file ArrayTexture.cpp
// 

#include "pxr/imaging/glf/glew.h"

#include "pxr/imaging/garch/uvTextureData.h"

#include "pxr/imaging/glf/arrayTexture.h"
#include "pxr/imaging/glf/diagnostic.h"
#include "pxr/imaging/glf/utils.h"
#include "pxr/base/tf/registryManager.h"
#include "pxr/base/tf/type.h"
#include "pxr/base/trace/trace.h"

PXR_NAMESPACE_OPEN_SCOPE


TF_REGISTRY_FUNCTION(TfType)
{
    TfType::Define<GlfArrayTexture, TfType::Bases<GarchArrayTexture> >();
}

GlfArrayTexture::GlfArrayTexture(
    TfTokenVector const &imageFilePaths,
    unsigned int arraySize,
    unsigned int cropTop,
    unsigned int cropBottom,
    unsigned int cropLeft,
    unsigned int cropRight,
    GarchImage::ImageOriginLocation originLocation)
    
    : GarchArrayTexture(imageFilePaths,
                        cropTop,
                        cropBottom,
                        cropLeft,
                        cropRight,
                        originLocation),

      _imageFilePaths(imageFilePaths),
    _arraySize(arraySize)
{
    // do nothing
}

void
GlfArrayTexture::_OnSetMemoryRequested(size_t targetMemory)
{
    GarchBaseTextureDataConstRefPtrVector texDataVec(_arraySize, 0);
    for (size_t i = 0; i < _arraySize; ++i) {
        GarchUVTextureDataRefPtr texData =
            GarchUVTextureData::New(_GetImageFilePath(i), targetMemory,
                                    _GetCropTop(), _GetCropBottom(),
                                    _GetCropLeft(), _GetCropRight());
        if (texData) {
            texData->Read(0, _GenerateMipmap(), GetOriginLocation());
        }

        _UpdateTexture(texData);

        if (texData && texData->HasRawBuffer()) {
            texDataVec[i] = texData;
        } else {
            TF_WARN("Invalid texture data for texture file: %s",
                    _GetImageFilePath(i).GetString().c_str());
        }
    }

    _CreateTextures(texDataVec, _GenerateMipmap());
}

/* virtual */
const TfToken&
GlfArrayTexture::_GetImageFilePath(size_t index) const
{
    if (TF_VERIFY(index < _imageFilePaths.size())) {
        return _imageFilePaths[index];
    }
    else {
        return _imageFilePaths.front();
    }
}

/* virtual */
GarchTexture::BindingVector
GlfArrayTexture::GetBindings(TfToken const & identifier,
                              GarchSamplerGPUHandle samplerName) const
{
    return BindingVector(1,
                Binding(identifier, GarchTextureTokens->texels,
                        GL_TEXTURE_2D_ARRAY, GetGlTextureName(), samplerName));
}

void
GlfArrayTexture::_CreateTextures(
    GarchBaseTextureDataConstRefPtrVector texDataVec,
    bool const generateMipmap)
{
    TRACE_FUNCTION();

    if (texDataVec.empty() || 
        !texDataVec[0]) {
        TF_WARN("No texture data for array texture.");
        return;
    }
    
    glBindTexture(
        GL_TEXTURE_2D_ARRAY,
        GetGlTextureName());
    
    glTexParameteri(
        GL_TEXTURE_2D_ARRAY,
        GL_GENERATE_MIPMAP,
        generateMipmap ? GL_TRUE
        : GL_FALSE);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // Create the data storage which will be filled in
    // by the subImage3D call below...
    // XXX assuming texture file format and size is
    //     going to be the same across the array.
    //     Maybe we need a check for this somewhere...
    glTexImage3D(
        GL_TEXTURE_2D_ARRAY,                 /* target         */
        0,                                   /* level          */
        texDataVec[0]->GLInternalFormat(),   /* internalFormat */
        texDataVec[0]->ResizedWidth(),       /* width          */
        texDataVec[0]->ResizedHeight(),      /* height         */
        _arraySize,                          /* depth          */
        0,                                   /* border         */
        texDataVec[0]->GLFormat(),           /* format         */
        texDataVec[0]->GLType(),             /* type           */
        NULL);                               /* data           */
    
    int memUsed = 0;
    for (size_t i = 0; i < _arraySize; ++i) {
        GarchBaseTextureDataConstPtr texData = texDataVec[i];
        if (texData && texData->HasRawBuffer()) {

            glTexSubImage3D(
                GL_TEXTURE_2D_ARRAY,         /* target         */
                0,                           /* level          */
                0,                           /* xOffset        */
                0,                           /* yOffset        */
                i,                           /* zOffset        */
                texData->ResizedWidth(),     /* width          */
                texData->ResizedHeight(),    /* height         */
                1,                           /* depth          */
                texData->GLFormat(),         /* format         */
                texData->GLType(),           /* type           */
                texData->GetRawBuffer());    /* data           */
            
            memUsed += texData->ComputeBytesUsed();
        }
        
    }
    
    glBindTexture(
        GL_TEXTURE_2D_ARRAY,
        0);

    _SetMemoryUsed(memUsed);
}

PXR_NAMESPACE_CLOSE_SCOPE

