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
#ifndef GARCH_TEXTURE_REGISTRY_H
#define GARCH_TEXTURE_REGISTRY_H

/// \file garch/textureRegistry.h

#include "pxr/pxr.h"
#include "pxr/imaging/garch/api.h"
#include "pxr/base/tf/declarePtrs.h"
#include "pxr/base/tf/refPtr.h"
#include "pxr/base/tf/singleton.h"
#include "pxr/base/tf/token.h"
#include "pxr/base/tf/weakPtr.h"
#include "pxr/base/vt/dictionary.h"

#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>
#include <map>

PXR_NAMESPACE_OPEN_SCOPE


TF_DECLARE_WEAK_AND_REF_PTRS(GarchTextureHandle);
TF_DECLARE_WEAK_AND_REF_PTRS(GarchTexture);

class GarchRankedTypeMap;
class GarchTextureFactoryBase;

/// \class GarchTextureRegistry
///
class GarchTextureRegistry : boost::noncopyable {
public:
    GARCH_API
    static GarchTextureRegistry & GetInstance();

    GARCH_API
    GarchTextureHandleRefPtr GetTextureHandle(const TfToken &texture);
    GARCH_API
    GarchTextureHandleRefPtr GetTextureHandle(const TfTokenVector &textures);
    GARCH_API
    GarchTextureHandleRefPtr GetTextureHandle(GarchTextureRefPtr texture);

    // garbage collection methods
    GARCH_API
    void RequiresGarbageCollection();
    GARCH_API
    void GarbageCollectIfNeeded();

    // Returns true if the registry contains a texture sampler for \a texture;
    GARCH_API
    bool HasTexture(const TfToken &texture) const;

    // diagnostics
    GARCH_API
    std::vector<VtDictionary> GetTextureInfos() const;

    // Resets the registry contents. Clients that call this are expected to
    // manage their texture handles accordingly.
    GARCH_API
    void Reset();

private:
    friend class TfSingleton< GarchTextureRegistry >;
    GarchTextureRegistry();

    GarchTextureHandleRefPtr _CreateTexture(const TfToken &texture);
    GarchTextureHandleRefPtr _CreateTexture(const TfTokenVector &textures,
                                           const size_t numTextures);
    GarchTextureFactoryBase* _GetTextureFactory(const TfToken &filename);

    // Metadata for texture files to aid in cache invalidation.
    // Because texture arrays are stored as a single registry entry, their
    // metadata is also aggregated into a single _TextureMetadata instance.
    class _TextureMetadata
    {
    public:
        _TextureMetadata();

        // Collect metadata for a texture.
        explicit _TextureMetadata(const TfToken &texture);

        // Collect metadata for a texture array.
        explicit _TextureMetadata(const TfTokenVector &textures);

        // Compares metadata (but not handles) to see if two _TextureMetadatas
        // are the same (i.e. they are very likely to be the same on disk.)
        bool IsMetadataEqual(const _TextureMetadata &other) const;

        const GarchTextureHandleRefPtr &GetHandle() const;
        void SetHandle(const GarchTextureHandleRefPtr &handle);

    private:
        _TextureMetadata(const TfToken *textures,
                         const std::uint32_t numTextures);

        std::uint32_t _numTextures;
        off_t _fileSize;
        double _mtime;
        GarchTextureHandleRefPtr _handle;
    };

public:
    typedef std::map<TfToken, _TextureMetadata> TextureRegistryMap;
    typedef std::map<GarchTexturePtr, GarchTextureHandlePtr> TextureRegistryNonSharedMap;

private:

    // Map of file extensions to texture types.
    boost::scoped_ptr<GarchRankedTypeMap> _typeMap;

    // registry for shared textures
    TextureRegistryMap _textureRegistry;

    // registry for non-shared textures (drawtargets)
    TextureRegistryNonSharedMap _textureRegistryNonShared;

    bool _requiresGarbageCollection;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif // GARCH_TEXTURE_REGISTRY_H
