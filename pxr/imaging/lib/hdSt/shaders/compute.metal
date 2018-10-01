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
                "source": [ "Compute.NormalsSrcFloat",
                           "Compute.NormalsDstFloat",
                           "Compute.SmoothNormals" ]
            },
            "smoothNormalsDoubleToDouble": {
                "source": [ "Compute.NormalsSrcDouble",
                           "Compute.NormalsDstDouble",
                           "Compute.SmoothNormals" ]
            },
            "smoothNormalsFloatToPacked": {
                "source": [ "Compute.NormalsSrcFloat",
                           "Compute.NormalsDstPacked",
                           "Compute.SmoothNormals" ]
            },
            "smoothNormalsDoubleToPacked": {
                "source": [ "Compute.NormalsSrcDouble",
                           "Compute.NormalsDstPacked",
                           "Compute.SmoothNormals" ]
            },
            "flatNormalsTriFloatToFloat": {
                "source": [ "Compute.NormalsSrcFloat",
                           "Compute.NormalsDstFloat",
                           "Compute.FlatNormals",
                           "Compute.FlatNormalsTri" ]
            },
            "flatNormalsTriDoubleToDouble": {
                "source": [ "Compute.NormalsSrcDouble",
                           "Compute.NormalsDstDouble",
                           "Compute.FlatNormals",
                           "Compute.FlatNormalsTri" ]
            },
            "flatNormalsTriFloatToPacked": {
                "source": [ "Compute.NormalsSrcFloat",
                           "Compute.NormalsDstPacked",
                           "Compute.FlatNormals",
                           "Compute.FlatNormalsTri" ]
            },
            "flatNormalsTriDoubleToPacked": {
                "source": [ "Compute.NormalsSrcDouble",
                           "Compute.NormalsDstPacked",
                           "Compute.FlatNormals",
                           "Compute.FlatNormalsTri" ]
            },
            "flatNormalsQuadFloatToFloat": {
                "source": [ "Compute.NormalsSrcFloat",
                           "Compute.NormalsDstFloat",
                           "Compute.FlatNormals",
                           "Compute.FlatNormalsQuad" ]
            },
            "flatNormalsQuadDoubleToDouble": {
                "source": [ "Compute.NormalsSrcDouble",
                           "Compute.NormalsDstDouble",
                           "Compute.FlatNormals",
                           "Compute.FlatNormalsQuad" ]
            },
            "flatNormalsQuadFloatToPacked": {
                "source": [ "Compute.NormalsSrcFloat",
                           "Compute.NormalsDstPacked",
                           "Compute.FlatNormals",
                           "Compute.FlatNormalsQuad" ]
            },
            "flatNormalsQuadDoubleToPacked": {
                "source": [ "Compute.NormalsSrcDouble",
                           "Compute.NormalsDstPacked",
                           "Compute.FlatNormals",
                           "Compute.FlatNormalsQuad" ]
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
-- glsl Compute.NormalsSrcFloat

#define POINTS_INPUT_TYPE    float
float3 getPoint(device const float* points, int index)
{
    return float3(points[index],
                  points[index + 1],
                  points[index + 2]);
}

#define ADJACENCY_INPUT_TYPE int
//layout(binding=0) buffer Points { float points[]; };
//layout(binding=2) buffer Adjacency { int entry[]; };

--- --------------------------------------------------------------------------
-- glsl Compute.NormalsSrcDouble

// UNSUPPORTED IN METAL
#define POINTS_INPUT_TYPE    double
float3 getPoint(device const double* points, int index)
{
    return float3(points[index],
                  points[index + 1],
                  points[index + 2]);
}
#define ADJACENCY_INPUT_TYPE int

--- --------------------------------------------------------------------------
-- glsl Compute.NormalsDstFloat

#define NORMAL_OUTPUT_TYPE float

void writeNormal(device float *normals, int nIndex, float3 normal)
{
    normals[nIndex+0] = normal.x;
    normals[nIndex+1] = normal.y;
    normals[nIndex+2] = normal.z;
}

--- --------------------------------------------------------------------------
-- glsl Compute.NormalsDstDouble

// UNSUPPORTED IN METAL
#define NORMAL_OUTPUT_TYPE double

void writeNormal(device double *normals, int nIndex, float3 normal)
{
    normals[nIndex+0] = normal.x;
    normals[nIndex+1] = normal.y;
    normals[nIndex+2] = normal.z;
}

--- --------------------------------------------------------------------------
-- glsl Compute.NormalsDstPacked

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

int getPointsIndex(device const Uniforms *uniform, int idx)
{
    return (idx + uniform->vertexOffset) * uniform->pointsStride + uniform->pointsOffset;
}

int getNormalsIndex(device const Uniforms *uniform, int idx)
{
    return (idx + uniform->vertexOffset) * uniform->normalsStride + uniform->normalsOffset;
}

kernel void computeEntryPoint(   device const POINTS_INPUT_TYPE     *points  [[buffer(0)]],
                                 device       NORMAL_OUTPUT_TYPE    *normals [[buffer(1)]],
                                 device const ADJACENCY_INPUT_TYPE  *entry   [[buffer(2)]],
                                 device const Uniforms              *uniforms[[buffer(3)]],
                                 uint2        GlobalInvocationID             [[ thread_position_in_grid ]])
{
    int index = int(GlobalInvocationID.x);

    int offIndex = index * 2 + uniforms->adjacencyOffset;

    int offset = entry[offIndex] + uniforms->adjacencyOffset;
    int valence = entry[offIndex + 1];

    float3 normal = float3(0);

    float3 current = getPoint(points, getPointsIndex(uniforms, index));
    for (int i = 0; i < valence; ++i) {
        int entryIdx = i * 2 + offset;

        int prevIdx = entry[entryIdx];
        int nextIdx = entry[entryIdx + 1];

        float3 next = getPoint(points, getPointsIndex(uniforms, nextIdx));
        float3 prev = getPoint(points, getPointsIndex(uniforms, prevIdx));
        normal += cross(next - current, prev - current);

    }
    float n = 1.0/max(length(normal), 0.000001);
    normal *= n;
    writeNormal(normals, getNormalsIndex(uniforms, index), normal);
}
                               
--- --------------------------------------------------------------------------
-- glsl Compute.FlatNormals

//layout(std430, binding=2) buffer Indices { int indices[]; };
//layout(std430, binding=3) buffer PrimitiveParam { int primitiveParam[]; };

struct Uniforms {
    int vertexOffset;       // offset in aggregated buffer
    int elementOffset;      // offset in aggregated buffer
    int topologyOffset;     // offset in aggregated buffer
    int pointsOffset;       // interleave offset
    int pointsStride;       // interleave stride
    int normalsOffset;      // interleave offset
    int normalsStride;      // interleave stride
    int indexOffset;        // interleave offset
    int indexStride;        // interleave stride
    int pParamOffset;       // interleave offset
    int pParamStride;       // interleave stride
};

int getPointsIndex(device const Uniforms *uniforms, int idx)
{
    return (idx + uniforms->vertexOffset) * uniforms->pointsStride + uniforms->pointsOffset;
}

int getNormalsIndex(device const Uniforms *uniforms, int idx)
{
    return (idx + uniforms->elementOffset) * uniforms->normalsStride + uniforms->normalsOffset;
}

int getIndicesIndex(device const Uniforms *uniforms, int idx)
{
    return (idx + uniforms->topologyOffset) * uniforms->indexStride + uniforms->indexOffset;
}

int getPrimitiveParamIndex(device const Uniforms *uniforms, int idx)
{
    return (idx + uniforms->topologyOffset) * uniforms->pParamStride + uniforms->pParamOffset;
}

int getEdgeFlag(int pParam)
{
    return pParam & 3;
}

int getFaceIndex(int pParam)
{
    return pParam >> 2;
}

float3 computeNormalForPrimIndex(device int *indices, device const Uniforms *uniforms, int primIndex);

kernel void computeEntryPoint(device const POINTS_INPUT_TYPE    *points             [[buffer(0)]],
                              device       NORMAL_OUTPUT_TYPE   *normals            [[buffer(1)]],
                              device const ADJACENCY_INPUT_TYPE *indices            [[buffer(2)]],
                              device const int                  *primitiveParam     [[buffer(3)]],
                              device const Uniforms             *uniforms           [[buffer(4)]],
                              uint2        GlobalInvocationID                       [[ thread_position_in_grid ]])
{
    int primIndex = int(GlobalInvocationID.x);
    int pParam = primitiveParam[getPrimitiveParamIndex(primIndex)];
    int edgeFlag = getEdgeFlag(pParam);
    int faceIndex = getFaceIndex(pParam);
    vec3 normal = vec3(0);

    if (getEdgeFlag(pParam) == 0) {
        // 0 indicates an unsplit face (as authored)
        normal += computeNormalForPrimIndex(indices, uniforms, primIndex);

    } else if (getEdgeFlag(pParam) == 1) {
        // A subdivided face will have a run of prims with
        // edge flags like: 1, 3, 3, 3, 2; where "3" denotes an interior
        // prim. Only compute normals for the first prim in a face.

        int primCounter = 0;
        do {
            pParam = primitiveParam[getPrimitiveParamIndex(
                                           primIndex + primCounter)];
            normal += computeNormalForPrimIndex(indices, uniforms, primIndex+primCounter);
            primCounter++;
        } while(getEdgeFlag(pParam) != 2);

    } else {
        return;
    }
    float n = 1.0/max(length(normal), 0.000001);
    normal *= n;
    writeNormal(normals, getNormalsIndex(uniforms, faceIndex), normal);
}

--- --------------------------------------------------------------------------
-- glsl Compute.FlatNormalsTri

int3 getIndices(device int *indices, int idx)
{
    return int3(indices[idx],
                indices[idx+1],
                indices[idx+2]);
}

float3 computeNormalForPrimIndex(device int *indices, device const Uniforms *uniforms, int primIndex)
{
    int3 primIndices = getIndices(indices, getIndicesIndex(uniforms, primIndex));

    float3 p0 = getPoint(points, getPointsIndex(uniforms, primIndices.x));
    float3 p1 = getPoint(points, getPointsIndex(uniforms, primIndices.y));
    float3 p2 = getPoint(points, getPointsIndex(uniforms, primIndices.z));

    return cross(p1-p0, p2-p0);
}

--- --------------------------------------------------------------------------
-- glsl Compute.FlatNormalsQuad

int4 getIndices(device int *indices, int idx)
{
    return int4(indices[idx],
                indices[idx+1],
                indices[idx+2],
                indices[idx+3]);
}

float3 computeNormalForPrimIndex(device int *indices, device const Uniforms *uniforms, int primIndex)
{
    int4 primIndices = getIndices(getIndicesIndex(primIndex));

    float3 p0 = getPoint(points, getPointsIndex(uniforms, primIndices.x));
    float3 p1 = getPoint(points, getPointsIndex(uniforms, primIndices.y));
    float3 p2 = getPoint(points, getPointsIndex(uniforms, primIndices.z));
    float3 p3 = getPoint(points, getPointsIndex(uniforms, primIndices.w));

    return cross(p0-p3, p2-p3) + cross(p2-p1, p0-p1);
}

--- --------------------------------------------------------------------------
-- glsl Compute.QuadrangulateFloat

//layout(binding=0) buffer Primvar { float primvar[]; };
//layout(binding=1) buffer QuadInfo { int quadInfo[]; };

#define DATATYPE float

--- --------------------------------------------------------------------------
-- glsl Compute.QuadrangulateDouble

//layout(binding=0) buffer Primvar { double primvar[]; };
//layout(binding=1) buffer QuadInfo { int quadInfo[]; };

#define DATATYPE double

--- --------------------------------------------------------------------------
-- glsl Compute.Quadrangulate

struct Uniform {
    int vertexOffset;      // offset in aggregated buffer
    int quadInfoStride;
    int quadInfoOffset;
    int maxNumVert;
    int primvarOffset;     // interleave offset
    int primvarStride;     // interleave stride
    int numComponents;     // interleave datasize
};

kernel void computeEntryPoint(device const DATATYPE    *primvar             [[buffer(0)]],
                              device       int         *quadInfo            [[buffer(1)]],
                              device const Uniforms    *uniforms            [[buffer(2)]],
                              uint2        GlobalInvocationID               [[ thread_position_in_grid ]])
{
    int index = int(gl_GlobalInvocationID.x);

    int quadInfoIndex = index * uniforms->quadInfoStride + uniforms->quadInfoOffset;
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

    for (int j = 0; j < uniforms->numComponents; ++j) {
        DATATYPE center = 0;
        for (int i = 0; i < uniforms->numVert; ++i) {
            int i0 = quadInfo[quadInfoIndex + 2 + i];
            int i1 = quadInfo[quadInfoIndex + 2 + (i+1)%uniforms->numVert];

            DATATYPE v0 =  primvar[(i0 + uniforms->vertexOffset)*uniforms->primvarStride + uniforms->primvarOffset + j];
            DATATYPE v1 =  primvar[(i1 + uniforms->vertexOffset)*uniforms->primvarStride + uniforms->primvarOffset + j];
            DATATYPE edge = (v0 + v1) * 0.5;
            center += v0;

            // edge
            primvar[(dstOffset + i +
                     uniforms->vertexOffset)*uniforms->primvarStride + uniforms->primvarOffset + j] = edge;
        }
        // center
        center /= numVert;
        primvar[(dstOffset + uniforms->numVert +
                 uniforms->vertexOffset)*uniforms->primvarStride + uniforms->primvarOffset + j] = center;
    }
}
