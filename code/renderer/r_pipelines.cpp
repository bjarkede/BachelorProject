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

static GraphicsPipeline graphicsPipeline;
static ComputePipeline computePipeline;
bool graphicsPipelineValid = false;
static bool computePipelineValid = false;

static void UnbindPipeline()
{
    d3ds.context->OMSetRenderTargetsAndUnorderedAccessViews(0, NULL, NULL, 0, NULL, NULL, NULL);
    ID3D11UnorderedAccessView* clearUAVs[8] = { NULL };
    d3ds.context->CSSetUnorderedAccessViews(0, ARRAY_LEN(clearUAVs), clearUAVs, NULL);
    computePipeline.numUAVs = 0;
    ID3D11ShaderResourceView* clearSRVs[128] = { NULL };
    d3ds.context->CSSetShaderResources(0, ARRAY_LEN(clearSRVs), clearSRVs);
}

#define CHECK(state) (invalid || newP->state != oldP->state)
#define TESTARRAY(count, array) (newP->count != oldP->count || memcmp(newP->array, oldP->array, sizeof(newP->array[0]) * newP->count))
#define CHECKA(count, array)           \
    n = MAX(newP->count, oldP->count); \
    if (invalid || TESTARRAY(count, array))

void SetPipeline(GraphicsPipeline* pipeline, bool unbind)
{
    if (unbind)
        UnbindPipeline();

    bool invalid = !graphicsPipelineValid;
    GraphicsPipeline* oldP = &graphicsPipeline;
    GraphicsPipeline* newP = pipeline;
    u32 n = 0;

    if (CHECK(ia.indexBuffer))
        d3ds.context->IASetIndexBuffer(pipeline->ia.indexBuffer, DXGI_FORMAT_R32_UINT, 0);

    if (CHECK(ia.layout))
        d3ds.context->IASetInputLayout(pipeline->ia.layout);

    if (CHECK(ia.topology))
        d3ds.context->IASetPrimitiveTopology(pipeline->ia.topology);

    CHECKA(ia.numVertexBuffers, ia.vertexBuffers)
    d3ds.context->IASetVertexBuffers(0, n, pipeline->ia.vertexBuffers, pipeline->ia.strides, pipeline->ia.offsets);

    if (CHECK(vs.shader))
        d3ds.context->VSSetShader(pipeline->vs.shader, NULL, 0);

    CHECKA(vs.numBuffers, vs.buffers)
    d3ds.context->VSSetConstantBuffers(0, n, pipeline->vs.buffers);

    CHECKA(vs.numSRVs, vs.srvs)
    d3ds.context->VSSetShaderResources(0, n, pipeline->vs.srvs);

    CHECKA(vs.numSamplers, vs.samplers)
    d3ds.context->VSSetSamplers(0, n, pipeline->vs.samplers);

    if (CHECK(gs.shader))
        d3ds.context->GSSetShader(pipeline->gs.shader, NULL, 0);

    CHECKA(gs.numBuffers, gs.buffers)
    d3ds.context->GSSetConstantBuffers(0, n, pipeline->gs.buffers);

    CHECKA(gs.numSRVs, gs.srvs)
    d3ds.context->GSSetShaderResources(0, n, pipeline->gs.srvs);

    CHECKA(gs.numSamplers, gs.samplers)
    d3ds.context->GSSetSamplers(0, n, pipeline->gs.samplers);

    CHECKA(rs.numViewports, rs.viewports)
    d3ds.context->RSSetViewports(n, pipeline->rs.viewports);

    CHECKA(rs.numScissors, rs.scissors)
    d3ds.context->RSSetScissorRects(n, pipeline->rs.scissors);

    if (CHECK(rs.state))
        d3ds.context->RSSetState(pipeline->rs.state);

    if (CHECK(ps.shader))
        d3ds.context->PSSetShader(pipeline->ps.shader, NULL, 0);

    CHECKA(ps.numBuffers, ps.buffers)
        d3ds.context->PSSetConstantBuffers(0, n, pipeline->ps.buffers);

    CHECKA(ps.numSRVs, ps.srvs)
        d3ds.context->PSSetShaderResources(0, n, pipeline->ps.srvs);

    CHECKA(ps.numSamplers, ps.samplers)
        d3ds.context->PSSetSamplers(0, n, pipeline->ps.samplers);

    if (CHECK(om.blendState))
        d3ds.context->OMSetBlendState(pipeline->om.blendState, NULL, 0xFFFFFFFF);

    if (CHECK(om.depthState))
        d3ds.context->OMSetDepthStencilState(pipeline->om.depthState, 0);

    if (unbind || invalid || CHECK(om.depthView) || TESTARRAY(om.numRTVs, om.rtvs) || TESTARRAY(om.numUAVs, om.uavs))
    {
        d3ds.context->OMSetRenderTargetsAndUnorderedAccessViews(pipeline->om.numRTVs, pipeline->om.rtvs, pipeline->om.depthView, 0, pipeline->om.numUAVs, pipeline->om.uavs, NULL);
    }

    graphicsPipelineValid = true;
    graphicsPipeline = *pipeline;
}

