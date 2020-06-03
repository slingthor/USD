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
#include "pxr/pxr.h"

#include "pxr/imaging/hdSt/textureBinder.h"
#include "pxr/imaging/hdSt/samplerObject.h"
#include "pxr/imaging/hdSt/textureHandle.h"
#include "pxr/imaging/hdSt/textureObject.h"
#include "pxr/imaging/hdSt/resourceBinder.h"
#include "pxr/imaging/hd/vtBufferSource.h"

PXR_NAMESPACE_OPEN_SCOPE

static const HdTupleType _bindlessHandleTupleType{ HdTypeUInt32Vec2, 1 };

static
TfToken
_Concat(const TfToken &a, const TfToken &b)
{
    return TfToken(a.GetString() + b.GetString());
}

void
HdSt_TextureBinder::GetBufferSpecs(
    const NamedTextureHandleVector &textures,
    const bool useBindlessHandles,
    HdBufferSpecVector * const specs)
{
    for (const NamedTextureHandle & texture : textures) {
        switch (texture.type) {
        case HdTextureType::Uv:
            if (useBindlessHandles) {
                specs->emplace_back(
                    texture.name,
                    _bindlessHandleTupleType);
            } else {
                specs->emplace_back(
                    _Concat(
                        texture.name,
                        HdSt_ResourceBindingSuffixTokens->valid),
                    HdTupleType{HdTypeUInt32, 1});
            }
            break;
        case HdTextureType::Field:
            if (useBindlessHandles) {
                specs->emplace_back(
                    texture.name,
                    _bindlessHandleTupleType);
            } else {
                specs->emplace_back(
                    _Concat(
                        texture.name,
                        HdSt_ResourceBindingSuffixTokens->valid),
                    HdTupleType{HdTypeUInt32, 1});
            }
            specs->emplace_back(
                _Concat(
                    texture.name,
                    HdSt_ResourceBindingSuffixTokens->samplingTransform),
                HdTupleType{HdTypeDoubleMat4, 1});
            break;
        case HdTextureType::Ptex:
            if (useBindlessHandles) {
                specs->emplace_back(
                    texture.name,
                    _bindlessHandleTupleType);
                specs->emplace_back(
                    _Concat(
                        texture.name,
                        HdSt_ResourceBindingSuffixTokens->layout),
                    _bindlessHandleTupleType);
            }
            break;
        case HdTextureType::Udim:
            if (useBindlessHandles) {
                specs->emplace_back(
                    texture.name,
                    _bindlessHandleTupleType);
                specs->emplace_back(
                    _Concat(
                        texture.name,
                        HdSt_ResourceBindingSuffixTokens->layout),
                    _bindlessHandleTupleType);
            }
            break;
        }
    }
}

namespace {

// A bindless GL sampler buffer.
// This identifies a texture as a 64-bit handle, passed to GLSL as "uvec2".
// See https://www.khronos.org/opengl/wiki/Bindless_Texture
class HdSt_BindlessSamplerBufferSource : public HdBufferSource {
public:
    HdSt_BindlessSamplerBufferSource(TfToken const &name,
                                     const GLuint64EXT value)
    : HdBufferSource()
    , _name(name)
    , _value(value)
    {
    }

    ~HdSt_BindlessSamplerBufferSource() override = default;

    TfToken const &GetName() const override {
        return _name;
    }
    void const* GetData() const override {
        return &_value;
    }
    HdTupleType GetTupleType() const override {
        return _bindlessHandleTupleType;
    }
    size_t GetNumElements() const override {
        return 1;
    }
    void GetBufferSpecs(HdBufferSpecVector *specs) const override {
        specs->emplace_back(_name, GetTupleType());
    }
    bool Resolve() override {
        if (!_TryLock()) return false;
        _SetResolved();
        return true;
    }

protected:
    bool _CheckValid() const override {
        return true;
    }

private:
    const TfToken _name;
    const GLuint64EXT _value;
};

class _ComputeBufferSourcesFunctor {
public:
    static void Compute(
        TfToken const &name,
        HdStUvTextureObject const &texture,
        HdStUvSamplerObject const &sampler,
        const bool useBindlessHandles,
        HdBufferSourceSharedPtrVector * const sources)
    {
        if (useBindlessHandles) {
            sources->push_back(
                std::make_shared<HdSt_BindlessSamplerBufferSource>(
                    name,
                    sampler.GetGLTextureSamplerHandle()));
        } else {
            sources->push_back(
                std::make_shared<HdVtBufferSource>(
                    _Concat(
                        name,
                        HdSt_ResourceBindingSuffixTokens->valid),
                    VtValue((uint32_t)texture.IsValid())));
        }
    }

