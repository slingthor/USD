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
#ifndef GLF_SIMPLE_SHADOW_ARRAY_H
#define GLF_SIMPLE_SHADOW_ARRAY_H

/// \file glf/simpleShadowArray.h

#include "pxr/pxr.h"
#include "pxr/imaging/glf/api.h"
#include "pxr/imaging/garch/gl.h"
#include "pxr/imaging/garch/simpleShadowArray.h"

#include <boost/noncopyable.hpp>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE


class GlfSimpleShadowArray : public GarchSimpleShadowArray {
public:

    GLF_API
    virtual void SetSize(GfVec2i const & size) override;

    GLF_API
    virtual void SetNumLayers(size_t numLayers) override;

    GLF_API
    virtual void BeginCapture(size_t index, bool clear) override;
    GLF_API
    virtual void EndCapture(size_t index) override;

protected:
    GLF_API
    GlfSimpleShadowArray(GfVec2i const & size, size_t numLayers);
    GLF_API
    virtual ~GlfSimpleShadowArray();

    friend class GlfResourceFactory;
    
private:
    void _AllocTextureArray();
    void _FreeTextureArray();

    void _BindFramebuffer(size_t index);
    void _UnbindFramebuffer();

    GLuint _unbindRestoreDrawFramebuffer;
    GLuint _unbindRestoreReadFramebuffer;

    GLint  _unbindRestoreViewport[4];
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif
