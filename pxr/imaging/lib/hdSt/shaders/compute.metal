-- glslfx version 0.1

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

--- This is what an import might look like.
--- #import $TOOLS/hdSt/shaders/compute.glslfx

-- configuration
{
    "techniques": {
        "default": {
            "smoothNormalsFloatToFloat": {
                "source": [ "Compute.SmoothNormalsSrcFloat",
                            "Compute.SmoothNormalsDstFloat",
                            "Compute.SmoothNormals" ]
            },
            "smoothNormalsDoubleToFloat": {
                "source": [ "Compute.SmoothNormalsSrcDouble",
                            "Compute.SmoothNormalsDstFloat",
                            "Compute.SmoothNormals" ]
            },
            "smoothNormalsFloatToDouble": {
                "source": [ "Compute.SmoothNormalsSrcFloat",
                            "Compute.SmoothNormalsDstDouble",
                            "Compute.SmoothNormals" ]
            },
            "smoothNormalsDoubleToDouble": {
                "source": [ "Compute.SmoothNormalsSrcDouble",
                            "Compute.SmoothNormalsDstDouble",
                            "Compute.SmoothNormals" ]
            },
            "smoothNormalsFloatToPacked": {
                "source": [ "Compute.SmoothNormalsSrcFloat",
                            "Compute.SmoothNormalsDstPacked",
                            "Compute.SmoothNormals" ]
            },
            "smoothNormalsDoubleToPacked": {
                "source": [ "Compute.SmoothNormalsSrcDouble",
                            "Compute.SmoothNormalsDstPacked",
                            "Compute.SmoothNormals" ]
            },
            "quadrangulateFloat": {
                "source": [ "Compute.QuadrangulateFloat",
                            "Compute.Quadrangulate" ]
            },
            "quadrangulateDouble": {
                "source": [ "Compute.QuadrangulateDouble",
                            "Compute.Quadrangulate" ]
            }
        }
    }
}

--- --------------------------------------------------------------------------
-- glsl Compute.SmoothNormalsSrcFloat

#define POINTS_INPUT_TYPE    float
#define ADJACENCY_INPUT_TYPE int
//layout(binding=0) buffer Points { float points[]; };
//layout(binding=2) buffer Adjacency { int entry[]; };

--- --------------------------------------------------------------------------
-- glsl Compute.SmoothNormalsSrcDouble

// UNSUPPORTED IN METAL
#define POINTS_INPUT_TYPE    double
#define ADJACENCY_INPUT_TYPE int

--- --------------------------------------------------------------------------
-- glsl Compute.SmoothNormalsDstFloat

#define NORMAL_OUTPUT_TYPE float

void writeNormal(device float *normals, int nIndex, float3 normal)
{
    normals[nIndex+0] = normal.x;
    normals[nIndex+1] = normal.y;
    normals[nIndex+2] = normal.z;
}

--- --------------------------------------------------------------------------
-- glsl Compute.SmoothNormalsDstDouble

// UNSUPPORTED IN METAL
#define NORMAL_OUTPUT_TYPE double

void writeNormal(device double *normals, int nIndex, float3 normal)
{
    normals[nIndex+0] = normal.x;
    normals[nIndex+1] = normal.y;
    normals[nIndex+2] = normal.z;
}

--- --------------------------------------------------------------------------
-- glsl Compute.SmoothNormalsDstPacked

#define NORMAL_OUTPUT_TYPE int
//layout(binding=1) buffer Normals { int normals[]; };
void writeNormal(device int *normals, int nIndex, float3 normal)
{
    normal *= 511.0;
    normals[nIndex] =
    ((int(normal.x) & 0x3ff)      ) |
    ((int(normal.y) & 0x3ff) << 10) |
    ((int(normal.z) & 0x3ff) << 20);
}

--- --------------------------------------------------------------------------
-- glsl Compute.SmoothNormals

struct Uniforms {
    int vertexOffset;       // offset in aggregated buffer
    int adjacencyOffset;
    int pointsOffset;       // interleave offset
    int pointsStride;       // interleave stride
    int normalsOffset;      // interleave offset
    int normalsStride;      // interleave stride
};

