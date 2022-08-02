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

#include "../shaders/emittance_bounce_cs.h"

struct Local
{
    ComputePipeline pipeline;
};

Local local;

void EmittanceBounce_Init()
{
    ComputePipeline* p = &local.pipeline;
    p->shader = CreateComputeShader(&voxelPrivate.persistent, g_cs, "emittance bounce");

    // Texture3D Sampler
    D3D11_SAMPLER_DESC samplerDesc = CreateSamplerDesc(D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP, 1.0f);
    p->samplers[0] = CreateSamplerState(&samplerDesc);
    p->numSamplers = 1;
}

void EmittanceBounce_Run(u32 readIndex, u32 writeIndex)
{
    DEBUG_REGION("Emittance Bounce");
    QUERY_REGION(QueryId::EmittanceBounce);

    ComputePipeline* p = &local.pipeline;

    u32 mipWidth = voxelShared.gridSize.w;
    u32 mipHeight = voxelShared.gridSize.h;
    u32 mipDepth = voxelShared.gridSize.d;

    p->srvs[0] = voxelShared.emittanceMaps[readIndex].SRVs[0];
    p->srvs[1] = voxelShared.normalMapSRV;
    p->numSRVs = 2;
    p->uavs[0] = voxelShared.emittanceMaps[writeIndex].UAVs[0];
    p->numUAVs = 1;

    FLOAT clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    d3ds.context->ClearUnorderedAccessViewFloat(voxelShared.emittanceMaps[writeIndex].UAVs[0], clearColor);


    u32 x = (mipWidth + VOXEL_GROUP_SIZE - 1) / VOXEL_GROUP_SIZE;
    u32 y = (mipHeight + VOXEL_GROUP_SIZE - 1) / VOXEL_GROUP_SIZE;
    u32 z = (mipDepth + VOXEL_GROUP_SIZE - 1) / VOXEL_GROUP_SIZE;
    x = MAX(x, 1);
    y = MAX(y, 1);
    z = MAX(z, 1);

    SetPipeline(p);
    d3ds.context->Dispatch(x, y, z);
}