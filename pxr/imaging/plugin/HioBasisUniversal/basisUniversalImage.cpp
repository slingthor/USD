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

#include "pxr/base/tf/singleton.h"
#include "pxr/pxr.h"
#include "pxr/imaging/hio/image.h"
#include "pxr/imaging/hio/types.h"
#include "pxr/imaging/plugin/HioBasisUniversal/basisUniversalImageManager.h"
#include "pxr/usd/ar/asset.h"
#include "pxr/usd/ar/resolver.h"

#include "basisu/transcoder/basisu_transcoder.h"
#include "basisu/basisu_enc.h"

PXR_NAMESPACE_OPEN_SCOPE

using ETC1GlobalSelectorCodebook = basist::etc1_global_selector_codebook;
using BasisUTranscoder = basist::basisu_transcoder;
using BasisUTranscoderSharedPtr = std::shared_ptr<BasisUTranscoder>;
using BasisUTextureFormat = basist::transcoder_texture_format;

using BasisTTFmt = basist::transcoder_texture_format;

#define diffuseColor_IS_PREMULTIPLIED 1

/// \class HioBasisUniversalImage
///
/// This class is used for reading a texture format (compressed or not)
/// from the .basis file type.
///
/// .basis file : Basis Universal Texture format that allows multiple
/// types of texture compressions.

class HioBasisUniversalImage : public HioImage
{
public:
    using Base = HioImage;

    HioBasisUniversalImage()
    : _subImage(std::numeric_limits<int>::min())
    , _mipLevel(std::numeric_limits<int>::min())
    , _sourceColorSpace(HioImage::SourceColorSpace::Auto)
    , _basisFileContent(nullptr) {}

    virtual ~HioBasisUniversalImage() = default;

    virtual bool Read(const StorageSpec &storage) override;
    virtual bool ReadCropped(int cropTop,
                             int cropBottom,
                             int cropLeft,
                             int cropRight,
                             const StorageSpec &storage) override;
    virtual bool Write(const StorageSpec &storage,
                       const VtDictionary &metadata = VtDictionary()) override
    {
        return false;
    };
    virtual const std::string &GetFilename() const override;
    virtual int GetWidth() const override;
    virtual int GetHeight() const override;
    virtual HioFormat GetFormat() const override;
    virtual int GetBytesPerPixel() const override;
    virtual int GetNumMipLevels() const override;
    virtual bool IsColorSpaceSRGB() const override;
    virtual bool GetMetadata(const TfToken &key, VtValue *value) const override
    {
        return false;
    };
    virtual bool GetSamplerMetadata(HioAddressDimension pname,
                                    VtValue *param) const override
    {
        return false;
    };

protected:
    virtual bool _OpenForReading(const std::string &filename,
                                 int subImage,
                                 int mip,
                                 SourceColorSpace sourceColorSpace,
                                 bool suppressErrors) override;
    virtual bool _OpenForWriting(const std::string &filename) override
    {
        return true;
    };

private:
    /// Mipmap level information.
    struct _MipMapLevelInfo
    {
        _MipMapLevelInfo()
        : originalWidth(std::numeric_limits<uint32_t>::min())
        , originalHeight(std::numeric_limits<uint32_t>::min())
        , totalNumBlocks(std::numeric_limits<uint32_t>::min())
        , hasAlpha(false) {}

        std::shared_ptr<uint8_t> compressedData;
        uint32_t originalWidth;
        uint32_t originalHeight;
        uint32_t totalNumBlocks;
        bool hasAlpha;
    };

    using CompressedDataByLevel = std::vector<_MipMapLevelInfo>;

    /// Subimage information contained in a texture.
    struct _GpuImage
    {
        _GpuImage(uint32_t i, uint32_t nLevels, BasisTTFmt f)
            : imageIndex{i}, format{f}
        {
            compressedDataMipMapsLevel.reserve((int)nLevels);
        }

        uint32_t imageIndex; //subImageIndex in texture
        BasisTTFmt format;
        CompressedDataByLevel compressedDataMipMapsLevel;
    };

