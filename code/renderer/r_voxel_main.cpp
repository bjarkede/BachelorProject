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

VoxelPrivate voxelPrivate;

void CreateVoxelRasterStates(VoxelRasterState* state)
{
    D3D11_RASTERIZER_DESC rasterDesc;
    ZeroMemory(&rasterDesc, sizeof(rasterDesc));
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.CullMode = D3D11_CULL_NONE;
    rasterDesc.FrontCounterClockwise = FALSE;
    rasterDesc.ScissorEnable = TRUE;
    rasterDesc.DepthClipEnable = FALSE;
    rasterDesc.DepthBiasClamp = 0.0f;
    state->rasterState[0] = CreateRasterizerState(&rasterDesc);

    if (d3ds.context3 != NULL)
    {
        // Create the conservative raster
        D3D11_RASTERIZER_DESC2 conservativeRasterDesc;
        ZeroMemory(&conservativeRasterDesc, sizeof(conservativeRasterDesc));
        conservativeRasterDesc.FillMode = D3D11_FILL_SOLID;
        conservativeRasterDesc.CullMode = D3D11_CULL_NONE;
        conservativeRasterDesc.FrontCounterClockwise = FALSE;
        conservativeRasterDesc.DepthBiasClamp = 0.0f;
        conservativeRasterDesc.DepthClipEnable = FALSE;
        conservativeRasterDesc.ScissorEnable = TRUE;
        conservativeRasterDesc.ConservativeRaster = D3D11_CONSERVATIVE_RASTERIZATION_MODE_ON;

        //
        // check what HW conservative reasterization tier is enabled
        //
        D3D11_FEATURE_DATA_D3D11_OPTIONS2 featureData;
        d3ds.device->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS2, &featureData, sizeof(D3D11_FEATURE_DATA_D3D11_OPTIONS2));
        rendererInfo.consRasterInfo.tier = featureData.ConservativeRasterizationTier;
        if (rendererInfo.consRasterInfo.tier >= 2)
        {
            rendererInfo.consRasterInfo.available = true;
            state->rasterState[1] = CreateRasterizerState2(&conservativeRasterDesc);
        }
    }
}

void SetVoxelRasterMode(GraphicsPipeline* p, ConsRasterMode::Type mode, VoxelRasterState state)
{
    u32 conservativeIndex = mode == ConsRasterMode::Hardware;
    p->rs.state = state.rasterState[conservativeIndex];
}

