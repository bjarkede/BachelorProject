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
#include "../shaders/voxelization_fix_cs.h"

struct Local
{
    ComputePipeline pipeline;
};

#pragma pack(push, 1)
struct CSData
{
    u32 maxDistance;
    u32 axis;
    u32 dummy[2];
};
#pragma pack(pop)

static Local local;

void VoxelizationFix_Init()
{
    ComputePipeline* p = &local.pipeline;
    p->shader = CreateComputeShader(&voxelPrivate.persistent, g_cs, "voxelization fix");
    p->buffers[0] = CreateShaderConstantBuffer(sizeof(CSData), "voxelization fix cbuffer");
    p->numBuffers = 1;
}

void VoxelizationFix_Run()
{
    if (!r_backendFlags.useGapFilling)
    {
        return;
    }

    DEBUG_REGION("Voxelization Fix");
    QUERY_REGION(QueryId::VoxelizationFix);

    ComputePipeline* p = &local.pipeline;

    u32 mipWidth = voxelShared.gridSize.w;
    u32 mipHeight = voxelShared.gridSize.h;
    u32 mipDepth = voxelShared.gridSize.d;

    u32 x = (mipWidth + 8 - 1) / 8;
    u32 y = (mipHeight + 8 - 1) / 8;

    p->uavs[0] = voxelShared.emittanceMaps[0].UAVs[0];
    p->numUAVs = 1;
    
    SetPipeline(p);
    
    CSData csData;
    csData.maxDistance = voxelShared.maxGapDistance;

    for (u32 a = 0; a < 3; ++a)
    {
        csData.axis = a;
        SetShaderData(p->buffers[0], csData);
        d3ds.context->Dispatch(x, y, 1);
    }
}