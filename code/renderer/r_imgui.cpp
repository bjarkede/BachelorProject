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

#include "../imgui/imgui.h"
#include "r_private.h"

static ImguiState gui_state;

static bool renderSettings = true;
static bool renderPerformance = true;
static bool renderHelp = true;
static bool renderMaterials = true;

namespace ImGui
{
static void TooltipMarker()
{
    TextDisabled("(?)");
}

static void Tooltip(const char* desc, float maxWidth = 35.0f)
{
    if (IsItemHovered())
    {
        BeginTooltip();
        PushTextWrapPos(GetFontSize() * maxWidth);
        TextUnformatted(desc);
        PopTextWrapPos();
        EndTooltip();
    }
}

static void Tooltip(const char** table, int numRows, int numColumns, float maxWidth = 35.0f)
{
    if (IsItemHovered())
    {
        BeginTooltip();

        int tableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;
        if (BeginTable("Help Table", numColumns, tableFlags))
        {
            int i = 0;
            for (int row = 0; row < numRows; ++row)
            {
                TableNextRow();
                PushID(row);
                for (int column = 0; column < numColumns; ++column)
                {
                    TableSetColumnIndex(column);
                    Text(table[i++]);
                }
                PopID();
            }
            EndTable();
        }

        EndTooltip();
    }
}

static bool RadioButton(const char* label, bool* v, bool v_button)
{
    int intValue = !!*v;
    const bool result = RadioButton(label, &intValue, (int)v_button);
    *v = intValue != 0;
    return result;
}

static bool CheckBox(const char* name, s32* v, s32 bitMask)
{
    bool option = !!(*v & bitMask); // !! bool conversion
    bool clicked = Checkbox(name, &option);
    s32 result = *v & (~bitMask);
    if (option)
    {
        result |= bitMask;
    }
    *v = result;
    return clicked;
}

const float smallItemWidth = 100.0f;

static bool SmallSliderFloat(const char* label, float* v, float v_min, float v_max, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
    SetNextItemWidth(smallItemWidth);
    return SliderFloat(label, v, v_min, v_max, format, flags);
}

static bool SmallSliderInt(const char* label, int* v, int v_min, int v_max, const char* format = "%d", ImGuiSliderFlags flags = 0)
{
    SetNextItemWidth(smallItemWidth);
    return SliderInt(label, v, v_min, v_max, format, flags);
}

static bool BeginSmallCombo(const char* label, const char* preview_value, ImGuiComboFlags flags = 0)
{
    SetNextItemWidth(smallItemWidth);
    return BeginCombo(label, preview_value, flags);
}
}