    using GPUImages = std::vector<_GpuImage>;

    /// \class _BasisFile
    ///
    /// This class is responsible for parsing the .basis file format content
    /// into information that will be later used within the usd world.
    /// The class is instantiated with a data buffer and its size representing
    /// the .basis file content. This data is used for parsing all the necessary
    /// information is used to create a \class HioImage that will be used
    /// within the usd universe.
    class _BasisFile
    {
    public:
        _BasisFile(const std::string &fileName)
        : _decoder(BasisUniversalImageManager::GetInstance().
                   GetGlobalSelectorCodebook())
        , _data(nullptr)
        , _dataSize(0)
        , _isValidForReading(false)
        , _isReadyToUse(false)
        , _transcoderTexFmt(static_cast<BasisTTFmt>(
            basist::transcoder_texture_format::cTFRGBA32))
        {
            _Init(fileName);
        }

        bool
        IsValidFileForReading() const
        {
            return _isValidForReading;
        }

        bool
        IsReadyToUse()
        {
            return _isReadyToUse;
        }

        const _MipMapLevelInfo *
        GetImageMipMapLevelInfo(uint32_t imageIndex, uint32_t levelIndex) const;

        const BasisTTFmt &
        GetImageTTFmt(uint32_t imageIndex) const
        {
            return _gpuImages[imageIndex].format;
        }

        int
        GetImageNumberOfMipMapLevels(uint32_t imageIndex) const
        {
            return _gpuImages[imageIndex].compressedDataMipMapsLevel.size();
        }

    private:
        void _Init(const std::string &fileName);
        void _ParseBasisFileContent();

        basist::basisu_transcoder _decoder;
        std::shared_ptr<const char> _data;
        size_t _dataSize;
        bool _isValidForReading;
        bool _isReadyToUse;
        BasisTTFmt _transcoderTexFmt;
        GPUImages _gpuImages;
    };

    std::string _fileName;
    int _subImage;
    int _mipLevel;
    HioImage::SourceColorSpace _sourceColorSpace;
    std::shared_ptr<_BasisFile> _basisFileContent;
};

TF_REGISTRY_FUNCTION(TfType)
{
    using Image = HioBasisUniversalImage;
    TfType t = TfType::Define<Image, TfType::Bases<Image::Base>>();
    t.SetFactory<HioImageFactory<Image>>();
}

void
HioBasisUniversalImage::_BasisFile::_Init(const std::string &fileName)
{
    std::shared_ptr<ArAsset> asset =ArGetResolver().OpenAsset(fileName);
    if (!asset)
    {
        TF_CODING_ERROR("_BasisFile::_Init: Failed to open the file %s.",
                        fileName.c_str());
        return;
    }

    _data = asset->GetBuffer();
    if (_data == nullptr)
    {
        TF_CODING_ERROR("_BasisFile::_Init: Empty data buffer.");
        return;
    }
    _dataSize = asset->GetSize();
    _isValidForReading =
        _decoder.validate_header(_data.get(), _dataSize);
    _ParseBasisFileContent();
}

