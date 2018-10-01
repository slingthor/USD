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

#include "pxr/imaging/garch/uvTextureData.h"

#include "pxr/imaging/mtlf/arrayTexture.h"
#include "pxr/imaging/mtlf/diagnostic.h"
#include "pxr/imaging/mtlf/utils.h"
#include "pxr/base/tf/registryManager.h"
#include "pxr/base/tf/type.h"
#include "pxr/base/trace/trace.h"

PXR_NAMESPACE_OPEN_SCOPE


TF_REGISTRY_FUNCTION(TfType)
{
    TfType::Define<MtlfArrayTexture, TfType::Bases<GarchArrayTexture> >();
}

MtlfArrayTexture::MtlfArrayTexture(TfTokenVector const &imageFilePaths,
                                   unsigned int arraySize,
                                   unsigned int cropTop,
                                   unsigned int cropBottom,
                                   unsigned int cropLeft,
                                   unsigned int cropRight,
                                   GarchImage::ImageOriginLocation originLocation)

: GarchArrayTexture(imageFilePaths,
                    arraySize,
                    cropTop,
                    cropBottom,
                    cropLeft,
                    cropRight,
                    originLocation)
{
    // do nothing
}

/* virtual */
GarchTexture::BindingVector
MtlfArrayTexture::GetBindings(TfToken const & identifier,
                              GarchSamplerGPUHandle samplerName)
{
    TF_FATAL_CODING_ERROR("Not Implemented");

    return BindingVector(1,
                Binding(identifier, GarchTextureTokens->texels,
                        /*GL_TEXTURE_2D_ARRAY*/ 0, GetAPITextureName(), samplerName));
}

void
MtlfArrayTexture::_CreateTextures(
    GarchBaseTextureDataConstRefPtrVector texDataVec,
    bool const generateMipmap)
{
    TRACE_FUNCTION();

    if (texDataVec.empty() || 
        !texDataVec[0]) {
        TF_WARN("No texture data for array texture.");
        return;
    }
    
    TF_FATAL_CODING_ERROR("Not Implemented");
    /*
    glBindTexture(
        GL_TEXTURE_2D_ARRAY,
        GetAPITextureName());
    
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
        GL_TEXTURE_2D_ARRAY,
        0,
        texDataVec[0]->GLInternalFormat(),
        texDataVec[0]->ResizedWidth(),
        texDataVec[0]->ResizedHeight(),
        _arraySize,
        0,
        texDataVec[0]->Mtlformat(),
        texDataVec[0]->GLType(),
        NULL);
    
    int memUsed = 0;
    for (size_t i = 0; i < _arraySize; ++i) {
        GarchBaseTextureDataConstPtr texData = texDataVec[i];
        if (texData && texData->HasRawBuffer()) {

            glTexSubImage3D(
                GL_TEXTURE_2D_ARRAY,
                0,
                0,
                0,
                i,
                texData->ResizedWidth(),
                texData->ResizedHeight(),
                1,
                texData->Mtlformat(),
                texData->GLType(),
                texData->GetRawBuffer());
            
            memUsed += texData->ComputeBytesUsed();
        }
        
    }
    
    glBindTexture(
        GL_TEXTURE_2D_ARRAY,
        0);

    _SetMemoryUsed(memUsed);
     */
}

PXR_NAMESPACE_CLOSE_SCOPE

