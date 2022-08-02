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
#include "r_private.h"

#define VOXEL_GROUP_SIZE 4

#pragma pack(push, 1)
// Using vec4_t for 4 bytes of dummy data
struct VoxelizePSData
{
    vec4_t cst_inverseViewportSize[3];
    vec4_t cst_alphaTestedColor;
    float cst_fixedBias;
    u32 cst_flags;
    float dummy[2];
};

struct VoxelizeGSData
{
    vec4_t bounding_min;
    vec4_t bounding_max;
    vec4_t halfPixelSizeCS[3];
};
#pragma pack(pop)

struct VoxelRasterState
{
    ID3D11RasterizerState* rasterState[2]; // 0 is software 1 is conservative
};

struct VoxelPrivate
{
    u32 emittanceBounceRead;
    u32 numMipLevels;
    u32 numEmittanceMaps;
    u32 numVoxelFrames;
    ResourceArray persistent;
    ResourceArray resDependent;
};

extern VoxelPrivate voxelPrivate;

void CreateVoxelRasterStates(VoxelRasterState* state);
void SetVoxelRasterMode(GraphicsPipeline* p, ConsRasterMode::Type mode, VoxelRasterState state);

void OpacityVoxelization_Init();
void VoxelizeOpacity_Draw(RenderCommandQueue* cmdQueue, DrawBuffer* drawBuffer, Scene* scene);

void OpacityMipDownSample_Init();
void OpacityMipDownSample_Run();

void EmittanceVoxelization_Init();
void VoxelizeEmittance_Draw(RenderCommandQueue* cmdQueue, DrawBuffer* drawBuffer, Scene* scene);

void VoxelizationFix_Init();
void VoxelizationFix_Run();

void EmittanceFormatFix_Init();
void EmittanceFormatFix_Run();

void EmittanceOpacityFix_Init();
void EmittanceOpacityFix_Run();

void EmittanceBounce_Init();
void EmittanceBounce_Run(u32 readIndex, u32 writeIndex);

void EmittanceMipDownSample_Init();
void EmittanceMipDownSample_Run(u32 mapIndex);

void VoxelViz_Init();