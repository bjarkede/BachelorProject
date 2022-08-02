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

Texture2D<float> src : register(t0);
RWTexture2D<float> dst : register(u0);

cbuffer CSBuffer 
{
    uint cst_isOdd;
};

[numthreads(8, 8, 1)] 
void cs_main(uint3 id : SV_DispatchThreadID) 
{
    uint2 srcTC = id.xy * 2;
    uint2 dstID = id.xy;

    uint xIsOdd = cst_isOdd & (1 << 0);
    uint yIsOdd = cst_isOdd & (1 << 1);
    uint xStop = xIsOdd ? 3 : 2;
    uint yStop = yIsOdd ? 3 : 2;
    float maxDepth = 0.0;
    for (uint y = 0; y < yStop; ++y)
    {
        for (uint x = 0; x < xStop; ++x)
        {
            maxDepth = max(maxDepth, src[srcTC + uint2(x, y)]);
        }
    }

    dst[dstID] = maxDepth;
}