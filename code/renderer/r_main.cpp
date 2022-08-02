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

#include "../imgui/imgui_impl_dx11.h"
#include "r_private.h"

ConeTracingConstants tracingConstants;
ShadowMappingConstants shadowConstants;
TempConstants tempConstants;
VoxelVizConstants voxelVizConstants;
RenderStats renderStats;
RenderBackendFlags r_backendFlags;
BisectData bisect;

static MemoryPools renderMemory;

void R_Init(void* handle, MemoryPools* memory)
{
    SubArena(&renderMemory.persistent, &memory->persistent, Megabytes(4), "persistent renderer arena");
    SubArena(&renderMemory.transient, &memory->transient, Megabytes(10), "transient renderer arena");

    // Create D3D11 context and pipelines.
    D3D11_Init((HWND)handle);
    Voxel_Init();
    GeometryPass_Init();
    Shadows_Init();
    DeferredShading_Init();
    ScreenSpaceSpecularOcclusion_Init();
    SSSO_Filter_Init();
    HiZ_Init();
    DebugViz_Init();

    ImGui_ImplDX11_Init(d3ds.device, d3ds.context);
    ImGui_ImplDX11_NewFrame(); // force creation of the font atlas

    //ImGui::GetIO().IniFilename = NULL; // to disable .ini read/write
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
}

void R_ShutDown()
{
    ImGui_ImplDX11_Shutdown();
    DebugViz_Shutdown();
    DeferredShading_Shutdown();
    ScreenSpaceSpecularOcclusion_Shutdown();
    GeometryPass_Shutdown();
    Shadows_Shutdown();
    Voxel_Shutdown();
    SSSO_Filter_Shutdown();

    // always last
    D3D11_Shutdown();
}

static int GetSceneTriangleCount(Scene* scene)
{
    int numIndices = 0;
    for (u32 m = 0; m < scene->meshes.Length(); ++m)
    {
        MeshFileMesh* mesh = &scene->meshes[m];
        Material* material = &scene->materials[mesh->materialIndex];
        if (material->flags & IS_ALPHA_TESTED)
            continue;

        numIndices += mesh->numIndexes;
    }

    return numIndices / 3;
}

