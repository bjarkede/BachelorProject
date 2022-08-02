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

DXGI_FORMAT vbFormats[VertexBufferId::Count + 1] = {
#define VB_ITEM(EnumId, Format, Name, Type) Format,
    VB_LIST(VB_ITEM)
#undef VB_ITEM
        DXGI_FORMAT_UNKNOWN
};

const char* vbNames[VertexBufferId::Count + 1] = {
#define VB_ITEM(EnumId, Format, Name, Type) Name,
    VB_LIST(VB_ITEM)
#undef VB_ITEM
        ""
};

size_t vbItemSizes[VertexBufferId::Count + 1] = {
#define VB_ITEM(EnumId, Format, Name, Type) sizeof(Type),
    VB_LIST(VB_ITEM)
#undef VB_ITEM
        0
};

const char* queryNames[QueryId::Count + 1] = {
#define QUERY_ITEM(EnumId, Name) Name,
    QUERY_LIST(QUERY_ITEM)
#undef QUERY_ITEM
        ""
};

RendererInfo rendererInfo;

Direct3D d3d;
Direct3DStatic d3ds;

static void CheckAndName(HRESULT hr, const char* function, ID3D11DeviceChild* resource, const char* resourceName)
{
    if (SUCCEEDED(hr))
    {
        resource->SetPrivateData(WKPDID_D3DDebugObjectName, strlen(resourceName), resourceName);
        return;
    }

    Sys_FatalError("'%s' failed to create '%s' with code 0x%08X\n", function, resourceName, (u32)hr);
}

static void D3D11_CreateRasterizerState(const D3D11_RASTERIZER_DESC* rasterDesc,
    ID3D11RasterizerState** rasterState,
    const char* name)
{
    const HRESULT hr = d3ds.device->CreateRasterizerState(rasterDesc, rasterState);
    CheckAndName(hr, "CreateRasterizerState", *rasterState, fmt("%s RS", name));
}

static void D3D11_CreateRasterizerState2(const D3D11_RASTERIZER_DESC2* rasterDesc,
    ID3D11RasterizerState2** rasterState,
    const char* name)
{
    const HRESULT hr = d3ds.device3->CreateRasterizerState2(rasterDesc, rasterState);
    CheckAndName(hr, "CreateRasterizerState", *rasterState, fmt("%s RS2", name));
}

static void D3D11_CreateQuery(const D3D11_QUERY_DESC* queryDesc,
    ID3D11Query** query,
    const char* name)
{
    const HRESULT hr = d3ds.device->CreateQuery(queryDesc, query);
    CheckAndName(hr, "CreateQuery", *query, fmt("%s query", name));
}

static void D3D11_CreateDepthStencilState(const D3D11_DEPTH_STENCIL_DESC* dssDesc,
    ID3D11DepthStencilState** depthStencilState,
    const char* name)
{
    const HRESULT hr = d3ds.device->CreateDepthStencilState(dssDesc, depthStencilState);
    CheckAndName(hr, "CreateDepthStencilState", *depthStencilState, fmt("%s DSS", name));
}

static void D3D11_CreateBlendState(const D3D11_BLEND_DESC* bDesc,
    ID3D11BlendState** blendState,
    const char* name)
{
    const HRESULT hr = d3ds.device->CreateBlendState(bDesc, blendState);
    CheckAndName(hr, "CreateBlendState", *blendState, fmt("%s BS", name));
}

static void D3D11_CreateSamplerState(const D3D11_SAMPLER_DESC* samplerDesc,
    ID3D11SamplerState** samplerState,
    const char* name)
{
    const HRESULT hr = d3ds.device->CreateSamplerState(samplerDesc, samplerState);
    CheckAndName(hr, "CreateSamplerState", *samplerState, fmt("%s SS", name));
}

static void UpdateViewport(int x, int y, int w, int h, int th)
{
    const int top = th - y - h;

    D3D11_VIEWPORT vp;
    vp.TopLeftX = x;
    vp.TopLeftY = top;
    vp.Width = w;
    vp.Height = h;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    d3ds.context->RSSetViewports(1, &vp);
}

static void ApplyScissor(int x, int y, int w, int h, int th)
{
    const int top = th - y - h;
    const int bottom = th - y;

    D3D11_RECT sr;
    sr.left = x;
    sr.top = top;
    sr.right = x + w;
    sr.bottom = bottom;
    d3ds.context->RSSetScissorRects(1, &sr);
}

static void UpdateViewportAndScissor(int x, int y, int w, int h, int th)
{
    UpdateViewport(x, y, w, h, th);
    ApplyScissor(x, y, w, h, th);
}

void InitPipeline(GraphicsPipeline* pipeline)
{
    pipeline->ia.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    pipeline->rs.state = d3d.rasterState;
    pipeline->om.blendState = d3d.blendStateDisabled;
    pipeline->om.depthState = d3d.depthState;
}

