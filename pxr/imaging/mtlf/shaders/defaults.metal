/*
 Copyright (C) 2017 Apple Inc. All Rights Reserved.
 See LICENSE.txt for this sample’s licensing information
 */

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

typedef struct {
    float2 position [[attribute(0)]];
    float2 texcoord [[attribute(1)]];
} Vertex;

typedef struct {
    float4 position [[position]];
    float2 texcoord;
} Interpolated;

// Vertex shader function
vertex Interpolated quad_vs(Vertex v [[stage_in]])
{
    Interpolated out;
    
    out.position = float4(v.position, 0.0, 1.0);
    out.texcoord = v.texcoord;

    return out;
}

constexpr sampler s = sampler(coord::normalized,
                              address::clamp_to_edge,
                              filter::linear);

// Fragment shader function
fragment half4 tex_fs(Interpolated in [[stage_in]], texture2d<half> tex [[texture(0)]])
{
    return tex.sample(s, in.texcoord);
}

// Depth buffer copy function
kernel void copyDepth(depth2d<float, access::read> texIn,
                      texture2d<float, access::write> texOut,
                      uint2 gid [[thread_position_in_grid]])
{
    if(gid.x >= texOut.get_width() || gid.y >= texOut.get_height()) {
        return;
    }
    
    texOut.write(float(texIn.read(gid)), gid);
}

kernel void copyDepthMultisample(depth2d_ms<float, access::read> texIn,
                                 texture2d<float, access::write> texOut,
                                 uint2 gid [[thread_position_in_grid]])
{
    if(gid.x >= texOut.get_width() || gid.y >= texOut.get_height()) {
        return;
    }
    
    texOut.write(float(texIn.read(gid, 0)), gid);
}

float linear_to_srgb(float c) {
  if (c < 0.0031308)
    c = 12.92 * c;
  else
    c = 1.055 * powr(c, 1.0/2.4) - 0.055;
  return c;
}

float srgb_to_linear(float c) {
  float result;
  if (c <= 0.04045)
    result = c / 12.92;
  else
    result = powr((c + 0.055) / 1.055, 2.4);
  return result;
}

kernel void copyColour(texture2d<float, access::read> texIn,
                                 texture2d<float, access::write> texOut,
                                 uint2 gid [[thread_position_in_grid]])
{
    if(gid.x >= texOut.get_width() || gid.y >= texOut.get_height()) {
        return;
    }
    
    // TODO: colour correction
    float4 pixel = texIn.read(gid);
    pixel.rgb = float3(linear_to_srgb(pixel.r),
                       linear_to_srgb(pixel.g),
                       linear_to_srgb(pixel.b));
    pixel.a = 1.0;

    texOut.write(pixel, gid);
}

kernel void copyColourMultisample(texture2d_ms<float, access::read> texIn,
                                 texture2d<float, access::write> texOut,
                                 uint2 gid [[thread_position_in_grid]])
{
    if(gid.x >= texOut.get_width() || gid.y >= texOut.get_height()) {
        return;
    }
    
    int sampleCount = texIn.get_num_samples();

    float4 pixel = float4(0, 0, 0, 0);

    // TODO: colour correction
    for(int i = 0; i < sampleCount; i++)
        pixel += texIn.read(gid, i);

    pixel/=sampleCount;

    pixel.rgb = float3(linear_to_srgb(pixel.r),
                       linear_to_srgb(pixel.g),
                       linear_to_srgb(pixel.b));

    texOut.write(pixel, gid);
}
