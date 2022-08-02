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

#define WIN32_LEAN_AND_MEAN
#include "../common/shared.h"
#include <Windows.h>

void Sys_FatalError(const char* format, ...)
{
    char msg[1024];

    va_list ap;
    va_start(ap, format);
    vsprintf(msg, format, ap);
    va_end(ap);

    if (IsDebuggerPresent() || MessageBoxA(NULL, fmt("%s\n\nWould you like to break?", msg), "Fatal Error", MB_YESNO | MB_ICONERROR) == IDYES)
    {
        __debugbreak();
    }
    else
    {
        // do whatever other clean-up is needed if you changed system settings
        exit(0);
    }
}

struct FolderScan
{
    WIN32_FIND_DATA findData;
    HANDLE searchHandle;
    char filePath[MAX_PATH];
    char folderPath[MAX_PATH];
    char searchPath[MAX_PATH];
};

FolderScan* Sys_FolderScan_Begin(const char* dir, const char* type)
{
    FolderScan* fs = (FolderScan*)malloc(sizeof(FolderScan));
    if (fs == NULL)
    {
        Sys_FatalError("Sys_FolderScan_Begin: failed to allocate handle\n");
    }

    fs->searchHandle = INVALID_HANDLE_VALUE;

    strcpy(fs->folderPath, dir);
    PathCombine(fs->searchPath, dir, type);

    return fs;
}

bool Sys_FolderScan_Next(const char** fileName, const char** filePath, FolderScan* fs)
{
    assert(fs != NULL);
    assert(fs->folderPath != NULL);
    if (fs->searchHandle == INVALID_HANDLE_VALUE)
    {
        fs->searchHandle = FindFirstFileA(fs->searchPath, &fs->findData);
        if (fs->searchHandle == INVALID_HANDLE_VALUE)
        {
            Sys_FatalError("Sys_FolderScan_Next: FindFirstFileA failed\n");
        }
    }
    else if (!FindNextFileA(fs->searchHandle, &fs->findData))
    {
        return false;
    }

    if (fileName != NULL)
    {
        *fileName = fs->findData.cFileName;
    }
    if (filePath != NULL)
    {
        PathCombine(fs->filePath, fs->folderPath, fs->findData.cFileName);
        *filePath = fs->filePath;
    }

    return true;
}

void Sys_FolderScan_End(FolderScan* fs)
{
    assert(fs != NULL);
    assert(fs->searchHandle != INVALID_HANDLE_VALUE);
    FindClose(fs->searchHandle);
    free(fs);
}

bool Sys_ReadDataFromFile(void** data, size_t* size, const char* filePath)
{
    FILE* file = fopen(filePath, "rb");
    if (file == NULL)
    {
        return false;
    }

    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    fseek(file, 0, SEEK_SET);

    *data = malloc(*size);
    if (*data == NULL)
    {
        return false;
    }

    fread(*data, *size, 1, file);

    fclose(file);

    return true;
}