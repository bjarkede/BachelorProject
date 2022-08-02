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

cbuffer ShadowGeometryShaderBuffer
{
    float4x4 viewMatrix;
    float4x4 projMatrix;
    float4 lightPosition;
    float2 resolutionSlopeBias; // x = res, y = slope bias
};

struct VIn
{
    float4 position : POSITION;
    float3 normal : NORMAL;
};

struct VOut
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
};

struct GOut
{
    float4 position : SV_POSITION;
};

VOut vs_main(VIn input)
{
    VOut output;
    output.position = input.position;
    output.normal = input.normal;
    return output;
}

// Ignacio Castaño
float SlopeScaledBias(float NL)
{
    float cosAlpha = NL;
    float sinAlpha = sqrt(1.0 - cosAlpha * cosAlpha); // sin(acos(N.L))
    float tanAlpha = sinAlpha / cosAlpha; // tan(acos(N.L))
    return min(tanAlpha, 2.0);
}

// NL is max(dot(N, L), 0.0)
float NormalScale(float3 positionWS, float NL)
{
    float depth = -mul(float4(positionWS, 1.0), viewMatrix).z;
    float fovScale = max(projMatrix[0].x, projMatrix[1].y);
    float texelSize = resolutionSlopeBias.x;
    texelSize *= depth;
    texelSize *= fovScale;
    float normalScale = SlopeScaledBias(NL);
    normalScale *= texelSize;
    normalScale *= resolutionSlopeBias.y;
    return normalScale;
}

[maxvertexcount(3)] void gs_main(triangle VOut input[3], inout TriangleStream<GOut> os) {
    GOut output[3];

    for (uint i = 0; i < 3; ++i)
    {
        float3 positionWS = input[i].position.xyz;
        float3 N = input[i].normal;
        float3 L = normalize(lightPosition.xyz - positionWS);
        float NL = max(dot(N, L), 0.0);
        float3 newPos = positionWS - (N * NormalScale(positionWS, NL));
        output[i].position = mul(float4(newPos, 1.0), viewMatrix);
        output[i].position = mul(output[i].position, projMatrix);

        os.Append(output[i]);
    }

    os.RestartStrip();
}

    [earlydepthstencil] void ps_main()
{
}