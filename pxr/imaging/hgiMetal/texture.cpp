//
// Copyright 2019 Pixar
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
#include <Metal/Metal.h>

#include "pxr/base/arch/defines.h"

#include "pxr/imaging/hgiMetal/hgi.h"
#include "pxr/imaging/hgiMetal/diagnostic.h"
#include "pxr/imaging/hgiMetal/conversions.h"
#include "pxr/imaging/hgiMetal/texture.h"


PXR_NAMESPACE_OPEN_SCOPE

HgiMetalTexture::HgiMetalTexture(HgiMetal *hgi, HgiTextureDesc const & desc)
    : HgiTexture(desc)
    , _textureId(nil)
{
    MTLPixelFormat mtlFormat;
    MTLResourceOptions resourceOptions = MTLResourceStorageModePrivate;
    MTLTextureUsage usage = MTLTextureUsageUnknown;
    
    if (desc.initialData && desc.pixelsByteSize > 0) {
#if defined(ARCH_OS_MACOS)
        resourceOptions = MTLResourceStorageModeManaged;
#else
        resourceOptions = MTLResourceStorageModeShared;
#endif
    }

    mtlFormat = HgiMetalConversions::GetPixelFormat(desc.format);

    if (desc.usage & HgiTextureUsageBitsColorTarget) {
        usage = MTLTextureUsageRenderTarget;
    } else if (desc.usage & HgiTextureUsageBitsDepthTarget) {
        TF_VERIFY(desc.format == HgiFormatFloat32);
        mtlFormat = MTLPixelFormatDepth32Float;
        usage = MTLTextureUsageRenderTarget;
    }
    
//    if (desc.usage & HgiTextureUsageBitsShaderRead) {
        usage |= MTLTextureUsageShaderRead;
//    }
    if (desc.usage & HgiTextureUsageBitsShaderWrite) {
        usage |= MTLTextureUsageShaderWrite;
    }

    size_t width = desc.dimensions[0];
    size_t height = desc.dimensions[1];
    size_t depth = desc.dimensions[2];
    
    MTLTextureDescriptor* texDesc;
    
    texDesc =
        [MTLTextureDescriptor
         texture2DDescriptorWithPixelFormat:mtlFormat
                                      width:width
                                     height:height
                                  mipmapped:NO];
    
    texDesc.mipmapLevelCount = desc.mipLevels;

    texDesc.arrayLength = desc.layerCount;
    texDesc.resourceOptions = resourceOptions;
    texDesc.usage = usage;

    if (depth > 1) {
        texDesc.depth = depth;
        texDesc.textureType = MTLTextureType3D;
    }

    // Temp pending removal of blit encoder to perform MSAA resolve
//    if (desc.sampleCount > 1) {
//        texDesc.sampleCount = desc.sampleCount;
//        texDesc.textureType = MTLTextureType2DMultisample;
//    }

    _textureId = [hgi->GetDevice() newTextureWithDescriptor:texDesc];
    
    if (desc.initialData && desc.pixelsByteSize > 0) {
        TF_VERIFY(desc.mipLevels == 1, "Mipmap upload not implemented");
        [_textureId replaceRegion:MTLRegionMake2D(0, 0, width, height)
                        mipmapLevel:0
                          withBytes:desc.initialData
                        bytesPerRow:desc.pixelsByteSize / height];
    }
    
    HGIMETAL_DEBUG_LABEL(_textureId, _descriptor.debugName.c_str());
}

HgiMetalTexture::~HgiMetalTexture()
{
    if (_textureId != nil) {
        [_textureId release];
        _textureId = nil;
    }
}


PXR_NAMESPACE_CLOSE_SCOPE
