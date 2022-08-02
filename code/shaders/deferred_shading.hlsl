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

#define CONE_COUNT 9
#include "shared.hlsli"
#include "deferred_options.hlsli"
#include "material_flags.hlsli"
#include "voxel_types.hlsli"
#include "cone_tracing.hlsli"

Texture2D albedoTexture : register(t0);
Texture2D normalTexture : register(t1);
Texture2D<uint> material : register(t2);
Texture2D depthTexture : register(t3);
Texture2DArray shadowMap : register(t4);
Texture3D opacityMap : register(t5);
EmittanceTexture3D emittanceMap : register(t6);
Texture2D sssoTexture : register(t7);

SamplerState albedoSampler : register(s0);
SamplerState voxelSampler : register(s1);
SamplerState pointSampler : register(s2);
SamplerComparisonState shadowSampler : register(s3);

cbuffer DeferredShadingPixelShaderBuffer : register(b0)
{
    float4x4 invViewProjectionMatrix;

    float4 bounding_min;
    float4 bounding_max;
    float4 cameraPosition;
    float4 viewVector;

    // Constants
    float4 temp;

    float cst_opacityThreshold;
    float cst_traceStepScale;
    float cst_occlusionScale;
    float cst_specularConeRatio;
    float cst_maxDiameter;
    float cst_mipBias;
    float cst_slopeBias;
    float cst_fixedBias;
    float cst_maxBias;
    float fdummy1;
    float fdummy2;
    float fdummy3;

    uint cst_options;
};

// The array gets indexed by a material index written to the gbuffer
cbuffer MaterialShaderBuffer : register(b1)
{
    Material materials[MAX_MATERIALS];
};

cbuffer LightBuffer : register(b2)
{
    LightEntity pointLights[MAX_LIGHTS];
    uint cst_lightCount;
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
};

static const float2 poissonDisk[16] = {
    float2(-0.94201624, -0.39906216),
    float2(0.94558609, -0.76890725),
    float2(-0.094184101, -0.92938870),
    float2(0.34495938, 0.29387760),
    float2(-0.91588581, 0.45771432),
    float2(-0.81544232, -0.87912464),
    float2(-0.38277543, 0.27676845),
    float2(0.97484398, 0.75648379),
    float2(0.44323325, -0.97511554),
    float2(0.53742981, -0.47373420),
    float2(-0.26496911, -0.41893023),
    float2(0.79197514, 0.19090188),
    float2(-0.24188840, 0.99706507),
    float2(-0.81409955, 0.91437590),
    float2(0.19984126, 0.78641367),
    float2(0.14383161, -0.14100790)
};

#define POISSON_DISK_NUM_SAMPLES 16

float PCSS_SearchRadius(float lightSizeUV, float lzReceiver, float lzNear)
{
    // this uses similar triangles to compute what area of the shadow map we should search
    float radius = lightSizeUV * (lzReceiver - lzNear) / lzReceiver;
    return radius;
}

void PCSS_SearchBlockers(out float zBlockerSum, out float numBlockers, float2 uv, float slice, float2 searchRadiusUV, float zReceiver)
{
    zBlockerSum = 0.0;
    numBlockers = 0.0;
    for (int i = 0; i < POISSON_DISK_NUM_SAMPLES; ++i)
    {
        float2 tc = uv + poissonDisk[i] * searchRadiusUV;
        float zBlocker = shadowMap.SampleLevel(pointSampler, float3(tc, slice), 0).r;
        if (zBlocker > zReceiver)
        {
            zBlockerSum += zBlocker;
            numBlockers += 1.0;
        }
    }
}

float PCSS_FilterRadius(float lightSizeUV, float lzReceiver, float lzBlocker, float lzNear)
{
    // parallel plane estimation
    float penumbraRatio = (lzReceiver - lzBlocker) / lzBlocker;
    float size = penumbraRatio * lightSizeUV * (lzNear / lzReceiver);
    return size;
}

float PCSS_Filter(float2 uv, float filterRadiusUV, float zReceiver, float2x2 rotation, float slice)
{
    float sum = 0.0f;
    for (int i = 0; i < POISSON_DISK_NUM_SAMPLES; ++i)
    {
        float2 offset = poissonDisk[i] * filterRadiusUV;
        offset = mul(offset, rotation);
        sum += shadowMap.SampleCmpLevelZero(shadowSampler, float3(uv + offset, slice), zReceiver);
    }
    float average = sum / float(POISSON_DISK_NUM_SAMPLES);
    return average;
}

float PCSS_Visibility(float3 positionWS, LightEntity light, float slice)
{
    float4 positionLCS = float4(positionWS, 1.0);
    positionLCS = mul(positionLCS, light.lightView);
    positionLCS = mul(positionLCS, light.lightProj);
    positionLCS /= positionLCS.w; // light clip space -> NDC
    if (!IsPointInBox(positionLCS.xyz, float3(-1, -1, 0), float3(1, 1, 1)))
    {
        return 1.0;
    }
    float2 shadowMapTC = positionLCS.xy * 0.5 + 0.5; // shadow map texture space
    shadowMapTC.y = 1.0 - shadowMapTC.y; // fix tc

    // zReceiver is the depth of the current fragment from the light's point of view
    float zReceiver = positionLCS.z;
    float bias = cst_fixedBias;
    float lzFar = light.positionWS.w;
    float lzNear = light.params.y;
    float lightSizeUV = light.params.x;
    zReceiver += bias;
    float lzReceiver = LinearDepth(zReceiver, lzFar, lzNear);

    float searchRadiusUV = PCSS_SearchRadius(lightSizeUV, lzReceiver, lzNear);
    float zBlockerSum;
    float numBlockers;
    PCSS_SearchBlockers(zBlockerSum, numBlockers, shadowMapTC, slice, searchRadiusUV, zReceiver);
    if (numBlockers < 1.0)
    {
        return 1.0;
    }
    float zBlocker = zBlockerSum / numBlockers;
    float lzBlocker = LinearDepth(zBlocker, lzFar, lzNear);
    float filterRadiusUV = PCSS_FilterRadius(lightSizeUV, lzReceiver, lzBlocker, lzNear);
    float2x2 rotation = CreateRandomRotationMatrix(positionWS);
    float visibility = PCSS_Filter(shadowMapTC, filterRadiusUV, zReceiver, rotation, slice);

    return visibility;
}

float4 ps_main(VOut input)
    : SV_Target
{
    float depth = depthTexture.Sample(pointSampler, input.tc).r;
    if (depth == 0.0)
    {
        return float4(0, 0, 0, 1);
    }
    float3 positionWS = GetWorldSpacePositionFromDepth(invViewProjectionMatrix, depth, input.tc);

    uint2 dim;
    material.GetDimensions(dim.x, dim.y);

    float4 finalColor = float4(0.0f, 0.0f, 0.0f, 1.0f);
    float4 albedoSpec = albedoTexture.Sample(albedoSampler, input.tc);
    float3 N = normalize(OctDecode(normalTexture.Sample(albedoSampler, input.tc).xy));

    uint materialIndex = material.Load(uint3(input.tc * dim, 0));

    Material mtrl = materials[materialIndex];
    float4 s = mtrl.specular;
    s.rgb *= albedoSpec.a;

    float3 V = normalize(cameraPosition.xyz - positionWS);
    
    if (cst_options & DEFERRED_DIRECT_LIGHTING)
    {
        if (mtrl.flags & IS_EMISSIVE)
        {
            finalColor.rgb = albedoSpec.rgb;
        }
        else
        {
            for (uint i = 0; i < cst_lightCount; ++i)
            {

                float vis = 1.0;
                float slice = float(i);

                if (cst_options & DEFERRED_SHADOWS_PCSS)
                {
                    vis = PCSS_Visibility(positionWS, pointLights[i], slice);
                }
                else if (cst_options & DEFERRED_SHADOWS_PCF)
                {
                    vis = PCF_Visibility(shadowMap, shadowSampler, positionWS, pointLights[i], slice, cst_fixedBias);
                }

                finalColor.rgb += Shade(pointLights[i], positionWS, N, V, albedoSpec.rgb, s.rgb, s.a, vis);
            }
        }
    }
   
    float3 normalScale = 1.0 / (bounding_max.xyz - bounding_min.xyz);
    float3 position01 = (positionWS - bounding_min.xyz) / (bounding_max.xyz - bounding_min.xyz);
    float3 normal01 = normalize(N * normalScale);

    ConeTrace trace;
    trace.emittanceMap = emittanceMap;
    trace.opacityMap = opacityMap;
    trace.samp = voxelSampler;
    trace.origin = position01;
    trace.normal = normal01;
    trace.coneRatio = ConeRatioFromAperture(ConeAngleFromSpecularExponent(s.a));
    trace.stepScale = cst_traceStepScale;
    trace.mipBias = cst_mipBias;
    trace.maxDiameter = 8.0; // cst_maxDiameter
    trace.opacityThreshold = cst_opacityThreshold;
    trace.distanceScale = cst_occlusionScale;

    if (cst_options & DEFERRED_INDIRECT_SPECULAR)
    {
        float3 R = normalize(reflect(-V, N) * normalScale);
        float NL = max(0.0, dot(N, R));
        float f = SchlickFresnel(NL);
        float unused;
        float sOcclusion = 1.0;
        if (cst_options & DEFERRED_SSSO)
        {
            sOcclusion = sssoTexture.Sample(albedoSampler, input.tc).r;
        }

        trace.dir = R;
        finalColor.rgb += s.rgb * f * TraceEmittanceCone(trace, unused) * sOcclusion;
    }

    float ao = 0.0;
    if (cst_options & DEFERRED_INDIRECT_DIFFUSE)
    {
        finalColor.rgb += albedoSpec.rgb * ComputeIndirectDiffuse(trace, ao);
    }

    if (cst_options & DEFERRED_AMBIENT_OCCLUSION)
    {
        if (cst_options & DEFERRED_ONLY_AMBIENT_OCCLUSION)
        {
            finalColor.rgb = float3(1.0, 1.0, 1.0);
        }

        if ((cst_options & DEFERRED_INDIRECT_DIFFUSE) == 0)
        {
            ao = ComputeAmbientOcclusion(trace);
        }

        finalColor.rgb *= (1.0f - ao);
    }

    if ((cst_options & (DEFERRED_SSSO | DEFERRED_SSSO_VIS)) == (DEFERRED_SSSO | DEFERRED_SSSO_VIS))
    {
        finalColor.rgb = sssoTexture.Sample(albedoSampler, input.tc).rgb;
    }    

    return finalColor;
}