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

Texture2D albedoTexture : register(t0);
Texture2D bumpTexture : register(t1);
Texture2D specTexture : register(t2);
SamplerState albedoSampler : register(s0);

cbuffer GeometryPixelShaderData
{
    float4 camPosWS;
    float4 normalStrength;
    uint materialIndex;
    uint cst_useNormalMap;
};

struct VIn
{
    float4 position : POSITION;
    float3 normal : NORMAL;
    float2 tc : TEXCOORD0;
};

struct VOut
{
    float4 positionWS : POSITION;
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 tc : TEXCOORD0;
};

struct POut
{
    float4 albedo : SV_Target0;
    float4 normal : SV_Target1;
    uint material : SV_Target2;
};

VOut vs_main(VIn input)
{
    VOut output;

    output.positionWS = input.position;
    output.position = mul(input.position, modelViewMatrix);
    output.position = mul(output.position, projectionMatrix);

    output.normal = input.normal;

    output.tc = input.tc;

    return output;
};

// V is camera position - fragment position
float3 PerturbNormal(float3 N, float3 V, float2 uv)
{
    float3 map = DecodeTangentSpaceNormal(bumpTexture.Sample(albedoSampler, uv).xy);
    float3x3 TBN = CotangentFrame(N, -V, uv);
    return normalize(mul(map, TBN));
}

POut ps_main(VOut input)
{
    POut output;

    float4 albedo = albedoTexture.Sample(albedoSampler, input.tc);
    if (albedo.a == 0.0)
    {
        discard;
    }

    float spec = specTexture.Sample(albedoSampler, input.tc).r;
    output.albedo = float4(albedo.rgb, spec);

    float3 V = normalize(input.positionWS.xyz - camPosWS.xyz);
    float3 N = input.normal;
    if (cst_useNormalMap)
    {
        N = PerturbNormal(N, V, input.tc);
        N = slerp(input.normal, N, normalStrength.x);
    }

    output.normal = float4(OctEncode(N), 0.0, 0.0);
    
    float index = materialIndex;
    output.material = index;

    return output;
}