static void CreateDepthStencilView()
{
    // Set up the depth buffer
    D3D11_TEXTURE2D_DESC depthStencilTexDesc;
    ZeroMemory(&depthStencilTexDesc, sizeof(depthStencilTexDesc));
    depthStencilTexDesc.Width = r_videoConfig.width;
    depthStencilTexDesc.Height = r_videoConfig.height;
    depthStencilTexDesc.MipLevels = 1;
    depthStencilTexDesc.ArraySize = 1;
    depthStencilTexDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    depthStencilTexDesc.SampleDesc.Count = 1;
    depthStencilTexDesc.SampleDesc.Quality = 0;
    depthStencilTexDesc.Usage = D3D11_USAGE_DEFAULT;
    depthStencilTexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    depthStencilTexDesc.CPUAccessFlags = 0;
    depthStencilTexDesc.MiscFlags = 0;

    d3d.depthStencilTexture = CreateTexture2D(&d3d.resDependent, &depthStencilTexDesc, NULL, "d3d11 depth stencil texture2d");

    // Create the depth stencil view
    D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilDesc;
    ZeroMemory(&depthStencilDesc, sizeof(depthStencilDesc));
    depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthStencilDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;

    d3d.depthStencilView = CreateDepthStencilView(&d3d.resDependent, d3d.depthStencilTexture, &depthStencilDesc, "d3d11 depth stencil view");

    // Create shader resource view so we can sample the depth in the geometry pass
    D3D11_SHADER_RESOURCE_VIEW_DESC depthSRVDesc;
    ZeroMemory(&depthSRVDesc, sizeof(depthSRVDesc));
    depthSRVDesc.Format = DXGI_FORMAT_R32_FLOAT;
    depthSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    depthSRVDesc.Texture2D.MipLevels = 1;
    depthSRVDesc.Texture2D.MostDetailedMip = 0;
    d3d.depthSRV = CreateShaderResourceView(&d3d.resDependent, d3d.depthStencilTexture, &depthSRVDesc, "d3d11 depth");

    // depth copy for custom mip-mapping
    u32 numMipLevels = ComputeMipCount(r_videoConfig.height, r_videoConfig.width);
    depthStencilTexDesc.MipLevels = numMipLevels;
    depthStencilTexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    d3d.depthMap.map = CreateTexture2D(&d3d.resDependent, &depthStencilTexDesc, NULL, "depth buffer");

    depthSRVDesc.Texture2D.MipLevels = -1;
    d3d.depthMap.fullSRV = CreateShaderResourceView(&d3d.resDependent, d3d.depthMap.map, &depthSRVDesc, "depth buffer");

    depthSRVDesc.Texture2D.MipLevels = 1;

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
    ZeroMemory(&uavDesc, sizeof(uavDesc));
    uavDesc.Format = DXGI_FORMAT_R32_FLOAT;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    
    for (u32 m = 0; m < numMipLevels; ++m)
    {
        
        depthSRVDesc.Texture2D.MostDetailedMip = m;
        uavDesc.Texture2D.MipSlice = m;
        d3d.depthMap.SRVs[m] = CreateShaderResourceView(&d3d.resDependent, d3d.depthMap.map, &depthSRVDesc, fmt("depth map mip %d", m));
        d3d.depthMap.UAVs[m] = CreateUnorderedAccessView(&d3d.resDependent, d3d.depthMap.map, &uavDesc, fmt("depth map mip %d", m));
    }
}

static void CreateSwapchain(bool init)
{
    if (d3ds.swapChain == NULL)
    {
        return;
    }

    if (!init)
    {
        ReleaseResources(&d3d.resDependent);
        HRESULT hr = d3ds.swapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
        if (FAILED(hr))
        {
            Sys_FatalError("Couldn't resize swapchain");
        }
    }

    HRESULT hr = d3ds.swapChain->GetBuffer(0, IID_PPV_ARGS(&d3d.backBufferTexture));
    CheckAndName(hr, "GetBuffer", d3d.backBufferTexture, "back buffer");
    d3d.resDependent.Push(d3d.backBufferTexture);

    d3d.backBufferRTView = CreateRenderTargetView(&d3d.resDependent, d3d.backBufferTexture, NULL, "back buffer");

    CreateDepthStencilView();
}

void SetShaderData(ID3D11Buffer* buffer, const void* data, size_t bytes)
{
    D3D11_MAPPED_SUBRESOURCE ms;
    if (d3ds.context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, NULL, &ms) != S_OK)
        Sys_FatalError("Couldn't set shader data.");
    memcpy(ms.pData, data, bytes);
    d3ds.context->Unmap(buffer, NULL);
}

void AppendVertexData(VertexBuffer* buffer, const void* data, u32 itemCount)
{
    assert(buffer);
    assert(data);
    assert(itemCount > 0);
    assert(buffer->buffer);
    assert(buffer->itemSize > 0);
    assert(buffer->capacity > 0);
    assert(buffer->capacity > itemCount);

    D3D11_MAP mapType = D3D11_MAP_WRITE_NO_OVERWRITE;
    if (buffer->discard || buffer->writeIndex + itemCount > buffer->capacity)
    {
        mapType = D3D11_MAP_WRITE_DISCARD;
        buffer->writeIndex = 0;
        buffer->readIndex = 0;
        buffer->discard = false;
    }

    // this is not a real append function
    D3D11_MAPPED_SUBRESOURCE ms;
    if (d3ds.context->Map(buffer->buffer, NULL, mapType, NULL, &ms) != S_OK)
    {
        return;
    }
    memcpy((byte*)ms.pData + buffer->writeIndex * buffer->itemSize, data, itemCount * buffer->itemSize);
    d3ds.context->Unmap(buffer->buffer, NULL);
    buffer->writeIndex += itemCount;
}

