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

int IsDigit(char c) { return (c >= '0' && c <= '9'); }
int IsAlnum(char c) { return (c >= 'A' && c <= 'Z' || c >= 'a' && c <= 'z'); }

int IsNewline(char c) { return (c == '\n'); }

int IsWhitespace(char c) { return (c == ' ' || c == '\t' || c == '\r'); }

const char* SkipWhitespace(const char* ptr)
{
    while (IsWhitespace(*ptr))
        ptr++;
    return ptr;
}

const char* SkipLine(const char* ptr)
{
    while (!IsNewline(*ptr++))
        ;
    return ptr;
}

const char* ParseString(const char* buff, char* v)
{
    size_t n;

    const char* start = buff;

    while (IsAlnum(*buff) || *buff == '-' || *buff == '.' || *buff == '_' || *buff == '/' || IsDigit(*buff))
    {
        buff++;
    }
    char* end = (char*)buff;
    n = (size_t)(end - start);

    if (v)
    {
        memcpy(v, start, n);
        (v)[n] = '\0';
    }

    return buff;
}

void PathRemoveFileSpec(char* inout)
{
    assert(inout);
    const char* start = inout;
    const char* end = inout + strlen(inout);
    while (*end != '/' && *end != '\\')
    {
        end--;
    }
    inout[end - start] = '\0';
}

void GetDirectoryPath(char* output, const char* input)
{
    assert(output);
    assert(input);
    strcpy(output, input);
    PathRemoveFileSpec(output);
}

void PathCombine(char* output, const char* dir, const char* fileName)
{
    assert(dir);
    assert(fileName);
    assert(output);
    strcpy(output, dir);
    size_t end = strlen(output) - 1;
    if (output[end] != '/')
    {
        strcat(output, "/");
    }
    strcat(output, fileName);
}

void GetFileName(char* output, const char* input)
{
    assert(output);
    assert(input);
    const char* end = input + strlen(input);
    const char* start = end;
    while (*start != '/' && *start != '\\')
    {
        start--;
    }
    start++;

    size_t n = end - start;
    memcpy(output, start, n + 1);
}

void StripFileExtension(char* inout)
{
    assert(inout);
    const char* start = inout;
    const char* end = inout + strlen(inout);
    while (*end != '.')
    {
        end--;
    }
    inout[end - start] = '\0';
}