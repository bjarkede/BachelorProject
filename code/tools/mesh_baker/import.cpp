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

static ObjectGroup DefaultGroup()
{
    ObjectGroup group = {};

    group.numMaterialGroups = 1;

    return group;
}

static const char* GetSign(const char* buff, s32* sign)
{
    buff = SkipWhitespace(buff);

    switch (*buff)
    {
    case '-':
    {
        *sign = -1;
        buff++;
    }
    break;
    case '+':
    {
        *sign = +1;
        buff++;
    }
    break;
    default:
    {
        *sign = +1;
    }
    break;
    }

    return buff;
}

static const char* ParseU8(const char* buff, u8* value)
{
    s32 sign;
    buff = GetSign(buff, &sign);
    while (IsDigit(*buff))
    {
        *value *= 10;
        *value += (*buff++ - '0');
    }

    return buff;
}

static const char* ParseS32(const char* buff, s32* value)
{
    s32 sign;
    s32 num;

    switch (*buff)
    {
    case '-':
    {
        sign = -1;
        buff++;
    }
    break;
    case '+':
    {
        sign = +1;
        buff++;
    }
    break;
    default:
    {
        sign = +1;
    }
    break;
    }

    num = 0;

    while (IsDigit(*buff))
    {
        num *= 10; // shift over the num
        num += (*buff++ - '0'); // convert the ascii char to number
    }

    *value = sign * num;

    return buff;
}

static void GetStringExtension(const char* string, char* format)
{
    const char* start = string;
    while (*start != '.')
    {
        ++start;
    }

    start++;
    start = ParseString(start, format);
}

static const char* ParsePath(const char* buff, char** v)
{
    size_t n;

    const char* start = buff;

    while (IsAlnum(*buff) || *buff == '-' || *buff == '_' || *buff == '.' || *buff == '/' || IsDigit(*buff))
    {
        buff++;
    }

    char* end = (char*)buff;
    n = (size_t)(end - start);

    *v = (char*)realloc(0, n + 1); // +1 because string must be null terminated
    if (*v)
    {
        memcpy(*v, start, n);
        (*v)[n] = '\0';
    }

    return buff;
}

static const char* ParseF32(const char* buff, f32* value)
{
    s32 sign;

    buff = SkipWhitespace(buff);

    switch (*buff)
    {
    case '-':
    {
        sign = -1;
        buff++;
    }
    break;
    case '+':
    {
        sign = +1;
        buff++;
    }
    break;
    default:
    {
        sign = +1;
    }
    break;
    }

    // Check that the float is valid
    const char* start = buff;
    while (IsDigit(*buff))
    {
        buff++;
    } // Scan until the '.' delimiter is found
    if (*buff != '.')
    {
        *value = sign * strtof(start, (char**)&buff);
        return buff;
    }
    buff++; // When found lex the second half of the number
    while (IsDigit(*buff))
    {
        buff++;
    }

    // if e/E is used.
    if (tolower(*buff) == 'e')
    {
        buff++;
        if (*buff == '+' || *buff == '-')
        {
            buff++;
        }
        if (!IsDigit(*buff))
        {
            printf("expected digit after 'e' component, found '%c'.", *buff);
            exit(1);
        }
        while (IsDigit(*buff))
        {
            buff++;
        }
    }

    char* end = (char*)buff; // get the end of the number
    *value = sign * strtof(start, &end); // parse from start to end
    return buff;
}

static const char* ParseVec3(const char* buff, vec3_t* v)
{
    f32 r[3];
    for (int i = 0; i < 3; i++)
    {
        buff = ParseF32(buff, &r[i]);
    }

    v->x = r[0];
    v->y = r[1];
    v->z = r[2];

    return buff;
}

