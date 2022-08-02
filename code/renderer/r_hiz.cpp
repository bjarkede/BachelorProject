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

#include "r_private.h"
#include "../shaders/hiz_cs.h"

struct Local
{
    ComputePipeline pipeline;
};

#pragma pack(push, 1)
struct CSData
{
    u32 isOdd; // (1 << 0) = x is odd, (1 << 1) = y is odd
    u32 dummy[3];
};
#pragma pack(pop)

static Local local;

void HiZ_Init()
{
    ComputePipeline* p = &local.pipeline;
    p->shader = CreateComputeShader(&d3d.persistent, g_cs, "hierarchical z hiz");
}

void HiZ_Run()
{
    DEBUG_REGION("Hierarchical Z");
    QUERY_REGION(QueryId::HiZ);
    
    ComputePipeline* p = &local.pipeline;

    d3ds.context->CopySubresourceRegion(d3d.depthMap.map, 0, 0, 0, 0, d3d.depthStencilTexture, 0, NULL);

    u32 mipWidth = r_videoConfig.width;
    u32 mipHeight = r_videoConfig.height;
    u32 numMipLevels = ComputeMipCount(r_videoConfig.height, r_videoConfig.width);
    for (u32 m = 0; m < numMipLevels - 1; ++m)
    {
        u32 x = (mipWidth + 8 - 1) / 8;
        u32 y = (mipHeight + 8 - 1) / 8;
        x = MAX(x, 1);
        y = MAX(y, 1);
        
        p->srvs[0] = d3d.depthMap.SRVs[m];
        p->uavs[0] = d3d.depthMap.UAVs[m + 1];
        p->numSRVs = 1;
        p->numUAVs = 1;

        CSData csData = {};
        csData.isOdd |= x % 2 != 0 ? (1 << 0) : 0;
        csData.isOdd |= y % 2 != 0 ? (1 << 1) : 0;
       
        SetPipeline(p);
        d3ds.context->Dispatch(x, y, 1);

        mipWidth /= 2;
        mipHeight /= 2;
    }
}