void
HioBasisUniversalImage::_BasisFile::_ParseBasisFileContent()
{
    if (!_isValidForReading)
    {
        TF_CODING_ERROR(
            "_BasisFile::_Init(): Basis file not valid for reading.");
        return;
    }
    if (_data == nullptr)
    {
        TF_CODING_ERROR("_BasisFile::_Init(): Invalid data buffer.");
        return;
    }
    if (_dataSize == (size_t)0 ||
        _dataSize >= std::numeric_limits<size_t>::max())
    {
        TF_CODING_ERROR("_BasisFile::_Init(): Invalid data buffer size.");
        return;
    }
    basist::basisu_file_info fileInfo;

    /// Obtain the file info from the data buffer.
    if (!_decoder.get_file_info(
        _data.get(), (uint32_t)_dataSize, fileInfo))
    {
        TF_CODING_ERROR("_BasisFile::_Init(): Error while obtaining the"
                        " file information from the data buffer.");
        return;
    }

    if (!basist::basis_is_format_supported(_transcoderTexFmt,
                                           fileInfo.m_tex_format))
    {
        /* currently only one format supported */
        TF_CODING_ERROR("_BasisFile::_Init(): Currently only supporting"
                        " one texture format.");
        return;
    }

    TF_VERIFY(fileInfo.m_total_images == fileInfo.m_image_mipmap_levels.size());
    TF_VERIFY(fileInfo.m_total_images == _decoder.get_total_images(_data.get(),
              (uint32_t)_dataSize));

    /// Start decoding the info and the parsing process.
    /// Always make sure that the decoding process is stopped/restarted whenever we need it to.
    if (!_decoder.start_transcoding(_data.get(), (uint32_t)_dataSize))
    {
        TF_CODING_ERROR("_BasisFile::_Init(): Error while starting the"
                        " transocding process.");
        return;
    }
    for (uint32_t imageIndex = 0;
         imageIndex < fileInfo.m_total_images;
         ++imageIndex)
    {
        _GpuImage gpuImage(imageIndex,
            (uint32_t)fileInfo.m_image_mipmap_levels.size(),
            _transcoderTexFmt);
        for (uint32_t levelIndex = 0;
             levelIndex < fileInfo.m_image_mipmap_levels[imageIndex];
             ++levelIndex)
        {
            basist::basisu_image_level_info levelInfo;
            if (!_decoder.get_image_level_info(_data.get(),
                (uint32_t)_dataSize, levelInfo, imageIndex, levelIndex))
            {
                TF_CODING_ERROR("_BasisFile::_Init(): Error while"
                                " obtaining the image level info.");
                return;
            }
            _MipMapLevelInfo mipMapLevelInfo;
            mipMapLevelInfo.hasAlpha = fileInfo.m_has_alpha_slices;
            mipMapLevelInfo.totalNumBlocks = levelInfo.m_total_blocks;
            mipMapLevelInfo.originalWidth = levelInfo.m_width;
            mipMapLevelInfo.originalHeight = levelInfo.m_height;

            std::shared_ptr<uint8_t> compressedData = nullptr;
            if (basist::basis_transcoder_format_is_uncompressed(
                _transcoderTexFmt))
            {
                uint32_t flags =
                    basist::cDecodeFlagsTranscodeAlphaDataToOpaqueFormats;
                const uint32_t bytesPerPixel =
                    basist::basis_get_uncompressed_bytes_per_pixel(
                        _transcoderTexFmt);
                const uint32_t bytesPerLine =
                    mipMapLevelInfo.originalWidth * bytesPerPixel;
                const uint32_t bytesPerSlice =
                    bytesPerLine * mipMapLevelInfo.originalHeight;
                compressedData = std::shared_ptr<uint8_t>(
                    new uint8_t[bytesPerSlice], [](uint8_t *ptr) {
                        delete[] ptr;
                    });
                const uint32_t numPixels = mipMapLevelInfo.originalWidth *
                    mipMapLevelInfo.originalHeight;
                if (!_decoder.transcode_image_level(_data.get(),
                                                    (uint32_t)_dataSize,
                                                    imageIndex, levelIndex,
                                                    compressedData.get(),
                                                    numPixels,
                                                    _transcoderTexFmt,
                                                    flags))
                {
                    TF_CODING_ERROR(
                        "_BasisFile::_Init(): Error while transocding the image"
                        " level for an uncompressed texture.");
                    return;
                }
            }
            else
            {
                uint32_t flags =
                    basist::cDecodeFlagsTranscodeAlphaDataToOpaqueFormats |
                    (mipMapLevelInfo.hasAlpha ?
                        basist::cDecodeFlagsOutputHasAlphaIndices : 0);
                const uint32_t bytesPerBlock =
                    basist::basis_get_bytes_per_block_or_pixel(
                        _transcoderTexFmt);
                const uint32_t requiredSize =
                    mipMapLevelInfo.totalNumBlocks * bytesPerBlock;
                compressedData = std::shared_ptr<uint8_t>(
                    new uint8_t[requiredSize], [](uint8_t *ptr) {
                        delete[] ptr;
                    });
                if (!_decoder.transcode_image_level(_data.get(),
                                                    (uint32_t)_dataSize,
                                                    imageIndex, levelIndex,
                                                    compressedData.get(),
                                                    requiredSize /bytesPerBlock,
                                                    _transcoderTexFmt,
                                                    flags))
                {
                    TF_CODING_ERROR(
                        "_BasisFile::_Init(): Error while transocding the image"
                        " level for a compressed texture.");
                    return;
                }
            }
            mipMapLevelInfo.compressedData = compressedData;
            gpuImage.compressedDataMipMapsLevel.push_back(mipMapLevelInfo);
        }
        _gpuImages.push_back(gpuImage);
    }

    /// Always make sure that the decoding process is stopped/restarted whenever we need it to.
    if (!_decoder.stop_transcoding())
    {
        TF_CODING_ERROR(
            "_BasisFile::_Init(): Error while stopping the basisu decoder.");
        return;
    }

    if (_gpuImages.empty() ||
        _gpuImages.size() != fileInfo.m_total_images)
    {
        TF_CODING_ERROR(
            "_BasisFile::_Init(): Error on parsing the .basis file.");
        return;
    }
    _isReadyToUse = true;
}