static const char* ParseFace(ObjectData* data, const char* start)
{
    u32 count = 0;

    start = SkipWhitespace(start);

    // format can be either:
    // negative: means index was declared -int above
    // postive format: v1 v2 v3 v4
    // format: v1/vt/vn
    // format: v1//vn

    Vertex face;

    while (!IsNewline(*start))
    {
        s32 v = 0;
        s32 vt = 0;
        s32 vn = 0;

        start = ParseS32(start, &v);
        if (*start == '/')
        {
            start++;
            if (*start != '/')
            {
                start = ParseS32(start, &vt);
            }

            if (*start == '/')
            {
                start++;
                start = ParseS32(start, &vn);
            }
        }

        if (v < 0)
        {
            face.xyz = data->m->xyz.Length() - ABS(v);
        }
        else
        {
            face.xyz = v - 1;
        }

        if (vt < 0)
        {
            face.tc = data->m->tc.Length() - ABS(vt);
        }
        else if (vt > 0)
        {
            face.tc = vt - 1;
        }
        else
        {
            face.tc = 0;
        }

        if (vn < 0)
        {
            face.normal = data->m->normals.Length() - ABS(vn);
        }
        else if (vn > 0)
        {
            face.normal = vn - 1;
        }
        else
        {
            face.normal = 0;
        }

        data->m->vertexes.Push(face);

        count++;
        start = SkipWhitespace(start);
    }

    if (count == 3)
    {
        // right now this is not pr. smoothing group
        data->currentGroup.materialGroups[data->currentGroup.numMaterialGroups - 1].numIndexes += 3;
    }
    else
    {
        data->currentGroup.materialGroups[data->currentGroup.numMaterialGroups - 1].numIndexes += 6;
    }

    data->m->faceVertexCount.Push(count); // how many vertices did we find in the face
    data->m->faceSGroupIndices.Push(data->currentSgroup);
    data->currentGroup.numFaces++; // how many faces in the current group

    return start;
}

static const char* ParseVertex(ObjectData* data, const char* start)
{
    vec3_t v;

    f32 r[3];
    for (int i = 0; i < 3; i++)
    {
        start = ParseF32(start, &r[i]);
    }

    v.x = r[0];
    v.y = r[1];
    v.z = r[2];

    data->m->xyz.Push(v);

    return start;
}

static const char* ParseNormal(ObjectData* data, const char* start)
{
    vec3_t v;

    f32 r[3];
    for (int i = 0; i < 3; i++)
    {
        start = ParseF32(start, &r[i]);
    }

    v.x = r[0];
    v.y = r[1];
    v.z = r[2];

    data->m->normals.Push(v);

    return start;
}

static const char* ParseTexcoord(ObjectData* data, const char* start)
{
    vec2_t v;

    f32 r[2];
    for (int i = 0; i < 2; i++)
    {
        start = ParseF32(start, &r[i]);
    }

    v.u = r[0];
    v.v = r[1];

    data->m->tc.Push(v);

    return start;
}

static void PushGroup(ObjectData* data)
{
    if (data->currentGroup.numFaces > 0)
    {
        data->m->groups.Push(data->currentGroup);
    }
    else
    {
        memset(&data->currentGroup, 0, sizeof(ObjectGroup));
    }

    data->currentGroup = DefaultGroup();
    data->currentGroup.faceOffset = data->m->faceVertexCount.Length();
    data->currentGroup.indexOffset = data->m->vertexes.Length();
    data->currentGroup.vertexOffset = data->m->xyz.Length();
    data->currentGroup.normalOffset = data->m->normals.Length();
}

static const char* ParseGroup(ObjectData* data, const char* start)
{
    char name[MAX_PATH];
    start = SkipWhitespace(start);
    start = ParseString(start, name);

    PushGroup(data);

    data->currentGroup.name = name;

    return start;
}

