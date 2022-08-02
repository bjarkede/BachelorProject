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

#include "../shaders/opacity_voxelization_cr0_gs.h"
#include "../shaders/opacity_voxelization_cr0_ps.h"
#include "../shaders/opacity_voxelization_cr1_gs.h"
#include "../shaders/opacity_voxelization_cr1_ps.h"
#include "../shaders/opacity_voxelization_vs.h"

// rasterState[0] = no conservative
// rasterState[1] = conservative
struct Local
{
    GraphicsPipeline pipeline;
    VoxelRasterState rasterState;

    ID3D11GeometryShader* geometryShaders[2]; // 0 software 1 conservative
    ID3D11PixelShader* pixelShaders[2]; // same

    vec2_t halfPixelSizeCS[3];
};

static Local local;
VoxelSharedData voxelShared;
static const VertexBufferId::Type vbIds[] = { VertexBufferId::Position };

void OpacityVoxelization_Init()
{
    GraphicsPipeline* p = &local.pipeline;
    InitPipeline(p);

    p->ia.layout = CreateInputLayout(&voxelPrivate.persistent, vbIds, g_vs, "opacity voxelization");

    p->vs.shader = CreateVertexShader(&voxelPrivate.persistent, g_vs, "opacity voxelization");

    local.geometryShaders[0] = CreateGeometryShader(&voxelPrivate.persistent, g_gs_cr0, "opacity voxelization cr0");
    local.geometryShaders[1] = CreateGeometryShader(&voxelPrivate.persistent, g_gs_cr1, "opacity voxelization cr1");

    local.pixelShaders[0] = CreatePixelShader(&voxelPrivate.persistent, g_ps_cr0, "opacity voxelization cr0");
    local.pixelShaders[1] = CreatePixelShader(&voxelPrivate.persistent, g_ps_cr1, "opacity voxelization cr1");

    CreateVoxelRasterStates(&local.rasterState);

    p->gs.buffers[0] = CreateShaderConstantBuffer(sizeof(VoxelizeGSData), "voxelization geometry cbuffer");
    p->gs.numBuffers = 1;

    p->ps.buffers[0] = CreateShaderConstantBuffer(sizeof(VoxelizePSData), "voxelization pixel cbuffer");
    p->ps.numBuffers = 1;
}

void VoxelizeOpacity_Draw(RenderCommandQueue* cmdQueue, DrawBuffer* drawBuffer, Scene* scene)
{
    DEBUG_REGION("Opacity Voxelization");
    QUERY_REGION(QueryId::OpacityVoxelization);
    GraphicsPipeline* p = &local.pipeline;

    for (u32 i = 0; i < 3; ++i)
    {
        local.halfPixelSizeCS[i].u = 1.0f / p->rs.viewports[i].Width;
        local.halfPixelSizeCS[i].v = 1.0f / p->rs.viewports[i].Height;
    }

    VoxelizeGSData gsData = {};
    Vec3Copy(gsData.bounding_min, cmdQueue->aabb.min);
    Vec3Copy(gsData.bounding_max, cmdQueue->aabb.max);
    gsData.halfPixelSizeCS[0].x = local.halfPixelSizeCS[0].u;
    gsData.halfPixelSizeCS[0].y = local.halfPixelSizeCS[0].v;
    gsData.halfPixelSizeCS[1].x = local.halfPixelSizeCS[1].u;
    gsData.halfPixelSizeCS[1].y = local.halfPixelSizeCS[1].v;
    gsData.halfPixelSizeCS[2].x = local.halfPixelSizeCS[2].u;
    gsData.halfPixelSizeCS[2].y = local.halfPixelSizeCS[2].v;
    SetShaderData(p->gs.buffers[0], gsData);

    VoxelizePSData psData = {};
    psData.cst_inverseViewportSize[0].x = local.halfPixelSizeCS[0].u;
    psData.cst_inverseViewportSize[0].y = local.halfPixelSizeCS[0].v;
    psData.cst_inverseViewportSize[1].x = local.halfPixelSizeCS[1].u;
    psData.cst_inverseViewportSize[1].y = local.halfPixelSizeCS[1].v;
    psData.cst_inverseViewportSize[2].x = local.halfPixelSizeCS[2].u;
    psData.cst_inverseViewportSize[2].y = local.halfPixelSizeCS[2].v;

    SetVoxelRasterMode(p, r_backendFlags.consRasterMode, local.rasterState);
    u32 conservativeIndex = r_backendFlags.consRasterMode == ConsRasterMode::Software ? 1 : 0;
    p->gs.shader = local.geometryShaders[conservativeIndex];
    p->ps.shader = local.pixelShaders[conservativeIndex];

    SetDrawBuffer(p, drawBuffer, vbIds);

    const u32 clear[] = { 0, 0, 0, 0 };
    d3ds.context->ClearUnorderedAccessViewUint(voxelShared.opacityMapUAVs[0], clear);

    p->om.uavs[0] = voxelShared.opacityMapUAVs[0];
    p->om.numUAVs = 1;

    p->rs.numScissors = 0;
    p->rs.numViewports = 0;
    PushViewportAndScissor(p, 0, 0, voxelShared.gridSize.h, voxelShared.gridSize.d);
    PushViewportAndScissor(p, 0, 0, voxelShared.gridSize.w, voxelShared.gridSize.d);
    PushViewportAndScissor(p, 0, 0, voxelShared.gridSize.w, voxelShared.gridSize.h);

    SetPipeline(p);
    // this is incorrect as it attempts to draw alpha-tested surfaces!
    // there should be 1 draw call for all the opaque meshes,
    // then 1 draw call per alpha-tested material
    //DrawIndexed(drawBuffer, bisect.child.count, bisect.child.start);
    for (u32 m = 0; m < scene->meshes.Length(); ++m)
    {
        MeshFileMesh* mesh = &scene->meshes[m];
        Material* material = &scene->materials[mesh->materialIndex];
        Vec4Copy(psData.cst_alphaTestedColor, material->alphaTestedColor);
        psData.cst_flags = material->flags;
        SetShaderData(p->ps.buffers[0], psData);
        DrawIndexed(drawBuffer, mesh->numIndexes, mesh->firstIndex);
    }
}
