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

#pragma

#define MAX_CIRCLES 42

#include "../../common/shared.h"

struct Circle
{
    uint32_t numDetectors;
    double elevationAngle, viewAngle, azimuthAngle;
};

struct Disposition
{
    Circle circles[MAX_CIRCLES];
    uint32_t numCircles;
};

struct vec5_t
{
    f32 x, y, z, r, w;
};

static Disposition dispositionTable[] = {
    // 6
    {
        { { 5, 30.0f, 30.0f, 0.0f },
            { 1, 90.0f, 30.0f, 0.0f } },
        2 },
    // 8
    {
        { { 7, 23.5f, 23.5f, 0.0f },
            { 1, 90.0f, 43.0f, 0.0f } },
        2 },
    // 11
    {
        { { 8, 20.9f, 20.9f, 0.0f },
            { 3, 64.1f, 22.3f, 7.5f } },
        2 },
    // 15
    {
        { { 9, 18.0f, 18.0f, 0.0f },
            { 5, 54.0f, 18.0f, 4.0f },
            { 1, 90.0f, 18.0f, 0.0f } },
        3 },
    // 17
    {
        { { 10, 17.2f, 17.2f, 0.0f },
            { 6, 52.2f, 17.8f, 6.0f },
            { 1, 90.0f, 20.0f, 0.0f } },
        3 },
    // 32
    {
        { { 14, 12.5f, 12.5f, 0.0f },
            { 11, 37.9f, 12.9f, 1.2f },
            { 6, 63.6f, 12.9f, 2.7f },
            { 1, 90.0f, 13.4f, 0.0f } },
        4 },
    // 40
    {
        { { 16, 11.0f, 11.0f, 0.0f },
            { 13, 33.2f, 11.2f, 0.9f },
            { 9, 55.6f, 11.2f, 1.5f },
            { 2, 78.3f, 11.5f, 10.0f } },
        4 }
};