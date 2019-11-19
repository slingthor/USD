//
// Copyright 2019 Pixar
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
#include "pxr/usdImaging/usdImagingGL/materialTextureAdapter.h"
#include "pxr/usdImaging/usdImaging/textureUtils.h"
#include "pxr/imaging/garch/image.h"

PXR_NAMESPACE_OPEN_SCOPE


TF_REGISTRY_FUNCTION(TfType)
{
    typedef UsdImagingGLMaterialTextureAdapter Adapter;
    TfType t = TfType::Define<Adapter, TfType::Bases<Adapter::BaseAdapter> >();
    t.SetFactory< UsdImagingPrimAdapterFactory<Adapter> >();
}

UsdImagingGLMaterialTextureAdapter::~UsdImagingGLMaterialTextureAdapter() 
{
}

HdTextureResourceSharedPtr
UsdImagingGLMaterialTextureAdapter::GetTextureResource(
    UsdPrim const& usdPrim,
    SdfPath const &id,
    UsdTimeCode time) const
{
    // The usdPrim we receive is the Material prim, since that is the prim we
    // inserted the SPrim and primInfo for. However, the texture is authored on
    // the texture prim, so we get the texture prim.
    UsdPrim texturePrim = _GetPrim(id.GetParentPath());
    return UsdImaging_GetTextureResource(texturePrim, id, time);
}

PXR_NAMESPACE_CLOSE_SCOPE
