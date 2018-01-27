/*
 Copyright (C) 2017 Apple Inc. All Rights Reserved.
 See LICENSE.txt for this sample's licensing information
 */

#if __VERSION__ >= 140
in vec2         texCoord;
out vec4        fragColor;
#endif

// A GL_TEXTURE_RECTANGLE
uniform sampler2DRect interopTexture;

// The dimensions of the source texture. The sampler coordinates for a GL_TEXTURE_RECTANGLE are in pixels,
// rather than the usual normalised 0..1 range.
uniform vec2 texSize;

void main(void)
{
#if __VERSION__ >= 140
    vec2 uv = texCoord * texSize;
	fragColor = texture(interopTexture, uv.st);
#else
	vec2 uv = gl_TexCoord[0].st * texSize;
    gl_FragColor = texture2DRect(interopTexture, uv.st);
#endif
}
