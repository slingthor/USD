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
#ifndef GARCH_RESOURCEFACTORY_H
#define GARCH_RESOURCEFACTORY_H

/// \file garch/resourceFactory.h

#include "pxr/pxr.h"
#include "pxr/base/tf/declarePtrs.h"
#include "pxr/base/tf/singleton.h"
#include "pxr/base/tf/token.h"
#include "pxr/base/gf/vec2i.h"

#include <boost/noncopyable.hpp>

PXR_NAMESPACE_OPEN_SCOPE

class GarchBaseTexture;
class GarchBindingMap;
class GarchContextCaps;
class GarchDrawTarget;
class GarchSimpleLightingContext;
class GarchSimpleShadowArray;
class GarchUniformBlock;
class GLSLFX;
class HdSt_CodeGen;

TF_DECLARE_WEAK_AND_REF_PTRS(GarchArrayTexture);
TF_DECLARE_WEAK_AND_REF_PTRS(GarchDrawTarget);
TF_DECLARE_WEAK_AND_REF_PTRS(GarchUniformBlock);

class GarchResourceFactoryInterface {
public:
    virtual ~GarchResourceFactoryInterface() {}
    
    // GarchContextCaps
    virtual GarchContextCaps const& GetContextCaps() const = 0;
    
    // SimpleLightingContext creation
    virtual GarchSimpleLightingContext *NewSimpleLightingContext() const = 0;
    
    // SimpleShadowArray creation
    virtual GarchSimpleShadowArray *NewSimpleShadowArray(GfVec2i const & size, size_t numLayers) const = 0;
    
    // BindingMap
    virtual GarchBindingMap *NewBindingMap() const = 0;
    
    // DrawTarget
    virtual GarchDrawTarget *NewDrawTarget(GfVec2i const & size, bool requestMSAA) const = 0;
    virtual GarchDrawTarget *NewDrawTarget(GarchDrawTargetPtr const & drawtarget) const = 0;
    
    // UniformBlock creation
    virtual GarchUniformBlockRefPtr NewUniformBlock(char const *label = nullptr) const = 0;
    
    // Package Name accessor
    virtual std::string GetPackageName() const = 0;
    
    // ArrayTexture creation
    virtual GarchArrayTextureRefPtr NewArrayTexture(TfTokenVector const &imageFilePaths,
                                                    unsigned int arraySize,
                                                    unsigned int cropTop,
                                                    unsigned int cropBottom,
                                                    unsigned int cropLeft,
                                                    unsigned int cropRight) const = 0;
    
    // BaseTexture
    virtual GarchBaseTexture *NewBaseTexture() const = 0;
    
protected:
    GarchResourceFactoryInterface() {}
};

class GarchResourceFactory : boost::noncopyable {
public:
    static GarchResourceFactory& GetInstance() {
         return TfSingleton<GarchResourceFactory>::GetInstance();
    }

    GarchResourceFactoryInterface *operator ->() const;
    void SetResourceFactory(GarchResourceFactoryInterface *factory);

private:
    GarchResourceFactory();
    ~GarchResourceFactory();

    GarchResourceFactoryInterface *factory;

    friend class TfSingleton<GarchResourceFactory>;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif // GARCH_RESOURCEFACTORY_H
