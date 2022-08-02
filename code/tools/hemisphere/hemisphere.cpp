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

#include <Windows.h>
#include <assert.h>
#include <iostream>
#include <math.h>
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hemisphere.h"

//
// This tool generates cones to cover the hemisphere and encodes them using:
// - a unit-length direction vector
// - 2 * tan(aperture / 2) for the angle
// - the cosine weight
//
// It then writes out the data as HLSL code ready to be pasted.
// The data comes from tables published in
// "The packing of circles on a hemisphere"
// by J. Appelbaum and Y. Weiss
//

static vec5_t Transform(m3x3 a, vec5_t p, float Pw = 1.0f)
{
    vec5_t R;
    R.x = p.x * a.E[0][0] + p.y * a.E[0][1] + p.z * a.E[0][2];
    R.y = p.x * a.E[1][0] + p.y * a.E[1][1] + p.z * a.E[1][2];
    R.z = p.x * a.E[2][0] + p.y * a.E[2][1] + p.z * a.E[2][2];
    R.r = p.r;
    R.w = p.w;
    return R;
}

static vec5_t operator*(m3x3 m, vec5_t p)
{
    vec5_t R = Transform(m, p);
    return R;
}

static vec5_t Rotate(vec5_t p, float a)
{
    m3x3 RTM = {

        { { cos(a), sin(a), 0.0f },
            { -sin(a), cos(a), 0.0f },
            { 0.0f, 0.0f, 1.0f } },
    };
    return RTM * p;
}

static void CalculateHemisphere(Disposition* d)
{
    vec5_t cones[256] = { 0 };

    u32 coneCount = 0;
    for (u32 i = 0; i < d->numCircles; ++i)
    {
        coneCount += d->circles[i].numDetectors;
    }

    u32 coneIndex = 0;
    float weightsSum = 0.0f;
    for (u32 ci = 0; ci < d->numCircles; ++ci)
    {
        const Circle* circle = &d->circles[ci];
        const float ea = TO_RADIANS(circle->elevationAngle);
        float aa = TO_RADIANS(circle->azimuthAngle);
        for (u32 di = 0; di < circle->numDetectors; ++di)
        {
            vec5_t* cone = &cones[coneIndex++];
            cone->x = cos(ea) * cos(aa);
            cone->y = cos(ea) * sin(aa);
            cone->z = sin(ea);
            cone->r = 2.0f * tan(TO_RADIANS(circle->viewAngle));
            cone->w = sin(ea);
            aa += TO_RADIANS(360.0f) / (float)(circle->numDetectors);
            weightsSum += cone->w;
        }
    }

    assert(coneIndex == coneCount);

    for (u32 ci = 0; ci < coneCount; ++ci)
    {
        cones[ci].w /= weightsSum;
    }

    printf("const uint coneCount = %d;\n\n", (unsigned int)coneCount);

    printf("const float4 cones[coneCount] = {\n");
    for (u32 ci = 0; ci < coneCount; ++ci)
    {
        const vec5_t* c = &cones[ci];
        printf("\t{ %f, %f, %f, %f }%s\n", c->x, c->y, c->z, c->r, ci < coneCount - 1 ? "," : "");
    }
    printf("};\n\n");

    printf("const float weights[coneCount] = {\n\t");
    for (u32 ci = 0; ci < coneCount; ++ci)
    {
        const vec5_t* c = &cones[ci];
        printf("%f%s%s", c->w, ci < coneCount - 1 ? ", " : "", ((ci + 1) % 4 == 0 && ci < coneCount - 1) ? "\n\t" : "");
    }
    printf("\n};\n\n");
}

int main(int argc, char** argv)
{

    for (u32 i = 0; i < ARRAY_LEN(dispositionTable); ++i)
    {
        CalculateHemisphere(&dispositionTable[i]);
        printf("\n\n");
    }
    system("pause");

    return 0;
}
