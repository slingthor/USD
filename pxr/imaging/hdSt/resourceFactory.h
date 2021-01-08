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
#ifndef HDST_RESOURCEFACTORY_H
#define HDST_RESOURCEFACTORY_H

/// \file hdSt/resourceFactory.h

#include "pxr/pxr.h"
#include "pxr/imaging/hdSt/api.h"

#include "pxr/imaging/hd/bufferArray.h"
#include "pxr/imaging/hd/resource.h"
#include "pxr/imaging/hd/sceneDelegate.h"
#include "pxr/imaging/hd/types.h"

#include "pxr/imaging/hgi/texture.h"

#include "pxr/imaging/garch/textureHandle.h"

#include "pxr/base/tf/singleton.h"

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>

PXR_NAMESPACE_OPEN_SCOPE

class Hd_VertexAdjacency;

class HdSt_CodeGen;
class HdStDrawItemInstance;
class HdSt_MeshTopology;
class HdStGLSLProgram;
class HdStRenderPassState;
class HdSt_ResourceBinder;
class HdStSimpleTextureResource;
class HdStResourceRegistry;

using HdStTextureResourceSharedPtr = std::shared_ptr<class HdStTextureResource>;

using HdSt_DrawBatchSharedPtr = std::shared_ptr<class HdSt_DrawBatch>;
using HdSt_GeometricShaderPtr = std::shared_ptr<class HdSt_GeometricShader>;
using HdStRenderPassShaderSharedPtr =
    std::shared_ptr<class HdStRenderPassShader>;
using HdStShaderCodeSharedPtr = std::shared_ptr<class HdStShaderCode>;
using HdStShaderCodeSharedPtrVector = std::vector<HdStShaderCodeSharedPtr>;
using HdStSimpleLightingShaderPtr =
    std::weak_ptr<class HdStSimpleLightingShader>;

class HdStResourceFactoryInterface {
public:

    HDST_API
    virtual ~HdStResourceFactoryInterface() {}

    // Temp
    virtual bool IsOpenGL() const = 0;

    // HdSt_CodeGen
    HDST_API
    virtual HdSt_CodeGen *NewCodeGen(
        HdSt_GeometricShaderPtr const &geometricShader,
        HdStShaderCodeSharedPtrVector const &shaders,
        TfToken const &materialTag) const = 0;
    
    HDST_API
    virtual HdSt_CodeGen *NewCodeGen(
        HdStShaderCodeSharedPtrVector const &shaders) const = 0;
    
    /// Creates an indirect draw batch
    HDST_API
    virtual HdSt_DrawBatchSharedPtr NewIndirectDrawBatch(
        HdStDrawItemInstance * drawItemInstance) const = 0;

    /// Creates a new render pass state
    HDST_API
    virtual HdStRenderPassState *NewRenderPassState() const = 0;
    
    /// Creates a new render pass state
    HDST_API
    virtual HdStRenderPassState *NewRenderPassState(
        HdStRenderPassShaderSharedPtr const &renderPassShader) const = 0;
    
    /// Creates a resource binder
    HDST_API
    virtual HdSt_ResourceBinder *NewResourceBinder() const = 0;

    HDST_API
    virtual char const *const GetComputeShaderFilename() const = 0;
    
    /// Creates a graphics API specific program
    HDST_API
    virtual HdStGLSLProgram *NewProgram(
        TfToken const &role, HdStResourceRegistry *const registry) const = 0;
    
    HDST_API
    virtual HdStRenderPassShaderSharedPtr NewRenderPassShader() const = 0;

    HDST_API
    virtual HdStRenderPassShaderSharedPtr NewRenderPassShader(
        TfToken const &glslfxFile) const = 0;


protected:
    HDST_API
    HdStResourceFactoryInterface() {}

};

class HdStResourceFactory : boost::noncopyable {
public:
    HDST_API
    static HdStResourceFactory& GetInstance();
    
    HDST_API
    HdStResourceFactoryInterface *operator ->() const;
    
    HDST_API
    void SetResourceFactory(HdStResourceFactoryInterface *factory);
    
private:
    HDST_API
    HdStResourceFactory();
    
    HDST_API
    ~HdStResourceFactory();
    
    HdStResourceFactoryInterface *factory;
    
    friend class TfSingleton<HdStResourceFactory>;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDST_RESOURCEFACTORY_H