static const char* ParseSmoothingGroup(ObjectData* data, const char* start)
{
    start = SkipWhitespace(start);

    s32 smoothingGroupIndex = 0;
    if (IsDigit(*start) && *start != '0')
    {
        start = ParseS32(start, &smoothingGroupIndex);
    }

    start = SkipLine(start);

    data->currentSgroup = smoothingGroupIndex;

    for (u32 i = 0; i < data->currentGroup.sgroupIndices.Length(); ++i)
    {
        if (data->currentGroup.sgroupIndices[i] == smoothingGroupIndex)
            return start;
    }

    data->currentGroup.sgroupIndices.Push(smoothingGroupIndex);

    return start;
}

static const char* ParseMap(const char* start, ParseMaterial* material, TextureId::Type id)
{

    start = SkipWhitespace(start);
    start = ParsePath(start, &material->mapPaths[id]);

    if (!material->isAlphaTested)
        material->isAlphaTested = id == TextureId::Alpha;

    return start;
}

static const char* ParseNewMaterial(ObjectData* data, const char* start)
{
    //ParseMaterial* material = (ParseMaterial*)malloc(sizeof(ParseMaterial));
    //memset(material, 0, sizeof(ParseMaterial));
    ParseMaterial material = {};

    material.Ka = { 0.725f, 0.71f, 0.68f };
    material.Kd = { 0.725f, 0.71f, 0.68f };
    material.Ks = { 0.0f, 0.0f, 0.0f };
    material.Ke = { 0.0f, 0.0f, 0.0f };
    material.Ns = 10.0f;
    material.Ni = 1.0f;
    material.Tr = 0.0f;
    material.d = 1.0f;
    material.Tf = { 0.0f, 0.0f, 0.0f };
    material.Illum = 2;

    start = SkipWhitespace(start);
    start = ParseString(start, material.name);

    data->m->materials.Push(material);

    return start;
}