const HioBasisUniversalImage::_MipMapLevelInfo *
HioBasisUniversalImage::_BasisFile::GetImageMipMapLevelInfo(
    uint32_t imageIndex, uint32_t levelIndex) const
{
    const auto imageIter = std::find_if(_gpuImages.begin(),
        _gpuImages.end(), [&](const _GpuImage &elem) {
        return elem.imageIndex == imageIndex;
    });
    if (imageIter == _gpuImages.end())
    {
        TF_CODING_ERROR("_BasisFile::IniGetImageMipMapLevelInfot():"
                        "Invalid image index in texture.");
        return nullptr;
    }
    return &imageIter->compressedDataMipMapsLevel[levelIndex];
}

bool 
HioBasisUniversalImage::_OpenForReading(const std::string &fileName,
                                        int subImage,
                                        int mip,
                                        SourceColorSpace sourceColorSpace,
                                        bool suppressErrors)
{
    if (fileName.empty())
    {
        TF_CODING_ERROR(
            "HioBasisUniversalImage::_OpenForReading: File name is empty.");
        return false;
    }
    _fileName = fileName;
    _subImage = subImage;
    _mipLevel = mip;
    _sourceColorSpace = sourceColorSpace;

    _basisFileContent = std::make_shared<_BasisFile>(_fileName);
    return _basisFileContent->IsValidFileForReading();
}

bool 
HioBasisUniversalImage::Read(const StorageSpec &storage)
{
    return ReadCropped(0, 0, 0, 0, storage);
}

bool 
HioBasisUniversalImage::ReadCropped(int cropTop,
                                     int cropBottom,
                                     int cropLeft,
                                     int cropRight,
                                     const StorageSpec &storage)
{
    TF_VERIFY(_basisFileContent->IsReadyToUse());
    if (cropTop > 0 ||
        cropBottom > 0 ||
        cropLeft > 0 ||
        cropRight > 0)
    {
        TF_CODING_ERROR(
            "HioBasisUniversalImage::ReadCropped: Cropping not yet supported"
            " for .basis file format.");
        return false;
    }
    else
    {
        const _MipMapLevelInfo *mipMapLevelInfo =
            _basisFileContent->GetImageMipMapLevelInfo(_subImage, _mipLevel);
        if (mipMapLevelInfo == nullptr)
        {
            TF_CODING_ERROR(
                "HioBasisUniversalImage::ReadCropped: no mipmap level info"
                " found for subimage %i and level %i.", _subImage, _mipLevel);
            return false;
        }
        const BasisTTFmt & transcoderTexFmt =
            _basisFileContent->GetImageTTFmt((uint32_t)_subImage);
        uint32_t imageSize = 0;
        if (basist::basis_transcoder_format_is_uncompressed(transcoderTexFmt))
        {
            const uint32_t bytesPerPixel =
                basist::basis_get_uncompressed_bytes_per_pixel(
                    transcoderTexFmt);
            const uint32_t bytesPerLine =
                mipMapLevelInfo->originalWidth * bytesPerPixel;
            imageSize = bytesPerLine * mipMapLevelInfo->originalHeight;
        }
        else
        {
            const uint32_t bytesPerBlock =
                basist::basis_get_bytes_per_block_or_pixel(transcoderTexFmt);
            imageSize = mipMapLevelInfo->totalNumBlocks * bytesPerBlock;
        }
        memcpy(storage.data, mipMapLevelInfo->compressedData.get(), imageSize);
    }
    return true;
}

