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

#include "../shaders/emittance_voxelization_cr0_ps.h"
#include "../shaders/emittance_voxelization_cr1_ps.h"
#include "../shaders/emittance_voxelization_cr0_gs.h"
#include "../shaders/emittance_voxelization_cr1_gs.h"
#include "../shaders/emittance_voxelization_vs.h"

struct Local
{
    GraphicsPipeline pipeline;
    VoxelRasterState rasterState;

    ID3D11GeometryShader* geometryShaders[2]; // 0 software 1 conservative
    ID3D11PixelShader* pixelShaders[2]; // conservative, hdr

    vec2_t halfPixelSizeCS[3];
};

static Local local;
static const VertexBufferId::Type vbIds[] = { VertexBufferId::Position, VertexBufferId::Tc };

void EmittanceVoxelization_Init()
{
    GraphicsPipeline* p = &local.pipeline;
    InitPipeline(p);

    p->ia.layout = CreateInputLayout(&voxelPrivate.persistent, vbIds, g_vs, "emittance voxelization");

    p->vs.shader = CreateVertexShader(&voxelPrivate.persistent, g_vs, "emittance voxelization");

    local.geometryShaders[0] = CreateGeometryShader(&voxelPrivate.persistent, g_gs_cr0, "emittance voxelization cr0");
    local.geometryShaders[1] = CreateGeometryShader(&voxelPrivate.persistent, g_gs_cr1, "emittance voxelization cr1");

    local.pixelShaders[0] = CreatePixelShader(&voxelPrivate.persistent, g_ps_cr0, "emittance voxelization cr0");
    local.pixelShaders[1] = CreatePixelShader(&voxelPrivate.persistent, g_ps_cr1, "emittance voxelization cr1");

    CreateVoxelRasterStates(&local.rasterState);

    p->gs.buffers[0] = CreateShaderConstantBuffer(sizeof(VoxelizeGSData), "voxelization geometry cbuffer");
    p->gs.numBuffers = 1;

    p->ps.buffers[0] = CreateShaderConstantBuffer(sizeof(VoxelizePSData), "voxelization pixel cbuffer");
    p->ps.buffers[1] = CreateShaderConstantBuffer(sizeof(LightPSData), "emittance light cbuffer");
    p->ps.numBuffers = 2;

    // Texture2D Sampler
    D3D11_SAMPLER_DESC samplerDesc = CreateSamplerDesc(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP, 1.0f);
    p->ps.samplers[0] = CreateSamplerState(&samplerDesc);

    // shadow map sampler
    samplerDesc = CreateSamplerDesc(D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, D3D11_TEXTURE_ADDRESS_BORDER, 0.0f);
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_GREATER;
    p->ps.samplers[1] = CreateSamplerState(&samplerDesc);
    p->ps.numSamplers = 2;
}

void VoxelizeEmittance_Draw(RenderCommandQueue* cmdQueue, DrawBuffer* drawBuffer, Scene* scene)
{
    DEBUG_REGION("Emittance Voxelization");
    QUERY_REGION(QueryId::EmittanceVoxelization);

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

    LightPSData lpsData;
    for (u32 i = 0; i < ARRAY_LEN(cmdQueue->lights); i++)
    {
        Light* e = cmdQueue->lights + i;

        Vec3Copy(lpsData.lights[i].position, e->position);
        Vec4Copy(lpsData.lights[i].color, e->color);
        Vec3Copy(lpsData.lights[i].direction, e->dir);
        lpsData.lights[i].direction.w = cos(TO_RADIANS(e->umbraAngle));
        lpsData.lights[i].position.w = e->radius;
        lpsData.lights[i].color.w = cos(TO_RADIANS(e->penumbraAngle));
        lpsData.lights[i].params.x = e->sizeUV;
        lpsData.lights[i].params.y = e->zNear;
        memcpy(lpsData.lights[i].lightView, &e->viewMatrix.E, sizeof(m4x4));
        memcpy(lpsData.lights[i].lightProj, &e->projMatrix.E, sizeof(m4x4));
    }
    lpsData.lightCount = cmdQueue->lightCount;
    SetShaderData(p->ps.buffers[1], lpsData);

    VoxelizePSData psData = {};
    psData.cst_inverseViewportSize[0].x = local.halfPixelSizeCS[0].u;
    psData.cst_inverseViewportSize[0].y = local.halfPixelSizeCS[0].v;
    psData.cst_inverseViewportSize[1].x = local.halfPixelSizeCS[1].u;
    psData.cst_inverseViewportSize[1].y = local.halfPixelSizeCS[1].v;
    psData.cst_inverseViewportSize[2].x = local.halfPixelSizeCS[2].u;
    psData.cst_inverseViewportSize[2].y = local.halfPixelSizeCS[2].v;
    psData.cst_fixedBias = shadowConstants.cst_fixedBias;
    
    SetVoxelRasterMode(p, r_backendFlags.consRasterMode, local.rasterState);

    u32 conservativeIndex = r_backendFlags.consRasterMode == ConsRasterMode::Software ? 1 : 0;
    p->gs.shader = local.geometryShaders[conservativeIndex];
    p->ps.shader = local.pixelShaders[conservativeIndex]; // @TODO: should be be able to switch between no HDR and HDR?

    SetDrawBuffer(p, drawBuffer, vbIds);

    const u32 clear[] = { 0, 0, 0, 0 };
    p->om.numUAVs = 0;
    for (u32 m = 0; m < ARRAY_LEN(voxelShared.emittanceHDR); ++m)
    {
        d3ds.context->ClearUnorderedAccessViewUint(voxelShared.emittanceHDR[m].UAVs[0], clear);
        p->om.uavs[m] = voxelShared.emittanceHDR[m].UAVs[0];
        p->om.numUAVs++;
    }
    p->om.uavs[p->om.numUAVs++] = voxelShared.normalMapUAV;

    p->rs.numScissors = 0;
    p->rs.numViewports = 0;
    PushViewportAndScissor(p, 0, 0, voxelShared.gridSize.h, voxelShared.gridSize.d);
    PushViewportAndScissor(p, 0, 0, voxelShared.gridSize.w, voxelShared.gridSize.d);
    PushViewportAndScissor(p, 0, 0, voxelShared.gridSize.w, voxelShared.gridSize.h);

    SetPipeline(p);
    for (u32 m = 0; m < scene->meshes.Length(); ++m)
    {
        MeshFileMesh* mesh = &scene->meshes[m];
        Material* material = &scene->materials[mesh->materialIndex];
        Vec4Copy(psData.cst_alphaTestedColor, material->alphaTestedColor);
        psData.cst_flags = material->flags;
        SetShaderData(p->ps.buffers[0], psData);
 
        p->ps.srvs[0] = assetsShared.textureViews[material->textureIndex[TextureId::Albedo]];
        p->ps.srvs[1] = shadowShared.srvs;
        p->ps.numSRVs = 2;

        SetPipeline(p, false);
        DrawIndexed(drawBuffer, mesh->numIndexes, mesh->firstIndex);
    }
}