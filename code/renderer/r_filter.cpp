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
#include "../shaders/ssso_filter_vs.h"
#include "../shaders/ssso_filter_ps.h"

#pragma pack(push, 1)
struct PSData2
{
    vec4_t dir;
    f32 temp[4];
};
#pragma pack(pop)

struct Local
{
    GraphicsPipeline pipeline;
    ResourceArray persistent;
    ResourceArray resDependent;
};

static Local local;

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
    
    sssoShared.tex[1] = CreateTexture2D(&local.resDependent, &texDesc, NULL, "specular occlusion temp");
    sssoShared.RTVs[1] = CreateRenderTargetView(&local.resDependent, sssoShared.tex[1], &rtvDesc, "specular occlusion temp");
    sssoShared.SRVs[1] = CreateShaderResourceView(&local.resDependent, sssoShared.tex[1], &srvDesc, "specular occlusion temp");
}

void SSSO_Filter_Init()
{
    GraphicsPipeline* p = &local.pipeline;
    InitPipeline(p);

    p->vs.shader = CreateVertexShader(&local.persistent, g_vs, "ssso filter");
    p->ps.shader = CreatePixelShader(&local.persistent, g_ps, "ssso filter");

    p->ps.buffers[0] = CreateShaderConstantBuffer(sizeof(PSData2), "ssso filter ps cbuffer");
    p->ps.numBuffers = 1;

    p->om.depthState = d3d.postProcessDepthState;

    // point sampler
    // we can use the same sampler for normals and depth because
    // we use reverse depth i.e. far clip is at 0
    D3D11_SAMPLER_DESC samplerDesc = CreateSamplerDesc(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP, 0.0f);
    p->ps.samplers[0] = CreateSamplerState(&samplerDesc);
    p->ps.numSamplers = 1;

    CreateTextures();
}

f32 Gaussian(float d, float sigma = 1.0f)
{
    float result = 0.0f;
    float sqrt2pi = sqrtf(2.0f * M_PI);
    float d2 = d * d;
    float s2 = sigma * sigma;
    float t1 = 1.0f / (sqrt2pi * sigma);
    float t2 = expf(-d2 / (2.0f * s2));
    result = t1 * t2;
    return result;
}

void SSSO_Filter_Draw(RenderCommandQueue* cmdQueue)
{
    DEBUG_REGION("SSSO Filter");
    QUERY_REGION(QueryId::SSSOFilter);

    GraphicsPipeline* p = &local.pipeline;
    SetDefaultViewportAndScissor(p);
    
    PSData2 psData = {};

    psData.dir.z = 1.0f / p->rs.viewports[0].Width;
    psData.dir.w = 1.0f / p->rs.viewports[0].Height;
    memcpy(psData.temp, tempConstants.temp, sizeof(tempConstants.temp));

    for (u32 i = 0; i < 2; ++i)
    {
        psData.dir.x = (i + 1) % 2;
        psData.dir.y = i % 2;

        p->ps.srvs[0] = sssoShared.SRVs[i % 2];
        p->ps.srvs[1] = d3d.depthSRV;
        p->ps.srvs[2] = gBufferShared.srvs[GBufferId::Normal];
        p->ps.numSRVs = 3;
        p->om.rtvs[0] = sssoShared.RTVs[(i + 1) % 2];
        p->om.numRTVs = 1;

        SetShaderData(p->ps.buffers[0], psData);
        SetPipeline(p);
        d3ds.context->Draw(3, 0);
    }
}

void SSSO_Filter_Shutdown()
{
    ReleaseResources(&local.persistent);
    ReleaseResources(&local.resDependent);
}

void SSSO_Filter_WindowSizeChanged()
{
    if (d3ds.device != NULL)
    {
        CreateTextures();
    }
}