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
#ifndef PXR_IMAGING_PLUGIN_HIOBASISUNIVERSAL_BASIS_UNIVERSAL_MANAGER_H
#define PXR_IMAGING_PLUGIN_HIOBASISUNIVERSAL_BASIS_UNIVERSAL_MANAGER_H

#include "pxr/pxr.h"
#include "pxr/base/tf/singleton.h"

#include "basisu/transcoder/basisu_transcoder.h"

#include <memory>


PXR_NAMESPACE_OPEN_SCOPE

using GlobalSelectorCodebook = basist::etc1_global_selector_codebook;

class BasisUniversalImageManager : public TfSingleton<BasisUniversalImageManager> {
public: 
    static BasisUniversalImageManager& GetInstance() {
        return TfSingleton<BasisUniversalImageManager>::GetInstance();
    }
    GlobalSelectorCodebook* GetGlobalSelectorCodebok();
    
private:
    friend class TfSingleton<BasisUniversalImageManager>;
    BasisUniversalImageManager() = default;
    GlobalSelectorCodebook* g_pGlobal_codebook = new GlobalSelectorCodebook(basist::g_global_selector_cb_size, basist::g_global_selector_cb);
    bool basisu_transcoder_initialized = false;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif
