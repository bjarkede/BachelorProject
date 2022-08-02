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

Texture2D sssoTexture   : register(t0);
Texture2D depthTexture  : register(t1);
Texture2D normalTexture : register(t2);

SamplerState pointSampler : register(s0);

cbuffer PSData {
    float4 dir; //xy = dir, zw = pixelSize
    float4 temp;
};

struct VOut
{
    float4 position : SV_POSITION;
    float2 tc : TEXCOORD;
};


#if 0
#define KERNEL_RADIUS 16
static const float kernel[] = {
    0.075073,
    0.073764,
    0.069975,
    0.064088,
    0.056668,
    0.048376,
    0.039871,
    0.031726,
    0.024372,
    0.018077,
    0.012944,
    0.008949,
    0.005973,
    0.003849,
    0.002394,
    0.001438,
};
#else
#define KERNEL_RADIUS 4
static const float kernel[] = 
{
    0.301377,
    0.227491,
    0.097843,
    0.023977,
};
#endif

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

static const float epsilon = 1.0 / float(1 << 20);

float4 ps_main(VOut input)
    : SV_Target
{
    float2 stepDistance01 = dir.xy * dir.zw;
    float2 startPos01 = input.tc - stepDistance01 * KERNEL_RADIUS;

    float3 N = OctDecode(normalTexture.Sample(pointSampler, input.tc).xy);
    float z = depthTexture.Sample(pointSampler, input.tc).r;

    float4 result = float4(0, 0, 0, 0);
    float totalWeight = 0.0;
    for (uint i = 0; i < (KERNEL_RADIUS * 2) - 1; ++i)
    {
        float2 samplePos = startPos01 + stepDistance01 * i;

        float ws = kernel[(i + (1 * uint(i / KERNEL_RADIUS))) % KERNEL_RADIUS];
        
        float3 sampleN = OctDecode(normalTexture.Sample(pointSampler, samplePos).xy);
        float NsampleN = max(dot(N, sampleN), 0.0);
        float wn = pow(NsampleN, 32);

        float sampleDepth = depthTexture.Sample(pointSampler, samplePos).r;
        float depthDifference = abs(sampleDepth - z);
        float wd = 1.0 / (0.25 + depthDifference);
        float weight = ws * wn * wd;
        totalWeight += weight;

        result += sssoTexture.Sample(pointSampler, samplePos).rgba * weight;
    }

    return result / totalWeight;
}