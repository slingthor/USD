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
kernel void copyDepth(texture2d<float, access::read> texIn,
                      texture2d<float, access::write> texOut,
                      ushort2 gid [[thread_position_in_grid]])
{
    if(gid.x >= 1024 || gid.y >= 1024) {
        return;
    }
    
    texOut.write(float4(texIn.read(gid).rgb, 1), gid);
}
