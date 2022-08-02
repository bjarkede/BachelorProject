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

#include <assert.h>
#include <ctype.h>
#include <float.h>
#include <math.h>
#include <memory.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIN(x, y) ((x) <= (y) ? (x) : (y))
#define MAX(x, y) ((x) >= (y) ? (x) : (y))
#define CLAMP_MAX(x, max) MIN(x, max)
#define CLAMP_MIN(x, min) MAX(x, min)
#define IS_POW2(x) (((x) != 0) && ((x) & ((x)-1)) == 0)
#define ALIGN_DOWN(n, a) ((n) & ~((a)-1))
#define ALIGN_UP(n, a) ALIGN_DOWN((n) + (a)-1, (a))
#define ALIGN_DOWN_PTR(p, a) ((void*)ALIGN_DOWN((uintptr_t)(p), (a)))
#define ALIGN_UP_PTR(p, a) ((void*)ALIGN_UP((uintptr_t)(p), (a)))
#define MAX_PATH 260

// clang-format off
#define ARRAY_LEN(Array) (sizeof(Array) / sizeof(Array[0]))
#define ARRAY_FILL(arr, v) \
for(u32 i = 0; i < ARRAY_LEN(arr); ++i) { arr[i] = v; }
// clang-format on

#define Kilobytes(Value) ((Value)*1024)
#define Megabytes(Value) (Kilobytes(Value) * 1024)
#define Gigabytes(Value) (Megabytes(Value) * 1024)
#define ABS(N) ((N < 0) ? (-N) : (N))

#define Vec2Copy(a, b) ((a).u = (b).u, (a).v = (b).v)
#define Vec3Copy(a, b) ((a).x = (b).x, (a).y = (b).y, (a).z = (b).z)
#define Vec4Copy(a, b) ((a).x = (b).x, (a).y = (b).y, (a).z = (b).z, (a).w = (b).w)

#define Uint3Copy(a, b) ((a).w = (b).w, (a).h = (b).h, (a).d = (b).d)

#define ARENA_ALIGNMENT 16
#define ARENA_BLOCK_SIZE (1024)

#define s8 int8_t
#define s16 int16_t
#define s32 int32_t
#define s64 int64_t
#define sptr int64_t

#define u8 uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t
#define uptr uint64_t

#define f32 float
#define f64 double

#define b32 bool

#define SCREEN_HEIGHT 720
#define SCREEN_WIDTH 1280

#define MAX_LOAD_FILES 20

// Functions
#define MAX3(a, b, c) MAX(a, MAX(b, c))
#define MIN3(a, b, c) MIN(a, MIN(b, c))

//
// System interface
//

struct InputState
{
    u32 isDown;
};

struct RawMouse
{
    union {
        InputState Btn[5];
        struct
        {
            InputState Mouse1;
            InputState Mouse2;
            InputState Mouse3;
            InputState Mouse4;
            InputState Mouse5;
        };
    };

    s32 dx, dy;
    f32 wheel_dt;
};

struct RawKeyboard
{
    u32 isDown[256];
    u32 wasDown[256];
};

struct ClientInput
{
    RawMouse mouse;
    RawKeyboard keyboard;
    f32 dt;
};

struct SystemMemory
{
    u64 permanentStorageSize;
    void* permanentStorage;
    u64 transientStorageSize;
    void* transientStorage;
};

struct FolderScan;

FolderScan* Sys_FolderScan_Begin(const char* dir, const char* type);
bool Sys_FolderScan_Next(const char** fileName, const char** filePath, FolderScan* fs);
void Sys_FolderScan_End(FolderScan* fs);
// the data must be deallocated with free
bool Sys_ReadDataFromFile(void** data, size_t* size, const char* filePath);

u64 Sys_GetTimestamp();
u64 Sys_GetElapsedMilliseconds(u64 startTimestamp);
u64 Sys_GetElapsedMicroseconds(u64 startTimestamp);
void Sys_FatalError(const char* format, ...);
void Sys_Quit();

// You can do almost anything with:
// 1: Strechy buffers
// 2: Pointer/uintptr hash tables (uintptr -> uintptr key-value mapping)
// 3: String intern table (stores a string internally and if it is seen again, it can be fetched) const char* str_intern(const char* str) -> returns ptr to str if seen.

// double underscore (under the hood)
// single underscore (public)

char* FormatBytes(u64 byteCount);

struct KeyCode
{
    enum Type
    {
        Space = 0x20,
        Control = 0x11,
        Enter = 0x0D,
        Shift = 0x10,
        Alt = 0x12,
        Capslock = 0x14,
        Up = 0x26,
        Left = 0x25,
        Right = 0x27,
        Down = 0x28,
        Count
    };
};

// Memory arenas
typedef struct
{
    const char* name;
    u8* base_ptr;
    size_t size;
    size_t mem_used;

    u32 temp_count; // the amount of times we allocated some memory that should be deleted again
} MemoryArena;

typedef struct
{
    MemoryArena* arena;
    size_t mem_used;
} TemporaryMemory;

inline void* GrowArena(MemoryArena* a, size_t size)
{
    assert(a->mem_used + size <= a->size);
    u8* result = a->base_ptr + a->mem_used;
    a->mem_used += size;
    return (void*)result;
}

inline void AllocateArena(MemoryArena* a, size_t size, void* base_ptr, const char* name)
{
    a->size = size;
    a->base_ptr = (u8*)base_ptr;
    a->mem_used = 0;
    a->name = name;
    a->temp_count = 0;
}

