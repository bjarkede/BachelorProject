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
#include <Windows.h>

static void PrintHelp()
{
    printf("need valid obj file\n");
}

static void ReadEntireFile(void** data, size_t* size, const char* path)
{
    FILE* file = fopen(path, "rb");
    if (file == NULL)
    {
        fprintf(stderr, "Can't open file.\n");
        exit(__LINE__);
    }

    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    fseek(file, 0, SEEK_SET);

    *data = malloc(*size);
    if (*data == NULL)
    {
        fprintf(stderr, "Can't allocate %d bytes.\n", (int)*size);
        exit(__LINE__);
    }

    fread(*data, *size, 1, file);

    fclose(file);
}

int main(int argc, char** argv)
{
#ifdef _DEBUG
    argc = 2;
    argv[1] = "../bachelor/assets/Sponza/sponza_low.obj";
#endif

    if (argc != 2)
    {
        fprintf(stderr, "Invalid argument.\n");
        PrintHelp();
        return 1;
    }

    size_t fileSize;
    void* fileData;
    ReadEntireFile(&fileData, &fileSize, argv[1]);

    char fileName[MAX_PATH];
    GetFileName(fileName, argv[1]);

    Mesh m = {};
    LoadObject(&m, NULL, fileData, fileSize, fileName, argv[1]);

    StripFileExtension(fileName);
    WriteBinaryMeshToFile(&m, fmt("%s.scene", fileName));
    WriteBinaryMaterialToFile(&m, fmt("%s.material", fileName));

    return 0;
}