/*
 Copyright (C) 2017 Apple Inc. All Rights Reserved.
 See LICENSE.txt for this sample's licensing information
 */

#if __VERSION__ >= 140
in vec2         texCoord;
out vec4        fragColor;
#else
varying vec2    texCoord;
#endif

// A GL_TEXTURE_RECTANGLE
uniform sampler2DRect interopTexture;
uniform sampler2DRect depthTexture;

// The dimensions of the source texture. The sampler coordinates for a GL_TEXTURE_RECTANGLE are in pixels,
// rather than the usual normalised 0..1 range.
uniform vec2 texSize;

void main(void)
{
	vec2 uv = vec2(texCoord.x, 1.0 - texCoord.y) * texSize;
#if __VERSION__ >= 140
    fragColor = texture(interopTexture, uv.st);
	gl_FragDepth = texture(depthTexture, uv.st).r;
#else
    gl_FragColor = texture2DRect(interopTexture, uv.st);
    gl_FragDepth = texture2DRect(depthTexture, uv.st).r;
#endif
}
