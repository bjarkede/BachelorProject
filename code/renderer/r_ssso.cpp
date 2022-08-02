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
#include "../shaders/ssso_vs.h"
#include "../shaders/ssso_ps.h"

struct Local
{
    GraphicsPipeline pipeline;
    ResourceArray persistent;
    ResourceArray resDependent;
};

#pragma pack(push, 1)
struct PSData
{
    f32 modelViewMatrix[16];
    f32 invViewProjectionMatrix[16];
    f32 projectionMatrix[16];
    vec4_t cameraPosition;
    vec4_t pixelSize01;
    vec4_t boundingBoxMin;
    vec4_t boundingBoxMax;
    vec4_t temp;
    vec4_t ab;
    uint3_t gridDim;
    u32 dummy;
};
#pragma pack(pop)

static Local local;
SSSOShared sssoShared;

static void CreateTextures()
{
    ReleaseResources(&local.resDependent);
    GraphicsPipeline* p = &local.pipeline;
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

    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.Format = texDesc.Format;
    rtvDesc.Format = texDesc.Format;

    sssoShared.tex[0] = CreateTexture2D(&local.resDependent, &texDesc, NULL, "specular occlusion");
    sssoShared.RTVs[0] = CreateRenderTargetView(&local.resDependent, sssoShared.tex[0], &rtvDesc, "specular occlusion");
    sssoShared.SRVs[0] = CreateShaderResourceView(&local.resDependent, sssoShared.tex[0], &srvDesc, "specular occlusion");
}

void ScreenSpaceSpecularOcclusion_Init()
{
    GraphicsPipeline* p = &local.pipeline;
    InitPipeline(p);

    p->vs.shader = CreateVertexShader(&local.persistent, g_vs, "ssso");
    p->ps.shader = CreatePixelShader(&local.persistent, g_ps, "ssso");

    p->ps.buffers[0] = CreateShaderConstantBuffer(sizeof(PSData), "ssso ps cbuffer");
    p->ps.buffers[1] = CreateShaderConstantBuffer(sizeof(MaterialShadingData), "ssso materials");
    p->ps.numBuffers = 2;

    p->om.depthState = d3d.postProcessDepthState;

    // point sampler
    // we can use the same sampler for normals and depth because
    // we use reverse depth i.e. far clip is at 0
    D3D11_SAMPLER_DESC samplerDesc = CreateSamplerDesc(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_BORDER, 0.0f);
    p->ps.samplers[0] = CreateSamplerState(&samplerDesc);
    p->ps.numSamplers = 1;

    CreateTextures();
}

void ScreenSpaceSpecularOcclusion_Shutdown()
{
    ReleaseResources(&local.persistent);
    ReleaseResources(&local.resDependent);
}

void ScreenSpaceSpecularOcclusion_Run(RenderCommandQueue* cmdQueue, DrawBuffer* drawBuffer)
{
    DEBUG_REGION("SSSO");
    QUERY_REGION(QueryId::SSSOPass);

    GraphicsPipeline* p = &local.pipeline;
    
    d3ds.context->CopySubresourceRegion(d3d.depthMap.map, 0, 0, 0, 0, d3d.depthStencilTexture, 0, NULL);

    SetDrawBuffer(p, drawBuffer, NULL, 0);

    p->om.rtvs[0] = sssoShared.RTVs[0];
    p->om.numRTVs = 1;

    p->ps.srvs[0] = d3d.depthMap.fullSRV;
    p->ps.srvs[1] = gBufferShared.srvs[GBufferId::Normal];
    p->ps.srvs[2] = gBufferShared.srvs[GBufferId::Material];
    p->ps.numSRVs = 3;

    SetDefaultViewportAndScissor(p);

    PSData psData;
    Vec3Copy(psData.cameraPosition, cmdQueue->cameraPosition);

    memcpy(&psData.invViewProjectionMatrix, &cmdQueue->invViewProjectionMatrix, sizeof(m4x4));
    memcpy(&psData.modelViewMatrix, &cmdQueue->modelViewMatrix, sizeof(m4x4));
    memcpy(&psData.projectionMatrix, &cmdQueue->projectionMatrix, sizeof(m4x4));
    Uint3Copy(psData.gridDim, voxelShared.gridSize);
    psData.pixelSize01.x = 1.0f / p->rs.viewports[0].Width;
    psData.pixelSize01.y = 1.0f / p->rs.viewports[0].Height;
    Vec3Copy(psData.boundingBoxMin, cmdQueue->aabb.min);
    Vec3Copy(psData.boundingBoxMax, cmdQueue->aabb.max);
    memcpy(psData.temp.v, tempConstants.temp, sizeof(tempConstants.temp));
    psData.ab.x = cmdQueue->ab.u;
    psData.ab.y = cmdQueue->ab.v;
    SetShaderData(p->ps.buffers[0], psData);

    UploadMaterialsToBuffer(&assetsShared.currentMesh->materials, p->ps.buffers[1]);

    SetPipeline(p);
    d3ds.context->Draw(3, 0);
}

void ScreenSpaceSpecularOcclusion_WindowSizeChanged()
{
    if (d3ds.device != NULL)
    {
        CreateTextures();
    }
}