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

#include "../../common/shared.h"
#include "../../scene/s_public.h"
#include "../../shaders/material_flags.hlsli"

struct ParseMaterial
{
    char name[MAX_PATH];
    vec3_t Ka; // Ambient color
    vec3_t Kd; // Diffuse color
    vec3_t Ks; // Specular color
    vec3_t Ke; // Emissive
    f32 Ns; // Specular exponent
    f32 Tr; // Dissolve or inverted dissovle Tr = (1 - d)
    f32 d;
    vec3_t Tf; // Transmission Filter Color
    f32 Ni; // Index of refraction
    s32 Illum; // Illumination
    u32 isAlphaTested;
    Image maps[TextureId::Count];
    char* mapPaths[TextureId::Count];
    u32 textureIndex[TextureId::Count];
};

// Each member is the index in the corresponding vertex/tc/normal buffers.
struct Vertex
{
    u32 xyz;
    u32 tc;
    u32 normal;
};

struct MaterialGroup
{
    u32 materialIndex;
    u32 numIndexes;
};

// A group consists of a name, face amount, offsets in the mesh.
struct ObjectGroup
{
    char* name;

    u32 numFaces;
    u32 faceOffset; // where does this group start in the face_vertices buffer;
    u32 indexOffset; // where does this group start in the index buffer;
    u32 materialOffset; // which material does this group of vertices use

    u32 vertexOffset; // indicates the end of vertices in this group
    u32 normalOffset; // indicates the end of normals in this group
    MaterialGroup materialGroups[64];
    u32 numMaterialGroups;
    DynamicArray<u32> sgroupIndices;
};

// Describes and object with a name that is loaded.
struct Object
{
    const char* fileName;

    // Per vertex data
    DynamicArray<vec3_t> xyz;
    DynamicArray<vec3_t> normals;
    DynamicArray<vec2_t> tc;

    DynamicArray<Vertex> vertexes;

    // Per face data
    DynamicArray<u32> faceVertexCount; // Specifies the amount of vertices in the face
    DynamicArray<u32> faceSGroupIndices; // Smoothing group index for the face

    // Materials
    DynamicArray<ParseMaterial> materials;

    // Groups
    DynamicArray<ObjectGroup> groups;

    // AABB
    vec3_t min;
    vec3_t max;
};

struct ObjectData
{
    char objPath[MAX_PATH];
    Object* m; // the mesh
    ObjectGroup currentGroup;
    u32 currentSgroup;
};

struct SGroup
{
    u32 firstIndex;
    u32 numIndexes;
};

struct Mesh
{
    const char* name;
    u32 meshId;

    RenderAABB aabb;
    DynamicArray<vec3_t> xyz;
    DynamicArray<vec2_t> tc;
    DynamicArray<vec3_t> normal;
    DynamicArray<u32> indexes;
    DynamicArray<ParseMaterial> materials;
    DynamicArray<ObjectGroup> groups;
    DynamicArray<SGroup> sgroups;
};

void LoadObject(Mesh* mesh, MemoryArena* arena, void* data, u64 size, const char* name, const char* objPath);
void WriteBinaryMeshToFile(Mesh* mesh, const char* filePath);
void WriteBinaryMaterialToFile(Mesh* mesh, const char* filePath);