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
/// \file texture.cpp
#include "pxr/imaging/garch/texture.h"
#include "pxr/base/tf/registryManager.h"
#include "pxr/base/tf/type.h"

#include <climits>

PXR_NAMESPACE_OPEN_SCOPE

#if defined(PXR_METAL_SUPPORT_ENABLED)
GarchTextureGPUHandle::GarchTextureGPUHandle(GarchTextureGPUHandle const & _gpuHandle) {
    handle = _gpuHandle.handle;
}
GarchTextureGPUHandle::GarchTextureGPUHandle(id<MTLTexture> _texture) {
    handle = (void*)_texture;
}
GarchTextureGPUHandle& GarchTextureGPUHandle::operator =(id<MTLTexture> _texture) {
    handle = (void*)_texture;
    return *this;
}
GarchTextureGPUHandle::operator id<MTLTexture>() const {
    return id<MTLTexture>(handle);
}

GarchSamplerGPUHandle::GarchSamplerGPUHandle(GarchSamplerGPUHandle const & _gpuHandle) {
    handle = _gpuHandle.handle;
}
GarchSamplerGPUHandle::GarchSamplerGPUHandle(id<MTLSamplerState> _sampler) {
    handle = (void*)_sampler;
}
GarchSamplerGPUHandle& GarchSamplerGPUHandle::operator =(id<MTLSamplerState> _sampler) {
    handle = (void*)_sampler;
    return *this;
}
GarchSamplerGPUHandle::operator id<MTLSamplerState>() const {
    return id<MTLSamplerState>(handle);
}
#endif

TF_REGISTRY_FUNCTION(TfType)
{
    TfType::Define<GarchTexture>();
}

TF_DEFINE_PUBLIC_TOKENS(GarchTextureTokens, GARCH_TEXTURE_TOKENS);

static size_t _TextureMemoryAllocated=0;
static size_t _TextureContentsID=0;

static size_t
_GetNewContentsID()
{
    return ++_TextureContentsID;
}

GarchTexture::GarchTexture( )
    : _memoryUsed(0)
    , _memoryRequested(INT_MAX)
    , _contentsID(_GetNewContentsID())
    , _originLocation(GarchImage::OriginUpperLeft)
{
}

GarchTexture::GarchTexture(GarchImage::ImageOriginLocation originLocation)
    : _memoryUsed(0)
    , _memoryRequested(INT_MAX)
    , _contentsID(_GetNewContentsID())
    , _originLocation(originLocation)
{
}

GarchTexture::~GarchTexture( )
{
    _TextureMemoryAllocated-=_memoryUsed;
}

size_t
GarchTexture::GetMemoryRequested( ) const
{
    return _memoryRequested;
}

void
GarchTexture::SetMemoryRequested(size_t targetMemory)
{
    if (_memoryRequested != targetMemory) {
        _memoryRequested = targetMemory;
        _OnMemoryRequestedDirty();
    }
}

void
GarchTexture::_OnMemoryRequestedDirty()
{
    // do nothing in base class
}

size_t 
GarchTexture::GetMemoryUsed( ) const
{
    return _memoryUsed;
}

void
GarchTexture::_SetMemoryUsed( size_t s )
{
    _TextureMemoryAllocated += s - _memoryUsed;

    _memoryUsed = s;        
}

bool
GarchTexture::IsMinFilterSupported(GLenum filter)
{
    return true;
}

bool
GarchTexture::IsMagFilterSupported(GLenum filter)
{
    return true;
}

size_t 
GarchTexture::GetTextureMemoryAllocated()
{
    return _TextureMemoryAllocated;
}

size_t
GarchTexture::GetContentsID() const
{
    return _contentsID;
}

void
GarchTexture::_UpdateContentsID()
{
    _contentsID = _GetNewContentsID();
}

GarchImage::ImageOriginLocation
GarchTexture::GetOriginLocation() const
{
    return _originLocation;
}

bool
GarchTexture::IsOriginLowerLeft() const
{
    return _originLocation == GarchImage::OriginLowerLeft;
}

void
GarchTexture::GarbageCollect()
{
    // Nothing to do here.
    // Only needed for containers of textures.
}

PXR_NAMESPACE_CLOSE_SCOPE

