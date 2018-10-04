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
/// \file textureRegistry.cpp

#include "pxr/imaging/garch/textureRegistry.h"
#include "pxr/imaging/garch/debugCodes.h"
#include "pxr/imaging/garch/rankedTypeMap.h"
#include "pxr/imaging/garch/texture.h"
#include "pxr/imaging/garch/textureHandle.h"
#include "pxr/imaging/garch/image.h"

#include "pxr/usd/ar/resolver.h"

#include "pxr/base/arch/fileSystem.h"
#include "pxr/base/tf/instantiateSingleton.h"
#include "pxr/base/tf/stl.h"

#include "pxr/base/trace/trace.h"

#include <cstdint>

PXR_NAMESPACE_OPEN_SCOPE


TF_INSTANTIATE_SINGLETON( GarchTextureRegistry );

GarchTextureRegistry &
GarchTextureRegistry::GetInstance() {
    return TfSingleton<GarchTextureRegistry>::GetInstance();
}

GarchTextureRegistry::GarchTextureRegistry() :
    _typeMap(new GarchRankedTypeMap),
    _requiresGarbageCollection(false)
{
    TfSingleton<GarchTextureRegistry>::SetInstanceConstructed(*this);

    // Register all texture types using plugin metadata.
    _typeMap->Add(TfType::Find<GarchTexture>(), "textureTypes",
                 GARCH_DEBUG_TEXTURE_PLUGINS);
}

GarchTextureHandleRefPtr
GarchTextureRegistry::GetTextureHandle(const TfToken &texture,
                                       GarchImage::ImageOriginLocation originLocation)
{
    GarchTextureHandleRefPtr textureHandle;

    _TextureMetadata md(texture);

    // look into exisiting textures
    std::map<std::pair<TfToken, GarchImage::ImageOriginLocation>,
             _TextureMetadata>::iterator it =
        _textureRegistry.find(std::make_pair(texture, originLocation));

    if (it != _textureRegistry.end() && it->second.IsMetadataEqual(md)) {
        textureHandle = it->second.GetHandle();
    } else {
        // if not exists, create it
        textureHandle = _CreateTexture(texture, originLocation);
        md.SetHandle(textureHandle);
        _textureRegistry[std::make_pair(texture, originLocation)] = md;
    }

    return textureHandle;
}

GarchTextureHandleRefPtr
GarchTextureRegistry::GetTextureHandle(const TfTokenVector &textures,
                                       GarchImage::ImageOriginLocation originLocation)
{
    if (textures.empty()) {
        TF_WARN("Attempting to register arrayTexture with empty token vector.");
        return GarchTextureHandlePtr();
    }

    const size_t numTextures = textures.size();
    // We register an array texture with the
    // path of the first texture in the array
    TfToken texture = textures[0];
    GarchTextureHandleRefPtr textureHandle;

    _TextureMetadata md(textures);

    // look into exisiting textures
    std::map<std::pair<TfToken, GarchImage::ImageOriginLocation>,
             _TextureMetadata>::iterator it =
        _textureRegistry.find(std::make_pair(texture, originLocation));
    
    if (it != _textureRegistry.end() && it->second.IsMetadataEqual(md)) {
        textureHandle = it->second.GetHandle();
    } else {
        // if not exists, create it
        textureHandle = _CreateTexture(textures, numTextures,
                                       originLocation);
        md.SetHandle(textureHandle);
        _textureRegistry[std::make_pair(texture, originLocation)] = md;
    }

    return textureHandle;
}

GarchTextureHandleRefPtr
GarchTextureRegistry::GetTextureHandle(GarchTextureRefPtr texture)
{
    GarchTextureHandleRefPtr textureHandle;

    // Texture maybe null if an error occured, don't
    // add it to the registry in this case
    if (texture) {
        std::map<GarchTexturePtr, GarchTextureHandlePtr>::iterator it =
            _textureRegistryNonShared.find(texture);

        if (it != _textureRegistryNonShared.end()) {
            textureHandle = it->second;
        }

        // if not exists or it has expired, create a handle
        if (!textureHandle)
        {
            textureHandle = GarchTextureHandle::New(texture);
            _textureRegistryNonShared[texture] = textureHandle;
        }
    }

    return textureHandle;
}

bool
GarchTextureRegistry::HasTexture(const TfToken &texture,
                                 GarchImage::ImageOriginLocation originLocation) const
{
    // look into exisiting textures
    std::map<std::pair<TfToken, GarchImage::ImageOriginLocation>,
             _TextureMetadata>::const_iterator it =
        _textureRegistry.find(std::make_pair(texture, originLocation));
    
    return (it != _textureRegistry.end());
}

