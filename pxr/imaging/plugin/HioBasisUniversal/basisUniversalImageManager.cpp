//
// Copyright 2020 Pixar
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
#include "pxr/imaging/plugin/HioBasisUniversal/basisUniversalImageManager.h"

#include "pxr/base/tf/instantiateSingleton.h"

PXR_NAMESPACE_OPEN_SCOPE

TF_INSTANTIATE_SINGLETON(BasisUniversalImageManager);

BasisUniversalImageManager::BasisUniversalImageManager()
{
    _globalCodebook.reset(new GlobalSelectorCodebook(
        basist::g_global_selector_cb_size, basist::g_global_selector_cb));
}

GlobalSelectorCodebook*
BasisUniversalImageManager::GetGlobalSelectorCodebook() const {
    static std::once_flag once;
    std::call_once(once, [](){
        basist::basisu_transcoder_init();
    });
    return _globalCodebook.get();
}

PXR_NAMESPACE_CLOSE_SCOPE
