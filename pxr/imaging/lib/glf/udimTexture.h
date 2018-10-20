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
#ifndef GLF_UDIMTEXTURE_H
#define GLF_UDIMTEXTURE_H

/// \file Glf/udimTexture.h

#include "pxr/pxr.h"
#include "pxr/imaging/garch/api.h"

#include "pxr/imaging/garch/udimTexture.h"

#include <string>
#include <vector>
#include <tuple>

PXR_NAMESPACE_OPEN_SCOPE

class GlfUdimTexture;
TF_DECLARE_WEAK_AND_REF_PTRS(GlfUdimTexture);

class GlfUdimTexture : public GarchUdimTexture {
public:
    GLF_API
    virtual ~GlfUdimTexture();

protected:
    GLF_API
    GlfUdimTexture(
        TfToken const& imageFilePath,
        GarchImage::ImageOriginLocation originLocation,
        std::vector<std::tuple<int, TfToken>>&& tiles);

    friend class GlfResourceFactory;

    GLF_API
    virtual void _FreeTextureObject() override;

    GLF_API
    virtual void _CreateGPUResources(unsigned int numChannels,
                                     GLenum const type,
                                     std::vector<_TextureSize> &mips,
                                     std::vector<std::vector<uint8_t>> &mipData,
                                     std::vector<float> &layoutData) override;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // GLF_UDIMTEXTURE_H
