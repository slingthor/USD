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
#ifndef MTLF_RESOURCEFACTORY_H
#define MTLF_RESOURCEFACTORY_H

/// \file mtlf/resourceFactory.h

#include "pxr/pxr.h"
#include "pxr/imaging/mtlf/contextCaps.h"
#include "pxr/imaging/garch/resourceFactory.h"

PXR_NAMESPACE_OPEN_SCOPE

class MtlfResourceFactory : public GarchResourceFactoryInterface {
public:
    MTLF_API
    MtlfResourceFactory();
    
    MTLF_API
    virtual ~MtlfResourceFactory();
    
    // GarchContextCaps
    MTLF_API
    virtual GarchContextCaps const& GetContextCaps() const override {
        return contextCaps;
    }

    // GarchSimpleLightingContext creation
    MTLF_API
    virtual GarchSimpleLightingContext *NewSimpleLightingContext() const override;
    
    // GarchSimpleShadowArray creation
    MTLF_API
    virtual GarchSimpleShadowArray *NewSimpleShadowArray(GfVec2i const & size, size_t numLayers) const override;
    
    // GarchBindingMap creation
    MTLF_API
    virtual GarchBindingMap *NewBindingMap() const override;
    
    // GarchDrawTarget creation
    MTLF_API
    virtual GarchDrawTarget *NewDrawTarget(GfVec2i const & size, bool requestMSAA) const override;
    MTLF_API
    virtual GarchDrawTarget *NewDrawTarget(GarchDrawTargetPtr const & drawtarget) const override;
    
    // UniformBlock creation
    MTLF_API
    virtual GarchUniformBlockRefPtr NewUniformBlock(char const *label = nullptr) const override;
    
    // Package Name accessor
    MTLF_API
    virtual std::string GetPackageName() const override { return "mtlf"; }
    
    // ArrayTexture creation
    MTLF_API
    virtual GarchArrayTextureRefPtr NewArrayTexture(TfTokenVector const &imageFilePaths,
                                                    unsigned int arraySize,
                                                    unsigned int cropTop,
                                                    unsigned int cropBottom,
                                                    unsigned int cropLeft,
                                                    unsigned int cropRight,
                                                    GarchImage::ImageOriginLocation originLocation) const override;
    
    // BaseTexture
    MTLF_API
    virtual GarchBaseTexture *NewBaseTexture() const override;
    
    // pTex
    MTLF_API
    virtual bool IsSupportedPtexTexture(std::string const & imageFilePath) const override;
    
private:
    MtlfContextCaps contextCaps;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // MTLF_RESOURCEFACTORY_H
