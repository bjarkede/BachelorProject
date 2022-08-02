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

Texture2D depthTexture : register(t0);
Texture2D normalTexture : register(t1);
Texture2D<uint> materialTexture : register(t2);

SamplerState pointSampler : register(s0);

cbuffer PSData : register(b0)
{
    float4x4 cst_modelViewMatrix;
    float4x4 cst_invViewProjectionMatrix;
    float4x4 cst_projectionMatrix;
    float4 cst_cameraPosition;
    float4 cst_pixelSize;
    float4 cst_boundingBoxMin;
    float4 cst_boundingBoxMax;
    float4 cst_temp; // x = depthBias, y = slopeBias;
    float4 cst_ab; 
    uint3 cst_gridDim;
};

// The array gets indexed by a material index written to the gbuffer
cbuffer MaterialShaderBuffer : register(b1)
{
    Material materials[MAX_MATERIALS];
};

struct VOut
{
    float4 position : SV_POSITION;
    float2 tc : TEXCOORD;
};

VOut vs_main(uint id
             : SV_VertexID) 
{
    VOut output;

    output.position.x = (float)(id / 2) * 4.0 - 1.0;
    output.position.y = (float)(id % 2) * 4.0 - 1.0;
    output.position.z = 0.0;
    output.position.w = 1.0;

    output.tc.x = (float)(id / 2) * 2.0;
    output.tc.y = 1.0 - (float)(id % 2) * 2.0;

    return output;
}

float4 ps_main(VOut input)
    : SV_Target
{
    float startZoverW = depthTexture.Sample(pointSampler, input.tc).r;
    if (startZoverW == 0.0)
    {
        return float4(0, 0, 0, 1);
    }

    float3 startPosWS = GetWorldSpacePositionFromDepth(cst_invViewProjectionMatrix, startZoverW, input.tc);
    float3 voxelSizeWS3 = (cst_boundingBoxMax.xyz - cst_boundingBoxMin.xyz) / cst_gridDim;
   
    float3 N = normalize(OctDecode(normalTexture.Sample(pointSampler, input.tc).xy));
    float3 V = normalize(cst_cameraPosition.xyz - startPosWS);
    float3 R = normalize(reflect(-V, N));
    const float epsilon = 1.0 / float(1 << 20);
    float3 voxelDimOverDir = voxelSizeWS3 / max(abs(R), epsilon);
    float voxelSizeWS = min3(voxelDimOverDir.x, voxelDimOverDir.y, voxelDimOverDir.z);

    float3 endPosWS = startPosWS + R * voxelSizeWS;

    float startInvDepth = 1.0 / distance(startPosWS, cst_cameraPosition.xyz);
    float endInvDepth = 1.0 / distance(endPosWS, cst_cameraPosition.xyz);

    float4 endPosCS = float4(endPosWS, 1.0);
    endPosCS = mul(endPosCS, cst_modelViewMatrix);
    endPosCS = mul(endPosCS, cst_projectionMatrix);
    endPosCS /= endPosCS.w;

    float2 startPosSS = input.tc;
    float2 endPosSS = NDCToTC(endPosCS.xy);
 
    float endZoverW = depthTexture.Sample(pointSampler, endPosSS).r;
    float2 traceDir = normalize((endPosSS - startPosSS)) * cst_pixelSize.xy;
    float2 tracePosSS = input.tc + traceDir;

    float maxDistance = length(endPosSS - startPosSS);
    float stepDistance = length(traceDir);
    uint numSteps = min(uint(maxDistance / stepDistance), 255);

    uint2 dim;
    materialTexture.GetDimensions(dim.x, dim.y);
    uint materialIndex = materialTexture.Load(uint3(input.tc * dim, 0));
    Material mtrl = materials[materialIndex];
    float coneRatio = ConeRatioFromAperture(ConeAngleFromSpecularExponent(mtrl.specular.a));

    //float coneRatio = 2.0f * tan(cst_temp.z * 0.5 * (3.14 / 180.0));
    float maxRadius = distance(startPosWS, endPosWS) * coneRatio * 0.5;  

#if 1
    float occlusion = 0.0;
    const float depthBias = (1.0 - dot(N, R)) * cst_temp.y + cst_temp.x;
    for (uint s = 0; s < numSteps; ++s)
    {
        float t = float(s + 1) / float(numSteps);
        float traceZoverW = depthTexture.SampleLevel(pointSampler, tracePosSS, 0).r;
        
        float3 tracePosWS = GetWorldSpacePositionFromDepth(cst_invViewProjectionMatrix, traceZoverW, tracePosSS);
        float storedDepth = distance(tracePosWS, cst_cameraPosition.xyz) + depthBias;
        float sampleDepth = 1.0 / lerp(startInvDepth, endInvDepth, t);
        float traceRadius = sampleDepth * maxRadius * endInvDepth * t;
        float minSampleDepth = sampleDepth - traceRadius;
        float maxSampleDepth = sampleDepth + traceRadius;
        
        float sampleDepthRange = maxSampleDepth - minSampleDepth; 
        float occFactor = saturate((maxSampleDepth - storedDepth) / sampleDepthRange);

        if (storedDepth < minSampleDepth)
        {
            float distance = minSampleDepth - storedDepth;
            float attenuation = 1.0 - saturate(cst_temp.w * distance / sampleDepthRange);
            occFactor *= attenuation;
        }

        occlusion = max(occlusion, occFactor);

        if (occlusion >= 1.0)
        {
            break;
        }

        tracePosSS += traceDir;
    }
#endif
    float vis = 1.0 - occlusion;

    return float4(vis, vis, vis, 1.0);
}