void AppendImmutableData(const D3D11_BUFFER_DESC* bDesc, void* data, ID3D11Buffer** buffer)
{
    assert(bDesc);
    assert(buffer);

    // this is not a real append function
    if (data != NULL)
    {
        D3D11_SUBRESOURCE_DATA sr;
        sr.pSysMem = data;
        sr.SysMemPitch = 0;
        sr.SysMemSlicePitch = 0;
        *buffer = CreateBuffer(&d3d.persistent, bDesc, &sr, "append immutable data");
    }
}

void DrawIndexed(DrawBuffer* buffer, u32 numIndexes)
{
    d3ds.context->DrawIndexed(numIndexes, buffer->indexBuffer.readIndex, 0);
}

void DrawIndexed(DrawBuffer* buffer, u32 numIndexes, u32 indexOffset)
{
    d3ds.context->DrawIndexed(numIndexes, indexOffset, 0);
}

void DrawBuffer_Init(DrawBuffer* buffer)
{
    VertexBuffer* const vbs = buffer->vertexBuffers;
    for (u32 b = 0; b < VertexBufferId::Count; ++b)
    {
        VertexBuffer* buffer = &vbs[b];
        buffer->itemSize = vbItemSizes[b];
    }
    buffer->indexBuffer.itemSize = sizeof(u32);
}

void DrawBuffer_Allocate(ResourceArray* resources, DrawBuffer* buffer, const char* name)
{
    VertexBuffer* const vbs = buffer->vertexBuffers;
    for (int v = 0; v < VertexBufferId::Count; ++v)
    {
        vbs[v].capacity = MAX_VERTEXES;
        D3D11_BUFFER_DESC bufferDesc;
        ZeroMemory(&bufferDesc, sizeof(bufferDesc));
        bufferDesc.ByteWidth = vbs[v].itemSize * MAX_VERTEXES;
        bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
        bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        vbs[v].buffer = CreateBuffer(resources, &bufferDesc, NULL, fmt("%s %s", name, vbNames[v]));
    }
    buffer->indexBuffer.capacity = MAX_INDEXES;

    D3D11_BUFFER_DESC indexBuffer;
    ZeroMemory(&indexBuffer, sizeof(indexBuffer));
    indexBuffer.ByteWidth = buffer->indexBuffer.itemSize * MAX_INDEXES;
    indexBuffer.Usage = D3D11_USAGE_DYNAMIC;
    indexBuffer.BindFlags = D3D11_BIND_INDEX_BUFFER;
    indexBuffer.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    buffer->indexBuffer.buffer = CreateBuffer(resources, &indexBuffer, NULL, fmt("%s index buffer", name));
}

