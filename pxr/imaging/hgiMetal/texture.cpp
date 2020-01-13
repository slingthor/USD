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

    if (desc.dimensions[2] > 1) {
        TF_CODING_ERROR("Missing implementation for texture layers");
    }

    MTLPixelFormat mtlFormat = MTLPixelFormatInvalid;

    if (desc.usage & HgiTextureUsageBitsColorTarget) {
        mtlFormat = HgiMetalConversions::GetFormat(desc.format);
    } else if (desc.usage & HgiTextureUsageBitsDepthTarget) {
        TF_VERIFY(desc.format == HgiFormatFloat32);
        mtlFormat = MTLPixelFormatDepth32Float;
    } else {
        TF_CODING_ERROR("Unknown HgTextureUsage bit");
    }

    MTLTextureDescriptor* texDesc =
        [MTLTextureDescriptor
         texture2DDescriptorWithPixelFormat:mtlFormat
                                      width:desc.dimensions[0]
                                     height:desc.dimensions[1]
                                  mipmapped:NO];
    //texDesc.resourceOptions = MTLResourceStorageModeDefault;
    texDesc.resourceOptions = MTLResourceStorageModePrivate;
    texDesc.usage = MTLTextureUsageShaderRead;

#if (__MAC_OS_X_VERSION_MAX_ALLOWED >= 101500) || (__IPHONE_OS_VERSION_MAX_ALLOWED >= 130000) /* __MAC_10_15 __IOS_13_00 */
    if (hgi->GetAPIVersion() >= APIVersion_Metal3_0) {
        texDesc.swizzle = MTLTextureSwizzleChannelsMake(
            MTLTextureSwizzleRed,
            MTLTextureSwizzleRed,
            MTLTextureSwizzleRed,
            MTLTextureSwizzleRed);
    }
#endif
    
    _textureId = [hgi->GetDevice() newTextureWithDescriptor:texDesc];

//    if (desc.sampleCount == HgiSampleCount1) {
//        glCreateTextures(GL_TEXTURE_2D, 1, &_textureId);
//    } else {
//        glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE, 1, &_textureId);
//    }    

    if (desc.sampleCount == HgiSampleCount1) {
        // XXX sampler state etc should all be set via tex descriptor.
        //     (probably pass in HgiSamplerHandle in tex descriptor)
        /*
        glTextureParameteri(_textureId, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(_textureId, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(_textureId, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTextureParameteri(_textureId, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(_textureId, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        float aniso = 2.0f;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &aniso);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, aniso);
        const uint8_t mips = 1;
        glTextureParameteri(_textureId, GL_TEXTURE_BASE_LEVEL, 0);
        glTextureParameteri(_textureId, GL_TEXTURE_MAX_LEVEL, mips-1);

        glTextureStorage2D(_textureId, mips, glInternalFormat,
                           desc.dimensions[0], desc.dimensions[1]);
         */
    } else {
        /*
        // Note: Setting sampler state values on multi-sample texture is invalid
        glTextureStorage2DMultisample(
            _textureId, desc.sampleCount, glInternalFormat,
            desc.dimensions[0], desc.dimensions[1], GL_TRUE);
         */
    }
}

HgiMetalTexture::~HgiMetalTexture()
{
    if (_textureId != nil) {
        [_textureId release];
        _textureId = nil;
    }
}


PXR_NAMESPACE_CLOSE_SCOPE
