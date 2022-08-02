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

#pragma once
#include "../common/shared.h"
#include "../shaders/shader_constants.hlsli"
#include "../scene/s_public.h"

struct VideoConfig
{
    int height, width;
};

extern VideoConfig r_videoConfig;

struct ConsRasterMode
{
    enum Type
    {
        Disabled,
        Software,
        Hardware,
        Count
    };
};

struct RenderBackendFlags
{
    bool drawAABB;
    bool drawLights;
    bool drawVoxelViz;
    bool drawDeferredShading;
    bool drawTextures;
    ConsRasterMode::Type consRasterMode;
    bool useNormalMap;
    f32 normalStrength;
    s32 maxAnisotropy;
    s32 deferredOptions;
    bool voxelizeLowRes;
    bool useGapFilling;
    bool shouldUpdateDirectLight;
};

extern RenderBackendFlags r_backendFlags;

void R_Init(void* handle, MemoryPools* memory);
void R_ShutDown();
void R_DrawFrame(ClientInput* input);
void R_WindowSizeChanged();

//
// shared with scene
//

struct TextureId
{
    enum Type
    {
        Ambient,
        Albedo,
        Specular,
        SpecularHighlight,
        Alpha,
        Bump,
        Displacement,
        Stencil,
        Count
    };
};
extern Image defaultTextures[TextureId::Count];

#pragma pack(push, 1)
struct MeshFileHeader
{
    u32 numVertexes;
    u32 numIndexes;
    u32 numMaterials;
    u32 numMeshes;
    u32 numStringBytes;
    vec3_t aabbMin;
    vec3_t aabbMax;
};

struct MeshFileMaterial
{
    // offsets into strings are in bytes
    // ~0 if unused
    u32 albedoOffset;
    u32 normalOffset;
    u32 specularOffset;
    u32 materialOffset;
    vec3_t specularColor;
    vec4_t alphaTestedColor; // average color (rgb) + opacity
    f32 specularExponent;
    u32 flags;
};

struct MeshFileMesh
{
    u32 materialIndex;
    u32 firstIndex;
    u32 numIndexes;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct MaterialFileHeader
{
    u32 numMaterials;
    u32 numStringBytes;
};

struct MaterialFileData
{
    u32 albedoOffset;
    u32 normalOffset;
    u32 specularOffset;
    //u32 materialOffset;

    vec3_t specularColor;
    f32 specularExponent;
    u32 isAlphaTested;
};
#pragma pack(pop)

struct Material
{
    u32 textureIndex[TextureId::Count];
    u32 flags;
    vec4_t specular; // w exponent, xyz color
    vec4_t alphaTestedColor; // rgb + opacity
};

struct RenderAABB
{
    vec3_t min;
    vec3_t max;
};

struct Scene
{
    char name[256];
    u32 meshId;
    bool valid;

    RenderAABB aabb;
    DynamicArray<vec3_t> xyz;
    DynamicArray<vec2_t> tc;
    DynamicArray<vec3_t> normal;
    DynamicArray<u32> indexes;
    DynamicArray<MeshFileMaterial> fileMaterials;
    DynamicArray<MeshFileMesh> meshes;
    DynamicArray<Material> materials;
    MemoryArena strings;
};

struct Light
{
    vec3_t position;
    vec3_t dir;
    vec4_t color;
    f32 zNear;
    f32 zFar;
    f32 sizeUV;
    f32 radius;
    f32 umbraAngle;
    f32 penumbraAngle;
    f32 azimuth;
    f32 inclination;
    m4x4 viewMatrix;
    m4x4 projMatrix;
};

struct RenderEntry
{
    enum Type
    {
        Scene,
        Box,
        BeginDebugRegion,
        EndDebugRegion,
        Count,
    };
};

struct RenderEntryHeader
{
    RenderEntry::Type type;
};

struct RenderEntryScene
{
    RenderEntryHeader hdr;
    Scene* scene;
    Scene* sceneLowRes;
};

struct RenderEntryBox
{
    RenderEntryHeader hdr;
    vec3_t min;
    vec3_t max;
    vec4_t color;
};

struct RenderEntryBeginDebugRegion
{
    RenderEntryHeader hdr;
    const wchar_t* name;
};

struct RenderEntryEndDebugRegion
{
    RenderEntryHeader hdr;
};

struct SceneAssets
{
    MemoryArena arena;

    DynamicArray<Light> lights;

    Scene* meshes;
    u32 numMeshes;

    u32 current_asset;
};

struct RenderCommandQueue
{
    size_t max_size;
    size_t size;
    u8* base_ptr;

    SceneAssets* assets;

    // object info
    u32 obj_flags;

    // backend globals
    m4x4 modelViewMatrix; // @TODO: make sure to init to identity matrix
    m4x4 invViewMatrix;
    m4x4 projectionMatrix;
    m4x4 invProjectionMatrix;
    m4x4 invViewProjectionMatrix;
    RenderAABB aabb;

    // Camera
    vec2_t ab;
    vec3_t viewVector;
    vec3_t cameraPosition;

    Light lights[MAX_LIGHTS];
    u32 lightCount;
};

//
// render command queue
//

void R_RenderCommandQueueToOutput(RenderCommandQueue* cmdQueue);
void R_RenderImGui(RenderCommandQueue* cmdQueue, SceneAssets* assets, f32 dt);

void R_PushBox(RenderCommandQueue* cmdQueue, vec4_t color, vec3_t min, vec3_t max);
void R_PushMesh(RenderCommandQueue* cmdQueue, Scene* asset, Scene* assetLowRes);
void R_PushBeginDebugRegion(RenderCommandQueue* cmdQueue, const wchar_t* name);
void R_PushEndDebugRegion(RenderCommandQueue* cmdQueue);
