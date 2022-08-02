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

#include "../shaders/geometry_pass_ps.h"
#include "../shaders/geometry_pass_vs.h"

struct Local
{
    GraphicsPipeline pipeline;
    ResourceArray persistent;
    ResourceArray gBuffer;
    ID3D11SamplerState* albedoSampler[4];
};

#pragma pack(push, 1)

struct GeometryPassVSData
{
    f32 modelViewMatrix[16];
    f32 projectionMatrix[16];
};

struct GeometryPassPSData
{
    vec4_t camPosWS;
    vec4_t normalStrength;
    u32 materialIndex;
    u32 useNormalMap;
    u32 dummy[2];
};

#pragma pack(pop)

GBufferShared gBufferShared;
static Local local;

static const VertexBufferId::Type vbIds[] = { VertexBufferId::Position, VertexBufferId::Normal, VertexBufferId::Tc };

static void CreateGBufferTexture(DXGI_FORMAT format,
    D3D11_TEXTURE2D_DESC* texDesc,
    D3D11_RENDER_TARGET_VIEW_DESC* rtvDesc,
    D3D11_SHADER_RESOURCE_VIEW_DESC* srvDesc,
    GBufferId::Type id, const char* name)
{
    texDesc->Format = format;
    rtvDesc->Format = format;
    srvDesc->Format = format;
    gBufferShared.textures[id] = CreateTexture2D(&local.gBuffer, texDesc, NULL, fmt("gbuffer %s", name));
    gBufferShared.rtvs[id] = CreateRenderTargetView(&local.gBuffer, gBufferShared.textures[id], rtvDesc, fmt("gbuffer %s", name));
    gBufferShared.srvs[id] = CreateShaderResourceView(&local.gBuffer, gBufferShared.textures[id], srvDesc, fmt("gbuffer %s", name));
}

static void CreateTextures()
{
    ReleaseResources(&local.gBuffer);
    // Set up the deferred textures
    // Depth, albedo, normal, material
    D3D11_TEXTURE2D_DESC texDesc;
    ZeroMemory(&texDesc, sizeof(texDesc));
    texDesc.ArraySize = 1;
    texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags = 0;
    texDesc.MiscFlags = 0;
    texDesc.MipLevels = 1;
    texDesc.Width = r_videoConfig.width;
    texDesc.Height = r_videoConfig.height;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;

    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
    ZeroMemory(&rtvDesc, sizeof(rtvDesc));
    rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = 0;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    ZeroMemory(&srvDesc, sizeof(srvDesc));
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;

    CreateGBufferTexture(DXGI_FORMAT_R8G8B8A8_UNORM, &texDesc, &rtvDesc, &srvDesc, GBufferId::Albedo, "albedo");
    CreateGBufferTexture(DXGI_FORMAT_R16G16_SNORM, &texDesc, &rtvDesc, &srvDesc, GBufferId::Normal, "normal");
    CreateGBufferTexture(DXGI_FORMAT_R8_UINT, &texDesc, &rtvDesc, &srvDesc, GBufferId::Material, "material");
}

void GeometryPass_Init()
{
    GraphicsPipeline* p = &local.pipeline;
    InitPipeline(p);

    CreateTextures();

    // Create Geometry Pass
    p->vs.shader = CreateVertexShader(&local.persistent, g_vs, "gbuffer");
    p->ps.shader = CreatePixelShader(&local.persistent, g_ps, "gbuffer");

    p->ia.layout = CreateInputLayout(&local.persistent, vbIds, g_vs, "gbuffer");

    p->vs.buffers[0] = CreateShaderConstantBuffer(sizeof(GeometryPassVSData), "gbuffer vertex cbuffer");
    p->vs.numBuffers = 1;

    p->ps.buffers[0] = CreateShaderConstantBuffer(sizeof(GeometryPassPSData), "gbuffer pixel cbuffer");
    p->ps.numBuffers = 1;

    // Texture2D Sampler
    D3D11_SAMPLER_DESC samplerDesc = CreateSamplerDesc(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP, 1.0f);

    samplerDesc.MaxAnisotropy = 1;
    local.albedoSampler[0] = CreateSamplerState(&samplerDesc);
    samplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
    samplerDesc.MaxAnisotropy = 4;
    local.albedoSampler[1] = CreateSamplerState(&samplerDesc);
    samplerDesc.MaxAnisotropy = 8;
    local.albedoSampler[2] = CreateSamplerState(&samplerDesc);
    samplerDesc.MaxAnisotropy = 16;
    local.albedoSampler[3] = CreateSamplerState(&samplerDesc);
}

void GeometryPass_Shutdown()
{
    ReleaseResources(&local.persistent);
    ReleaseResources(&local.gBuffer);
}

void GeometryPass_Draw(RenderCommandQueue* cmdQueue, DrawBuffer* drawBuffer)
{
    DEBUG_REGION("Geometry");
    QUERY_REGION(QueryId::GeometryPass);

    GraphicsPipeline* p = &local.pipeline;

    SetDrawBuffer(p, drawBuffer, vbIds);

    p->ps.samplers[0] = local.albedoSampler[r_backendFlags.maxAnisotropy];
    p->ps.numSamplers = 1;

    for (u32 i = 0; i < GBufferId::Count; ++i)
    {
        p->om.rtvs[i] = gBufferShared.rtvs[i];
    }
    p->om.numRTVs = GBufferId::Count;
    p->om.depthView = d3d.depthStencilView;

    SetDefaultViewportAndScissor(p);

    const FLOAT clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    d3ds.context->ClearRenderTargetView(gBufferShared.rtvs[GBufferId::Albedo], clearColor);
    d3ds.context->ClearRenderTargetView(gBufferShared.rtvs[GBufferId::Normal], clearColor);
    d3ds.context->ClearRenderTargetView(gBufferShared.rtvs[GBufferId::Material], clearColor);

    u32 indexOffset = 0;
    u32 vertexOffset = 0;

    SetPipeline(p);
    for (u32 m = 0; m < assetsShared.currentMesh->meshes.Length(); ++m)
    {
        MeshFileMesh* mesh = &assetsShared.currentMesh->meshes[m];
        Material* material = &assetsShared.currentMesh->materials[mesh->materialIndex];
 
        GeometryPassVSData vsData;
        memcpy(vsData.modelViewMatrix, cmdQueue->modelViewMatrix.E, sizeof(m4x4));
        memcpy(vsData.projectionMatrix, cmdQueue->projectionMatrix.E, sizeof(m4x4));
        SetShaderData(p->vs.buffers[0], vsData);

        GeometryPassPSData psData;
        Vec3Copy(psData.camPosWS, cmdQueue->cameraPosition);
        psData.materialIndex = mesh->materialIndex;
        psData.useNormalMap = r_backendFlags.useNormalMap;
        psData.normalStrength.x = r_backendFlags.normalStrength;
        SetShaderData(p->ps.buffers[0], psData);

        p->ps.srvs[0] = assetsShared.textureViews[material->textureIndex[TextureId::Albedo]];
        p->ps.srvs[1] = assetsShared.textureViews[material->textureIndex[TextureId::Bump]];
        p->ps.srvs[2] = assetsShared.textureViews[material->textureIndex[TextureId::Specular]];
        p->ps.numSRVs = 3;

        SetPipeline(p, false);
        DrawIndexed(drawBuffer, mesh->numIndexes, mesh->firstIndex);
    }
}

void GeometryPass_WindowSizeChanged()
{
    if (d3ds.device != NULL)
    {
        CreateTextures();
    }
}