    static void Compute(
        TfToken const &name,
        HdStFieldTextureObject const &texture,
        HdStFieldSamplerObject const &sampler,
        const bool useBindlessHandles,
        HdBufferSourceSharedPtrVector * const sources)
    {
        sources->push_back(
            std::make_shared<HdVtBufferSource>(
                _Concat(
                    name,
                    HdSt_ResourceBindingSuffixTokens->samplingTransform),
                VtValue(texture.GetSamplingTransform())));

        if (useBindlessHandles) {
            sources->push_back(
                std::make_shared<HdSt_BindlessSamplerBufferSource>(
                    name,
                    sampler.GetGLTextureSamplerHandle()));
        } else {
            sources->push_back(
                std::make_shared<HdVtBufferSource>(
                    _Concat(
                        name,
                        HdSt_ResourceBindingSuffixTokens->valid),
                    VtValue((uint32_t)texture.IsValid())));
        }
    }

    static void Compute(
        TfToken const &name,
        HdStPtexTextureObject const &texture,
        HdStPtexSamplerObject const &sampler,
        const bool useBindlessHandles,
        HdBufferSourceSharedPtrVector * const sources)
    {
        if (!useBindlessHandles) {
            return;
        }

        sources->push_back(
            std::make_shared<HdSt_BindlessSamplerBufferSource>(
                name,
                sampler.GetTexelsGLTextureHandle()));

        sources->push_back(
            std::make_shared<HdSt_BindlessSamplerBufferSource>(
                _Concat(
                    name,
                    HdSt_ResourceBindingSuffixTokens->layout),
                sampler.GetLayoutGLTextureHandle()));
    }

    static void Compute(
        TfToken const &name,
        HdStUdimTextureObject const &texture,
        HdStUdimSamplerObject const &sampler,
        const bool useBindlessHandles,
        HdBufferSourceSharedPtrVector * const sources)
    {
        if (!useBindlessHandles) {
            return;
        }

        sources->push_back(
            std::make_shared<HdSt_BindlessSamplerBufferSource>(
                name,
                sampler.GetTexelsGLTextureHandle()));

        sources->push_back(
            std::make_shared<HdSt_BindlessSamplerBufferSource>(
                _Concat(
                    name,
                    HdSt_ResourceBindingSuffixTokens->layout),
                sampler.GetLayoutGLTextureHandle()));
    }
};


template<HdTextureType textureType, class Functor, typename ...Args>
void _CastAndCompute(
    HdStShaderCode::NamedTextureHandle const &namedTextureHandle,
    Args&& ...args)
{
    // e.g. HdStUvTextureObject
    using TextureObject = HdStTypedTextureObject<textureType>;
    // e.g. HdStUvSamplerObject
    using SamplerObject = HdStTypedSamplerObject<textureType>;

    const TextureObject * const typedTexture =
        dynamic_cast<TextureObject *>(
            namedTextureHandle.handle->GetTextureObject().get());
    if (!typedTexture) {
        TF_CODING_ERROR("Bad texture object");
        return;
    }

    const SamplerObject * const typedSampler =
        dynamic_cast<SamplerObject *>(
            namedTextureHandle.handle->GetSamplerObject().get());
    if (!typedSampler) {
        TF_CODING_ERROR("Bad sampler object");
        return;
    }

    Functor::Compute(namedTextureHandle.name, *typedTexture, *typedSampler,
                     std::forward<Args>(args)...);
}

template<class Functor, typename ...Args>
void _Dispatch(
    HdStShaderCode::NamedTextureHandle const &namedTextureHandle,
    Args&& ...args)
{
    switch (namedTextureHandle.type) {
    case HdTextureType::Uv:
        _CastAndCompute<HdTextureType::Uv, Functor>(
            namedTextureHandle, std::forward<Args>(args)...);
        break;
    case HdTextureType::Field:
        _CastAndCompute<HdTextureType::Field, Functor>(
            namedTextureHandle, std::forward<Args>(args)...);
        break;
    case HdTextureType::Ptex:
        _CastAndCompute<HdTextureType::Ptex, Functor>(
            namedTextureHandle, std::forward<Args>(args)...);
        break;
    case HdTextureType::Udim:
        _CastAndCompute<HdTextureType::Udim, Functor>(
            namedTextureHandle, std::forward<Args>(args)...);
        break;
    }
}

template<class Functor, typename ...Args>
void _Dispatch(
    HdStShaderCode::NamedTextureHandleVector const &textures,
    Args &&... args)
{
    for (const HdStShaderCode::NamedTextureHandle & texture : textures) {
        _Dispatch<Functor>(texture, std::forward<Args>(args)...);
    }
}

} // end anonymous namespace

void
HdSt_TextureBinder::ComputeBufferSources(
    const NamedTextureHandleVector &textures,
    const bool useBindlessHandles,
    HdBufferSourceSharedPtrVector * const sources)
{
    _Dispatch<_ComputeBufferSourcesFunctor>(
        textures, useBindlessHandles, sources);
}

PXR_NAMESPACE_CLOSE_SCOPE