static void RenderAssets(SceneAssets* assets)
{
    if (ImGui::TreeNode("Object list"))
    {
        for (u32 i = 0; i < assets->numMeshes; i++)
        {
            Scene* mesh = &assets->meshes[i];

            ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
            bool node_open = ImGui::TreeNodeEx((void*)(intptr_t)i, node_flags, mesh->name);

            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
                assets->current_asset = i;
            if (node_open)
            {
                ImGui::Text("Information:");
                ImGui::Text("Amount of vertices: %d", mesh->xyz.Length());
                ImGui::Text("Amount of normals: %d", mesh->normal.Length());
                ImGui::Text("Amount of indexes: %d", mesh->indexes.Length());
                ImGui::Text("AABB: %f.2, %f.2 %f.2 (min)", mesh->aabb.min.x, mesh->aabb.min.y, mesh->aabb.min.z);
                ImGui::Text("AABB: %f.2, %f.2 %f.2 (max)", mesh->aabb.max.x, mesh->aabb.max.y, mesh->aabb.max.z);
                ImGui::Text("Amount of uv coordinates: %d", mesh->tc.Length());
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }
}

static Light currentLight;

static void EditLight(Light* light, vec3_t min, vec3_t max)
{
    f32* pos[3];
    f32* color[4];

    pos[0] = &light->position.x;
    pos[1] = &light->position.y;
    pos[2] = &light->position.z;

    color[0] = &light->color.x;
    color[1] = &light->color.y;
    color[2] = &light->color.z;
    color[3] = &light->color.w;

    ImGui::Text("Adjust Settings");
    ImGui::Text("Position:");
    ImGui::SmallSliderFloat("x", pos[0], min.x, max.x);
    ImGui::SmallSliderFloat("y", pos[1], min.y, max.y);
    ImGui::SmallSliderFloat("z", pos[2], min.z, max.z);

    ImGui::Text("Direction:");
    ImGui::SmallSliderFloat("azimuth", &light->azimuth, 0, 360);
    ImGui::SmallSliderFloat("inclination", &light->inclination, 0, 180);
    light->dir.x = cos(TO_RADIANS(light->azimuth)) * sin(TO_RADIANS(light->inclination));
    light->dir.y = sin(TO_RADIANS(light->azimuth)) * sin(TO_RADIANS(light->inclination));
    light->dir.z = cos(TO_RADIANS(light->inclination));

    ImGui::Text("Angle:");
    ImGui::SmallSliderFloat("umbra", &light->umbraAngle, light->penumbraAngle, 180);
    ImGui::SmallSliderFloat("penumbra", &light->penumbraAngle, 0, light->umbraAngle);

    ImGui::SmallSliderFloat("radius", &light->radius, 0.01f, 2000.0f);
    ImGui::SmallSliderFloat("size", &light->sizeUV, 0.0f, 0.1f);
    ImGui::ColorEdit4("rgba", *color);
}

static void RenderLightEditor(SceneAssets* assets)
{
    static s32 selectedLight = 0;

    r_backendFlags.shouldUpdateDirectLight = true;

    // Edit
    {
        ImGui::BeginChild("ChildL", { ImGui::GetWindowContentRegionWidth() * 0.65f, 300 }, false, ImGuiWindowFlags_None);
        EditLight(&assets->lights[selectedLight], assets->meshes[assets->current_asset].aabb.min,
            assets->meshes[assets->current_asset].aabb.max);
        ImGui::EndChild();
    }

    ImGui::SameLine();

    // Selection Window
    {
        ImGui::BeginChild("ChildR", { 0, 300 }, true, ImGuiWindowFlags_None);
        if (ImGui::BeginTable("split", 1, ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings))
        {
            for (int i = 0; i < assets->lights.Length(); i++)
            {
                ImGui::TableNextColumn();
                if (ImGui::Button(fmt("light #%d", i + 1), ImVec2(-FLT_MIN, 0.0f)))
                {
                    selectedLight = i;
                }
            }
            ImGui::EndTable();
        }
        ImGui::EndChild();
    }
}

static void BisectMethod()
{
    enum Range
    {
        Full,
        Left,
        Right
    };

    static s32 range = Full;

    ImGui::RadioButton("Full", &range, Full);
    ImGui::SameLine();
    ImGui::RadioButton("Left", &range, Left);
    ImGui::SameLine();
    ImGui::RadioButton("Right", &range, Right);
    // update drawn index range

    IndexRange& root = bisect.root;
    IndexRange& parent = bisect.parent;
    IndexRange& child = bisect.child;

    const s32 leftCount = ((parent.count / 3) / 2) * 3;
    const s32 rightCount = parent.count - leftCount;

    if (range == Left) // left
    {
        child.start = parent.start;
        child.count = leftCount;
    }
    else if (range == Right) // right
    {
        child.start = parent.start + leftCount;
        child.count = rightCount;
    }
    else
    {
        child = parent;
    }

    if (bisect.parent.count >= 6)
    {
        ImGui::SameLine();
        if (ImGui::Button("Bisect"))
        {
            parent = child;
        }
    }

    ImGui::Text("Root: %d %d - Parent: %d %d - Child: %d %d",
        root.start, root.count,
        parent.start, parent.count,
        child.start, child.count);

    ImGui::InputInt("Parent offset", &parent.start, 1, 100, ImGuiInputTextFlags_ReadOnly);
    ImGui::SameLine();
    if (ImGui::Button("Reset"))
    {
        parent = root;
    }
}

static void RenderTransform(RenderCommandQueue* cmdQueue)
{
    static f32 x = 0.0f;
    static f32 y = 0.0f;
    static f32 z = 0.0f;
    static f32 s = 1.0f;

    ImGui::Text("Rotation:");
    ImGui::SmallSliderFloat("x", &x, -180.0f, 180.0f);
    ImGui::SmallSliderFloat("y", &y, -180.0f, 180.0f);
    ImGui::SmallSliderFloat("z", &z, -180.0f, 180.0f);
    ImGui::Text("Scale:");
    ImGui::SmallSliderFloat("s", &s, 0.0f, 2.0f);

    m4x4 t = XRotation(TO_RADIANS(x)) * YRotation(TO_RADIANS(y)) * ZRotation(TO_RADIANS(z));
    cmdQueue->modelViewMatrix = Identity();
}

static void ImGui_Init()
{
    static bool isInitialized = false;
    if (!isInitialized)
    {
        tracingConstants.cst_opacityThreshold = 0.95f;
        tracingConstants.cst_traceStepScale = 0.5f;
        tracingConstants.cst_occlusionScale = 200.0f;
        tracingConstants.cst_coneAngle = 10.0f;
        tracingConstants.cst_maxMipLevel = ComputeMipCount(voxelShared.gridSize.h, voxelShared.gridSize.w, voxelShared.gridSize.d) - 1;
        tracingConstants.cst_mipBias = 0.0f;
        shadowConstants.cst_slopeBias = 1.0f;
        shadowConstants.cst_fixedBias = 0.0025f;
        shadowConstants.cst_maxBias = 0.005f;

        voxelVizConstants.cst_mipLevel = 0;
        voxelVizConstants.cst_wireFrameMode = VoxelWireFrameMode::Overlay;
        voxelVizConstants.cst_singleChannel = false;
        voxelVizConstants.emittanceIndex = 1;

        r_backendFlags.drawDeferredShading = true;
        r_backendFlags.consRasterMode = ConsRasterMode::Software;
        r_backendFlags.voxelizeLowRes = true;
        r_backendFlags.useNormalMap = true;
        r_backendFlags.normalStrength = 1.0f;
        r_backendFlags.deferredOptions = DEFERRED_SHADOWS_PCSS | DEFERRED_DIRECT_LIGHTING | DEFERRED_INDIRECT_DIFFUSE | DEFERRED_INDIRECT_SPECULAR | DEFERRED_SSSO;
        r_backendFlags.drawLights = true;
        r_backendFlags.maxAnisotropy = 1; // 4X
        r_backendFlags.shouldUpdateDirectLight = true;
        r_backendFlags.useGapFilling = true;
        
        tempConstants.temp[0] = 0.023f;
        tempConstants.temp[1] = 15.0f;
        tempConstants.temp[2] = 50.0f;
        tempConstants.temp[3] = 0.5f;

        isInitialized = true;
    }
}

static void RenderVoxelViz()
{
    ImGui::Checkbox("Draw Voxel Viz", &r_backendFlags.drawVoxelViz);
    // ImGui::SmallSliderInt("Emittance Index", &group->voxelVizConstants.emittanceIndex, 0, 1);
    ImGui::SmallSliderInt("Mip Level", &voxelVizConstants.cst_mipLevel, 0, ComputeMipCount(voxelShared.gridSize.h, voxelShared.gridSize.w, voxelShared.gridSize.d) - 1);
    ImGui::Text("Attribute:");
    ImGui::RadioButton("Emittance", &voxelVizConstants.cst_singleChannel, false);
    ImGui::SameLine();
    ImGui::RadioButton("Opacity", &voxelVizConstants.cst_singleChannel, true);

    const char* help[] = {
        "Off", "Draw faces only",
        "Overlay", "Draw faces and wireframe edges",
        "Exclusive", "Draw wireframe edges only"
    };
    ImGui::Text("Wireframe:");
    ImGui::SameLine();
    ImGui::TooltipMarker();
    ImGui::Tooltip(help, ARRAY_LEN(help) / 2, 2);
    ImGui::RadioButton("Off", &voxelVizConstants.cst_wireFrameMode, VoxelWireFrameMode::Disabled);
    ImGui::SameLine();
    ImGui::RadioButton("Overlay", &voxelVizConstants.cst_wireFrameMode, VoxelWireFrameMode::Overlay);
    ImGui::SameLine();
    ImGui::RadioButton("Exclusive", &voxelVizConstants.cst_wireFrameMode, VoxelWireFrameMode::Exclusive);
}

static void RenderDeferredShading()
{
    ImGui::Checkbox("Draw Deferred Shading", &r_backendFlags.drawDeferredShading);

    s32 options = r_backendFlags.deferredOptions;
    s32 shadowMask = DEFERRED_SHADOWS_PCF | DEFERRED_SHADOWS_PCSS;
    s32 shadowFlags = options & shadowMask;
    options &= ~shadowMask;

    ImGui::Text("Main Options:");
    ImGui::CheckBox("Direct Lighting", &options, DEFERRED_DIRECT_LIGHTING);
    ImGui::CheckBox("Indirect Diffuse", &options, DEFERRED_INDIRECT_DIFFUSE);
    ImGui::CheckBox("Indirect Specular", &options, DEFERRED_INDIRECT_SPECULAR);
    ImGui::CheckBox("Ambient Occlusion", &options, DEFERRED_AMBIENT_OCCLUSION);
    ImGui::CheckBox("Only Ambient Occlusion", &options, DEFERRED_ONLY_AMBIENT_OCCLUSION);
    ImGui::Checkbox("Normal Mapping", &r_backendFlags.useNormalMap);
    ImGui::SliderFloat("Normal Mapping Strength", &r_backendFlags.normalStrength, 0.0f, 1.0f);
    ImGui::CheckBox("SS Specular Occlusion", &options, DEFERRED_SSSO);
    ImGui::CheckBox("Only Specular Occlusion", &options, DEFERRED_SSSO_VIS);
    enum AFLevel
    {
        X1,
        X4,
        X8,
        X16
    };
    ImGui::Text("Anisotropic filtering:");
    ImGui::RadioButton("1x", &r_backendFlags.maxAnisotropy, X1);
    ImGui::SameLine();
    ImGui::RadioButton("4x", &r_backendFlags.maxAnisotropy, X4);
    ImGui::SameLine();
    ImGui::RadioButton("8x", &r_backendFlags.maxAnisotropy, X8);
    ImGui::SameLine();
    ImGui::RadioButton("16x", &r_backendFlags.maxAnisotropy, X16);

    ImGui::Separator();

    ImGui::Text("Voxel cone tracing:");
    ImGui::SmallSliderFloat("Opacity Threshold", &tracingConstants.cst_opacityThreshold, 0.8f, 1.0f);
    ImGui::SmallSliderFloat("Trace Step Scale", &tracingConstants.cst_traceStepScale, 0.1f, 1.0f);
    ImGui::SmallSliderFloat("Occlusion Scale", &tracingConstants.cst_occlusionScale, 0.0f, 500.0f);
    ImGui::SmallSliderFloat("Specular Cone Angle", &tracingConstants.cst_coneAngle, 0.125f, 30.0f);
    ImGui::SmallSliderFloat("Mip Bias", &tracingConstants.cst_mipBias, -1.0f, 0.0f);
    // currently unused
    //ImGui::SmallSliderFloat("Max Mip Level", &tracingConstants.cst_maxMipLevel, 1.0f, (f32)ComputeMipCount(voxelShared.gridSize.h, voxelShared.gridSize.w, voxelShared.gridSize.d) - 1);

    ImGui::Separator();

    ImGui::Text("Shadows:");
    ImGui::RadioButton("Off", &shadowFlags, 0);
    ImGui::SameLine();
    ImGui::RadioButton("PCSS", &shadowFlags, DEFERRED_SHADOWS_PCSS);
    ImGui::SameLine();
    ImGui::RadioButton("PCF", &shadowFlags, DEFERRED_SHADOWS_PCF);
    ImGui::SmallSliderFloat("Slope Bias", &shadowConstants.cst_slopeBias, 0.0f, 10.0f, "%.6f");
    ImGui::SmallSliderFloat("Fixed Bias", &shadowConstants.cst_fixedBias, 0.0f, 0.01f, "%.6f");
    ImGui::SmallSliderFloat("Max Bias", &shadowConstants.cst_maxBias, 0.0f, 10.0f, "%.6f");

#if defined(_DEBUG)
    ImGui::Separator();

    ImGui::Text("Temp:");
    ImGui::SmallSliderFloat("Temp 0", &tempConstants.temp[0], 0.0f, 1.0f);
    ImGui::SmallSliderFloat("Temp 1", &tempConstants.temp[1], 0.0f, 1.0f);
    ImGui::SmallSliderFloat("Temp 2", &tempConstants.temp[2], 0.0f, 1.0f);
    ImGui::SmallSliderFloat("Temp 3", &tempConstants.temp[3], 0.0f, 1.0f);
#endif

    if (options & DEFERRED_ONLY_AMBIENT_OCCLUSION)
    {
        options |= DEFERRED_AMBIENT_OCCLUSION;
    }

    r_backendFlags.deferredOptions = options | shadowFlags;
}

static void RenderCPUTimings()
{
    ImGui::Text("Frame time: %.1f ms", gui_state.time_info.frame_dt);
    ImGui::Text("FPS: %.0f", 1000.0f / gui_state.time_info.frame_dt);
}

static void RenderGPUTimings()
{
    int tableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;
    if (ImGui::BeginTable("GPU timings", 4, tableFlags))
    {
        ImGui::TableSetupColumn("render pass", 0);
        ImGui::TableSetupColumn("med", 0);
        ImGui::TableSetupColumn("min", 0);
        ImGui::TableSetupColumn("max", 0);
        ImGui::TableHeadersRow();

        for (int row = 0; row < QueryId::Count; row++)
        {
            if (row == QueryId::FullFrame && d3d.swapInterval != 0)
            {
                continue;
            }

            ImGui::TableNextRow();
            ImGui::PushID(row);
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", queryNames[row]);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%5.2f", d3d.frameQueries[row].medianUS / 1000.0f);
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%5.2f", d3d.frameQueries[row].minUS / 1000.0f);
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%5.2f", d3d.frameQueries[row].maxUS / 1000.0f);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}

static void SelectSwapInterval()
{
    ImGui::Text("Swap Interval");

    s32 swapInterval = d3d.swapInterval;

    ImGui::RadioButton("0", &swapInterval, 0);
    ImGui::SameLine();
    ImGui::RadioButton("1", &swapInterval, 1);
    ImGui::SameLine();
    ImGui::RadioButton("2", &swapInterval, 2);
    ImGui::SameLine();
    ImGui::RadioButton("3", &swapInterval, 3);

    d3d.swapInterval = swapInterval;
}

static void RenderMenuBar()
{
    ImGui::BeginMainMenuBar();
    if (ImGui::BeginMenu("File", true))
    {
        if (ImGui::MenuItem("Quit", "ESC", false, true))
        {
            Sys_Quit();
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Show", true))
    {
        ImGui::MenuItem("Help", "F1", &renderHelp, true);
        ImGui::MenuItem("Settings", "F2", &renderSettings, true);
        ImGui::MenuItem("Performance", "F3", &renderPerformance, true);
        ImGui::MenuItem("Materials", "F4", &renderMaterials, true);
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
}

static void RenderPerformance()
{
    if (!renderPerformance)
    {
        return;
    }

    ImGui::SetNextWindowPos(ImVec2(300, 25), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Performance", &renderPerformance, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Emittance map: %s", FormatBytes(voxelShared.emittanceNumBytes));
        ImGui::Text("Opacity   map: %s", FormatBytes(voxelShared.opacityNumBytes));
        ImGui::Text("Normal    map: %s", FormatBytes(voxelShared.normalNumBytes));

        ImGui::Text("Rendered  triangles: %d", renderStats.numRenderedTriangles);
        ImGui::Text("Voxelized triangles: %d", renderStats.numVoxelizedTriangles);

        SelectSwapInterval();

        ImGui::Separator();
        if (ImGui::TreeNodeEx("CPU timings", ImGuiTreeNodeFlags_DefaultOpen))
        {
            RenderCPUTimings();
            ImGui::TreePop();
            ImGui::Separator();
        }

        if (ImGui::TreeNodeEx("GPU timings", ImGuiTreeNodeFlags_DefaultOpen))
        {
            RenderGPUTimings();
            ImGui::TreePop();
            ImGui::Separator();
        }
    }
    ImGui::End();
}

static void RenderHelp()
{
    if (!renderHelp)
    {
        return;
    }

    const int flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize;
    // ImGuiCond_FirstUseEver
    // ImGuiCond_Appearing
    ImGui::SetNextWindowPos(ImVec2(r_videoConfig.width - 240, 25), 0);
    if (ImGui::Begin("Help", &renderHelp, flags))
    {
        ImGui::Text("Key Mapping");
        ImGui::Text("ESC   Quit");
        ImGui::Text("F1    Toggle help window");
        ImGui::Text("F2    Toggle settings window");
        ImGui::Text("F3    Toggle performance window");
        ImGui::Text("F4    Toggle materials window");
        ImGui::Text("N     Toggle normal mapping");
        ImGui::Text("G     Toggle GUI rendering");
        ImGui::Text("F     Toggle full-screen mode");
        ImGui::Text("V     Toggle voxel viz");
        ImGui::Text("O     Toggle voxel viz mode");
        ImGui::Text("      (opacity or emittance)");
    }
    ImGui::End();
}

static void VoxelGridComboBox(const char* name, u32* dim)
{
    const int options[] = { 256, 128, 64, 32 };
    if (ImGui::BeginSmallCombo(name, fmt("%d", (int)*dim), 0))
    {
        for (u32 n = 0; n < ARRAY_LEN(options); ++n)
        {
            const char* option = fmt("%d", options[n]);
            int optionValue = options[n];
            bool selected = optionValue == (int)*dim;
            if (ImGui::Selectable(option, selected))
            {
                *dim = optionValue;
            }
        }
        ImGui::EndCombo();
    }
}

#if defined(_DEBUG)
static void InflateAABB(RenderAABB* aabb, const char* nameP, const char* nameN, int axis, float delta)
{
    if (ImGui::Button(nameP))
    {
        aabb->min[axis] -= delta;
        aabb->max[axis] += delta;
    }
    ImGui::SameLine();
    if (ImGui::Button(nameN))
    {
        aabb->min[axis] += delta;
        aabb->max[axis] -= delta;
    }
    ImGui::SameLine();
    if (ImGui::Button(fmt("%s min", nameP)))
        aabb->min[axis] += delta;
    ImGui::SameLine();
    if (ImGui::Button(fmt("%s min", nameN)))
        aabb->min[axis] -= delta;
    ImGui::SameLine();
    if (ImGui::Button(fmt("%s max", nameP)))
        aabb->max[axis] += delta;
    ImGui::SameLine();
    if (ImGui::Button(fmt("%s max", nameN)))
        aabb->max[axis] -= delta;
}
#endif

static void RenderSettings(RenderCommandQueue* cmdQueue, SceneAssets* assets)
{
    if (!renderSettings)
    {
        return;
    }

    ImGui::SetNextWindowPos(ImVec2(10, 25), ImGuiCond_FirstUseEver);
    ImGui::Begin("Settings", &renderSettings, ImGuiWindowFlags_AlwaysAutoResize);

    if (ImGui::CollapsingHeader("Rendering Pipeline"), ImGuiTreeNodeFlags_DefaultOpen)
    {
        if (ImGui::TreeNodeEx("Debug visualization", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("Draw AABB", &r_backendFlags.drawAABB);
            ImGui::Checkbox("Draw lights", &r_backendFlags.drawLights);
            ImGui::TreePop();
            ImGui::Separator();
        }

        if (ImGui::TreeNodeEx("Voxelization", ImGuiTreeNodeFlags_DefaultOpen))
        {
            static uint3_t localGridSize = voxelShared.gridSize;
            if (ImGui::Button("Preset: 128x64x64"))
            {
                localGridSize = { 128, 64, 64 };
            }
            if (ImGui::Button("Preset:  64x32x32"))
            {
                localGridSize = { 64, 32, 32 };
            }
            if (ImGui::Button("Preset:  32x16x16"))
            {
                localGridSize = { 32, 16, 16 };
            }
            VoxelGridComboBox("X", &localGridSize.w);
            VoxelGridComboBox("Y", &localGridSize.h);
            VoxelGridComboBox("Z", &localGridSize.d);

            static int localNumBounces = (int)voxelShared.numBounces;
            ImGui::SmallSliderInt("Bounces", &localNumBounces, 1, 8);

            if (ImGui::Button("Apply"))
            {
                voxelShared.numBounces = (u32)localNumBounces;
                voxelShared.gridSize = localGridSize;
                Voxel_SettingsChanged();
            }

            ImGui::Separator();

            ImGui::Checkbox("Gap Filling", &r_backendFlags.useGapFilling);
            s32 maxGapDistance = voxelShared.maxGapDistance;
            ImGui::SliderInt("Max Gap Distance", &maxGapDistance, 3, 200);
            voxelShared.maxGapDistance = maxGapDistance;

            s32 localMode = r_backendFlags.consRasterMode;

            ImGui::Text("Conservative rasterization:");
            ImGui::RadioButton("Disabled", &localMode, ConsRasterMode::Disabled);
            ImGui::SameLine();
            ImGui::RadioButton("Software", &localMode, ConsRasterMode::Software);

            if (rendererInfo.consRasterInfo.available && rendererInfo.consRasterInfo.tier >= 2)
            {
                ImGui::SameLine();
                ImGui::RadioButton("Hardware", &localMode, ConsRasterMode::Hardware);
            }

            r_backendFlags.consRasterMode = (ConsRasterMode::Type)localMode;

            ImGui::Checkbox("Use low-res scene", &r_backendFlags.voxelizeLowRes);

            ImGui::TreePop();
            ImGui::Separator();
        }

        if (ImGui::TreeNodeEx("Deferred Shading", ImGuiTreeNodeFlags_DefaultOpen))
        {
            RenderDeferredShading();
            ImGui::TreePop();
            ImGui::Separator();
        }

        if (ImGui::TreeNodeEx("Voxel visualization", ImGuiTreeNodeFlags_DefaultOpen))
        {
            RenderVoxelViz();
            ImGui::TreePop();
            ImGui::Separator();
        }
    }

    if (ImGui::CollapsingHeader("Lights", ImGuiTreeNodeFlags_DefaultOpen))
    {
        RenderLightEditor(assets);
    }

#if defined(_DEBUG)
    if (ImGui::CollapsingHeader("Scene AABB", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::SliderFloat3("Min", cmdQueue->aabb.min.v, -10666.0f, 10666.0f);
        ImGui::SliderFloat3("Max", cmdQueue->aabb.max.v, -10666.0f, 10666.0f);
        //ImGui::SliderFloat3("Min", assets->meshes[assets->current_asset].aabb.min.v, -10666.0f, 10666.0f);
        //ImGui::SliderFloat3("Max", assets->meshes[assets->current_asset].aabb.max.v, -10666.0f, 10666.0f);
        RenderAABB* aabb = &assets->meshes[assets->current_asset].aabb;
        const float delta = 20.0f;
        ImGui::PushButtonRepeat(true);
        InflateAABB(aabb, "+X", "-X", 0, delta);
        InflateAABB(aabb, "+Y", "-Y", 1, delta);
        InflateAABB(aabb, "+Z", "-Z", 2, delta);
        ImGui::PopButtonRepeat();
        ImGui::Separator();
    }
#endif

#if defined(_DEBUG)
    //BisectMethod(); // not currently usable

    if (ImGui::CollapsingHeader("Scene configuration", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::TreeNodeEx("Assets", ImGuiTreeNodeFlags_DefaultOpen))
        {
            RenderAssets(cmdQueue->assets);
            RenderTransform(cmdQueue);
            ImGui::TreePop();
            ImGui::Separator();
        }
    }
#else
   // cmdQueue->modelMatrix = Identity();
#endif

    ImGui::NewLine();

    ImGui::End();
}

static void EditMaterial(Scene* scene, Material* material, u32 materialIndex)
{
    ImGui::SmallSliderFloat("Specular Exponent", &material->specular.w, 0.01f, 10000.0f, "%.2f", ImGuiSliderFlags_None);
    ImGui::ColorEdit3("Specular Color", (f32*)&material->specular.x);
    ImGui::ColorEdit4("Alpha Tested Color", material->alphaTestedColor.v);

    s32 flags = material->flags;

    ImGui::Text("Flags");
    ImGui::CheckBox("Is Alpha Tested", &flags, IS_ALPHA_TESTED);
    ImGui::CheckBox("Is Emissive", &flags, IS_EMISSIVE);

    material->flags = flags;

    // Albedo selection
    static bool isPicking = false;
    if(ImGui::Button("Albedo Texture"))
    {
        isPicking = !isPicking;
    }
    if (isPicking)
    {
        if (ImGui::Begin("Picker", &isPicking, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize))
        {
            ImGui::SetWindowSize({ -FLT_MIN, 250 });
            ImGui::SetWindowPos(ImGui::GetMousePos(), ImGuiCond_FirstUseEver);
            if (ImGui::BeginTable("split", 4, ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings))
            {
                u8* begin = assetsShared.oldStrings.base_ptr;
                u8* end = assetsShared.oldStrings.base_ptr + assetsShared.oldStrings.mem_used;
                {
                    while (++begin != end)
                    {
                        const char* string = (const char*)begin;
                        u32 sLen = strlen(string);
                        
                        unsigned long hash = HashTextureName(string);
                        Image* hTexture = assetsShared.textureMap.TryGet(hash);
                        if (hTexture != NULL)
                        {
                            void* texView = (void*)assetsShared.textureViews[hTexture->index];
                            if (ImGui::ImageButton(texView, { 50, 50 }))
                            {

                                material->textureIndex[TextureId::Albedo] = hTexture->index;
                                scene->fileMaterials[materialIndex].albedoOffset = PushString(&assetsShared.newStrings, string);
                                isPicking = false;
                            }
                            ImGui::TableNextColumn();
                        }

                        if (sLen > 0)
                        {
                            begin += sLen;
                        }
                    }
                }
                ImGui::EndTable();
            }
            ImGui::End();
        }
    }
}

static void RenderMaterials(Scene* scene)
{
    if (!renderMaterials || scene->materials.Length() == 0)
    {
        return;
    }

    static s32 selectedMaterial = 0;

    ImGui::Begin("Materials", &renderMaterials, ImGuiWindowFlags_None);
    
    // Preview
    {
        void* texView = (void*)assetsShared.textureViews[scene->materials[selectedMaterial].textureIndex[TextureId::Albedo]];
        ImGui::Image(texView, { 250, 250 });
    }

    ImGui::SameLine();

    // Selection Window
    {
        ImGui::BeginChild("ChildR", { 0, 300 }, true, ImGuiWindowFlags_None);
        if (ImGui::BeginTable("split", 1, ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings))
        {
            for (int i = 1; i < scene->fileMaterials.Length(); i++)
            {
                ImGui::TableNextColumn();
                if (ImGui::Button(fmt("%s", (const char*)scene->strings.base_ptr + scene->fileMaterials[i].materialOffset), ImVec2(-FLT_MIN, 0.0f)))
                {
                    selectedMaterial = i;
                }
            }
            ImGui::EndTable();
        }
        ImGui::EndChild();
    }

    ImGui::Separator();
    
    // Editor
    {
#if 0
        ImGui::BeginChild("ChildL", { ImGui::GetWindowContentRegionWidth() * 0.65f, 50}, false, ImGuiWindowFlags_None);
        EditMaterial(scene, &scene->materials[selectedMaterial], selectedMaterial);
        ImGui::EndChild();
#else
        ImGui::BeginGroup();
        EditMaterial(scene, &scene->materials[selectedMaterial], selectedMaterial);
        ImGui::EndGroup();
#endif
    }

    ImGui::Separator();

    {
        if (ImGui::Button("Save To File"))
        {
            WriteBinaryMaterialToFile(scene, fmt("%s.material", scene->name));
        }
    }

    ImGui::End();
}

void R_RenderImGui(RenderCommandQueue* cmdQueue, SceneAssets* assets, f32 dt)
{
    ImGui::NewFrame();
    ImGui_Init();

    static bool renderGUI = true;
    if (ImGui::IsKeyPressed('G', false))
    {
        renderGUI = !renderGUI;
    }

    if (ImGui::IsKeyPressed('V', false))
    {
        r_backendFlags.drawVoxelViz = !r_backendFlags.drawVoxelViz;
    }
    else if (ImGui::IsKeyPressed('O', false))
    {
        voxelVizConstants.cst_singleChannel = !voxelVizConstants.cst_singleChannel;
    }
    else if (ImGui::IsKeyPressed('N', false))
    {
        r_backendFlags.useNormalMap = !r_backendFlags.useNormalMap;
    }
    else if (ImGui::IsKeyPressed('Q', false))
    {
        r_backendFlags.useGapFilling = !r_backendFlags.useGapFilling;   
    }
    else if (ImGui::IsKeyPressed(0x70, false))
    {
        renderHelp = !renderHelp;
        if (!renderGUI)
        {
            renderGUI = true;
            renderHelp = true;
        }
    }
    else if (ImGui::IsKeyPressed(0x71, false))
    {
        renderSettings = !renderSettings;
    }
    else if (ImGui::IsKeyPressed(0x72, false))
    {
        renderPerformance = !renderPerformance;
    }
    else if (ImGui::IsKeyPressed(0x73, false))
    {
        renderMaterials = !renderMaterials;
    }

    if (!renderGUI)
    {
        ImGui::Render();
        return;
    }

#if defined(_DEBUG)
    if (ImGui::IsKeyPressed('P', false))
    {
        const char* rasterModesString[ConsRasterMode::Count] = { "Off", "Software", "Hardware" };
        const char* onOffString[2] = { "Off", "On" };
        const char* shadowModeString[3] = { "Off", "PCSS", "PCF" };

        FILE* file = fopen("./stats.txt", "a+");
        fputs("=====================================\n", file);
        fputs("Options: \n", file);
        fprintf(file, "Grid Size: %dx%dx%d\n", voxelShared.gridSize.w, voxelShared.gridSize.h, voxelShared.gridSize.d);
        fprintf(file, "Raster Mode: %s\n", rasterModesString[r_backendFlags.consRasterMode]);

        s32 shadowMode = !!(r_backendFlags.deferredOptions & DEFERRED_SHADOWS_PCSS) ? 1 : (!!(r_backendFlags.deferredOptions & DEFERRED_SHADOWS_PCF) != 0 ? 2 : 0);

        fprintf(file, "Shadow Filtering Mode: %s\n", shadowModeString[shadowMode]);
        fprintf(file, "Indirect Diffuse: %s\n", onOffString[!!(r_backendFlags.deferredOptions & DEFERRED_INDIRECT_DIFFUSE)]);
        fprintf(file, "Indirect Specular: %s\n", onOffString[!!(r_backendFlags.deferredOptions & DEFERRED_INDIRECT_SPECULAR)]);
        fprintf(file, "Ambient Occlusion: %s\n", onOffString[!!(r_backendFlags.deferredOptions & DEFERRED_AMBIENT_OCCLUSION)]);
        fprintf(file, "Number of Bounces: %d\n", (int)voxelShared.numBounces);

        fputs("\n\n", file);

        fputs("Timings:\n", file);
        fprintf(file, "Opacity Voxelization: %.2f\n", d3d.frameQueries[QueryId::OpacityVoxelization].medianUS / 1000.0f);
        fprintf(file, "Opacity Mip-mapping: %.2f\n\n", d3d.frameQueries[QueryId::OpacityMipMapping].medianUS / 1000.0f);

        fprintf(file, "Emittance Voxelization: %.2f\n", d3d.frameQueries[QueryId::EmittanceVoxelization].medianUS / 1000.0f);
        fprintf(file, "Emittance Format Fix: %.2f\n", d3d.frameQueries[QueryId::EmittanceFormatFix].medianUS / 1000.0f);
        fprintf(file, "Emittance Opacity Fix: %.2f\n", d3d.frameQueries[QueryId::EmittanceOpacityFix].medianUS / 1000.0f);
        fprintf(file, "Emittance Bounce: %.2f\n", d3d.frameQueries[QueryId::EmittanceBounce].medianUS / 1000.0f);
        fprintf(file, "Emittance Mip-mapping: %.2f\n\n", d3d.frameQueries[QueryId::EmittanceMipMapping].medianUS / 1000.0f);

        fprintf(file, "Deferred Shading: %.2f\n", d3d.frameQueries[QueryId::DeferredShading].medianUS / 1000.0f);
        fprintf(file, "Geometry Pass: %.2f\n\n", d3d.frameQueries[QueryId::GeometryPass].medianUS / 1000.0f);

        fprintf(file, "Frame Total: %.2f\n\n", d3d.frameQueries[QueryId::FullFrame].medianUS / 1000.0f);

        fclose(file);
    }
#endif

    gui_state.time_info.frame_dt = dt;

    // ImGuiDockNodeFlags_NoResize
    int dockFlags = ImGuiDockNodeFlags_NoDockingInCentralNode | ImGuiDockNodeFlags_PassthruCentralNode;
    ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), dockFlags);

    RenderMenuBar();

    RenderPerformance();

    RenderHelp();

    RenderSettings(cmdQueue, assets);

    RenderMaterials(&assets->meshes[0]);

    ImGui::Render();
}