static void CreateTexturesAndViews()
{
    //
    // opacity
    //

    u32 minGridSize = MIN3(voxelShared.gridSize.w, voxelShared.gridSize.h, voxelShared.gridSize.d);
    uint32_t numMipLevels = ComputeMipCount(minGridSize);

    D3D11_TEXTURE3D_DESC texDesc;
    ZeroMemory(&texDesc, sizeof(texDesc));
    texDesc.Width = voxelShared.gridSize.w * 6;
    texDesc.Height = voxelShared.gridSize.h;
    texDesc.Depth = voxelShared.gridSize.d;
    texDesc.MipLevels = numMipLevels;
    texDesc.Format = DXGI_FORMAT_R8_UNORM;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

    voxelShared.opacityMap = CreateTexture3D(&voxelPrivate.resDependent, &texDesc, NULL, "opacity map");

    voxelPrivate.numMipLevels = numMipLevels;

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
    ZeroMemory(&uavDesc, sizeof(uavDesc));
    uavDesc.Format = DXGI_FORMAT_R8_UNORM;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
    uavDesc.Texture3D.FirstWSlice = 0;
    uavDesc.Texture3D.WSize = -1;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    ZeroMemory(&srvDesc, sizeof(srvDesc));
    srvDesc.Format = DXGI_FORMAT_R8_UNORM;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
    srvDesc.Texture3D.MipLevels = 1;

    for (u32 i = 0; i < numMipLevels; ++i)
    {
        srvDesc.Texture3D.MostDetailedMip = i;
        uavDesc.Texture3D.MipSlice = i;
        voxelShared.opacityMapUAVs[i] = CreateUnorderedAccessView(&voxelPrivate.resDependent, voxelShared.opacityMap, &uavDesc, fmt("opacity map mip %d", i));
        voxelShared.opacityMapSRVs[i] = CreateShaderResourceView(&voxelPrivate.resDependent, voxelShared.opacityMap, &srvDesc, fmt("opacity map mip %d ", i));
    }

    voxelShared.opacityNumBytes = GetTexture3DSizeBytes(voxelShared.opacityMap);

    //
    // emittance
    //

    voxelPrivate.numEmittanceMaps = MIN(voxelShared.numBounces, 3);
    assert(voxelPrivate.numEmittanceMaps <= ARRAY_LEN(voxelShared.emittanceMaps));

    ZeroMemory(&texDesc, sizeof(texDesc));
    texDesc.Width = voxelShared.gridSize.w * 6;
    texDesc.Height = voxelShared.gridSize.h;
    texDesc.Depth = voxelShared.gridSize.d;
    texDesc.MipLevels = voxelPrivate.numMipLevels;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

    texDesc.Format = DXGI_FORMAT_R32_UINT;
    for (u32 m = 0; m < ARRAY_LEN(voxelShared.emittanceHDR); ++m)
    {
        voxelShared.emittanceHDR[m].map = CreateTexture3D(&voxelPrivate.resDependent, &texDesc, NULL, fmt("emittance map %d", m));
    }
    texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;

    voxelShared.emittanceMaps[0].map = CreateTexture3D(&voxelPrivate.resDependent, &texDesc, NULL, "emittance map");
    for (u32 m = 1; m < voxelPrivate.numEmittanceMaps; ++m)
    {
        voxelShared.emittanceMaps[m].map = CreateTexture3D(&voxelPrivate.resDependent, &texDesc, NULL, fmt("emittance bounce map %d", m));
    }

    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_TYPELESS;
    voxelShared.normalMap = CreateTexture3D(&voxelPrivate.resDependent, &texDesc, NULL, "normal map");

    ZeroMemory(&uavDesc, sizeof(uavDesc));
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
    uavDesc.Texture3D.FirstWSlice = 0;
    uavDesc.Texture3D.WSize = -1;

    ZeroMemory(&srvDesc, sizeof(srvDesc));
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
    srvDesc.Texture3D.MipLevels = 1;

 
    for (u32 m = 0; m < ARRAY_LEN(voxelShared.emittanceHDR); ++m)
    {
        uavDesc.Format = DXGI_FORMAT_R32_UINT;
        voxelShared.emittanceHDR[m].UAVs[0] = CreateUnorderedAccessView(&voxelPrivate.resDependent, voxelShared.emittanceHDR[m].map, &uavDesc, fmt("emittance map %d", m));
        srvDesc.Format = DXGI_FORMAT_R32_UINT;
        voxelShared.emittanceHDR[m].SRVs[0] = CreateShaderResourceView(&voxelPrivate.resDependent, voxelShared.emittanceHDR[m].map, &srvDesc, fmt("emittance map %d", m));
    }

    for (u32 m = 0; m < voxelPrivate.numEmittanceMaps; ++m)
    {
        for (u32 i = 0; i < voxelPrivate.numMipLevels; ++i)
        {
            srvDesc.Texture3D.MostDetailedMip = i;
            uavDesc.Texture3D.MipSlice = i;

            uavDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            voxelShared.emittanceMaps[m].UAVs[i] = CreateUnorderedAccessView(&voxelPrivate.resDependent, voxelShared.emittanceMaps[m].map, &uavDesc, fmt("emittance map mip %d", i));
            voxelShared.emittanceMaps[m].SRVs[i] = CreateShaderResourceView(&voxelPrivate.resDependent, voxelShared.emittanceMaps[m].map, &srvDesc, fmt("emittance map mip %d ", i));

        }
    }

    srvDesc.Texture3D.MostDetailedMip = 0;
    srvDesc.Texture3D.MipLevels = 1;
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    voxelShared.normalMapSRV = CreateShaderResourceView(&voxelPrivate.resDependent, voxelShared.normalMap, &srvDesc, "normal map");

    uavDesc.Texture3D.MipSlice = 0;
    uavDesc.Format = DXGI_FORMAT_R32_UINT;
    voxelShared.normalMapUAV = CreateUnorderedAccessView(&voxelPrivate.resDependent, voxelShared.normalMap, &uavDesc, "normal map");

    UINT numBytes = 0;

    for (u32 m = 0; m < ARRAY_LEN(voxelShared.emittanceHDR); ++m)
    {
        numBytes += GetTexture3DSizeBytes(voxelShared.emittanceHDR[m].map);
    }
 

    voxelShared.emittanceNumBytes = numBytes;
    voxelShared.normalNumBytes = GetTexture3DSizeBytes(voxelShared.normalMap);

    //
    // voxel viz
    //

    D3D11_SHADER_RESOURCE_VIEW_DESC texViewDesc;
    ZeroMemory(&texViewDesc, sizeof(texViewDesc));
    texViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
    texViewDesc.Texture3D.MostDetailedMip = 0;
    texViewDesc.Texture3D.MipLevels = -1;
    texViewDesc.Format = DXGI_FORMAT_R8_UNORM;
    voxelShared.opacityMapFullSRV = CreateShaderResourceView(&voxelPrivate.resDependent, voxelShared.opacityMap, &texViewDesc, "opacity map");

    for (u32 m = 0; m < voxelPrivate.numEmittanceMaps; ++m)
    {
        texViewDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        voxelShared.emittanceMaps[m].fullSRV = CreateShaderResourceView(&voxelPrivate.resDependent, voxelShared.emittanceMaps[m].map, &texViewDesc, "emittance map");
    }
}

