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
#ifndef GARCH_UVTEXTURESTORAGE_DATA_H
#define GARCH_UVTEXTURESTORAGE_DATA_H

#include "pxr/pxr.h"
#include "pxr/imaging/garch/api.h"
#include "pxr/imaging/garch/baseTextureData.h"

#include "pxr/base/vt/value.h"

#include <boost/shared_ptr.hpp>

PXR_NAMESPACE_OPEN_SCOPE


TF_DECLARE_WEAK_AND_REF_PTRS(GarchUVTextureStorageData);

class GarchUVTextureStorageData : public GarchBaseTextureData
{
public:
    GARCH_API
    static GarchUVTextureStorageDataRefPtr
    New(unsigned int width,
        unsigned int height, 
        const VtValue &storageData);

    GARCH_API
    virtual ~GarchUVTextureStorageData();

   // GarchBaseTextureData overrides
    virtual int ResizedWidth(int mipLevel = 0) const override {
        return _resizedWidth;
    };

    virtual int ResizedHeight(int mipLevel = 0) const override {
        return _resizedHeight;
    };

    virtual GLenum GLInternalFormat() const override {
        return _glInternalFormat;
    };

    virtual GLenum GLFormat() const override {
        return _glFormat;
    };

    virtual GLenum GLType() const override {
        return _glType;
    };

    virtual size_t TargetMemory() const override {
        return _targetMemory;
    };

    virtual WrapInfo GetWrapInfo() const override {
        return _wrapInfo;
    };

    GARCH_API
    virtual size_t ComputeBytesUsed() const override;

    virtual size_t ComputeBytesUsedByMip(int mipLevel = 0) const override {
        return ComputeBytesUsed();
    }

    GARCH_API
    virtual bool HasRawBuffer(int mipLevel = 0) const override;

    GARCH_API
    virtual unsigned char * GetRawBuffer(int mipLevel = 0) const override;

    GARCH_API
    virtual bool Read(int degradeLevel, bool generateMipmap) override;

    GARCH_API
    virtual bool IsCompressed() const override;

    GARCH_API
    virtual int GetNumMipLevels() const override;

private:

    GarchUVTextureStorageData(
        unsigned int width,
        unsigned int height, 
        const VtValue &storageData);
        
    size_t _targetMemory;

    int _resizedWidth, _resizedHeight;
    int _bytesPerPixel;

    // Note: may not want to retain copy of original data
    // if _storageData is used for larger images
    VtValue _storageData; 

    GLenum  _glInternalFormat, _glFormat, _glType;

    WrapInfo _wrapInfo;

    int _size;

    unsigned char *_rawBuffer;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // GARCH_UVTEXTURESTORAGE_DATA_H
