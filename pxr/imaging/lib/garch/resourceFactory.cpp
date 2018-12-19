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
// resourceFactory.cpp
//
#include "pxr/imaging/garch/resourceFactory.h"
#include "pxr/base/tf/diagnostic.h"

#include "pxr/base/tf/instantiateSingleton.h"

PXR_NAMESPACE_OPEN_SCOPE

TF_INSTANTIATE_SINGLETON(GarchResourceFactory);

GarchResourceFactory&
GarchResourceFactory::GetInstance() {
    return TfSingleton<GarchResourceFactory>::GetInstance();
}

GarchResourceFactory::GarchResourceFactory():
    factory(NULL)
{
    TfSingleton<GarchResourceFactory>::SetInstanceConstructed(*this);
}

GarchResourceFactory::~GarchResourceFactory()
{
    // Empty
}

GarchResourceFactoryInterface *GarchResourceFactory::operator -> () const 
{
    if (!factory)
    {
        TF_FATAL_CODING_ERROR("No resource factory currently set");
    }
    return factory;
}

void GarchResourceFactory::SetResourceFactory(GarchResourceFactoryInterface *_factory)
{
    factory = _factory;
}

PXR_NAMESPACE_CLOSE_SCOPE

