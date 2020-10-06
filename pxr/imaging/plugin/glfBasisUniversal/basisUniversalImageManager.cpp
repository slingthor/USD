//
//  basisUManager.cpp
//
//  Created by Calin Stanciu on 7/8/20.
//

#include "pxr/imaging/plugin/glfBasisUniversal/basisUniversalImageManager.h"

#include "pxr/base/tf/instantiateSingleton.h"

PXR_NAMESPACE_OPEN_SCOPE

TF_INSTANTIATE_SINGLETON(BasisUniversalImageManager);

GlobalSelectorCodebook*
BasisUniversalImageManager::GetGlobalSelectorCodebok() {
    if (!basisu_transcoder_initialized) {
        basist::basisu_transcoder_init();
        basisu_transcoder_initialized = true;
    }
    return g_pGlobal_codebook;
}

PXR_NAMESPACE_CLOSE_SCOPE