void SetPipeline(ComputePipeline* pipeline)
{
    UnbindPipeline();

    bool invalid = !computePipelineValid;
    ComputePipeline* oldP = &computePipeline;
    ComputePipeline* newP = pipeline;
    u32 n = 0;

    if (CHECK(shader))
        d3ds.context->CSSetShader(pipeline->shader, NULL, 0);

#if DEBUG
    for (u32 i = pipeline->numBuffers; i < ARRAY_LEN(pipeline->buffers); ++i)
    {
        assert(pipeline->buffers[i] == NULL);
    }
    for (u32 i = pipeline->numSRVs; i < ARRAY_LEN(pipeline->srvs); ++i)
    {
        assert(pipeline->srvs[i] == NULL);
    }
    for (u32 i = pipeline->numUAVs; i < ARRAY_LEN(pipeline->uavs); ++i)
    {
        assert(pipeline->uavs[i] == NULL);
    }
    for (u32 i = pipeline->numSamplers; i < ARRAY_LEN(pipeline->samplers); ++i)
    {
        assert(pipeline->samplers[i] == NULL);
    }
#endif
    CHECKA(numBuffers, buffers)
        d3ds.context->CSSetConstantBuffers(0, ARRAY_LEN(pipeline->buffers), pipeline->buffers);
    CHECKA(numSRVs, srvs)
        d3ds.context->CSSetShaderResources(0, ARRAY_LEN(pipeline->srvs), pipeline->srvs);
    CHECKA(numSamplers, samplers)
        d3ds.context->CSSetSamplers(0, ARRAY_LEN(pipeline->samplers), pipeline->samplers);
    CHECKA(numUAVs, uavs)
        d3ds.context->CSSetUnorderedAccessViews(0, pipeline->numUAVs, pipeline->uavs, NULL);
       
    computePipelineValid = true;
    computePipeline = *pipeline;
}

#undef CHECK
#undef TESTARRAY
#undef CHECKA

void SetDrawBuffer(GraphicsPipeline* pipeline, DrawBuffer* buffer, const VertexBufferId::Type* ids, s32 count)
{
    for (int i = 0; i < d3d.vbCount; ++i)
    {
        VertexBuffer* const vb = &buffer->vertexBuffers[d3d.vbIds[i]]; // Get the buffer for the current VertexBufferId
        pipeline->ia.offsets[i] = vb->readIndex * vb->itemSize; // Get the offset
    }

    for (int i = 0; i < count; ++i)
    {
        VertexBuffer* const vb = &buffer->vertexBuffers[ids[i]];
        pipeline->ia.vertexBuffers[i] = vb->buffer;
        pipeline->ia.strides[i] = vb->itemSize;
    }
    pipeline->ia.numVertexBuffers = count;

    pipeline->ia.indexBuffer = buffer->indexBuffer.buffer;
}
