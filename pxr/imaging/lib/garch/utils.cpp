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
// utils.cpp
//

#include "pxr/imaging/garch/gl.h"
#include "pxr/imaging/garch/utils.h"

#include "pxr/base/tf/diagnostic.h"

PXR_NAMESPACE_OPEN_SCOPE


int
GarchGetNumElements(GLenum format)
{
    switch (format) {
        case GL_DEPTH_COMPONENT:
	case GL_COLOR_INDEX:
	case GL_ALPHA:
	case GL_LUMINANCE:
	case GL_RED:
            return 1;
	case GL_LUMINANCE_ALPHA :
	    return 2;
        case GL_RGB:
            return 3;
        case GL_RGBA:
            return 4;
        default:
            TF_CODING_ERROR("Unsupported format");
            return 1;
    }
}

int
GarchGetElementSize(GLenum type)
{
    switch (type) {
        case GL_UNSIGNED_BYTE:
        case GL_BYTE:
            return sizeof(GLubyte);
        case GL_UNSIGNED_SHORT:
        case GL_SHORT:
            return sizeof(GLshort);
        case GL_FLOAT:
            return sizeof(GLfloat);
        case GL_DOUBLE:
            return sizeof(GLdouble);
        default:
            TF_CODING_ERROR("Unsupported type");
            return sizeof(GLfloat);
    }
}


bool GarchIsCompressedFormat(GLenum format)
{
    if (format == GL_COMPRESSED_RGBA_BPTC_UNORM ||
        format == GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT) {
        return true;
    }
    return false;
}

size_t GarchGetCompressedTextureSize(int width, int height, GLenum format, GLenum type)
{
    int blockSize = 0;
    int tileSize = 0;
    int alignSize = 0;
    
    // XXX Only BPTC is supported right now
    if (format == GL_COMPRESSED_RGBA_BPTC_UNORM ||
        format == GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT) {
        blockSize = 16;
        tileSize = 4;
        alignSize = 3;
    }
    
    size_t numPixels = ((width + alignSize)/tileSize) *
    ((height + alignSize)/tileSize);
    return numPixels * blockSize;
}

PXR_NAMESPACE_CLOSE_SCOPE