GarchTextureHandleRefPtr
GarchTextureRegistry::_CreateTexture(const TfToken &texture,
                                     GarchImage::ImageOriginLocation originLocation)
{
    GarchTextureRefPtr result;
    if (GarchTextureFactoryBase* factory = _GetTextureFactory(texture)) {
        result = factory->New(texture, originLocation);
        if (!result) {
            TF_CODING_ERROR("[PluginLoad] Cannot construct texture for "
                            "type '%s'\n",
                            TfStringGetSuffix(texture).c_str());
        }
    }
    return result ? GarchTextureHandle::New(result) : TfNullPtr;
}

GarchTextureHandleRefPtr
GarchTextureRegistry::_CreateTexture(const TfTokenVector &textures,
                                     const size_t numTextures,
                                     GarchImage::ImageOriginLocation originLocation)
{
    GarchTextureRefPtr result;
    TfToken filename = textures.empty() ? TfToken() : textures.front();
    if (GarchTextureFactoryBase* factory = _GetTextureFactory(filename)) {
        result = factory->New(textures, originLocation);
        if (!result) {
            TF_CODING_ERROR("[PluginLoad] Cannot construct texture for "
                            "type '%s'\n",
                            TfStringGetSuffix(filename).c_str());
        }
    }
    return result ? GarchTextureHandle::New(result) : TfNullPtr;
}

GarchTextureFactoryBase*
GarchTextureRegistry::_GetTextureFactory(const TfToken &filename)
{
    // Lookup the plug-in type name based on the file extension.
    TfToken fileExtension(ArGetResolver().GetExtension(filename));

    TfType pluginType = _typeMap->Find(fileExtension);
    if (!pluginType) {
        // Unknown type.  Try the wildcard.
        pluginType = _typeMap->Find(TfToken("*"));
        if (!pluginType) {
            TF_DEBUG(GARCH_DEBUG_TEXTURE_PLUGINS).Msg(
                    "[PluginLoad] Unknown texture type '%s'\n",
                    fileExtension.GetText());
            return nullptr;
        }
    }

    PlugRegistry& plugReg = PlugRegistry::GetInstance();
    PlugPluginPtr plugin = plugReg.GetPluginForType(pluginType);
    if (!plugin || !plugin->Load()) {
        TF_CODING_ERROR("[PluginLoad] PlugPlugin could not be loaded for "
                        "TfType '%s'\n",
                        pluginType.GetTypeName().c_str());
        return nullptr;
    }

    TF_DEBUG(GARCH_DEBUG_TEXTURE_IMAGE_PLUGINS).Msg(
    	        "[PluginLoad] Loaded plugin '%s' for texture type '%s'\n",
                pluginType.GetTypeName().c_str(),
                fileExtension.GetText());

    if (GarchTextureFactoryBase* factory =
            pluginType.GetFactory<GarchTextureFactoryBase>()) {
        return factory;
    }
    TF_CODING_ERROR("[PluginLoad] Cannot manufacture type '%s' "
                    "for texture type '%s'\n",
                    pluginType.GetTypeName().c_str(),
                    fileExtension.GetText());

    return nullptr;
}

void
GarchTextureRegistry::RequiresGarbageCollection()
{
    // Not executes GC right now to ensure the texture existance
    // between sampler reassignment in short term.
    _requiresGarbageCollection = true;
}

void
GarchTextureRegistry::GarbageCollectIfNeeded()
{
    // Even if we hold the list of texture handles to be deleted, we have to
    // traverse entire map to remove the entry for them. So simple flag works
    // enough to avoid unnecessary process.
    if (!_requiresGarbageCollection) return;

    // XXX:
    // Frequent garbage collection causing slow UI when reading textures.
    // We're freeing and re-loading textures instead of caching them.
    // 
    // Can we only garbage collect when GPU memory is high?  Or have a
    // least-recently-used queue or something?
    TRACE_FUNCTION();

    std::map<std::pair<TfToken, GarchImage::ImageOriginLocation>,
             _TextureMetadata>::iterator it =
        _textureRegistry.begin();
    while (it != _textureRegistry.end()){
        if ((it->second.GetHandle()->IsUnique()) ) {
            _textureRegistry.erase(it++);
            // TextureHandle (and its GarchTexture) will be released here.
        } else {
            ++it;
        }
    }

    // we only have a weakptr for non-shared texture handle (i.e. DrawTarget)
    // note: Since the lifetime of drawtarget attachment is controlled by
    // GarchDrawTarget, even though there are no samplers refers to that
    // attachment, it may still exists when this GC function is called.
    // As a result the entry of textureHandle might remain in 
    // _textureRegistryNonShared, but it just holds an invalid WeakPtr and 
    // will be cleaned at the next GC opportunity. so it's no harm.
    {
        std::map<GarchTexturePtr, GarchTextureHandlePtr>::iterator it =
            _textureRegistryNonShared.begin();
        while (it != _textureRegistryNonShared.end() ){
            if (it->second.IsExpired()) {
                // TextureHandle has already been released by its owner
                // (GarchDrawTarget)
                _textureRegistryNonShared.erase(it++);
            } else {
                ++it;
            }
        }
    }

    _requiresGarbageCollection = false;
}

std::vector<VtDictionary>
GarchTextureRegistry::GetTextureInfos() const
{
    std::vector<VtDictionary> result;

    // In the event of errors, both texture handle or the texture the
    // handle can point to can be null.
    for (TextureRegistryMap::value_type const& p : _textureRegistry) {
        GarchTextureHandlePtr const &textureHandle = p.second.GetHandle();
        if (textureHandle) {
            GarchTexturePtr const &texture = textureHandle->GetTexture();
            VtDictionary info;
            if (texture) {
                info = texture->GetTextureInfo(false);
            }

            info["uniqueIdentifier"] =
                (uint64_t)textureHandle.GetUniqueIdentifier();
            result.push_back(info);
        }
    }
    for (TextureRegistryNonSharedMap::value_type const& p :
            _textureRegistryNonShared) {
        GarchTextureHandlePtr const &textureHandle = p.second;

        // note: Since textureRegistryNonShared stores weak ptr, we have to 
        // check whether it still exists here.
        if (!textureHandle.IsExpired()) {
            GarchTexturePtr const &texture = textureHandle->GetTexture();

            VtDictionary info;
            if (texture) {
                info = texture->GetTextureInfo(false);
            }
            info["uniqueIdentifier"] =
                (uint64_t)textureHandle->GetUniqueIdentifier();
            result.push_back(info);
        }
    }

    return result;
}

void
GarchTextureRegistry::Reset()
{
    TfReset(_textureRegistry);
    TfReset(_textureRegistryNonShared);
}


GarchTextureRegistry::_TextureMetadata::_TextureMetadata()
    : _TextureMetadata(nullptr, 0)
{}

GarchTextureRegistry::_TextureMetadata::_TextureMetadata(
    const TfToken &texture)
    : _TextureMetadata(&texture, 1)
{}

GarchTextureRegistry::_TextureMetadata::_TextureMetadata(
    const TfTokenVector &textures)
    : _TextureMetadata(textures.data(), textures.size())
{}

GarchTextureRegistry::_TextureMetadata::_TextureMetadata(
    const TfToken *textures, const std::uint32_t numTextures)
    : _numTextures(numTextures)
    , _fileSize(0)
    , _mtime(0)
{
    TRACE_FUNCTION();

    for (std::uint32_t i = 0; i < numTextures; ++i) {
        const TfToken& tex = textures[i];
        double time;
        if (!ArchGetModificationTime(tex.GetText(), &time)) {
            continue;
        }
        int64_t size = ArchGetFileLength(tex.GetText());
        if (size == -1) {
            continue;
        }

        // The file size is not a particularly good indicator that the texture
        // has changed (i.e. uncompressed images with the same dimensions,
        // depth, etc are very likely to have the same size even if they are
        // different.)
        //
        // We aggregate the size of every file in the texture array, but use
        // the most recent mtime of any file so that we reload the array if any
        // file is modified.
        _fileSize += size;
        _mtime = std::max(_mtime, time);
    }
}

inline bool
GarchTextureRegistry::_TextureMetadata::IsMetadataEqual(
    const _TextureMetadata &other) const
{
    return _numTextures == other._numTextures && 
        _fileSize == other._fileSize && 
        _mtime == other._mtime;
}

inline const GarchTextureHandleRefPtr &
GarchTextureRegistry::_TextureMetadata::GetHandle() const
{
    return _handle;
}

inline void
GarchTextureRegistry::_TextureMetadata::SetHandle(
    const GarchTextureHandleRefPtr &handle)
{
    _handle = handle;
}

PXR_NAMESPACE_CLOSE_SCOPE

