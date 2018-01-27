/*
 Copyright (C) 2017 Apple Inc. All Rights Reserved.
 See LICENSE.txt for this sample's licensing information
 */

#if __VERSION__ >= 140
in vec2 inPosition;
in vec2 inTexCoord;
out vec2 texCoord;
#else
attribute vec2 inPosition;
attribute vec2 inTexCoord;
varying vec2 texCoord;
#endif

void main()
{
    texCoord = inTexCoord;
    gl_Position = vec4(inPosition, 0.0, 1.0);
}
