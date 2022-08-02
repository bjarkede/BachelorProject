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
#include "material_flags.hlsli"

// X4715
#pragma warning(disable : 4715)

RWTexture3D<uint> emittanceRG : register(u0);
RWTexture3D<uint> emittanceBA : register(u1);
RWTexture3D<uint> emittanceC : register(u2);
RWTexture3D<uint> normalMap : register(u3);

Texture2D albedoTexture : register(t0);
Texture2DArray shadowMap : register(t1);

SamplerState albedoSampler : register(s0);
SamplerComparisonState shadowSampler : register(s1);

cbuffer PixelShaderBufferVoxelization : register(b0)
{
    float2 cst_inverseViewportSize[3];
    float2 dummy;
    float4 cst_alphaTestedColor;
    float cst_fixedBias;
    uint cst_flags;
};

cbuffer LightBuffer : register(b1)
{
    LightEntity pointLights[MAX_LIGHTS];
    uint cst_numLights;
};

cbuffer GeometryShaderBufferVoxelization : register(b0)
{
    float3 g_bounding_min;
    float3 g_bounding_max;
    float2 halfPixelSizeCS[3]; // 1 / viewportSize
};

struct VIn
{
    float4 position : POSITION;
    float2 tc : TEXCOORD0;
};

struct VOut
{
    float4 position : SV_POSITION;
    float2 tc : TEXCOORD0;
};

struct GOut
{
    float4 posCS : SV_POSITION;
    float3 pos01 : POSITION01;
    float3 posWS : POSITIONWS;
    float3 normal : NORMAL;
    float2 tc : TEXCOORD0;
#ifdef CONS_RASTER
    float4 aabbCS : AABB;
#endif
    uint viewportAxis : SV_ViewportArrayIndex;
};

VOut vs_main(VIn input)
{
    VOut output;
    output.position = input.position;
    output.tc = input.tc;
    return output;
}

#define EMITTANCE
#include "voxelization.hlsli"
#undef EMITTANCE

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

    float3 finalColor = float3(0.0, 0.0, 0.0);
    float3 albedo = float3(0.0, 0.0, 0.0);
    if (cst_flags & IS_ALPHA_TESTED)
    {
        albedo = cst_alphaTestedColor.rgb;
    }
    else
    {
        albedo = albedoTexture.Sample(albedoSampler, input.tc).rgb;
    }
    
    if (!(cst_flags & IS_EMISSIVE))
    {
        for (uint i = 0; i < cst_numLights; ++i)
        {
            float vis = PCF_Visibility(shadowMap, shadowSampler, input.posWS, pointLights[i], float(i), cst_fixedBias);
            //float vis = 1.0;
            finalColor += ShadeDiffuse(pointLights[i], input.posWS, input.normal, albedo, vis);
        }
    } 
    else
    {
        finalColor = albedo;
    }
   
    uint3 texDim, tc, tcOffsets;
    emittanceRG.GetDimensions(texDim.x, texDim.y, texDim.z);
    ComputeTC(texDim, input.pos01, input.normal, tc, tcOffsets);

    float4 outputColor = float4(finalColor, cst_alphaTestedColor.a);
    AtomicAddHDR(emittanceRG, emittanceBA, emittanceC, tc + uint3(tcOffsets.x, 0, 0), outputColor);
    AtomicAddHDR(emittanceRG, emittanceBA, emittanceC, tc + uint3(tcOffsets.y, 0, 0), outputColor);
    AtomicAddHDR(emittanceRG, emittanceBA, emittanceC, tc + uint3(tcOffsets.z, 0, 0), outputColor);

    float3 N = (input.normal * 0.5) + 0.5;
    AtomicAverage(normalMap, tc + uint3(tcOffsets.x, 0, 0), N, 20);
    AtomicAverage(normalMap, tc + uint3(tcOffsets.y, 0, 0), N, 20);
    AtomicAverage(normalMap, tc + uint3(tcOffsets.z, 0, 0), N, 20);
}