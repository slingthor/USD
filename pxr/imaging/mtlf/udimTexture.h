//
// Copyright 2018 Pixar
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
#ifndef MTLF_UDIMTEXTURE_H
#define MTLF_UDIMTEXTURE_H

/// \file mtlf/udimTexture.h

#include "pxr/pxr.h"
#include "pxr/imaging/mtlf/api.h"

#include "pxr/imaging/garch/udimTexture.h"

#include <string>
#include <vector>
#include <tuple>

PXR_NAMESPACE_OPEN_SCOPE

class MtlfUdimTexture;
TF_DECLARE_WEAK_AND_REF_PTRS(MtlfUdimTexture);

class MtlfUdimTexture : public GarchUdimTexture {
public:
    MTLF_API
    virtual ~MtlfUdimTexture();

protected:
    MTLF_API
    MtlfUdimTexture(
        TfToken const& imageFilePath,
        GarchImage::ImageOriginLocation originLocation,
        std::vector<std::tuple<int, TfToken>>&& tiles,
        bool const premultiplyAlpha,
        GarchImage::SourceColorSpace sourceColorSpace);

    friend class MtlfResourceFactory;

    MTLF_API
    virtual void _FreeTextureObject() override;

    MTLF_API
    virtual void _CreateGPUResources(unsigned int numChannels,
                                     GLenum const type,
                                     std::vector<_TextureSize> &mips,
                                     std::vector<std::vector<uint8_t>> &mipData,
                                     std::vector<float> &layoutData) override;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // MTLF_UDIMTEXTURE_H