// At the end of a frame we want to make sure that we didn't forget to
// reset some temporary memory, so we call this function on that specific arena.
inline void CheckArena(MemoryArena* a)
{
    assert(a->temp_count == 0);
}

// We use this function to set some temporary memory.
// The function takes the memory arena, and saves how much memory was used
// when we wanted to start the temporary memory pool.
inline TemporaryMemory BeginTemporaryMemory(MemoryArena* a)
{
    TemporaryMemory result;
    result.arena = a;
    result.mem_used = a->mem_used;
    a->temp_count++;
    return result;
}

// When we finished using some temporary memory pool, we want to 'spring back'
// to where the memory was pointing when we started.
// we saved this information in the 'temporary_memory' struct, so we can just set the
// memory used back to this point.
inline void EndTemporaryMemory(TemporaryMemory temp)
{
    MemoryArena* arena = temp.arena;
    assert(arena->temp_count > 0);
    assert(arena->mem_used >= temp.mem_used);
    arena->mem_used = temp.mem_used; // revert to the start;
    arena->temp_count--;
}

#define PushArray(arena, count, type) (type*)PushSize(arena, (count) * sizeof(type))
#define PushStruct(arena, type) (type*)PushSize((arena), sizeof(type))

inline void* PushSize(MemoryArena* arena, size_t initSize)
{
    size_t size = initSize;
    if ((arena->mem_used + size) >= arena->size)
    {
        Sys_FatalError("PushSize failed. Tried pushing %s to: %s.\nLimit is: %s\nAmount over limit: %s\n", FormatBytes(size), arena->name, FormatBytes(arena->size), FormatBytes((arena->mem_used + size) - arena->size));
    }
    void* result = arena->base_ptr + arena->mem_used;
    arena->mem_used += size;
    assert(size >= initSize);
    return result;
}

inline u32 PushString(MemoryArena* arena, const char* string)
{
    if (string == NULL)
        return 0;
    char* arenaString = (char*)PushSize(arena, strlen(string) + 1);
    strcpy(arenaString, string);
    return arenaString - (char*)arena->base_ptr;
}


inline void SubArena(MemoryArena* result, MemoryArena* arena, size_t size, const char* name)
{
    result->size = size;
    result->base_ptr = (u8*)PushSize(arena, size);
    result->mem_used = 0;
    result->temp_count = 0;
    result->name = 0;
}

inline u32 GetArenaSizeRemaining(MemoryArena* arena)
{
    return arena->size - arena->mem_used;
}

struct MemoryPools
{
    MemoryArena persistent;
    MemoryArena transient;
};

typedef struct
{
    f32 v;
} vec1_t;

typedef struct
{
    f32 u, v;
} vec2_t;

struct vec3_t
{
    union {
        struct
        {
            f32 x, y, z;
        };

        f32 v[3];
    };

    f32& operator[](s32 index)
    {
        return v[index];
    }

    f32 operator[](s32 index) const
    {
        return v[index];
    }
};

struct vec4_t
{
    union {
        struct
        {
            f32 x, y, z, w;
        };

        f32 v[4];
    };

    f32& operator[](s32 index)
    {
        return v[index];
    }

    f32 operator[](s32 index) const
    {
        return v[index];
    }
};

typedef struct
{
    u32 w, h, d;
} uint3_t;

struct m3x3
{
    f32 E[3][3];
};

struct m4x4
{
    f32 E[4][4];
};

//
inline u32 ComputeMipCount(u32 w, u32 h, u32 d)
{
    u32 mipCount = 1;
    while (h > 1 || w > 1 || d > 1)
    {
        h = MAX(h >> 1, 1);
        w = MAX(w >> 1, 1);
        d = MAX(d >> 1, 1);
        ++mipCount;
    }
    return mipCount;
}

inline u32 ComputeMipCount(u32 h, u32 w)
{
    u32 mipCount = 1;
    while (h > 1 || w > 1)
    {
        h = MAX(h >> 1, 1);
        w = MAX(w >> 1, 1);
        ++mipCount;
    }
    return mipCount;
}

inline u32 ComputeMipCount(u32 v)
{
    u32 mipCount = 1;
    while (v > 1)
    {
        v = MAX(v >> 1, 1);
        ++mipCount;
    }
    return mipCount;
}

struct Image
{
    s32 width, height, channels;
    void* data;
    u32 index;
    u32 hash;
    Image* next;
    const char* fileName; // TODO(bjarke): image name is not being set at the moment
};

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#define TO_RADIANS(x) ((x)*M_PI / 180.0f)
#define TO_DEGREES(x) ((x) * (180.0f / M_PI))
#define cot(x) (cos(x) / sin(x));

#if defined(_MSC_VER)
#define THREAD_LOCAL __declspec(thread)
#else
#define THREAD_LOCAL thread_local
#endif

//
// parsing
//

int IsDigit(char c);
int IsAlnum(char c);
int IsNewline(char c);
int IsWhitespace(char c);
const char* SkipWhitespace(const char* ptr);
const char* SkipLine(const char* ptr);
const char* ParseString(const char* buff, char* v);
void PathRemoveFileSpec(char* inout);
void GetDirectoryPath(char* output, const char* input);
void PathCombine(char* output, const char* dir, const char* fileName);
void GetFileName(char* output, const char* input);
void StripFileExtension(char* inout);

//
// common interface
//

char* fmt(const char* format, ...);
bool FileExists(const char* filePath);
bool ReadEntireFile(void** memoryBuffer, s32* size, const char* filePath);
char* FormatBytes(u64 byteCount);

#include "dynamic_array.h"
#include "math.h"
#include "static_array.h"
#include "static_hash_map.h"