float3 getPoint(device const float* points, device const Uniforms *uniform, int index)
{
    int offsetIdx = (index+uniform->vertexOffset);
    int pointsIdx = offsetIdx * uniform->pointsStride + uniform->pointsOffset;

    return float3(points[pointsIdx],
    points[pointsIdx + 1],
    points[pointsIdx + 2]);
}

kernel void computeEntryPoint(   device const POINTS_INPUT_TYPE *points  [[buffer(0)]],
                                 device       NORMAL_OUTPUT_TYPE     *normals [[buffer(1)]],
                                 device const ADJACENCY_INPUT_TYPE   *entry   [[buffer(2)]],
                                 device const Uniforms               *uniforms[[buffer(3)]],
                                 uint2        GlobalInvocationID              [[ thread_position_in_grid ]])
{
    int index = int(GlobalInvocationID.x);

    int offIndex = index * 2 + uniforms->adjacencyOffset;

    int offset = entry[offIndex] + uniforms->adjacencyOffset;
    int valence = entry[offIndex + 1];

    float3 normal = float3(0);

    float3 current = getPoint(points, uniforms, index);
    for (int i = 0; i < valence; ++i) {
        int entryIdx = i * 2 + offset;

        int prevIdx = entry[entryIdx];
        int nextIdx = entry[entryIdx + 1];

        float3 next = getPoint(points, uniforms, nextIdx);
        float3 prev = getPoint(points, uniforms, prevIdx);
        normal += cross(next - current, prev - current);

    }
    float n = 1.0/max(length(normal), 0.000001);
    normal *= n;
    int nIndex = (index+uniforms->vertexOffset)*uniforms->normalsStride + uniforms->normalsOffset;
    writeNormal(normals, nIndex, normal);
}

--- --------------------------------------------------------------------------
-- glsl Compute.QuadrangulateFloat

layout(binding=0) buffer Primvar { float primvar[]; };
layout(binding=1) buffer QuadInfo { int quadInfo[]; };

#define DATATYPE float

--- --------------------------------------------------------------------------
-- glsl Compute.QuadrangulateDouble

layout(binding=0) buffer Primvar { double primvar[]; };
layout(binding=1) buffer QuadInfo { int quadInfo[]; };

#define DATATYPE double

--- --------------------------------------------------------------------------
-- glsl Compute.Quadrangulate

layout(std140, binding=0) uniform Uniform {
    int vertexOffset;      // offset in aggregated buffer
    int quadInfoStride;
    int quadInfoOffset;
    int maxNumVert;
    int primvarOffset;     // interleave offset
    int primvarStride;     // interleave stride
    int numComponents;     // interleave datasize
};
layout(local_size_x=1, local_size_y=1, local_size_z=1) in;

void main()
{
    int index = int(gl_GlobalInvocationID.x);

    int quadInfoIndex = index * quadInfoStride + quadInfoOffset;
    int numVert = quadInfo[quadInfoIndex];
    int dstOffset = quadInfo[quadInfoIndex+1];

    // GPU quadinfo table layout
    //
    // struct NonQuad {
    //     int numVert;
    //     int dstOffset;
    //     int index[maxNumVert];
    // } quadInfo[]
    //

    for (int j = 0; j < numComponents; ++j) {
        DATATYPE center = 0;
        for (int i = 0; i < numVert; ++i) {
            int i0 = quadInfo[quadInfoIndex + 2 + i];
            int i1 = quadInfo[quadInfoIndex + 2 + (i+1)%numVert];

            DATATYPE v0 =  primvar[(i0 + vertexOffset)*primvarStride + primvarOffset + j];
            DATATYPE v1 =  primvar[(i1 + vertexOffset)*primvarStride + primvarOffset + j];
            DATATYPE edge = (v0 + v1) * 0.5;
            center += v0;

            // edge
            primvar[(dstOffset + i + vertexOffset)*primvarStride + primvarOffset + j] = edge;
        }
        // center
        center /= numVert;
        primvar[(dstOffset + numVert + vertexOffset)*primvarStride + primvarOffset + j] = center;
    }
}