static int F32Compare(const void* a, const void* b)
{
    f32 f1 = *(f32*)a;
    f32 f2 = *(f32*)b;
    if (f1 > f2)
    {
        return -1;
    }
    else if (f1 == f2)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

static void InitQuery(QueryId::Type queryId, const char* name)
{
    d3d.frameQueries[queryId].name = name;
    d3d.frameQueries[queryId].medianUS = 0.0f;
}

// before the first BeginQuery call
void BeginFrameQueries()
{
    for (u32 q = 0; q < QueryId::Count; ++q)
    {
        FrameQuery* fq = &d3d.frameQueries[q];
        const u32 maxNumFrames = ARRAY_LEN(fq->frameTimes);
        fq->frameTimes[d3d.frameNumber % maxNumFrames] = 0.0f;
    }
}

// after the last EndQuery call
void EndFrameQueries()
{
    const u32 maxNumFrames = ARRAY_LEN(d3d.frameQueries[0].frameTimes);
    f32 frameTimes[maxNumFrames];

    for (u32 q = 0; q < QueryId::Count; ++q)
    {
        // copy valid times, sort them and find their median
        // if not a single valid time is known, we display 0
        FrameQuery* fq = &d3d.frameQueries[q];
        u32 numValid = 0;
        for (u32 i = 0; i < maxNumFrames; ++i)
        {
            if (fq->frameTimes[i] > 0.0f)
            {
                frameTimes[numValid++] = fq->frameTimes[i];
            }
        }
        if (numValid > 0)
        {
            qsort(frameTimes, numValid, sizeof(f32), &F32Compare);
            fq->medianUS = frameTimes[numValid / 2];
            fq->minUS = frameTimes[numValid - 1];
            fq->maxUS = frameTimes[0];
        }
        else
        {
            fq->medianUS = 0.0f;
            fq->minUS = 0.0f;
            fq->maxUS = 0.0f;
        }
    }
}

void BeginQuery(QueryId::Type queryId)
{
    FrameQuery* fq = &d3d.frameQueries[queryId];
    fq->active = true;

    Query* query = &fq->queries[fq->queryWriteIndex];

    // ready the query
    query->active = false;
    COM_RELEASE(query->disjoint);
    COM_RELEASE(query->begin);
    COM_RELEASE(query->end);

    D3D11_QUERY_DESC qDesc;
    ZeroMemory(&qDesc, sizeof(qDesc));
    qDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
    D3D11_CreateQuery(&qDesc, &query->disjoint, fmt("%s %d disjoint", queryNames[queryId], d3d.frameNumber));
    qDesc.Query = D3D11_QUERY_TIMESTAMP;
    D3D11_CreateQuery(&qDesc, &query->begin, fmt("%s %d begin", queryNames[queryId], d3d.frameNumber));
    D3D11_CreateQuery(&qDesc, &query->end, fmt("%s %d end", queryNames[queryId], d3d.frameNumber));
    if (query->disjoint != NULL && query->begin != NULL && query->end != NULL)
    {
        query->active = true;
        d3ds.context->Begin(query->disjoint);
        d3ds.context->End(query->begin);
    }
    else
    {
        COM_RELEASE(query->disjoint);
        COM_RELEASE(query->begin);
        COM_RELEASE(query->end);
    }
}

void EndQuery(QueryId::Type queryId)
{
    FrameQuery* fq = &d3d.frameQueries[queryId];
    Query* query = &fq->queries[fq->queryWriteIndex];
    if (query->active)
    {
        d3ds.context->End(query->end);
        d3ds.context->End(query->disjoint);
        fq->queryWriteIndex = (fq->queryWriteIndex + 1) % ARRAY_LEN(fq->queries); // use % to wrap
    }

    D3D11_QUERY_DATA_TIMESTAMP_DISJOINT qInfo;
    query = &fq->queries[fq->queryReadIndex];
    if (query->active && d3ds.context->GetData(query->disjoint, &qInfo, sizeof(qInfo), D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK)
    {
        u64 begin = 0;
        u64 end = 0;
        if (!qInfo.Disjoint && qInfo.Frequency > 0 && d3ds.context->GetData(query->begin, &begin, sizeof(begin), D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK && d3ds.context->GetData(query->end, &end, sizeof(end), D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK)
        {
            const u32 maxNumFrames = ARRAY_LEN(fq->frameTimes);
            fq->frameTimes[d3d.frameNumber % maxNumFrames] = (f32(end - begin) / f32(qInfo.Frequency) * 1000000.0f);

            COM_RELEASE(query->begin);
            COM_RELEASE(query->end);
            COM_RELEASE(query->disjoint);
            query->active = false;
        }

        fq->queryReadIndex = (fq->queryReadIndex + 1) % ARRAY_LEN(fq->queries);
    }
}

void D3D11_WindowSizeChanged()
{
    CreateSwapchain(false);
}

void D3D11_Init(HWND windowHandle)
{
    ZeroMemory(&d3d, sizeof(d3d));

    DXGI_SWAP_CHAIN_DESC swapChainDesc;
    const D3D_FEATURE_LEVEL featureLevels[2] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
#if _DEBUG
    UINT flags = D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_DEBUG;
#else
    UINT flags = D3D11_CREATE_DEVICE_SINGLETHREADED;
#endif
    ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));

    // Set up the swapchain
    swapChainDesc.BufferDesc.Width = r_videoConfig.width;
    swapChainDesc.BufferDesc.Height = r_videoConfig.height;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferDesc.RefreshRate.Numerator = 0;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = windowHandle;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferCount = 1;
    swapChainDesc.Windowed = TRUE;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, flags, featureLevels,
            ARRAY_LEN(featureLevels), D3D11_SDK_VERSION,
            &swapChainDesc, &d3ds.swapChain, &d3ds.device, NULL, &d3ds.context)
        != S_OK)
        Sys_FatalError("Fatal Error: D3D11 couldn't create device and swap chain");

    // If we get 11.1 QueryInterface 11.3
    if (d3ds.device->GetFeatureLevel() == D3D_FEATURE_LEVEL_11_1)
    {
        if (SUCCEEDED(d3ds.device->QueryInterface(IID_PPV_ARGS(&d3ds.device3))))
        {
            if (SUCCEEDED(d3ds.context->QueryInterface(IID_PPV_ARGS(&d3ds.context3))))
            {
            }
        }
    }

    // Breakpoints
    ID3D11InfoQueue* infoQueue;
    if (SUCCEEDED(d3ds.device->QueryInterface(IID_PPV_ARGS(&infoQueue))))
    {
        infoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        infoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_WARNING, TRUE);
        infoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, TRUE);

        D3D11_MESSAGE_ID filterList[] = {
            D3D11_MESSAGE_ID_SETPRIVATEDATA_CHANGINGPARAMS,
            D3D11_MESSAGE_ID_LIVE_OBJECT_SUMMARY,
            D3D11_MESSAGE_ID_LIVE_DEVICE
        };

        D3D11_INFO_QUEUE_FILTER filter;
        ZeroMemory(&filter, sizeof(filter));
        filter.DenyList.NumIDs = ARRAY_LEN(filterList);
        filter.DenyList.pIDList = filterList;

        infoQueue->AddStorageFilterEntries(&filter);
        COM_RELEASE(infoQueue);
    }

    D3D11_BLEND_DESC blendDesc;
    ZeroMemory(&blendDesc, sizeof(blendDesc));
    blendDesc.RenderTarget[0].BlendEnable = FALSE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    d3d.blendStateDisabled = CreateBlendState(&blendDesc);

    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    d3d.blendStateStandard = CreateBlendState(&blendDesc);

    // Rasterizer state
    D3D11_RASTERIZER_DESC rasterDesc;
    ZeroMemory(&rasterDesc, sizeof(rasterDesc));
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.CullMode = D3D11_CULL_BACK;
    rasterDesc.FrontCounterClockwise = FALSE;
    rasterDesc.ScissorEnable = TRUE;
    rasterDesc.DepthClipEnable = FALSE;
    rasterDesc.DepthBiasClamp = 0.0f;
    d3d.rasterState = CreateRasterizerState(&rasterDesc);
    d3ds.context->RSSetState(d3d.rasterState);

    // Create the depth stencil state
    D3D11_DEPTH_STENCIL_DESC depthDesc;
    ZeroMemory(&depthDesc, sizeof(depthDesc));
    depthDesc.DepthEnable = TRUE;
    depthDesc.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;
    depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    depthDesc.StencilEnable = FALSE;
    d3d.depthState = CreateDepthStencilState(&depthDesc);

    depthDesc.DepthEnable = FALSE;
    d3d.postProcessDepthState = CreateDepthStencilState(&depthDesc);

    d3ds.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    CreateSwapchain(true);

    //
    // find a good V-Sync interval so that we get 60+ FPS but not much higher
    //

    // figure out which screen we're running on
    d3d.swapInterval = 1;
    const char* monitorName = NULL;
    const HMONITOR monitor = MonitorFromWindow(windowHandle, MONITOR_DEFAULTTONEAREST);
    MONITORINFOEXA monInfo {};
    monInfo.cbSize = sizeof(monInfo);
    if (GetMonitorInfoA(monitor, &monInfo) != 0)
    {
        monitorName = monInfo.szDevice;
    }

    // figure out the swap interval from the screen's current refresh rate
    DEVMODEA devMode {};
    devMode.dmSize = sizeof(devMode);
    if (EnumDisplaySettingsA(monitorName, ENUM_CURRENT_SETTINGS, &devMode) != 0 && devMode.dmDisplayFrequency >= 59)
    {
        // because of fractional values, 60 Hz really can be lower (e.g. 59.94) and
        // encoded as an integer, it would be lower than (59 with the previous example)
        d3d.swapInterval = (UINT)(devMode.dmDisplayFrequency + 2) / 60;
    }

    d3ds.context->QueryInterface(IID_PPV_ARGS(&d3ds.annotation));

