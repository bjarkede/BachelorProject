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

RenderCommandQueue* AllocateRenderCommandQueue(MemoryArena* arena, size_t size)
{
    RenderCommandQueue* result = PushStruct(arena, RenderCommandQueue);

    result->base_ptr = (u8*)PushSize(arena, size);
    result->max_size = size;
    result->size = 0;

    return result;
}

#define PushRenderElement(cmdQueue, type) \
    (RenderEntry##type*)__PushRenderElement(cmdQueue, sizeof(RenderEntry##type), RenderEntry::type);

// Whenever we want to push something to our render buffer
// we use this call to get the memory. Then we just need to set the fields
// corresponding to the type we just added.
static void* __PushRenderElement(RenderCommandQueue* cmdQueue, size_t elem_size, RenderEntry::Type type)
{
    assert(cmdQueue->size + elem_size <= cmdQueue->max_size);
    RenderEntryHeader* hdr = (RenderEntryHeader*)(cmdQueue->base_ptr + cmdQueue->size); // get a pointer to the memory
    cmdQueue->size += elem_size; // increment the size with what we just pushed
    hdr->type = type;
    return hdr;
}

void R_PushBox(RenderCommandQueue* cmdQueue, vec4_t color, vec3_t min, vec3_t max)
{
    RenderEntryBox* entry = PushRenderElement(cmdQueue, Box);
    entry->min = min;
    entry->max = max;
    entry->color = color;
}

void R_PushBeginDebugRegion(RenderCommandQueue* cmdQueue, const wchar_t* name)
{
    RenderEntryBeginDebugRegion* entry = PushRenderElement(cmdQueue, BeginDebugRegion);
    entry->name = name;
}

void R_PushEndDebugRegion(RenderCommandQueue* cmdQueue)
{
    PushRenderElement(cmdQueue, EndDebugRegion);
}

void R_PushMesh(RenderCommandQueue* cmdQueue, Scene* scene, Scene* sceneLowRes)
{
    if (scene->valid && sceneLowRes->valid)
    {
        RenderEntryScene* entry = PushRenderElement(cmdQueue, Scene);
        cmdQueue->obj_flags = cmdQueue->obj_flags | scene->meshId;
        entry->scene = scene;
        entry->sceneLowRes = sceneLowRes;
    }
    else
    {
        Sys_FatalError("Tried loading a NULL object.");
    }
}