static const char* ParseMTLLib(ObjectData* data, const char* start)
{
    start = SkipWhitespace(start);

    char fileName[MAX_PATH];
    start = ParseString(start, fileName);
    start = SkipLine(start);

    char materialPath[MAX_PATH];
    char folderPath[MAX_PATH];
    GetDirectoryPath(folderPath, data->objPath);
    PathCombine(materialPath, folderPath, fileName);

    // Open the file
    void* fileData;
    size_t fileSize;
    if (!Sys_ReadDataFromFile(&fileData, &fileSize, materialPath))
    {
        Sys_FatalError("ParseMTLLib: failed to read file");
    }

    // Parse the file
    const char* fileBegin = (char*)fileData;
    const char* fileEnd = fileBegin + fileSize;

    while (fileBegin < fileEnd)
    {
        u32 mtlIndex = data->m->materials.Length() - 1;

        fileBegin = SkipWhitespace(fileBegin);
        switch (*fileBegin)
        {
        case '#':
        {
            fileBegin = SkipLine(fileBegin);
        }
        break;
        case 'n':
        {
            fileBegin++;
            if (fileBegin[0] == 'e' && fileBegin[1] == 'w' && fileBegin[2] == 'm' && fileBegin[3] == 't' && fileBegin[4] == 'l')
            {
                fileBegin = ParseNewMaterial(data, fileBegin + 5);
            }
        }
        break;
        case 'i':
        {
            fileBegin++;
            if (fileBegin[0] == 'l' && fileBegin[1] == 'l' && fileBegin[2] == 'u' && fileBegin[3] == 'm')
            {
                fileBegin = ParseS32(fileBegin + 4, &data->m->materials[mtlIndex].Illum);
            }
        }
        break;
        case 'm':
        {
            fileBegin++;
            if (fileBegin[0] == 'a' && fileBegin[1] == 'p' && fileBegin[2] == '_')
            {
                fileBegin = fileBegin + 3;
                switch (*fileBegin++)
                {
                case 'K':
                {
                    if (fileBegin[0] == 'a')
                    {
                        fileBegin = ParseMap(fileBegin + 1, &data->m->materials[mtlIndex], TextureId::Ambient);
                    }
                    if (fileBegin[0] == 'd')
                    {
                        fileBegin = ParseMap(fileBegin + 1, &data->m->materials[mtlIndex], TextureId::Albedo);
                    }
                    if (fileBegin[0] == 's')
                    {
                        fileBegin = ParseMap(fileBegin + 1, &data->m->materials[mtlIndex], TextureId::Specular);
                    }
                }
                break;
                case 'N':
                {
                    if (fileBegin[0] == 's')
                    {
                        fileBegin = ParseMap(fileBegin + 1, &data->m->materials[mtlIndex], TextureId::SpecularHighlight);
                    }
                }
                break;
                case 'd':
                {
                    fileBegin = ParseMap(fileBegin, &data->m->materials[mtlIndex], TextureId::Alpha);
                }
                break;
                case 'B':
                case 'b':
                {
                    if (fileBegin[0] == 'u' && fileBegin[1] == 'm' && fileBegin[2] == 'p')
                    {
                        fileBegin = ParseMap(fileBegin + 3, &data->m->materials[mtlIndex], TextureId::Bump);
                    }
                }
                break;
                }
            }
        }
        break;
        case 'b':
        {
            fileBegin++;
            if (fileBegin[0] == 'u' && fileBegin[1] == 'm' && fileBegin[2] == 'p')
            {
                fileBegin = ParseMap(fileBegin + 3, &data->m->materials[mtlIndex], TextureId::Bump);
            }
        }
        break;
        case 'd':
        {
            fileBegin++;
            switch (*fileBegin++)
            {
            case 'i':
            {
                if (fileBegin[0] == 's' && fileBegin[1] == 'p')
                {
                    fileBegin = ParseMap(fileBegin + 2, &data->m->materials[mtlIndex], TextureId::Displacement);
                }
            }
            break;
            case 'e':
            {
                if (fileBegin[0] == 'c' && fileBegin[1] == 'a' && fileBegin[2] == 'l')
                {
                    fileBegin = ParseMap(fileBegin + 3, &data->m->materials[mtlIndex], TextureId::Stencil);
                }
            }
            break;
            default:
            {
                fileBegin--;
            }
            break;
            }
        }
        break;
        case 'K':
        {
            fileBegin++;
            switch (*fileBegin++)
            {
            case 'a':
            {
                fileBegin = ParseVec3(fileBegin, &data->m->materials[mtlIndex].Ka);
            }
            break;
            case 'd':
            {
                fileBegin = ParseVec3(fileBegin, &data->m->materials[mtlIndex].Kd);
            }
            break;
            case 's':
            {
                fileBegin = ParseVec3(fileBegin, &data->m->materials[mtlIndex].Ks);
            }
            break;
            case 'e':
            {
                fileBegin = ParseVec3(fileBegin, &data->m->materials[mtlIndex].Ke);
            }
            break;
            default:
            {
                fileBegin--;
            }
            break;
            }
        }
        break;
        case 'N':
        {
            fileBegin++;
            switch (*fileBegin++)
            {
            case 's':
            {
                fileBegin = ParseF32(fileBegin, &data->m->materials[mtlIndex].Ns);
            }
            break;
            case 'i':
            {
                fileBegin = ParseF32(fileBegin, &data->m->materials[mtlIndex].Ni);
            }
            break;
            default:
            {
                fileBegin--;
            }
            break;
            }
        }
        break;
        case 'T':
        {
            fileBegin++;
            switch (*fileBegin++)
            {
            case 'r':
            {
                fileBegin = ParseF32(fileBegin, &data->m->materials[mtlIndex].Tr);
            }
            break;
            case 'f':
            {
                fileBegin = ParseVec3(fileBegin, &data->m->materials[mtlIndex].Tf);
            }
            break;
            default:
            {
                fileBegin--;
            }
            break;
            }
        }
        break;
        default:
        {
            fileBegin = SkipLine(fileBegin);
        }
        break;
        }
    }

    return start;
}