// init queries
#define QUERY_ITEM(EnumId, Name) InitQuery(QueryId::EnumId, Name);
    QUERY_LIST(QUERY_ITEM)
#undef QUERY_ITEM
}

static bool IsWindows8OrGreater()
{
#pragma warning(push)
#pragma warning(disable : 4996) // GetVersionExA is deprecated

    OSVERSIONINFO version {};
    version.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    if (GetVersionExA(&version) == FALSE)
    {
        return false;
    }

    // @NOTE: Windows 7 is "6.1"
    // @NOTE: GetVersionEx will incorrectly return Windows 8 version numbers for Windows 10
    //        when the app's not manifested right
    const bool isIt = (version.dwMajorVersion > 6) || (version.dwMajorVersion == 6 && version.dwMinorVersion >= 2);

    return isIt;

#pragma warning(pop)
}

#define RELEASE_CACHED_STATE(numStates, states) \
    for (u32 d = 0; d < numStates; ++d)         \
    {                                           \
        COM_RELEASE(states[d].state);           \
    }

void D3D11_Shutdown()
{
    RELEASE_CACHED_STATE(d3d.numDepthStates, d3d.depthStates);
    RELEASE_CACHED_STATE(d3d.numSamplerStates, d3d.samplerStates);
    RELEASE_CACHED_STATE(d3d.numRasterStates, d3d.rasterStates);
    RELEASE_CACHED_STATE(d3d.numRasterStates2, d3d.rasterStates2);
    RELEASE_CACHED_STATE(d3d.numBlendStates, d3d.blendStates);

    ReleaseResources(&d3d.persistent);
    ReleaseResources(&d3d.resDependent);

    for (u32 fq = 0; fq < ARRAY_LEN(d3d.frameQueries); ++fq)
    {
        FrameQuery* frameQuery = &d3d.frameQueries[fq];
        for (u32 q = 0; q < ARRAY_LEN(frameQuery->queries); ++q)
        {
            if (frameQuery->queries[q].active)
            {
                COM_RELEASE(frameQuery->queries[q].begin);
                COM_RELEASE(frameQuery->queries[q].end);
                COM_RELEASE(frameQuery->queries[q].disjoint);
            }
        }
    }

    d3ds.context->ClearState();
    d3ds.context->Flush();

    COM_RELEASE(d3ds.annotation);
    COM_RELEASE(d3ds.context3);
    COM_RELEASE(d3ds.context);
    COM_RELEASE(d3ds.swapChain);
    COM_RELEASE(d3ds.device3);

#if _DEBUG

    // @NOTE: ReportLiveDeviceObjects will fail on Windows 7 with D3D11_RLDO_IGNORE_INTERNAL
    bool canIgnoreInternalRefs = IsWindows8OrGreater();

    ID3D11InfoQueue* infoQueue;
    if (!canIgnoreInternalRefs && SUCCEEDED(d3ds.device->QueryInterface(IID_PPV_ARGS(&infoQueue))))
    {
        infoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_WARNING, FALSE);
        COM_RELEASE(infoQueue);
    }

    ID3D11Debug* debug;
    if (SUCCEEDED(d3ds.device->QueryInterface(IID_PPV_ARGS(&debug))))
    {
        D3D11_RLDO_FLAGS flags = D3D11_RLDO_SUMMARY | D3D11_RLDO_DETAIL;
        if (canIgnoreInternalRefs)
        {
            flags |= D3D11_RLDO_IGNORE_INTERNAL;
        }
        debug->ReportLiveDeviceObjects(flags);
        COM_RELEASE(debug);
    }

