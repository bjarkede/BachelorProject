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

#define DDSKTX_IMPLEMENT
#include "../dds-ktx/dds-ktx.h"

AssetsSharedData assetsShared;

unsigned long HashTextureName(const char* str)
{
    unsigned long hash = 5381;
    int c;

    while (c = *str++)
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

static void CreateTexture(ID3D11Texture2D** tex, ID3D11ShaderResourceView** texSRV, ddsktx_texture_info* tc, void* fileData, s32 size, const char* fileName)
{
    DXGI_FORMAT format;
    switch (tc->format)
    {
    case DDSKTX_FORMAT_BC7:
        format = DXGI_FORMAT_BC7_UNORM;
        break;
    case DDSKTX_FORMAT_BC5:
        format = DXGI_FORMAT_BC5_UNORM;
        break;
    case DDSKTX_FORMAT_BC4:
        format = DXGI_FORMAT_BC4_UNORM;
        break;
    default:
        assert(0);
    }

    D3D11_TEXTURE2D_DESC texDesc;
    ZeroMemory(&texDesc, sizeof(texDesc));
    texDesc.ArraySize = 1;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags = 0;
    texDesc.Format = format;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.Width = tc->width;
    texDesc.Height = tc->height;
    texDesc.MipLevels = 0;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.MiscFlags = 0;

    *tex = CreateTexture2D(&d3d.persistent, &texDesc, NULL, fmt("%s", fileName));

    D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc;
    ZeroMemory(&viewDesc, sizeof(viewDesc));
    viewDesc.Format = texDesc.Format;
    viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    viewDesc.Texture2DArray.MipLevels = -1;
    viewDesc.Texture2DArray.ArraySize = 1;
    viewDesc.Texture2DArray.MostDetailedMip = 0;

    *texSRV = CreateShaderResourceView(&d3d.persistent, *tex, &viewDesc, fmt("%s", fileName));

    for (int m = 0; m < tc->num_mips; ++m)
    {
        ddsktx_sub_data subData;
        ddsktx_get_sub(tc, &subData, fileData, size, 0, 0, m);

        d3ds.context->UpdateSubresource(*tex, m, NULL, subData.buff, subData.row_pitch_bytes, 0);
    }
}

static void CreateTexture(ID3D11Texture2D** tex, ID3D11ShaderResourceView** texSRV, Image* image)
{
    D3D11_TEXTURE2D_DESC texDesc;
    ZeroMemory(&texDesc, sizeof(texDesc));
    texDesc.ArraySize = 1;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    texDesc.CPUAccessFlags = 0;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.Width = image->width;
    texDesc.Height = image->height;
    texDesc.MipLevels = 0;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

    *tex = CreateTexture2D(&d3d.persistent, &texDesc, NULL, fmt("%s", image->fileName));

    D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc;
    ZeroMemory(&viewDesc, sizeof(viewDesc));
    viewDesc.Format = texDesc.Format;
    viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    viewDesc.Texture2DArray.MipLevels = -1;
    viewDesc.Texture2DArray.ArraySize = 1;
    viewDesc.Texture2DArray.MostDetailedMip = 0;

    *texSRV = CreateShaderResourceView(&d3d.persistent, *tex, &viewDesc, fmt("%s", image->fileName));

    D3D11_SUBRESOURCE_DATA sr;
    sr.pSysMem = image->data;
    sr.SysMemPitch = image->width * 4;
    sr.SysMemSlicePitch = image->height * image->width * 4;

    d3ds.context->UpdateSubresource(*tex, 0, 0, sr.pSysMem, sr.SysMemPitch, sr.SysMemSlicePitch);
    d3ds.context->GenerateMips(*texSRV);
}

void AllocateMeshTextures(Scene* mesh)
{
    u64 timestampBegin = Sys_GetTimestamp();

    assert(assetsShared.numTextures + TextureId::Count <= MAX_TEXTURES);
    for (u32 textureIndex = 0; textureIndex < TextureId::Count; ++textureIndex)
    {
        CreateTexture(&assetsShared.textures[assetsShared.numTextures], &assetsShared.textureViews[assetsShared.numTextures], &defaultTextures[textureIndex]);
        assetsShared.numTextures++;
    }

    if (assetsShared.newStrings.size == 0)
    {
        AllocateArena(&assetsShared.newStrings, Megabytes(4), malloc(Megabytes(4)), "string arena");
        void* stringRegion = PushSize(&assetsShared.newStrings, mesh->strings.mem_used);
        memcpy(stringRegion, mesh->strings.base_ptr, mesh->strings.mem_used);
        assetsShared.newStrings.mem_used = mesh->strings.mem_used;
    }

    if (assetsShared.oldStrings.size == 0)
    {
        AllocateArena(&assetsShared.oldStrings, Megabytes(4), malloc(Megabytes(4)), "string arena");
        void* stringRegion = PushSize(&assetsShared.oldStrings, mesh->strings.mem_used);
        memcpy(stringRegion, mesh->strings.base_ptr, mesh->strings.mem_used);
        assetsShared.oldStrings.mem_used = mesh->strings.mem_used;
    }

    char oldDir[MAX_PATH];
    GetCurrentDirectoryA(sizeof(oldDir), (LPSTR)&oldDir);
    char fullPath[MAX_PATH + 1];
    GetFullPathNameA("../bachelor/assets/Sponza", MAX_PATH, fullPath, NULL);
    strcat(fullPath, "/");
    SetCurrentDirectoryA(fullPath);

    for (u32 m = 0; m < mesh->fileMaterials.Length(); ++m)
    {
        MeshFileMaterial* material = &mesh->fileMaterials[m];
        Material newMaterial = {};
        Vec3Copy(newMaterial.specular, material->specularColor);
        newMaterial.specular.w = material->specularExponent;
        newMaterial.flags |= material->flags & IS_ALPHA_TESTED;
        Vec4Copy(newMaterial.alphaTestedColor, material->alphaTestedColor);

        u32 filePathOffset[TextureId::Count] = {};
        filePathOffset[TextureId::Albedo] = material->albedoOffset;
        filePathOffset[TextureId::Bump] = material->normalOffset;
        filePathOffset[TextureId::Specular] = material->specularOffset;

        for (u32 t = 0; t < TextureId::Count; ++t)
        {
            if (filePathOffset[t] == 0)
            {
                assert(assetsShared.numTextures < MAX_TEXTURES);
                assetsShared.textures[assetsShared.numTextures] = assetsShared.textures[t];
                assetsShared.textureViews[assetsShared.numTextures] = assetsShared.textureViews[t];
                newMaterial.textureIndex[t] = assetsShared.numTextures++;
                continue;
            }

            const char* filePath = (const char*)mesh->strings.base_ptr + filePathOffset[t];
            u32 tHash = HashTextureName(filePath); // hash the file name only
            filePath = fmt("textures/%s.dds", filePath); // now get the real file path
            Image* hTexture = assetsShared.textureMap.TryGet(tHash);
            static Image textures[MAX_TEXTURES] = {};
            static u32 numTextures = 0;
            if (hTexture == NULL)
            {
                // this helps diagnose invalid file names in debug builds
                assert(FileExists(filePath));

                void* fileData;
                s32 size;
                if (ReadEntireFile(&fileData, &size, filePath))
                {

                    ddsktx_texture_info tc = {};
                    if (ddsktx_parse(&tc, fileData, size, NULL))
                    {
                        Image* texture = &textures[numTextures];
                        texture->index = assetsShared.numTextures;
                        CreateTexture(&assetsShared.textures[assetsShared.numTextures], &assetsShared.textureViews[assetsShared.numTextures], &tc, fileData, size, filePath);
                        assetsShared.textureMap.Insert(tHash, texture);
                        numTextures += 1;
                    }
                    else
                    {
                        assetsShared.textures[assetsShared.numTextures] = assetsShared.textures[t];
                        assetsShared.textureViews[assetsShared.numTextures] = assetsShared.textureViews[t];
                    }

                    free(fileData);
                }
                else
                {
                    assetsShared.textures[assetsShared.numTextures] = assetsShared.textures[t];
                    assetsShared.textureViews[assetsShared.numTextures] = assetsShared.textureViews[t];
                }
            }
            else
            {
                assetsShared.textures[assetsShared.numTextures] = assetsShared.textures[hTexture->index];
                assetsShared.textureViews[assetsShared.numTextures] = assetsShared.textureViews[hTexture->index];
            }

            assert(assetsShared.numTextures < MAX_TEXTURES);
            newMaterial.textureIndex[t] = assetsShared.numTextures++;
        }

        mesh->materials.Push(newMaterial);
    }

    SetCurrentDirectoryA(oldDir);
    u64 msElapsed = Sys_GetElapsedMilliseconds(timestampBegin);
    OutputDebugStringA(fmt("textures load: %.3f (ms)\n", msElapsed));
}

void AddImmutableObject(DrawBuffer* buffer, Scene* m)
{
    VertexBuffer* const vbs = buffer->vertexBuffers;

    D3D11_BUFFER_DESC vertexBuffer;
    ZeroMemory(&vertexBuffer, sizeof(vertexBuffer));
    vertexBuffer.ByteWidth = vbs[VertexBufferId::Position].itemSize * m->xyz.Length();
    vertexBuffer.Usage = D3D11_USAGE_IMMUTABLE;
    vertexBuffer.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertexBuffer.CPUAccessFlags = NULL;

    AppendImmutableData(&vertexBuffer, m->xyz.GetStart(), &vbs[VertexBufferId::Position].buffer);
    AppendImmutableData(&vertexBuffer, m->normal.GetStart(), &vbs[VertexBufferId::Normal].buffer);

    D3D11_BUFFER_DESC tcBuffer;
    ZeroMemory(&tcBuffer, sizeof(tcBuffer));
    tcBuffer.ByteWidth = vbs[VertexBufferId::Tc].itemSize * m->tc.Length();
    tcBuffer.Usage = D3D11_USAGE_IMMUTABLE;
    tcBuffer.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    tcBuffer.CPUAccessFlags = NULL;

    AppendImmutableData(&tcBuffer, m->tc.GetStart(), &vbs[VertexBufferId::Tc].buffer);

    D3D11_BUFFER_DESC indexBuffer;
    ZeroMemory(&indexBuffer, sizeof(indexBuffer));
    indexBuffer.ByteWidth = buffer->indexBuffer.itemSize * m->indexes.Length();
    indexBuffer.Usage = D3D11_USAGE_IMMUTABLE;
    indexBuffer.BindFlags = D3D11_BIND_INDEX_BUFFER;
    indexBuffer.CPUAccessFlags = NULL;

    AppendImmutableData(&indexBuffer, m->indexes.GetStart(), &buffer->indexBuffer.buffer);

    buffer->numIndexes = m->indexes.Length();
}

void WriteBinaryMaterialToFile(Scene* scene, const char* filePath)
{
    FILE* file = fopen(filePath, "wb");
    if (!file)
    {
        Sys_FatalError("WriteBinaryMaterialToFile: failed to open file");
    }

    for (u32 m = 0; m < scene->materials.Length(); ++m)
    {
        Material* currentMaterial = &scene->materials[m];
        MeshFileMaterial* fileMaterial = &scene->fileMaterials[m];
        fileMaterial->flags = currentMaterial->flags;
        Vec3Copy(fileMaterial->specularColor, currentMaterial->specular);
        fileMaterial->specularExponent = currentMaterial->specular.w;
        Vec4Copy(fileMaterial->alphaTestedColor, currentMaterial->alphaTestedColor);
    }

    MaterialFileHeader hdr;
    hdr.numMaterials = scene->fileMaterials.Length();
    hdr.numStringBytes = assetsShared.newStrings.mem_used;

    fwrite(&hdr, sizeof(hdr), 1, file);
    fwrite(scene->fileMaterials.GetStart(), scene->fileMaterials.UsedBytes(), 1, file);
    fwrite(assetsShared.newStrings.base_ptr, assetsShared.newStrings.mem_used, 1, file);

    MemoryArena temp;
    temp = assetsShared.oldStrings;
    assetsShared.oldStrings = assetsShared.newStrings;
    assetsShared.newStrings = temp;

    fclose(file);
}