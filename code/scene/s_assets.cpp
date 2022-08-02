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

#include "../renderer/r_public.h"
#include "s_private.h"

Image defaultTextures[TextureId::Count];

static void CreateTextureImageWithColor(Image* image, u32 color, const char* name)
{
    image->fileName = strdup(name);
    image->width = 1;
    image->height = 1;
    image->channels = 4;
    image->data = malloc((size_t)image->width * image->height * image->channels);

    u32 bytesPerPixel = 4;
    u32 pitch = image->width * image->channels;

    u8* row = ((u8*)image->data);
    for (s32 y = 0; y < image->height; ++y)
    {
        u32* pixel = (u32*)row;
        for (s32 x = 0; x < image->width; ++x)
        {
            *pixel = color;
        }

        row += pitch;
    }
}

static void AllocateDefaultTextures()
{
    static bool defaultsLoaded = false;
    if (!defaultsLoaded)
    {
        CreateTextureImageWithColor(&defaultTextures[TextureId::Ambient], 0xFFFFFFFF, "default ambient");
        CreateTextureImageWithColor(&defaultTextures[TextureId::Albedo], 0x00FF00FF, "default albedo");
        CreateTextureImageWithColor(&defaultTextures[TextureId::Specular], 0xFFFFFFFF, "default specular");
        CreateTextureImageWithColor(&defaultTextures[TextureId::SpecularHighlight], 0xFF000000, "default specular highlight");
        CreateTextureImageWithColor(&defaultTextures[TextureId::Alpha], 0xFF000000, "default alpha");
        CreateTextureImageWithColor(&defaultTextures[TextureId::Bump], 0xFFFF8080, "default normal");
        CreateTextureImageWithColor(&defaultTextures[TextureId::Displacement], 0xFF000000, "default displacement");
        CreateTextureImageWithColor(&defaultTextures[TextureId::Stencil], 0xFF000000, "default stencil");
        defaultsLoaded = true;
    }
}

static void ReadBinaryMaterialFromFile(Scene* mesh, const char* filePath)
{
    FILE* file = fopen(filePath, "rb");
    if (!file)
        Sys_FatalError("Couldn't read binary file.");

    fseek(file, 0, SEEK_SET);
    MaterialFileHeader header;
    fread(&header, sizeof(header), 1, file);

    mesh->fileMaterials.Reserve(header.numMaterials);
    AllocateArena(&mesh->strings, header.numStringBytes + 1, malloc(header.numStringBytes + 1), "string arena");

    fread(mesh->fileMaterials.GetStart(), sizeof(MeshFileMaterial) * header.numMaterials, 1, file);
    fread(PushSize(&mesh->strings, header.numStringBytes), header.numStringBytes, 1, file);

    fclose(file);
}

static void ReadBinaryMeshFromFile(Scene* mesh, const char* filePath)
{
    FILE* file = fopen(filePath, "rb");
    if (!file)
        Sys_FatalError("Couldn't read binary file.");

    fseek(file, 0, SEEK_SET);
    MeshFileHeader header;

    fread(&header, sizeof(header), 1, file);

    mesh->aabb.min = header.aabbMin;
    mesh->aabb.max = header.aabbMax;

    mesh->xyz.Reserve(header.numVertexes);
    mesh->normal.Reserve(header.numVertexes);
    mesh->tc.Reserve(header.numVertexes);
    mesh->indexes.Reserve(header.numIndexes);
    mesh->meshes.Reserve(header.numMeshes);

    fread(mesh->xyz.GetStart(), sizeof(vec3_t) * header.numVertexes, 1, file);
    fread(mesh->normal.GetStart(), sizeof(vec3_t) * header.numVertexes, 1, file);
    fread(mesh->tc.GetStart(), sizeof(vec2_t) * header.numVertexes, 1, file);
    fread(mesh->indexes.GetStart(), sizeof(u32) * header.numIndexes, 1, file);
    fread(mesh->meshes.GetStart(), sizeof(MeshFileMesh) * header.numMeshes, 1, file);

    fclose(file);

    return;
}

SceneAssets* AllocateEditorAssets(MemoryArena* arena, SceneTransientState* tranState, size_t size)
{
    SceneAssets* result = PushStruct(arena, SceneAssets);
    SubArena(&result->arena, arena, size, "Asset Arena");

    const char* fullPath = "../bachelor/assets/Sponza";

    u32 numFiles = 0;
    FolderScan* fs = Sys_FolderScan_Begin(fullPath, "*.scene");
    while (Sys_FolderScan_Next(NULL, NULL, fs))
    {
        numFiles++;
    }
    Sys_FolderScan_End(fs);

    result->numMeshes = numFiles;
    result->meshes = PushArray(&result->arena, result->numMeshes, Scene);

#if 1
    AllocateDefaultTextures();
#endif

    fs = Sys_FolderScan_Begin(fullPath, "*.scene");
    for (u32 fileIndex = 0; fileIndex < result->numMeshes; ++fileIndex)
    {
        const char* fileName;
        const char* filePath;
        if (!Sys_FolderScan_Next(&fileName, &filePath, fs))
        {
            Sys_FatalError("AllocateEditorAssets: Sys_FolderScan_Next failed");
        }

        Scene* scene = result->meshes + fileIndex;

        ReadBinaryMeshFromFile(scene, filePath);

        scene->meshId = 1 << fileIndex;
        strcpy(scene->name, fileName);
        char filePathNX[MAX_PATH];
        strcpy(filePathNX, filePath);
        StripFileExtension(filePathNX);
        
        ReadBinaryMeshFromFile(scene, filePath);
        ReadBinaryMaterialFromFile(scene, fmt("%s.material", filePathNX));

        scene->meshId = 1 << fileIndex;
        strcpy(scene->name, filePathNX);
        scene->valid = true;
    }
    Sys_FolderScan_End(fs);

    Scene* m = &result->meshes[0];
    Light e = {};

    vec3_t l = m->aabb.max - m->aabb.min;
    vec3_t center = m->aabb.min + l / 2;
    e.position = center;
    e.position.x = -190.0f;
    e.color = { 1.0f, 1.0f, 1.0f, 1.0f };
    e.radius = 1000.0f;
    e.sizeUV = 0.05f;
    e.zNear = 25.0f;
    e.zFar = e.radius;
    e.azimuth = 230.0f;
    e.inclination = 60.0f;
    e.dir.x = cos(TO_RADIANS(e.azimuth)) * sin(TO_RADIANS(e.inclination));
    e.dir.y = sin(TO_RADIANS(e.azimuth)) * sin(TO_RADIANS(e.inclination));
    e.dir.z = cos(TO_RADIANS(e.inclination));
    e.umbraAngle = TO_DEGREES(1.334f);
    e.penumbraAngle = TO_DEGREES(0.175f);
    result->lights.Push(e);

    e.position.x = -500.0f;
    e.position.y = 720;
    e.position.z = -24;
    e.color = { 1.0f, 0.0f, 0.0f, 1.0f };
    e.azimuth = 230.0f;
    e.inclination = 135.0f;
    e.dir.x = cos(TO_RADIANS(e.azimuth)) * sin(TO_RADIANS(e.inclination));
    e.dir.y = sin(TO_RADIANS(e.azimuth)) * sin(TO_RADIANS(e.inclination));
    e.dir.z = cos(TO_RADIANS(e.inclination));
    result->lights.Push(e);

    return result;
}

Scene* GetLoadedMesh(SceneAssets* assets, u32 assetID)
{
    assert(assetID < assets->numMeshes);
    Scene* result = &assets->meshes[assetID];
    return result;
}