#endif

    COM_RELEASE(d3ds.device);
}

void BeginDebugRegion(const wchar_t* name)
{
    if (d3ds.annotation != NULL)
    {
        d3ds.annotation->BeginEvent(name);
    }
}

void EndDebugRegion()
{
    if (d3ds.annotation != NULL)
    {
        d3ds.annotation->EndEvent();
    }
}

void ClearDepthStencilBuffer()
{
    const FLOAT clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    d3ds.context->ClearDepthStencilView(d3d.depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.0f, 0);
}

void BeginFrame()
{
    const FLOAT clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    const FLOAT clearColorDebug[4] = { 1.0f, 0.0f, 0.5f, 1.0f };
    d3ds.context->ClearRenderTargetView(d3d.backBufferRTView, clearColor);
    ClearDepthStencilBuffer();
    d3ds.context->OMSetDepthStencilState(d3d.depthState, 0);
    UpdateViewportAndScissor(0, 0, r_videoConfig.width, r_videoConfig.height, r_videoConfig.height);
}

void PresentFrame()
{
    d3ds.swapChain->Present(d3d.swapInterval, 0);
}

ID3D11Buffer* CreateShaderConstantBuffer(UINT numBytes, const char* name)
{
    D3D11_BUFFER_DESC vBuffer;
    ZeroMemory(&vBuffer, sizeof(vBuffer));
    vBuffer.ByteWidth = numBytes;
    vBuffer.Usage = D3D11_USAGE_DYNAMIC;
    vBuffer.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    vBuffer.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    vBuffer.MiscFlags = 0;
    vBuffer.StructureByteStride = 0;

    ID3D11Buffer* buffer = CreateBuffer(&d3d.persistent, &vBuffer, NULL, name);
    return buffer;
}

void PushViewport(GraphicsPipeline* pipeline, f32 x, f32 y, f32 w, f32 h, f32 zmin, f32 zmax)
{
    assert(pipeline->rs.numViewports < ARRAY_LEN(pipeline->rs.viewports));
    D3D11_VIEWPORT* v = &pipeline->rs.viewports[pipeline->rs.numViewports++];
    v->TopLeftX = x;
    v->TopLeftY = y;
    v->Width = w;
    v->Height = h;
    v->MinDepth = zmin;
    v->MaxDepth = zmax;
}

void PushScissorRect(GraphicsPipeline* pipeline, LONG x, LONG y, LONG w, LONG h)
{
    assert(pipeline->rs.numScissors < ARRAY_LEN(pipeline->rs.scissors));
    D3D11_RECT* r = &pipeline->rs.scissors[pipeline->rs.numScissors++];
    r->left = x;
    r->top = y;
    r->right = x + w;
    r->bottom = y + h;
}

void PushViewportAndScissor(GraphicsPipeline* p, f32 x, f32 y, f32 w, f32 h, f32 zmin, f32 zmax)
{
    PushViewport(p, x, y, w, h, zmin, zmax);
    PushScissorRect(p, x, y, w, h);
}

void PushDefaultViewport(GraphicsPipeline* pipeline)
{
    PushViewport(pipeline, 0, 0, r_videoConfig.width, r_videoConfig.height);
}

void SetDefaultViewportAndScissor(GraphicsPipeline* pipeline)
{
    pipeline->rs.numViewports = 0;
    pipeline->rs.numScissors = 0;
    PushViewportAndScissor(pipeline, 0, 0, r_videoConfig.width, r_videoConfig.height);
}

D3D11_SAMPLER_DESC CreateSamplerDesc(D3D11_FILTER filter, D3D11_TEXTURE_ADDRESS_MODE addressMode, f32 borderColor)
{
    D3D11_SAMPLER_DESC samplerDesc;
    samplerDesc.Filter = filter;
    samplerDesc.AddressU = addressMode;
    samplerDesc.AddressV = addressMode;
    samplerDesc.AddressW = addressMode;
    samplerDesc.MinLOD = -FLT_MAX;
    samplerDesc.MaxLOD = FLT_MAX;
    samplerDesc.MipLODBias = 0.0f;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MaxAnisotropy = 1;
    samplerDesc.BorderColor[0] = borderColor;
    samplerDesc.BorderColor[1] = borderColor;
    samplerDesc.BorderColor[2] = borderColor;
    samplerDesc.BorderColor[3] = borderColor;
    return samplerDesc;
}

#define CREATE_CACHED_STATE(states, numStates, descType, descriptor, func) \
    for (u32 r = 0; r < numStates; ++r)                                    \
    {                                                                      \
        if (memcmp(descriptor, &states[r].desc, sizeof(descType)) == 0)    \
        {                                                                  \
            return states[r].state;                                        \
        }                                                                  \
    }                                                                      \
    assert(numStates < ARRAY_LEN(states));                                 \
    func(descriptor, &states[numStates].state, fmt("%d", numStates));      \
    states[numStates].desc = *descriptor;                                  \
    return states[numStates++].state

