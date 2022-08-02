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

#include "shared.h"

void WriteBinaryMaterialToFile(Mesh* mesh, const char* filePath)
{
    FILE* file = fopen(filePath, "wb");
    if (!file)
    {
        Sys_FatalError("WriteBinaryMaterialToFile: failed to open file");
    }

    DynamicArray<MeshFileMaterial> materials;
    MemoryArena strings;
    AllocateArena(&strings, Kilobytes(4), malloc(Kilobytes(4)), "string arena");
    strings.mem_used = 1; // 0 offset for null or invalid pointers
    for (u32 m = 0; m < mesh->materials.Length(); ++m)
    {
        MeshFileMaterial material;
        material.albedoOffset = PushString(&strings, mesh->materials[m].mapPaths[TextureId::Albedo]);
        material.normalOffset = PushString(&strings, mesh->materials[m].mapPaths[TextureId::Bump]);
        material.specularOffset = PushString(&strings, mesh->materials[m].mapPaths[TextureId::Specular]);
        material.materialOffset = PushString(&strings, mesh->materials[m].name);
        Vec3Copy(material.specularColor, mesh->materials[m].Ks);
        material.specularExponent = mesh->materials[m].Ns;
        material.flags |= mesh->materials[m].isAlphaTested ? IS_ALPHA_TESTED : 0;
        Vec3Copy(material.alphaTestedColor, mesh->materials[m].Kd);
        material.alphaTestedColor.w = mesh->materials[m].d;
        materials.Push(material);
    }

    MaterialFileHeader hdr;
    hdr.numMaterials = materials.Length();
    hdr.numStringBytes = strings.mem_used;

    fwrite(&hdr, sizeof(hdr), 1, file);
    fwrite(materials.GetStart(), materials.UsedBytes(), 1, file);
    fwrite(strings.base_ptr, strings.mem_used, 1, file);

    fclose(file);
}

void WriteBinaryMeshToFile(Mesh* mesh, const char* filePath)
{
    FILE* file = fopen(filePath, "wb");
    if (!file)
        Sys_FatalError("Couldn't write to binary file.");

    DynamicArray<MeshFileMesh> meshes;
    u32 indexOffset = 0;
    for (u32 groupIndex = 0; groupIndex < mesh->groups.Length(); ++groupIndex)
    {
        ObjectGroup objGroup = mesh->groups[groupIndex];
        for (u32 materialGroupIndex = 0; materialGroupIndex < mesh->groups[groupIndex].numMaterialGroups; ++materialGroupIndex)
        {
            MaterialGroup* materialGroup = &mesh->groups[groupIndex].materialGroups[materialGroupIndex];

            if (materialGroup->numIndexes == 0)
            {
                continue; // @TODO: why did we suddenly get meshes with 0 indexes???
            }

            MeshFileMesh mesh;
            mesh.materialIndex = materialGroup->materialIndex;
            mesh.firstIndex = indexOffset;
            mesh.numIndexes = materialGroup->numIndexes;
            meshes.Push(mesh);
            indexOffset += mesh.numIndexes;
        }
    }

    MeshFileHeader header;
    header.numVertexes = mesh->xyz.Length();
    header.numIndexes = mesh->indexes.Length();
    header.numMeshes = meshes.Length();
    header.numMeshes = meshes.Length();
    header.aabbMin = mesh->aabb.min;
    header.aabbMax = mesh->aabb.max;

    fwrite(&header, sizeof(header), 1, file);
    fwrite(mesh->xyz.GetStart(), mesh->xyz.UsedBytes(), 1, file);
    fwrite(mesh->normal.GetStart(), mesh->normal.UsedBytes(), 1, file);
    fwrite(mesh->tc.GetStart(), mesh->tc.UsedBytes(), 1, file);
    fwrite(mesh->indexes.GetStart(), mesh->indexes.UsedBytes(), 1, file);
    fwrite(meshes.GetStart(), meshes.UsedBytes(), 1, file);
    fwrite(meshes.GetStart(), meshes.UsedBytes(), 1, file);

    fclose(file);
}