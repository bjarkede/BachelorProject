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

#include "../shaders/voxel_vis_gs.h"
#include "../shaders/voxel_vis_ps.h"
#include "../shaders/voxel_vis_vs.h"

struct Local
{
    GraphicsPipeline pipeline;
};

#pragma pack(push, 1)
struct VoxelVizVSData
{
    uint3_t gridSize;
    u32 dummy;
};

struct VoxelVizGSData
{
    f32 modelViewMatrix[16];
    f32 projectionMatrix[16];

    vec4_t bounding_min;
    vec4_t bounding_max;

    vec4_t cameraPosition;
    uint3_t gridSize;
    u32 mipLevel;
    u32 cst_singleChannel;
    u32 dummy[3];
};

struct VoxelVizPSData
{
    u32 cst_mipLevel;
    u32 cst_wireFrameMode;
    u32 cst_singleChannel;
    u32 dummy;
};
#pragma pack(pop)

static Local local;

void VoxelViz_Init()
{
    GraphicsPipeline* p = &local.pipeline;
    InitPipeline(p);

    p->vs.shader = CreateVertexShader(&voxelPrivate.persistent, g_vs, "voxel viz");

    p->gs.shader = CreateGeometryShader(&voxelPrivate.persistent, g_gs, "voxel viz");

    p->ps.shader = CreatePixelShader(&voxelPrivate.persistent, g_ps, "voxel viz");

    p->ia.topology = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;

    p->vs.buffers[0] = CreateShaderConstantBuffer(sizeof(VoxelVizVSData), "voxel viz vertex cbuffer");
    p->vs.numBuffers = 1;

    p->gs.buffers[0] = CreateShaderConstantBuffer(sizeof(VoxelVizGSData), "voxel viz geometry cbuffer");
    p->gs.numBuffers = 1;

    p->ps.buffers[0] = CreateShaderConstantBuffer(sizeof(VoxelVizPSData), "voxel viz pixel cbuffer");
    p->ps.numBuffers = 1;

    D3D11_RASTERIZER_DESC rasterDesc;
    ZeroMemory(&rasterDesc, sizeof(rasterDesc));
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.CullMode = D3D11_CULL_BACK;
    rasterDesc.FrontCounterClockwise = FALSE;
    rasterDesc.ScissorEnable = TRUE;
    rasterDesc.DepthClipEnable = FALSE;
    rasterDesc.DepthBiasClamp = 0.0f;
    p->rs.state = CreateRasterizerState(&rasterDesc);
}

void Voxel_DrawViz(RenderCommandQueue* cmdQueue)
{
    DEBUG_REGION("Voxel viz");
    QUERY_REGION(QueryId::VoxelViz);

    GraphicsPipeline* p = &local.pipeline;

    uint3_t gridSize;
    Uint3Copy(gridSize, voxelShared.gridSize);
    gridSize.w >>= voxelVizConstants.cst_mipLevel;
    gridSize.h >>= voxelVizConstants.cst_mipLevel;
    gridSize.d >>= voxelVizConstants.cst_mipLevel;

    VoxelVizVSData vsData;
    Uint3Copy(vsData.gridSize, gridSize);
    SetShaderData(p->vs.buffers[0], vsData);

    VoxelVizGSData gsData;
    memcpy(gsData.modelViewMatrix, cmdQueue->modelViewMatrix.E, sizeof(m4x4));
    memcpy(gsData.projectionMatrix, cmdQueue->projectionMatrix.E, sizeof(m4x4));
    Vec3Copy(gsData.cameraPosition, cmdQueue->viewVector);
    Vec3Copy(gsData.bounding_min, cmdQueue->aabb.min);
    Vec3Copy(gsData.bounding_max, cmdQueue->aabb.max);
    Uint3Copy(gsData.gridSize, gridSize);
    gsData.mipLevel = voxelVizConstants.cst_mipLevel;
    gsData.cst_singleChannel = voxelVizConstants.cst_singleChannel;
    SetShaderData(p->gs.buffers[0], gsData);

    VoxelVizPSData psData;
    psData.cst_mipLevel = voxelVizConstants.cst_mipLevel;
    psData.cst_wireFrameMode = voxelVizConstants.cst_wireFrameMode;
    psData.cst_singleChannel = voxelVizConstants.cst_singleChannel ? 1 : 0;
    SetShaderData(p->ps.buffers[0], psData);

    SetDefaultViewportAndScissor(p);

    p->gs.srvs[0] = voxelVizConstants.cst_singleChannel ? voxelShared.opacityMapFullSRV : voxelShared.emittanceMaps[voxelShared.emittanceReadIndex].fullSRV;
    p->gs.numSRVs = 1;

    p->om.depthView = d3d.depthStencilView;
    p->om.rtvs[0] = d3d.backBufferRTView;
    p->om.numRTVs = 1;

    u32 numVoxels = gridSize.w * gridSize.h * gridSize.d;
    SetPipeline(&local.pipeline);
    d3ds.context->Draw(numVoxels, 0);
}