ID3D11RasterizerState* CreateRasterizerState(const D3D11_RASTERIZER_DESC* rasterDesc)
{
    CREATE_CACHED_STATE(d3d.rasterStates, d3d.numRasterStates, D3D11_RASTERIZER_DESC, rasterDesc, D3D11_CreateRasterizerState);
}

ID3D11RasterizerState2* CreateRasterizerState2(const D3D11_RASTERIZER_DESC2* rasterDesc)
{
    CREATE_CACHED_STATE(d3d.rasterStates2, d3d.numRasterStates2, D3D11_RASTERIZER_DESC2, rasterDesc, D3D11_CreateRasterizerState2);
}

ID3D11BlendState* CreateBlendState(const D3D11_BLEND_DESC* bDesc)
{
    CREATE_CACHED_STATE(d3d.blendStates, d3d.numBlendStates, D3D11_BLEND_DESC, bDesc, D3D11_CreateBlendState);
}

ID3D11SamplerState* CreateSamplerState(const D3D11_SAMPLER_DESC* samplerDesc)
{
    CREATE_CACHED_STATE(d3d.samplerStates, d3d.numSamplerStates, D3D11_SAMPLER_DESC, samplerDesc, D3D11_CreateSamplerState);
}

ID3D11DepthStencilState* CreateDepthStencilState(const D3D11_DEPTH_STENCIL_DESC* desc)
{
    CREATE_CACHED_STATE(d3d.depthStates, d3d.numDepthStates, D3D11_DEPTH_STENCIL_DESC, desc, D3D11_CreateDepthStencilState);
}

#undef CREATE_CACHED_STATE

ID3D11ComputeShader* CreateComputeShader(ResourceArray* resources, const void* bytecode, SIZE_T bytecodeLength, const char* name)
{
    ID3D11ComputeShader* shader;
    const HRESULT hr = d3ds.device->CreateComputeShader(bytecode, bytecodeLength, NULL, &shader);
    CheckAndName(hr, "CreateComputeShader", shader, fmt("%s CS", name));
    resources->Push(shader);
    return shader;
}

ID3D11VertexShader* CreateVertexShader(ResourceArray* resources, const void* bytecode, SIZE_T bytecodeLength, const char* name)
{
    ID3D11VertexShader* shader;
    const HRESULT hr = d3ds.device->CreateVertexShader(bytecode, bytecodeLength, NULL, &shader);
    CheckAndName(hr, "CreateVertexShader", shader, fmt("%s VS", name));
    resources->Push(shader);
    return shader;
}

ID3D11PixelShader* CreatePixelShader(ResourceArray* resources, const void* bytecode, SIZE_T bytecodeLength, const char* name)
{
    ID3D11PixelShader* shader;
    const HRESULT hr = d3ds.device->CreatePixelShader(bytecode, bytecodeLength, NULL, &shader);
    CheckAndName(hr, "CreatePixelShader", shader, fmt("%s PS", name));
    resources->Push(shader);
    return shader;
}

ID3D11GeometryShader* CreateGeometryShader(ResourceArray* resources, const void* bytecode, SIZE_T bytecodeLength, const char* name)
{
    ID3D11GeometryShader* shader;
    const HRESULT hr = d3ds.device->CreateGeometryShader(bytecode, bytecodeLength, NULL, &shader);
    CheckAndName(hr, "CreateGeometryShader", shader, fmt("%s GS", name));
    resources->Push(shader);
    return shader;
}

ID3D11InputLayout* CreateInputLayout(ResourceArray* resources, const VertexBufferId::Type* ids, u32 count,
    const void* shaderBytecodeWithInputSignature,
    SIZE_T bytecodeLength, const char* name)
{
    assert(count <= VertexBufferId::Count);
    D3D11_INPUT_ELEMENT_DESC layoutDesc[VertexBufferId::Count];
    for (u32 i = 0; i < count; ++i)
    {
        u32 id = (u32)(ids[i]);
        layoutDesc[i] = { vbNames[id], 0, vbFormats[id], (UINT)i, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 };
    }
    ID3D11InputLayout* layout;
    const HRESULT hr = d3ds.device->CreateInputLayout(layoutDesc, count, shaderBytecodeWithInputSignature, bytecodeLength, &layout);
    CheckAndName(hr, "CreateInputLayout", layout, fmt("%s IL", name));
    resources->Push(layout);
    return layout;
}

ID3D11Buffer* CreateBuffer(ResourceArray* resources, const D3D11_BUFFER_DESC* desc, const D3D11_SUBRESOURCE_DATA* initialData, const char* name)
{
    ID3D11Buffer* buffer;
    const HRESULT hr = d3ds.device->CreateBuffer(desc, initialData, &buffer);
    CheckAndName(hr, "CreateBuffer", buffer, name);
    resources->Push(buffer);
    return buffer;
}

