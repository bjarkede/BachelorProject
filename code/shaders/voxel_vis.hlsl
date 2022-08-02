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

Texture3D opacityMap : register(t0);

cbuffer VertexShaderBuffer
{
    uint3 cst_gridSize;
};

cbuffer GeometryShaderBuffer
{
    float4x4 modelViewMatrix;
    float4x4 projectionMatrix;

    float3 cst_bounding_min;
    float3 cst_bounding_max;

    float4 cst_cameraPosition;
    uint3 cst_gridSize_gs;
    uint cst_mipLevel_gs;
    uint cst_singleChannel_gs;
};

cbuffer PixelShaderBuffer
{
    uint cst_mipLevel;
    uint cst_wireFrameMode;
    uint cst_singleChannel;
    uint dummy;
};

struct VIn
{
    uint id : SV_VertexID; // 1D index
};

struct VOut
{
    uint3 voxelID : ID;
};

struct GOut
{
    float4 position : SV_POSITION;
    float4 cameraPositionWS : POSITION;
    float3 normal : NORMAL;
    float2 tc : TC;
    uint faceIndex : FID;
    uint3 id : ID; // 3D index
    float4 color : COLOR;
};

// Access voxel by a lookup in Texture3D by vertex index.
VOut vs_main(VIn input)
{
    uint numVoxelsInSlice = cst_gridSize.x * cst_gridSize.y;

    uint x, y, z;

    // input.id -> voxel id
    uint temp = input.id % numVoxelsInSlice; // make sure input.id wraps around when numVoxelsInSlice is reached

    x = temp % cst_gridSize.x; // x wraps around when we reach cst_gridSize.x (width)
    y = temp / cst_gridSize.x; // y increments when x reach cst_gridSize.x (width)
    z = input.id / numVoxelsInSlice; // z gets incremented when the number of voxels in a slice is reached

    VOut output;
    output.voxelID = uint3(x, y, z);

    return output;
}

static const float3 cubeOffsets[24] = {
    // left -x
    float3(-1, 1, 1),
    float3(-1, 1, -1),
    float3(-1, -1, 1),
    float3(-1, -1, -1),

    // right +x
    float3(1, 1, -1),
    float3(1, 1, 1),
    float3(1, -1, -1),
    float3(1, -1, 1),

    // top -y
    float3(-1, -1, -1),
    float3(1, -1, -1),
    float3(-1, -1, 1),
    float3(1, -1, 1),

    // bottom +y
    float3(1, 1, 1),
    float3(1, 1, -1),
    float3(-1, 1, 1),
    float3(-1, 1, -1),

    // back -z
    float3(-1, 1, -1),
    float3(1, 1, -1),
    float3(-1, -1, -1),
    float3(1, -1, -1),

    // front +z
    float3(1, -1, 1),
    float3(1, 1, 1),
    float3(-1, -1, 1),
    float3(-1, 1, 1),
};

static const float3 normalOffsets[6] = {
    float3(-1, 0, 0), // -x
    float3(1, 0, 0), // +x
    float3(0, -1, 0), // -y
    float3(0, 1, 0), // +y
    float3(0, 0, -1), // -z
    float3(0, 0, 1), // +z
};

static const float2 tcOffsets[4] = {
    float2(0, 0),
    float2(0, 1),
    float2(1, 0),
    float2(1, 1)
};

[maxvertexcount(24)] void gs_main(point VOut input[1], inout TriangleStream<GOut> os) {
    uint4 texDim;
    opacityMap.GetDimensions(cst_mipLevel_gs, texDim.x, texDim.y, texDim.z, texDim.w);

    uint4 tc = uint4(input[0].voxelID, cst_mipLevel_gs);

    // 6 faces, 4 vertex per face
    for (uint i = 0; i < 6; ++i)
    {

        float4 color = opacityMap.Load(int4(tc));
        if ((cst_singleChannel_gs && color.r == 0.0) || (!cst_singleChannel_gs && all(color == 0.0)))
        {
            tc.x += (texDim.x / 6);
            continue;
        }

        for (uint j = 0; j < 4; ++j)
        {
            GOut output;

            // texture space -> [0;1]
            float3 pos01 = ((input[0].voxelID.xyz + cubeOffsets[i * 4 + j] * 0.5f) + 0.5) / cst_gridSize_gs.xyz;
            // [0;1] -> world-space
            float3 posWS = (pos01 * (cst_bounding_max - cst_bounding_min)) + cst_bounding_min;

            output.position = mul(float4(posWS, 1.0), modelViewMatrix);
            output.position = mul(output.position, projectionMatrix);
            output.cameraPositionWS = cst_cameraPosition;
            output.normal = normalOffsets[i];
            output.tc = tcOffsets[j];
            output.faceIndex = i;
            output.id = input[0].voxelID;
            output.color = color;

            os.Append(output);
        }
        os.RestartStrip();

        tc.x += (texDim.x / 6);
    }
}

float4 ps_main(GOut input)
    : SV_Target
{
    float3 v;
    if (cst_singleChannel)
    {
        float3 N = normalize(input.normal);
        float3 L = normalize(input.cameraPositionWS.xyz);
        float diffuse = dot(N, L) * 0.5 + 0.5;
        v = diffuse * (input.color.r * 0.5 + 0.5);
    }
    else
    {
        v = input.color.rgb;
    }

    const float tcMin = 1.0 / 16.0;
    const float tcMax = 1.0 - tcMin;
    if (cst_wireFrameMode == 1)
    {
        if (input.tc.x < tcMin || input.tc.x > tcMax || input.tc.y < tcMin || input.tc.y > tcMax)
        {
            return float4(0.25, 0.25, 0.25, 1.0);
        }
    }
    else if (cst_wireFrameMode == 2)
    {
        if (input.tc.x < tcMin || input.tc.x > tcMax || input.tc.y < tcMin || input.tc.y > tcMax)
        {
            float3 N = normalize(input.normal);
            float3 L = normalize(input.cameraPositionWS.xyz);
            float d = dot(N, L) * 0.75 + 0.25;
            return float4(d, 0.0, d, 1.0);
        }
        else
        {
            discard;
        }
    }

    return float4(v, 1.0f);
}