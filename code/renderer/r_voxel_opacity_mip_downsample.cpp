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

#include "r_voxel_private.h"

#include "../shaders/opacity_mip_downsample_cs.h"

#define SHADER_GROUP_SIZE 4

struct Local
{
    ComputePipeline pipeline;
};

static Local local;

void OpacityMipDownSample_Init()
{
    ComputePipeline* p = &local.pipeline;
    p->shader = CreateComputeShader(&voxelPrivate.persistent, g_cs, "opacity mip downsample");
}

void OpacityMipDownSample_Run()
{
    DEBUG_REGION("Opacity Mip-map Generation");
    QUERY_REGION(QueryId::OpacityMipMapping);

    // Generate Mips
    u32 mipWidth = voxelShared.gridSize.w / 2;
    u32 mipHeight = voxelShared.gridSize.h / 2;
    u32 mipDepth = voxelShared.gridSize.d / 2;

    u32 numMipLevels = voxelPrivate.numMipLevels;
    for (u32 i = 0; i < numMipLevels - 1; ++i)
    {
        local.pipeline.srvs[0] = voxelShared.opacityMapSRVs[i];
        local.pipeline.uavs[0] = voxelShared.opacityMapUAVs[i + 1];
        local.pipeline.numSRVs = 1;
        local.pipeline.numUAVs = 1;

        u32 x = (mipWidth + SHADER_GROUP_SIZE - 1) / SHADER_GROUP_SIZE;
        u32 y = (mipHeight + SHADER_GROUP_SIZE - 1) / SHADER_GROUP_SIZE;
        u32 z = (mipDepth + SHADER_GROUP_SIZE - 1) / SHADER_GROUP_SIZE;
        x = MAX(x, 1);
        y = MAX(y, 1);
        z = MAX(z, 1);

        SetPipeline(&local.pipeline);
        d3ds.context->Dispatch(x, y, z);

        mipWidth /= 2;
        mipHeight /= 2;
        mipDepth /= 2;
    }
}