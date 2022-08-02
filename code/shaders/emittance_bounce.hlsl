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
#include "voxel_types.hlsli"
#include "cone_tracing.hlsli"

EmittanceTexture3D src : register(t0);
EmittanceRWTexture3D dst : register(u0);
Texture3D normal : register(t1);

SamplerState samp : register(s0);

[numthreads(4, 4, 4)] void cs_main(uint3 id
                                   : SV_DispatchThreadID) {
    uint3 texDim;
    dst.GetDimensions(texDim.x, texDim.y, texDim.z);
    texDim.x /= 6;

    uint3 tc = id;
    float3 P = (float3(tc) + float3(0.5, 0.5, 0.5)) / float3(texDim);

    for (uint f = 0; f < 6; ++f)
    {
        if (src[tc].a > 0.0)
        {
            float unused;
            float3 N = normalize((normal[tc].xyz * 2.0) - 1.0);
            ConeTrace trace;
            trace.emittanceMap = src;
            trace.samp = samp;
            trace.origin = P;
            trace.normal = N;
            trace.stepScale = 1.0;
            trace.mipBias = 0.0;
            trace.maxDiameter = 8.0;
            trace.opacityThreshold = 0.95;
            trace.distanceScale = 200.0;
            float3 emittance = ComputeIndirectDiffuse(trace, unused);
            dst[tc] = float4(emittance + src[tc].rgb, 1.0);
        }
        tc.x += texDim.x;
    }
}