bool 
HioBasisUniversalImage::IsColorSpaceSRGB() const
{
    // Texture had no (recognized) gamma hint, make a reasonable guess
    switch (_basisFileContent->GetImageTTFmt((uint32_t)_subImage))
    {
    case basist::transcoder_texture_format::cTFRGBA32:
    case basist::transcoder_texture_format::cTFBC3_RGBA:
    case basist::transcoder_texture_format::cTFBC7_RGBA:
    case basist::transcoder_texture_format::cTFBC1_RGB:
        return true;
    default:
        TF_CODING_ERROR("Unsupported basis u format");
        return false;
    }
}

const std::string &
HioBasisUniversalImage::GetFilename() const
{
    return _fileName;
}

int 
HioBasisUniversalImage::GetWidth() const
{
    const _MipMapLevelInfo *mipMapLevelinfo =
        _basisFileContent->GetImageMipMapLevelInfo(_subImage, _mipLevel);
    if (mipMapLevelinfo == nullptr)
    {
        return std::numeric_limits<int>::min();
    }
    return (int)mipMapLevelinfo->originalWidth;
}

int 
HioBasisUniversalImage::GetHeight() const
{
    const _MipMapLevelInfo *mipMapLevelinfo =
        _basisFileContent->GetImageMipMapLevelInfo(_subImage, _mipLevel);
    if (mipMapLevelinfo == nullptr)
    {
        return std::numeric_limits<int>::min();
    }
    return (int)mipMapLevelinfo->originalHeight;
}

int 
HioBasisUniversalImage::GetBytesPerPixel() const
{
    const _MipMapLevelInfo *mipMapLevelinfo =
        _basisFileContent->GetImageMipMapLevelInfo(_subImage, _mipLevel);
    if (mipMapLevelinfo == nullptr)
    {
        return std::numeric_limits<int>::min();
    }
    const BasisTTFmt & transcoderTexFmt =
        _basisFileContent->GetImageTTFmt((uint32_t)_subImage);
    if (basist::basis_transcoder_format_is_uncompressed(transcoderTexFmt))
    {
        return basist::basis_get_uncompressed_bytes_per_pixel(transcoderTexFmt);
    }

    return basist::basis_get_bytes_per_block_or_pixel(transcoderTexFmt);
}

int 
HioBasisUniversalImage::GetNumMipLevels() const
{
    return _basisFileContent->GetImageNumberOfMipMapLevels(_subImage);
}

HioFormat
HioBasisUniversalImage::GetFormat() const
{
    switch (_basisFileContent->GetImageTTFmt((uint32_t)_subImage))
    {
    case basist::transcoder_texture_format::cTFRGBA32:
        return HioFormatUNorm8Vec4;
    case basist::transcoder_texture_format::cTFBC1_RGB:
        return HioFormatBC1UNorm8Vec4;
    case basist::transcoder_texture_format::cTFBC3_RGBA:
        return HioFormatBC3UNorm8Vec4;
    case basist::transcoder_texture_format::cTFBC7_RGBA:
        return HioFormatBC7UNorm8Vec4;
    default:
        TF_CODING_ERROR("Unsupported basis u format");
        return HioFormatUNorm8Vec4;
    }
}
PXR_NAMESPACE_CLOSE_SCOPE
