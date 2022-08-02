/*
Copyright (c) 2021-2022 Bjarke Damsgaard Eriksen. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

    1. Redistributions of source code must retain the above
       copyright notice, this list of conditions and the
       following disclaimer.

    2. Redistributions in binary form must reproduce the above
       copyright notice, this list of conditions and the following
       disclaimer in the documentation and/or other materials
       provided with the distribution.

    3. Neither the name of the copyright holder nor the names of
       its contributors may be used to endorse or promote products
       derived from this software without specific prior written
       permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "shared.hlsli"

// X4715
#pragma warning(disable : 4715)

// anistropic voxel textures [0 -> 5]
// +x, -x, +y, -y, +z, -z
RWTexture3D<float4> opacityMap : register(u0);

cbuffer PixelShaderBufferVoxelization
{
    float2 cst_inverseViewportSize[3];
    float2 dummy;
    float4 cst_alphaTestedColor;
    float cst_fixedBias;
    uint cst_flags;
};

cbuffer GeometryShaderBufferVoxelization
{
    float3 g_bounding_min;
    float3 g_bounding_max;
    float2 halfPixelSizeCS[3]; // 1 / viewportSize
};

struct VIn
{
    float4 position : POSITION;
};

struct VOut
{
    float4 position : SV_POSITION;
};

struct GOut
{
    float4 posCS : SV_POSITION;
    float3 pos01 : POSITION01;
    float3 normal : NORMAL;
#ifdef CONS_RASTER
    float4 aabbCS : AABB;
#endif
    uint viewportAxis : SV_ViewportArrayIndex;
};

// Vertex shader that transforms the input vertices and normals to world space.
VOut vs_main(VIn input)
{
    VOut output;
    output.position = input.position;
    return output;
}

#include "voxelization.hlsli"

// Geometry shader that transform input triangles to voxelization stage.
[maxvertexcount(3)] void gs_main(triangle VOut input[3], inout TriangleStream<GOut> os) {
    VoxelizeGeometryShader(input, os);
}

// Pixel shader that stores the voxel in a 3d texture
void ps_main(GOut input)
{
#ifdef CONS_RASTER
    if (!IsPointInTriangle(input.posCS.xy, cst_inverseViewportSize[input.viewportAxis], input.aabbCS))
    {
        return;
    }
#endif

    uint3 texDim, tc, tcOffsets;
    opacityMap.GetDimensions(texDim.x, texDim.y, texDim.z);
    ComputeTC(texDim, input.pos01, input.normal, tc, tcOffsets);

    float4 isOccluded = float4(cst_alphaTestedColor.a, 1.0, 1.0, 1.0);
    opacityMap[tc + uint3(tcOffsets.x, 0, 0)] = isOccluded;
    opacityMap[tc + uint3(tcOffsets.y, 0, 0)] = isOccluded;
    opacityMap[tc + uint3(tcOffsets.z, 0, 0)] = isOccluded;
}