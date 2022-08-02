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

char* fmt(const char* format, ...)
{
    enum
    {
        BufferSize = 1024,
        BufferCount = 16
    };

    static THREAD_LOCAL char buffer[BufferCount][BufferSize];
    static THREAD_LOCAL u32 bufferIndex = 0;

    char* result = buffer[bufferIndex];
    va_list args;
    va_start(args, format);
    vsnprintf(result, BufferSize - 1, format, args);
    va_end(args);
    bufferIndex = (bufferIndex + 1) % BufferCount;

    return result;
}

bool FileExists(const char* filePath)
{
    FILE* file = fopen(filePath, "rb");
    if (file == NULL)
    {
        return false;
    }

    fclose(file);
    return true;
}

bool ReadEntireFile(void** memoryBuffer, s32* size, const char* filePath)
{
    FILE* file = fopen(filePath, "rb");
    if (file == NULL)
        return false;
    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    fseek(file, 0, SEEK_SET);
    *memoryBuffer = malloc(*size);
    if (*memoryBuffer == NULL)
        Sys_FatalError("Failed to allocate memory for file: %s", filePath);
    fread(*memoryBuffer, *size, 1, file);
    fclose(file);
    return true;
}

char* FormatBytes(u64 byteCount)
{
    if (byteCount == 0)
    {
        return "0 byte";
    }

    static const char* units[] { "bytes", "KB", "MB", "GB", "TB" };

    u32 unitIndex = 0;
    u32 prev = 0;
    u32 temp = byteCount;
    while (temp >= 1024)
    {
        ++unitIndex;
        prev = temp;
        temp >>= 10;
    }

    f32 number = (f32)prev / 1024.0;

    return fmt("%.3f %s", number, units[unitIndex]);
}