static const char* ParseUseMTL(ObjectData* data, const char* start)
{
    char name[MAX_PATH];

    start = SkipWhitespace(start);
    start = ParseString(start, name);
    start = SkipLine(start);

    // find the material
    u32 index = 0;
    for (; index < data->m->materials.Length(); ++index)
    {
        if (strcmp(name, data->m->materials[index].name) == 0)
        {
            assert(data->currentGroup.numMaterialGroups < ARRAY_LEN(data->currentGroup.materialGroups));
            data->currentGroup.materialOffset = index; // this group should use material at index
            data->currentGroup.materialGroups[data->currentGroup.numMaterialGroups++].materialIndex = index;
            data->currentGroup.materialGroups[data->currentGroup.numMaterialGroups].numIndexes = 0;
            break;
        }
    }

    // if no name was specied we use the default material
    if (index == data->m->materials.Length())
    {
        data->currentGroup.materialOffset = 0;
    }

    return start;
}

static void ComputeSmoothNormals(Mesh* m, u32 i0, u32 i1, u32 i2)
{
    vec3_t v0 = m->xyz[i0];
    vec3_t v1 = m->xyz[i1];
    vec3_t v2 = m->xyz[i2];

    // compute non-normalized normal
    vec3_t N = cross(v1 - v0, v2 - v0);

    // compute angle alpha between vectors going out of each vertex
    f32 a0 = angle(v1 - v0, v2 - v0);
    f32 a1 = angle(v2 - v1, v0 - v1);
    f32 a2 = angle(v0 - v2, v1 - v2);

    // add to the normal
    vec3_t n0 = N * a0;
    vec3_t n1 = N * a1;
    vec3_t n2 = N * a2;

    m->normal[i0] = m->normal[i0] + n0;
    m->normal[i1] = m->normal[i1] + n1;
    m->normal[i2] = m->normal[i2] + n2;
}

static vec3_t ComputeVertexNormal(Mesh* mesh, u32 i0, u32 i1, u32 i2, u32 i3, u32 i4, u32 i5)
{
    vec3_t v0 = mesh->xyz[i0];
    vec3_t v1 = mesh->xyz[i1];
    vec3_t v2 = mesh->xyz[i2];
    vec3_t v3 = mesh->xyz[i3];
    vec3_t v4 = mesh->xyz[i4];
    vec3_t v5 = mesh->xyz[i5];

    // compute non-normalized normal
    vec3_t N = cross(v1 - v0, v2 - v0);
    f32 a = angle(v4 - v3, v5 - v3);
    vec3_t result = N * a;
    return result;
}

static u32 PushVertex(Mesh* mesh, u32 firstVertexIndex, vec3_t xyz, vec2_t tc, vec3_t normal)
{
    for (u32 vertexIndex = firstVertexIndex; vertexIndex < mesh->xyz.Length(); ++vertexIndex)
    {
        if (mesh->xyz[vertexIndex] == xyz && mesh->tc[vertexIndex] == tc && mesh->normal[vertexIndex] == normal)
        {
            return vertexIndex;
        }
    }

    mesh->xyz.Push(xyz);
    mesh->tc.Push(tc);
    mesh->normal.Push(normal);
    assert(mesh->xyz.Length() == mesh->tc.Length());
    assert(mesh->tc.Length() == mesh->normal.Length());
    return mesh->xyz.Length() - 1;
}

static void ProcessVertex(Mesh* mesh, Object* parseData, u32 faceIndex, u32 firstVertexIndex)
{
    Vertex vertex = parseData->vertexes[faceIndex];
    vec3_t xyz = parseData->xyz[vertex.xyz];
    vec2_t tc = parseData->tc[vertex.tc];
    vec3_t normal = parseData->normals[vertex.normal];

    u32 index = PushVertex(mesh, firstVertexIndex, xyz, tc, normal);
    mesh->indexes.Push(index);
}

