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

#include "../shaders/deferred_shading_ps.h"
#include "../shaders/deferred_shading_vs.h"

struct Local
{
    GraphicsPipeline pipeline;
    ResourceArray persistent;
};

struct DeferredId
{
    enum Type
    {
        DepthBuffer = GBufferId::Count,
        ShadowMap,
        OpacityMap,
        EmittanceMap,
        SSSO,
        Count
    };
};

static Local local;

static void UploadPendingDeferredShadingShaderData(RenderCommandQueue* cmdQueue)
{
    GraphicsPipeline* p = &local.pipeline;

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
    SetShaderData(p->ps.buffers[2], lpsData);

    DeferredShadingPSData psData;

    memcpy(&psData.invProjectionMatrix, &cmdQueue->invViewProjectionMatrix, sizeof(m4x4));

    Vec3Copy(psData.bounding_min, cmdQueue->aabb.min);
    Vec3Copy(psData.bounding_max, cmdQueue->aabb.max);
    Vec3Copy(psData.cameraPosition, cmdQueue->cameraPosition);
    Vec3Copy(psData.viewVector, cmdQueue->viewVector);

    psData.cst_opacityThreshold = tracingConstants.cst_opacityThreshold;
    psData.cst_traceStepScale = tracingConstants.cst_traceStepScale;
    psData.cst_occlusionScale = tracingConstants.cst_occlusionScale;
    psData.cst_coneRatio = 2 * tan(TO_RADIANS(tracingConstants.cst_coneAngle / 2));
    psData.cst_maxMipLevel = tracingConstants.cst_maxMipLevel;
    psData.cst_mipBias = tracingConstants.cst_mipBias;
    psData.cst_slopeBias = shadowConstants.cst_slopeBias;
    psData.cst_fixedBias = shadowConstants.cst_fixedBias;
    psData.cst_maxBias = shadowConstants.cst_maxBias;
    memcpy(psData.temp, tempConstants.temp, sizeof(psData.temp));
    psData.cst_options = r_backendFlags.deferredOptions;

    SetShaderData(p->ps.buffers[0], psData);
}

void UploadMaterialsToBuffer(DynamicArray<Material>* materials, ID3D11Buffer* buffer)
{
    MaterialShadingData msData = {};
    for (u32 m = 0; m < materials->Length(); ++m)
    {
        Material* mtl = &(*materials)[m];
        Vec4Copy(msData.materials[m].specular, mtl->specular);
        msData.materials[m].flags = mtl->flags;
    }
    SetShaderData(buffer, msData);
}

void DeferredShading_Init()
{
    GraphicsPipeline* p = &local.pipeline;
    InitPipeline(p);

    p->vs.shader = CreateVertexShader(&local.persistent, g_vs, "deferred shading");
    p->ps.shader = CreatePixelShader(&local.persistent, g_ps, "deferred shading");

    p->ps.buffers[0] = CreateShaderConstantBuffer(sizeof(DeferredShadingPSData), "deferred shading pixel cbuffer");
    p->ps.buffers[1] = CreateShaderConstantBuffer(sizeof(MaterialShadingData), "deferred shading material cbuffer");
    p->ps.buffers[2] = CreateShaderConstantBuffer(sizeof(LightPSData), "deferred light cbuffer");
    p->ps.numBuffers = 3;

    p->om.depthState = d3d.postProcessDepthState;

    // Texture2D Sampler
    D3D11_SAMPLER_DESC samplerDesc = CreateSamplerDesc(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP, 1.0f);
    p->ps.samplers[0] = CreateSamplerState(&samplerDesc);

    // Texture3D Sampler
    samplerDesc = CreateSamplerDesc(D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP, 1.0f);
    p->ps.samplers[1] = CreateSamplerState(&samplerDesc);

    // point sampler
    samplerDesc = CreateSamplerDesc(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_BORDER, 0.0f);
    p->ps.samplers[2] = CreateSamplerState(&samplerDesc);

    // shadow map sampler
    samplerDesc = CreateSamplerDesc(D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, D3D11_TEXTURE_ADDRESS_BORDER, 0.0f);
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_GREATER;
    p->ps.samplers[3] = CreateSamplerState(&samplerDesc);
    p->ps.numSamplers = 4;
}

void DeferredShading_Shutdown()
{
    ReleaseResources(&local.persistent);
}

void DeferredShading_Draw(RenderCommandQueue* cmdQueue, DrawBuffer* drawBuffer)
{
    DEBUG_REGION("Deferred Shading");
    QUERY_REGION(QueryId::DeferredShading);

    GraphicsPipeline* p = &local.pipeline;

    UploadPendingDeferredShadingShaderData(cmdQueue);
    UploadMaterialsToBuffer(&assetsShared.currentMesh->materials, p->ps.buffers[1]);

    SetDrawBuffer(p, drawBuffer, NULL, 0);
    for (u32 i = 0; i < GBufferId::Count; ++i)
    {
        p->ps.srvs[i] = gBufferShared.srvs[i];
    }
    p->ps.srvs[DeferredId::DepthBuffer] = d3d.depthSRV;
    p->ps.srvs[DeferredId::ShadowMap] = shadowShared.srvs;
    p->ps.srvs[DeferredId::OpacityMap] = voxelShared.opacityMapFullSRV;
    p->ps.srvs[DeferredId::EmittanceMap] = voxelShared.emittanceMaps[voxelShared.emittanceReadIndex].fullSRV;
    p->ps.srvs[DeferredId::SSSO] = sssoShared.SRVs[0];
    p->ps.numSRVs = DeferredId::Count;

    p->om.rtvs[0] = d3d.backBufferRTView;
    p->om.numRTVs = 1;

    SetDefaultViewportAndScissor(p);

    SetPipeline(p);
    d3ds.context->Draw(3, 0);
}