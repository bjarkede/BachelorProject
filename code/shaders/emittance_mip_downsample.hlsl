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

#include "voxel_types.hlsli"

EmittanceTexture3D src : register(t0);
EmittanceRWTexture3D dst : register(u0);

// front to back compositing
// 8 texels becomes 4 groups of two texels
// for each group:
// result_opacity = front_opacity + (1 - front_opacity) * back_opacity
// result_color   = front_color   + (1 - front_opacity) * back_color
[numthreads(4, 4, 4)] void cs_main(uint3 id
                                   : SV_DispatchThreadID) {
    // go up one mip level and sample the neighboring texels
    uint3 dstDim;
    dst.GetDimensions(dstDim.x, dstDim.y, dstDim.z);
    dstDim.x /= 6;
    uint3 srcTC = id * 2;
    uint3 dstTC = id;

    // +x
    float4 result_px = float4(0.0, 0.0, 0.0, 0.0);
    result_px += src[srcTC + uint3(0, 0, 1)] + (1 - src[srcTC + uint3(0, 0, 1)].a) * src[srcTC + uint3(1, 0, 1)];
    result_px += src[srcTC + uint3(0, 0, 0)] + (1 - src[srcTC + uint3(0, 0, 0)].a) * src[srcTC + uint3(1, 0, 0)];
    result_px += src[srcTC + uint3(0, 1, 1)] + (1 - src[srcTC + uint3(0, 1, 1)].a) * src[srcTC + uint3(1, 1, 1)];
    result_px += src[srcTC + uint3(0, 1, 0)] + (1 - src[srcTC + uint3(0, 1, 0)].a) * src[srcTC + uint3(1, 1, 0)];
    result_px /= 4;
    dst[dstTC] = result_px;

    // -x
    srcTC.x += dstDim.x * 2;
    dstTC.x += dstDim.x;
    float4 result_nx = float4(0.0, 0.0, 0.0, 0.0);
    result_nx += src[srcTC + uint3(1, 0, 0)] + (1 - src[srcTC + uint3(1, 0, 0)].a) * src[srcTC + uint3(0, 0, 0)];
    result_nx += src[srcTC + uint3(1, 0, 1)] + (1 - src[srcTC + uint3(1, 0, 1)].a) * src[srcTC + uint3(0, 0, 1)];
    result_nx += src[srcTC + uint3(1, 1, 0)] + (1 - src[srcTC + uint3(1, 1, 0)].a) * src[srcTC + uint3(0, 1, 0)];
    result_nx += src[srcTC + uint3(1, 1, 1)] + (1 - src[srcTC + uint3(1, 1, 1)].a) * src[srcTC + uint3(0, 1, 1)];
    result_nx /= 4;
    dst[dstTC] = result_nx;

    // +y
    srcTC.x += dstDim.x * 2;
    dstTC.x += dstDim.x;
    float4 result_py = float4(0.0, 0.0, 0.0, 0.0);
    result_py += src[srcTC + uint3(0, 0, 0)] + (1 - src[srcTC + uint3(0, 0, 0)].a) * src[srcTC + uint3(0, 1, 0)];
    result_py += src[srcTC + uint3(0, 0, 1)] + (1 - src[srcTC + uint3(0, 0, 1)].a) * src[srcTC + uint3(0, 1, 1)];
    result_py += src[srcTC + uint3(1, 0, 0)] + (1 - src[srcTC + uint3(1, 0, 0)].a) * src[srcTC + uint3(1, 1, 0)];
    result_py += src[srcTC + uint3(1, 0, 1)] + (1 - src[srcTC + uint3(1, 0, 1)].a) * src[srcTC + uint3(1, 1, 1)];
    result_py /= 4;
    dst[dstTC] = result_py;

    // -y
    srcTC.x += dstDim.x * 2;
    dstTC.x += dstDim.x;
    float4 result_ny = float4(0.0, 0.0, 0.0, 0.0);
    result_ny += src[srcTC + uint3(0, 1, 0)] + (1 - src[srcTC + uint3(0, 1, 0)].a) * src[srcTC + uint3(0, 0, 0)];
    result_ny += src[srcTC + uint3(0, 1, 1)] + (1 - src[srcTC + uint3(0, 1, 1)].a) * src[srcTC + uint3(0, 0, 1)];
    result_ny += src[srcTC + uint3(1, 1, 1)] + (1 - src[srcTC + uint3(1, 1, 1)].a) * src[srcTC + uint3(1, 0, 1)];
    result_ny += src[srcTC + uint3(1, 1, 0)] + (1 - src[srcTC + uint3(1, 1, 0)].a) * src[srcTC + uint3(1, 0, 0)];
    result_ny /= 4;
    dst[dstTC] = result_ny;

    // +z
    srcTC.x += dstDim.x * 2;
    dstTC.x += dstDim.x;
    float4 result_pz = float4(0.0, 0.0, 0.0, 0.0);
    result_pz += src[srcTC + uint3(0, 0, 0)] + (1 - src[srcTC + uint3(0, 0, 0)].a) * src[srcTC + uint3(0, 0, 1)];
    result_pz += src[srcTC + uint3(1, 0, 0)] + (1 - src[srcTC + uint3(1, 0, 0)].a) * src[srcTC + uint3(1, 0, 1)];
    result_pz += src[srcTC + uint3(0, 1, 0)] + (1 - src[srcTC + uint3(0, 1, 0)].a) * src[srcTC + uint3(0, 1, 1)];
    result_pz += src[srcTC + uint3(1, 1, 0)] + (1 - src[srcTC + uint3(1, 1, 0)].a) * src[srcTC + uint3(1, 1, 1)];
    result_pz /= 4;
    dst[dstTC] = result_pz;

    // -z
    srcTC.x += dstDim.x * 2;
    dstTC.x += dstDim.x;
    float4 result_nz = float4(0.0, 0.0, 0.0, 0.0);
    result_nz += src[srcTC + uint3(0, 0, 1)] + (1 - src[srcTC + uint3(0, 0, 1)].a) * src[srcTC + uint3(0, 0, 0)];
    result_nz += src[srcTC + uint3(1, 0, 1)] + (1 - src[srcTC + uint3(1, 0, 1)].a) * src[srcTC + uint3(1, 0, 0)];
    result_nz += src[srcTC + uint3(0, 1, 1)] + (1 - src[srcTC + uint3(0, 1, 1)].a) * src[srcTC + uint3(0, 1, 0)];
    result_nz += src[srcTC + uint3(1, 1, 1)] + (1 - src[srcTC + uint3(1, 1, 1)].a) * src[srcTC + uint3(1, 1, 0)];
    result_nz /= 4;
    dst[dstTC] = result_nz;
}
