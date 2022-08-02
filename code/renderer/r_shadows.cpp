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

#include "../shaders/shadows_gs.h"
#include "../shaders/shadows_ps.h"
#include "../shaders/shadows_vs.h"

#define SHADOW_RES 4096

#pragma pack(push, 1)
struct ShadowsGSData
{
    f32 modelViewMatrix[16];
    f32 projectionMatrix[16];
    vec4_t lightPosition;
    f32 resolution;
    f32 slopeBias;
    f32 dummy[2];
};
#pragma pack(pop)

struct Local
{
    GraphicsPipeline pipeline;
    ResourceArray persistent;
    ID3D11Texture2D* texture;
    ID3D11DepthStencilView* depthViews[MAX_LIGHTS];
};

static Local local;
ShadowsSharedData shadowShared;
static const VertexBufferId::Type vbIds[] = { VertexBufferId::Position, VertexBufferId::Normal };

static void UploadPendingShadowsShaderData(RenderCommandQueue* cmdQueue, u32 lightIndex)
{
    ShadowsGSData gsData;

    Light* light = &cmdQueue->lights[lightIndex];
    light->viewMatrix = LookAt(light->position, light->position + (-light->dir), { 0.0f, 1.0f, 0.0f });
    light->projMatrix = PerspectiveProjection(2 * TO_RADIANS(light->umbraAngle), 1.0f, light->zFar, light->zNear);
    Vec3Copy(gsData.lightPosition, light->position);
    gsData.resolution = 1.0f / SHADOW_RES;
    gsData.slopeBias = shadowConstants.cst_slopeBias;
    memcpy(gsData.modelViewMatrix, light->viewMatrix.E, sizeof(m4x4));
    memcpy(gsData.projectionMatrix, light->projMatrix.E, sizeof(m4x4));

    SetShaderData(local.pipeline.gs.buffers[0], gsData);
}

void Shadows_Init()
{
    GraphicsPipeline* p = &local.pipeline;
    InitPipeline(p);

    p->ia.layout = CreateInputLayout(&local.persistent, vbIds, g_vs, "shadows");

    p->vs.shader = CreateVertexShader(&local.persistent, g_vs, "shadows");

    p->gs.shader = CreateGeometryShader(&local.persistent, g_gs, "shadows");
    p->gs.buffers[0] = CreateShaderConstantBuffer(sizeof(ShadowsGSData), "shadow geo cbuffer");
    p->gs.numBuffers = 1;

    p->ps.shader = CreatePixelShader(&local.persistent, g_ps, "shadows");

    D3D11_TEXTURE2D_DESC depthStencilTexDesc;
    ZeroMemory(&depthStencilTexDesc, sizeof(depthStencilTexDesc));
    depthStencilTexDesc.Width = SHADOW_RES;
    depthStencilTexDesc.Height = SHADOW_RES;
    depthStencilTexDesc.MipLevels = 1;
    depthStencilTexDesc.ArraySize = MAX_LIGHTS;
    depthStencilTexDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    depthStencilTexDesc.SampleDesc.Count = 1;
    depthStencilTexDesc.SampleDesc.Quality = 0;
    depthStencilTexDesc.Usage = D3D11_USAGE_DEFAULT;
    depthStencilTexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    depthStencilTexDesc.CPUAccessFlags = 0;
    depthStencilTexDesc.MiscFlags = 0;

    local.texture = CreateTexture2D(&local.persistent, &depthStencilTexDesc, NULL, "shadow map");

    D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilDesc;
    ZeroMemory(&depthStencilDesc, sizeof(depthStencilDesc));
    depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthStencilDesc.Texture2DArray.ArraySize = 1;
    depthStencilDesc.Texture2DArray.MipSlice = 0;
    depthStencilDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;

    for (u32 i = 0; i < MAX_LIGHTS; ++i)
    {
        depthStencilDesc.Texture2DArray.FirstArraySlice = i;
        local.depthViews[i] = CreateDepthStencilView(&local.persistent, local.texture, &depthStencilDesc, fmt("shadow map view %d", i));
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC depthSRVDesc;
    ZeroMemory(&depthSRVDesc, sizeof(depthSRVDesc));
    depthSRVDesc.Format = DXGI_FORMAT_R32_FLOAT;
    depthSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    depthSRVDesc.Texture2DArray.ArraySize = MAX_LIGHTS;
    depthSRVDesc.Texture2DArray.FirstArraySlice = 0;
    depthSRVDesc.Texture2DArray.MipLevels = 1;
    depthSRVDesc.Texture2DArray.MostDetailedMip = 0;

    shadowShared.srvs = CreateShaderResourceView(&local.persistent, local.texture, &depthSRVDesc, "shadow map");

    PushViewport(p, 0, 0, SHADOW_RES, SHADOW_RES);
    PushScissorRect(p, 0, 0, SHADOW_RES, SHADOW_RES);
}

void Shadows_Shutdown()
{
    ReleaseResources(&local.persistent);
}

void Shadows_Draw(RenderCommandQueue* cmdQueue, DrawBuffer* drawBuffer)
{
    DEBUG_REGION("Shadows");
    QUERY_REGION(QueryId::Shadows);

    SetDrawBuffer(&local.pipeline, drawBuffer, vbIds);

    for (u32 i = 0; i < cmdQueue->lightCount; ++i)
    {
        local.pipeline.om.depthView = local.depthViews[i];
        d3ds.context->ClearDepthStencilView(local.depthViews[i], D3D11_CLEAR_DEPTH, 0.0f, 0);
        UploadPendingShadowsShaderData(cmdQueue, i);
        SetPipeline(&local.pipeline);
        DrawIndexed(drawBuffer, drawBuffer->numIndexes);
    }
}