void Voxel_Init()
{
    voxelShared.gridSize.w = 128;
    voxelShared.gridSize.h = 64;
    voxelShared.gridSize.d = 64;
    voxelShared.numBounces = 3;
    voxelShared.maxGapDistance = 5;

    CreateTexturesAndViews();

    OpacityVoxelization_Init();
    OpacityMipDownSample_Init();
    EmittanceVoxelization_Init();
    VoxelizationFix_Init();
    EmittanceFormatFix_Init();
    EmittanceOpacityFix_Init();
    EmittanceBounce_Init();
    EmittanceMipDownSample_Init();
    VoxelViz_Init();
}

void Voxel_Shutdown()
{
    ReleaseResources(&voxelPrivate.persistent);
    ReleaseResources(&voxelPrivate.resDependent);
}

void Voxel_Run(RenderCommandQueue* cmdQueue, DrawBuffer* drawBuffer, Scene* scene, bool runOpacity, bool runEmittance)
{
    if (runOpacity)
    {
        VoxelizeOpacity_Draw(cmdQueue, drawBuffer, scene);
        OpacityMipDownSample_Run();
    }

    if (runEmittance)
    {
        u32 n = voxelShared.numBounces;
        if (n == 1)
        {
            VoxelizeEmittance_Draw(cmdQueue, drawBuffer, scene);
            EmittanceFormatFix_Run();
            VoxelizationFix_Run();
            EmittanceMipDownSample_Run(0);
            voxelShared.emittanceReadIndex = 0;
        }
        else if (n == 2)
        {
            int f = voxelPrivate.numVoxelFrames;
            if (f % 2 == 0)
            {
                VoxelizeEmittance_Draw(cmdQueue, drawBuffer, scene);
                EmittanceFormatFix_Run();
                VoxelizationFix_Run();
                EmittanceMipDownSample_Run(0);
            }
            else
            {
                EmittanceBounce_Run(0, 1);
                EmittanceMipDownSample_Run(1);
            }
            voxelShared.emittanceReadIndex = (f == 0) ? 0 : 1;
        }
        else
        {
            u32 f = voxelPrivate.numVoxelFrames;
            if (f % n == 0)
            {
                VoxelizeEmittance_Draw(cmdQueue, drawBuffer, scene);
                EmittanceFormatFix_Run();
                VoxelizationFix_Run();
                EmittanceMipDownSample_Run(0);
                voxelPrivate.emittanceBounceRead = 0;
            }
            else
            {
                u32 read = voxelPrivate.emittanceBounceRead;
                u32 write = (f % n == n - 1) ? 2 : (read ^ 1);
                EmittanceBounce_Run(read, write);
                EmittanceMipDownSample_Run(write);
                voxelPrivate.emittanceBounceRead = write;
                voxelShared.emittanceReadIndex = (f >= n - 1) ? 2 : write;
            }
        }

        voxelPrivate.numVoxelFrames++;
    }
}

void Voxel_SettingsChanged()
{
    ReleaseResources(&voxelPrivate.resDependent);
    CreateTexturesAndViews();
    voxelPrivate.numVoxelFrames = 0;
    voxelPrivate.emittanceBounceRead = 0;
    voxelShared.emittanceReadIndex = 0;
}