static void SmoothNormals(Mesh* mesh, u32 firstVertex, u32 lastVertex, u32 firstIndex, u32 lastIndex)
{
    for (u32 v = firstVertex; v < lastVertex; v++)
    {
        vec3_t N = { 0.0f, 0.0f, 0.0f };
        for (u32 i = firstIndex; i < lastIndex; i += 3)
        {
            u32 i0 = mesh->indexes[i];
            u32 i1 = mesh->indexes[i + 1];
            u32 i2 = mesh->indexes[i + 2];

            if (mesh->xyz[i0] == mesh->xyz[v])
            {
                N = N + ComputeVertexNormal(mesh, i0, i1, i2, i0, i1, i2);
            }
            else if (mesh->xyz[i1] == mesh->xyz[v])
            {
                N = N + ComputeVertexNormal(mesh, i0, i1, i2, i1, i2, i0);
            }
            else if (mesh->xyz[i2] == mesh->xyz[v])
            {
                N = N + ComputeVertexNormal(mesh, i0, i1, i2, i2, i0, i1);
            }
        }
        N = norm(N);
        mesh->normal[v] = N;
    }
}

static void ParseRenderable(Object* parseData, Mesh* mesh)
{
    parseData->min.x = parseData->min.y = parseData->min.z = FLT_MAX;
    parseData->max.x = parseData->max.y = parseData->max.z = -FLT_MAX;

    // Calcuate AABB for mesh
    for (u32 i = 0; i < parseData->xyz.Length(); ++i)
    {
        vec3_t v = parseData->xyz[i];
        if (v.x < parseData->min.x)
            parseData->min.x = v.x;
        if (v.y < parseData->min.y)
            parseData->min.y = v.y;
        if (v.z < parseData->min.z)
            parseData->min.z = v.z;
        if (v.x > parseData->max.x)
            parseData->max.x = v.x;
        if (v.y > parseData->max.y)
            parseData->max.y = v.y;
        if (v.z > parseData->max.z)
            parseData->max.z = v.z;
    }

    mesh->name = parseData->fileName;

    mesh->aabb.min = parseData->min;
    mesh->aabb.max = parseData->max;

    // Data used to draw groups
    for (u32 i = 0; i < parseData->materials.Length(); ++i)
        mesh->materials.Push(parseData->materials[i]);

    for (u32 i = 0; i < parseData->groups.Length(); ++i)
        mesh->groups.Push(parseData->groups[i]);

    for (u32 groupIndex = 0; groupIndex < mesh->groups.Length(); ++groupIndex)
    {
        ObjectGroup* group = &mesh->groups[groupIndex];
        for (u32 sg = 0; sg < group->sgroupIndices.Length(); ++sg)
        {
            SGroup sgroup;
            u32 sgi = group->sgroupIndices[sg];
            sgroup.firstIndex = mesh->indexes.Length();
            u32 firstVertexIndex = mesh->xyz.Length();
            u32 inputVertexIndex = group->indexOffset;
            u32 firstIndex = mesh->indexes.Length();
            for (u32 face = group->faceOffset; face < group->faceOffset + group->numFaces; ++face)
            {
                u32 numVertexes = parseData->faceVertexCount[face];
                if (parseData->faceSGroupIndices[face] != sgi)
                {
                    inputVertexIndex += numVertexes;
                    continue;
                }

                if (numVertexes == 3)
                {
                    ProcessVertex(mesh, parseData, inputVertexIndex + 0, firstVertexIndex);
                    ProcessVertex(mesh, parseData, inputVertexIndex + 1, firstVertexIndex);
                    ProcessVertex(mesh, parseData, inputVertexIndex + 2, firstVertexIndex);
                }
                else if (numVertexes == 4)
                {
                    ProcessVertex(mesh, parseData, inputVertexIndex + 0, firstVertexIndex);
                    ProcessVertex(mesh, parseData, inputVertexIndex + 1, firstVertexIndex);
                    ProcessVertex(mesh, parseData, inputVertexIndex + 2, firstVertexIndex);
                    ProcessVertex(mesh, parseData, inputVertexIndex + 3, firstVertexIndex);
                    ProcessVertex(mesh, parseData, inputVertexIndex + 0, firstVertexIndex);
                    ProcessVertex(mesh, parseData, inputVertexIndex + 2, firstVertexIndex);
                }
                else
                {
                    assert(numVertexes < 5);
                }

                inputVertexIndex += numVertexes;
            }

            SmoothNormals(mesh, firstVertexIndex, mesh->xyz.Length(), firstIndex, mesh->indexes.Length());

            sgroup.numIndexes = mesh->indexes.Length() - sgroup.firstIndex;
            mesh->sgroups.Push(sgroup);
        }
    }

    for (u32 n = 0; n < mesh->normal.Length(); ++n)
    {
        mesh->normal[n] = norm(mesh->normal[n]);
    }
}