void R_RenderCommandQueueToOutput(RenderCommandQueue* cmdQueue)
{
    for (u32 base_addr = 0; base_addr < cmdQueue->size;)
    {
        RenderEntryHeader* hdr = (RenderEntryHeader*)(cmdQueue->base_ptr + base_addr); // grap the current hdr
        switch (hdr->type)
        {
        case RenderEntry::Scene:
        {
            RenderEntryScene* entry = (RenderEntryScene*)hdr;
            Scene* scene = entry->scene;
            Scene* sceneLowRes = entry->sceneLowRes;

            if (scene->meshId != assetsShared.assetID)
            {
                DrawBuffer_Init(&assetsShared.drawBuffer);
                AddImmutableObject(&assetsShared.drawBuffer, scene);
                AllocateMeshTextures(scene);

                DrawBuffer_Init(&assetsShared.drawBufferLowRes);
                AddImmutableObject(&assetsShared.drawBufferLowRes, sceneLowRes);
                sceneLowRes->materials = scene->materials; // @FIXME: this would incur a double free

                assetsShared.currentMesh = scene;
                assetsShared.assetID = scene->meshId;
                bisect.root.count = assetsShared.drawBufferLowRes.numIndexes;
                bisect.root.start = 0;
                bisect.parent = bisect.root;
                bisect.child = bisect.parent;

// tight fitting AABB for our version of the Sponza scene
#if 0
					scene->aabb.min[0] = -1460.00708f;
					scene->aabb.min[1] = -16.6320190f;
					scene->aabb.min[2] = -676.621094f;
					scene->aabb.max[0] = 1338.96924f;
					scene->aabb.max[1] = 1399.62280f;
					scene->aabb.max[2] = 599.239990f;
#endif

// old code to inflate the AABB
#if 0
                    vec3_t textureDimensions;
                    textureDimensions.x = voxelShared.gridSize.w;
                    textureDimensions.y = voxelShared.gridSize.h;
                    textureDimensions.z = voxelShared.gridSize.d;
                    vec3_t vecOf2 = { 2, 2, 2 };

                    vec3_t oldDim = scene->aabb.max - scene->aabb.min;
                    vec3_t scale = 1 - (vecOf2 / textureDimensions);
                    vec3_t newDim = oldDim / scale;
                    vec3_t delta = (newDim - oldDim) / 2;

                    scene->aabb.min = scene->aabb.min - delta;
                    scene->aabb.max = scene->aabb.max + delta;
#endif
            }

            // Add AABB to cmdQueue
            cmdQueue->aabb.min = scene->aabb.min;
            cmdQueue->aabb.max = scene->aabb.max;

            Shadows_Draw(cmdQueue, &assetsShared.drawBuffer);

            bool wantAO = !!(r_backendFlags.deferredOptions & DEFERRED_AMBIENT_OCCLUSION);
            bool wantID = !!(r_backendFlags.deferredOptions & DEFERRED_INDIRECT_DIFFUSE);
            bool wantIS = !!(r_backendFlags.deferredOptions & DEFERRED_INDIRECT_SPECULAR);
            bool runOpacity = wantAO && !wantID;
            bool runEmittance = wantID || wantIS;
            if (r_backendFlags.drawVoxelViz && voxelVizConstants.cst_singleChannel)
            {
                runOpacity = true;
            }
            if (r_backendFlags.drawVoxelViz && !voxelVizConstants.cst_singleChannel)
            {
                runEmittance = true;
            }

            int numTriangles = GetSceneTriangleCount(scene);
            renderStats.numRenderedTriangles = numTriangles;
            if (r_backendFlags.voxelizeLowRes)
            {
                Voxel_Run(cmdQueue, &assetsShared.drawBufferLowRes, sceneLowRes, runOpacity, runEmittance);
                renderStats.numVoxelizedTriangles = GetSceneTriangleCount(sceneLowRes);
            }
            else
            {
                Voxel_Run(cmdQueue, &assetsShared.drawBuffer, scene, runOpacity, runEmittance);
                renderStats.numVoxelizedTriangles = numTriangles;
            }

            GeometryPass_Draw(cmdQueue, &assetsShared.drawBuffer);

            if ((r_backendFlags.deferredOptions & DEFERRED_SSSO) && wantIS)
            {
                //HiZ_Run();
                ScreenSpaceSpecularOcclusion_Run(cmdQueue, &assetsShared.drawBuffer);
                SSSO_Filter_Draw(cmdQueue);
            }

            if (r_backendFlags.drawDeferredShading)
            {
                DeferredShading_Draw(cmdQueue, &assetsShared.drawBuffer);
            }

            if (r_backendFlags.drawVoxelViz)
            {
                if (!r_backendFlags.drawDeferredShading)
                {
                    ClearDepthStencilBuffer();
                }
                Voxel_DrawViz(cmdQueue);
            }

            base_addr += sizeof(*entry);
        }
        break;
        case RenderEntry::Box:
        {
            RenderEntryBox* entry = (RenderEntryBox*)hdr;
            DebugViz_AppendBox(cmdQueue, entry);
            base_addr += sizeof(*entry);
        }
        break;
        case RenderEntry::BeginDebugRegion:
        {
            RenderEntryBeginDebugRegion* entry = (RenderEntryBeginDebugRegion*)hdr;
            BeginDebugRegion(entry->name);
            base_addr += sizeof(*entry);
        }
        break;
        case RenderEntry::EndDebugRegion:
        {
            RenderEntryEndDebugRegion* entry = (RenderEntryEndDebugRegion*)hdr;
            DebugViz_Draw(cmdQueue);
            EndDebugRegion();
            base_addr += sizeof(*entry);
        }
        break;
        default:
        {
            // Not allowed
        }
        break;
        }
    }
}

static void DrawImGui()
{
    DEBUG_REGION("GUI");
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    graphicsPipelineValid = false;
}

void R_DrawFrame(ClientInput* input)
{
    BeginFrameQueries();
    BeginQuery(QueryId::FullFrame);
    BeginFrame();

    ImGui_ImplDX11_NewFrame();
    
    TemporaryMemory cmdQueueMemory = BeginTemporaryMemory(&renderMemory.transient);
    RenderCommandQueue* cmdQueue = AllocateRenderCommandQueue(cmdQueueMemory.arena, Megabytes(4));

    S_Update(input, cmdQueue);
    R_RenderCommandQueueToOutput(cmdQueue);

    EndTemporaryMemory(cmdQueueMemory);
    CheckArena(&renderMemory.transient);

    DrawImGui();

    PresentFrame();
    EndQuery(QueryId::FullFrame);
    EndFrameQueries();
    d3d.frameNumber++;
}

void R_WindowSizeChanged()
{
    D3D11_WindowSizeChanged();
    GeometryPass_WindowSizeChanged();
    ScreenSpaceSpecularOcclusion_WindowSizeChanged();
    SSSO_Filter_WindowSizeChanged();
}