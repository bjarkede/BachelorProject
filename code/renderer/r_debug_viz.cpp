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

#include "../shaders/debug_viz_ps.h"
#include "../shaders/debug_viz_vs.h"

#pragma pack(push, 1)
struct DebugVizVSData
{
    f32 modelViewMatrix[16];
    f32 projectionMatrix[16];
};

struct DebugVizPSData
{
    vec4_t color;
};
#pragma pack(pop)

struct Local
{
    GraphicsPipeline pipeline;
    ResourceArray persistent;
    DrawBuffer drawBuffer;
};

static Local local;
static const VertexBufferId::Type vbIds[] = { VertexBufferId::Position, VertexBufferId::Color };

void DebugViz_Init()
{
    GraphicsPipeline* p = &local.pipeline;
    InitPipeline(p);

    p->ia.layout = CreateInputLayout(&local.persistent, vbIds, g_vs, "debug viz");

    p->vs.shader = CreateVertexShader(&local.persistent, g_vs, "debug viz");
    p->vs.buffers[0] = CreateShaderConstantBuffer(sizeof(DebugVizVSData), "debug viz vertex cbuffer");
    p->vs.numBuffers = 1;

    p->ps.shader = CreatePixelShader(&local.persistent, g_ps, "debug viz");
    p->ps.buffers[0] = CreateShaderConstantBuffer(sizeof(DebugVizPSData), "debug viz pixel cbuffer");
    p->ps.numBuffers = 1;

    D3D11_RASTERIZER_DESC rasterDesc;
    ZeroMemory(&rasterDesc, sizeof(rasterDesc));
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.CullMode = D3D11_CULL_NONE;
    rasterDesc.FrontCounterClockwise = FALSE;
    rasterDesc.ScissorEnable = TRUE;
    rasterDesc.DepthClipEnable = FALSE;
    rasterDesc.DepthBiasClamp = 0.0f;
    p->rs.state = CreateRasterizerState(&rasterDesc);

    p->om.blendState = d3d.blendStateStandard;

    DrawBuffer_Init(&local.drawBuffer);
    DrawBuffer_Allocate(&local.persistent, &local.drawBuffer, "debug viz draw buffer");
}

void DebugViz_Shutdown()
{
    ReleaseResources(&local.persistent);
}

static bool ShouldDiscardDrawBuffer(DrawBuffer* buffer, u32 numIndexes, u32 numVertexes)
{
    bool discard = false;
    for (u32 b = 0; b < VertexBufferId::Count; ++b)
    {
        VertexBuffer* vb = &buffer->vertexBuffers[b];
        discard = vb->writeIndex + numVertexes > vb->capacity;
        if (discard)
        {
            return discard;
        }
    }
    return buffer->indexBuffer.writeIndex + numIndexes > buffer->indexBuffer.capacity;
}

void DebugViz_AppendBox(RenderCommandQueue* cmdQueue, RenderEntryBox* box)
{
    DrawBuffer* buffer = &local.drawBuffer;
    
    const u32 numVertexes = 8; 
    const u32 numIndexes  = 36;
    u32 indexOffset = buffer->vertexBuffers->writeIndex;
    if (ShouldDiscardDrawBuffer(buffer, numIndexes, numVertexes))
    {
        if (buffer->numIndexes)
        {
            DebugViz_Draw(cmdQueue);
        }

        // make sure all buffers discard
        for (u32 i = 0; i < ARRAY_LEN(vbIds); ++i)
        {
            VertexBuffer* vb = &buffer->vertexBuffers[i];
            vb->discard = true;
        }
        buffer->indexBuffer.discard = true;
        indexOffset = 0;
    }
    
    vec3_t min = box->min;
    vec3_t max = box->max;
    vec3_t xyz[numVertexes] = {
        { min.x, min.y, min.z },
        { min.x, max.y, min.z },
        { max.x, max.y, min.z },
        { max.x, min.y, min.z },
        { max.x, min.y, max.z },
        { max.x, max.y, max.z },
        { min.x, max.y, max.z },
        { min.x, min.y, max.z },
    };

    u32 indexes[numIndexes] = { 0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4,
                                1, 6, 5, 5, 2, 1, 7, 0, 3, 3, 4, 7,
                                3, 2, 5, 5, 4, 3, 7, 6, 1, 1, 0, 7 };
    for (u32 i = 0; i < numIndexes; ++i)
    {
        indexes[i] += indexOffset;
    }

    vec3_t color[numVertexes];
    for (u32 i = 0; i < 8; ++i)
    {
        Vec3Copy(color[i], box->color);
    }

    AppendVertexData(&buffer->vertexBuffers[VertexBufferId::Position], xyz);
    AppendVertexData(&buffer->vertexBuffers[VertexBufferId::Color], color);
    AppendVertexData(&buffer->indexBuffer, indexes);
    buffer->numIndexes += numIndexes;
}

void DebugViz_Draw(RenderCommandQueue* cmdQueue)
{
    GraphicsPipeline* p = &local.pipeline;
    DrawBuffer* buffer = &local.drawBuffer;

    DebugVizVSData vsData;
    memcpy(vsData.modelViewMatrix, cmdQueue->modelViewMatrix.E, sizeof(m4x4));
    memcpy(vsData.projectionMatrix, cmdQueue->projectionMatrix.E, sizeof(m4x4));
    SetShaderData(p->vs.buffers[0], vsData);

    p->om.depthView = d3d.depthStencilView;
    p->om.rtvs[0] = d3d.backBufferRTView;
    p->om.numRTVs = 1;

    SetDrawBuffer(p, buffer, vbIds);

    SetDefaultViewportAndScissor(p);

    SetPipeline(p);
    DrawIndexed(buffer, buffer->numIndexes);
    buffer->indexBuffer.readIndex += buffer->numIndexes;
    buffer->numIndexes = 0;
}