ID3D11ShaderResourceView* CreateShaderResourceView(ResourceArray* resources, ID3D11Resource* resource, const D3D11_SHADER_RESOURCE_VIEW_DESC* srvDesc, const char* name)
{
    ID3D11ShaderResourceView* srv;
    const HRESULT hr = d3ds.device->CreateShaderResourceView(resource, srvDesc, &srv);
    CheckAndName(hr, "CreateShaderResourceView", srv, fmt("%s SRV", name));
    resources->Push(srv);
    return srv;
}

ID3D11UnorderedAccessView* CreateUnorderedAccessView(ResourceArray* resources, ID3D11Resource* resource, const D3D11_UNORDERED_ACCESS_VIEW_DESC* uavDesc, const char* name)
{
    ID3D11UnorderedAccessView* uav;
    const HRESULT hr = d3ds.device->CreateUnorderedAccessView(resource, uavDesc, &uav);
    CheckAndName(hr, "CreateUnorderedAccessView", uav, fmt("%s UAV", name));
    resources->Push(uav);
    return uav;
}

ID3D11Texture3D* CreateTexture3D(ResourceArray* resources, const D3D11_TEXTURE3D_DESC* texDesc,
    const D3D11_SUBRESOURCE_DATA* initialData,
    const char* name)
{
    ID3D11Texture3D* tex;
    const HRESULT hr = d3ds.device->CreateTexture3D(texDesc, initialData, &tex);
    CheckAndName(hr, "CreateTexture3D", tex, name);
    resources->Push(tex);
    return tex;
}

ID3D11Texture2D* CreateTexture2D(ResourceArray* resources, const D3D11_TEXTURE2D_DESC* texDesc,
    const D3D11_SUBRESOURCE_DATA* initialData,
    const char* name)
{
    ID3D11Texture2D* tex;
    const HRESULT hr = d3ds.device->CreateTexture2D(texDesc, initialData, &tex);
    CheckAndName(hr, "CreateTexture2D", tex, name);
    resources->Push(tex);
    return tex;
}

ID3D11RenderTargetView* CreateRenderTargetView(ResourceArray* resources, ID3D11Resource* resource,
    const D3D11_RENDER_TARGET_VIEW_DESC* rtvDesc,
    const char* name)
{
    ID3D11RenderTargetView* rtv;
    const HRESULT hr = d3ds.device->CreateRenderTargetView(resource, rtvDesc, &rtv);
    CheckAndName(hr, "CreateRenderTargetView", rtv, fmt("%s RTV", name));
    resources->Push(rtv);
    return rtv;
}

ID3D11DepthStencilView* CreateDepthStencilView(ResourceArray* resources, ID3D11Resource* resource,
    const D3D11_DEPTH_STENCIL_VIEW_DESC* dsvDesc,
    const char* name)
{
    ID3D11DepthStencilView* dsv;
    const HRESULT hr = d3ds.device->CreateDepthStencilView(resource, dsvDesc, &dsv);
    CheckAndName(hr, "CreateDepthStencilView", dsv, fmt("%s DSV", name));
    resources->Push(dsv);
    return dsv;
}

void ReleaseResources(ResourceArray* resources)
{
    for (u32 r = 0; r < resources->Length(); ++r)
    {
        COM_RELEASE((*resources)[r]);
    }
    resources->Clear();
}

static UINT DXGIFormatBytesPerPixel(DXGI_FORMAT format)
{
    switch (format)
    {
    case DXGI_FORMAT_R8_UNORM:
    {
        return 1;
    }
    break;
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_R8G8B8A8_UINT:
    case DXGI_FORMAT_R8G8B8A8_SNORM:
    case DXGI_FORMAT_R8G8B8A8_SINT:
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_R32_FLOAT:
    case DXGI_FORMAT_R32_UINT:
    case DXGI_FORMAT_R32_SINT:
    {
        return 4;
    }
    break;
    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
    case DXGI_FORMAT_R16G16B16A16_UNORM:
    case DXGI_FORMAT_R16G16B16A16_UINT:
    case DXGI_FORMAT_R16G16B16A16_SNORM:
    case DXGI_FORMAT_R16G16B16A16_SINT:
    case DXGI_FORMAT_R32G32_TYPELESS:
    case DXGI_FORMAT_R32G32_FLOAT:
    case DXGI_FORMAT_R32G32_UINT:
    case DXGI_FORMAT_R32G32_SINT:
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
    {
        return 8;
    }
    break;
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
    case DXGI_FORMAT_R32G32B32A32_UINT:
    case DXGI_FORMAT_R32G32B32A32_SINT:
    {
        return 16;
    }
    break;
    default:
    {
        assert(0);
    }
    break;
    }

    return 0;
}

UINT GetTexture3DSizeBytes(ID3D11Texture3D* texture)
{
    D3D11_TEXTURE3D_DESC desc;
    texture->GetDesc(&desc);
    UINT bytesPerPixel = DXGIFormatBytesPerPixel(desc.Format);
    UINT x = desc.Width;
    UINT y = desc.Height;
    UINT z = desc.Depth;
    UINT numMipLevels = desc.MipLevels;
    UINT result = 0;
    for (UINT m = 0; m < numMipLevels; ++m)
    {
        result += bytesPerPixel * x * y * z;
        x = MAX(x / 2, 1);
        y = MAX(y / 2, 1);
        z = MAX(z / 2, 1);
    }
    return result;
}
