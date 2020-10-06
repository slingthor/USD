//
//  basisUManager.h
//
//  Created by Calin Stanciu on 7/8/20.
//

#ifndef PXR_IMAGING_PLUGIN_GLFBASISUNIVERSAL_BASIS_UNIVERSAL_MANAGER_H
#define PXR_IMAGING_PLUGIN_GLFBASISUNIVERSAL_BASIS_UNIVERSAL_MANAGER_H

#include "pxr/pxr.h"
#include "pxr/base/tf/singleton.h"

#include "basisu_transcoder.h"

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