// @TODO:
// Allocate all the dynamic arrays in the arena
void LoadObject(Mesh* mesh, MemoryArena* arena, void* data, u64 size, const char* name, const char* objPath)
{

    ObjectData parseData = {};
    Object m; // @TODO: are we sure no clearing is needed?

    strcpy(parseData.objPath, objPath);
    m.fileName = name;

    // @TODO:
    // why is this still needed when parsing?
#if 1
    ParseMaterial x = {};
    m.materials.Push(x);
#endif

    parseData.m = &m;
    parseData.currentGroup = DefaultGroup();

    // Ensure that the buffer ends with a newline
    ((char*)data)[size] = '\n';

    // Set the start and end pointer
    const char* start = (char*)data;
    const char* ep = start + size;

    while (start < ep)
    {
        start = SkipWhitespace(start);
        switch (*start)
        {
        case '#':
        {
            start = SkipLine(start);
        }
        break;
        case 'v':
        {
            start++;
            switch (*start++)
            {
            case ' ':
            case '\t': // Geometric vertex
            {
                start = ParseVertex(&parseData, start);
            }
            break;
            case 't': // Texture
            {
                start = ParseTexcoord(&parseData, start);
            }
            break;
            case 'n': // Normal vector
            {
                start = ParseNormal(&parseData, start);
            }
            break;
            default:
            {
                start--;
            }
            break;
            }
            start = SkipLine(start);
        }
        break;
        case 'f':
        {
            start++;
            switch (*start++)
            {
            case ' ':
            case '\t':
            {
                start = ParseFace(&parseData, start);
            }
            break;
            default:
                start--;
            }
            start = SkipLine(start);
        }
        break;
        case 'g':
        {
            start++;
            switch (*start++)
            {
            case ' ':
            case '\t':
            {
                start = ParseGroup(&parseData, start);
            }
            break;
            default:
                start--;
            }
            start = SkipLine(start);
        }
        break;
        case 's':
        {
            start = ParseSmoothingGroup(&parseData, start + 1);
        }
        break;
        case 'm':
        {
            start++;
            if (start[0] == 't' && start[1] == 'l' && start[2] == 'l' && start[3] == 'i' && start[4] == 'b' && IsWhitespace(start[5]))
            {
                start = ParseMTLLib(&parseData, start + 5);
            }
        }
        break;
        case 'u':
        {
            start++;
            if (start[0] == 's' && start[1] == 'e' && start[2] == 'm' && start[3] == 't' && start[4] == 'l' && IsWhitespace(start[5]))
            {
                start = ParseUseMTL(&parseData, start + 5);
            }
        }
        break;
        case '\n':
        {
            start = SkipLine(start);
        }
        break;
        default:
        {
            assert(false);
            start = SkipLine(start);
        }
        break;
        }
    }

    // When we are finished make sure to push the final group
    PushGroup(&parseData);

    // Before returning the object, parse the indices and materials to an index/color buffer
    ParseRenderable(&m, mesh);
}
