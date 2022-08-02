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

inline vec3_t
operator*(vec3_t a, vec3_t b)
{
    vec3_t R = { a.x * b.x, a.y + b.y, a.z + b.z };
    return R;
}

inline vec3_t
operator-(vec3_t a)
{
    vec3_t R = { -a.x, -a.y, -a.z };
    return R;
}

inline vec3_t
operator*(vec3_t a, f32 s)
{
    vec3_t R = { a.x * s, a.y * s, a.z * s };
    return R;
}

inline vec3_t
operator/(vec3_t a, s32 s)
{
    vec3_t R = { a.x / s, a.y / s, a.z / s };
    return R;
}

inline vec3_t
operator+(vec3_t a, vec3_t b)
{
    vec3_t R = { a.x + b.x, a.y + b.y, a.z + b.z };
    return R;
}

inline vec3_t
operator+(vec3_t a, f32 v)
{
    vec3_t R = { a.x + v, a.y + v, a.z + v };
    return R;
}

inline vec3_t
operator+(f32 v, vec3_t a)
{
    vec3_t R = { a.x + v, a.y + v, a.z + v };
    return R;
}

inline vec3_t
operator-(vec3_t a, f32 v)
{
    vec3_t R = { a.x - v, a.y - v, a.z - v };
    return R;
}

inline bool
operator==(vec3_t a, vec3_t b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z ? true : false;
}

inline bool
operator==(vec2_t a, vec2_t b)
{
    return a.u == b.u && a.v == b.v ? true : false;
}

inline vec3_t
operator-(f32 v, vec3_t a)
{
    vec3_t R = { v - a.x, v - a.y, v - a.z };
    return R;
}

inline vec3_t
operator-(vec3_t a, vec3_t b)
{
    vec3_t R = { a.x - b.x, a.y - b.y, a.z - b.z };
    return R;
}

inline vec3_t
operator/(vec3_t a, vec3_t b)
{
    vec3_t R = { a.x / b.x, a.y / b.y, a.z / b.z };
    return R;
}

inline f32
length(vec3_t a)
{
    f32 length = sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
    return length;
}

inline vec3_t
norm(vec3_t a)
{
    f32 length = sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
    vec3_t R = { a.x / length, a.y / length, a.z / length };
    return R;
}

inline vec3_t
cross(vec3_t a, vec3_t b)
{
    vec3_t R = { a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x };
    return R;
}

inline vec3_t
normal(vec3_t a, vec3_t b, vec3_t c)
{
    vec3_t R = norm(cross(b * a, b * c));
    return R;
}

inline f32
dot(vec3_t a, vec3_t b)
{
    f32 R = (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
    return R;
}

inline f32
angle(vec3_t a, vec3_t b)
{
    f32 R = acos(dot(a, b) / (length(a) * length(b)));
    return R;
}

static vec3_t
Transform(m4x4 a, vec3_t p, f32 Pw = 1.0f)
{
    vec3_t R;
    R.x = p.x * a.E[0][0] + p.y * a.E[0][1] + p.z * a.E[0][2] + Pw * a.E[0][3];
    R.y = p.x * a.E[1][0] + p.y * a.E[1][1] + p.z * a.E[1][2] + Pw * a.E[1][3];
    R.z = p.x * a.E[2][0] + p.y * a.E[2][1] + p.z * a.E[2][2] + Pw * a.E[2][3];
    return R;
}

inline vec3_t
operator*(m4x4 a, vec3_t p)
{
    vec3_t R = Transform(a, p, 1.0f);
    return R;
}

inline uint3_t
operator/(uint3_t a, s32 b)
{
    uint3_t R = { a.w / b, a.h / b, a.d / b };
    return R;
}

inline m4x4
operator*(m4x4 a, m4x4 b)
{
    m4x4 R = {};

    for (int r = 0; r <= 3; ++r)
    {
        for (int c = 0; c <= 3; ++c)
        {
            for (int i = 0; i <= 3; ++i)
            {
                R.E[r][c] += a.E[r][i] * b.E[i][c];
            }
        }
    }

    return R;
}

inline m4x4
Identity()
{
    m4x4 R = {
        { { 1.0f, 0.0f, 0.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f, 0.0f },
            { 0.0f, 0.0f, 1.0f, 0.0f },
            { 0.0f, 0.0f, 0.0f, 1.0f } },
    };

    return R;
}

inline m4x4
XRotation(f32 angle)
{
    f32 c = cos(angle);
    f32 s = sin(angle);

    m4x4 R = {
        { { 1.0f, 0.0f, 0.0f, 0.0f },
            { 0.0f, c, -s, 0.0f },
            { 0.0f, s, c, 0.0f },
            { 0.0f, 0.0f, 0.0f, 1.0f } },
    };

    return R;
}

inline m4x4
YRotation(f32 angle)
{
    f32 c = cos(angle);
    f32 s = sin(angle);

    m4x4 R = {
        { { c, 0.0f, s, 0.0f },
            { 0.0f, 1.0f, 0.0f, 0.0f },
            { -s, 0.0f, c, 0.0f },
            { 0.0f, 0.0f, 0.0f, 1.0f } },
    };

    return R;
}

inline m4x4
ZRotation(f32 angle)
{
    f32 c = cos(angle);
    f32 s = sin(angle);

    m4x4 R = {
        { { c, -s, 0.0f, 0.0f },
            { s, c, 0.0f, 0.0f },
            { 0.0f, 0.0f, 1.0f, 0.0f },
            { 0.0f, 0.0f, 0.0f, 1.0f } },
    };

    return R;
}

inline m4x4
scale(f32 s)
{
    m4x4 R = {
        { { 1.0f, 0.0f, 0.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f, 0.0f },
            { 0.0f, 0.0f, 1.0f, 0.0f },
            { 0.0f, 0.0f, 0.0f, 1.0f / s } },
    };
    return R;
}

inline m4x4
Transpose(m4x4 a)
{
    m4x4 R;

    for (int j = 0; j <= 3; ++j)
    {
        for (int i = 0; i <= 3; ++i)
        {
            R.E[j][i] = a.E[i][j];
        }
    }

    return R;
}

inline m4x4
PerspectiveProjection(f32 fov, f32 aspect, f32 zn, f32 zf)
{
    /*f32 w = 1 / (fov * tanf(fov/2.0f));
      f32 h = 1 / (tanf(fov/2.0f));

      m4x4 R =
        {
         {{h,      0.0f,     0.0f,             0.0f},
          {0.0f,   w,        0.0f,             0.0f},
          {0.0f,   0.0f,    (zf/(zf-zn)),      1.0f},
          {0.0f,   0.0f,   -((zf*zn)/(zf-zn)), 0.0f}}
          };*/

    f32 h, w, Q, Q2;

    // h = 1 / (aspect * tan(fov/2.0f));
    w = 1 / tan(fov / 2.0f);
    h = w / aspect;
    Q = (zf) / (zn - zf);
    Q2 = zf * zn / (zn - zf);

    m4x4 R = {
        {   { h, 0.0f, 0.0f, 0.0f },
            { 0.0f, w, 0.0f, 0.0f },
            { 0.0f, 0.0f, Q, -1.0f },
            { 0.0f, 0.0f, Q2, 0.0f } }
    };

    return Transpose(R);
}

inline m4x4
LookAt(vec3_t eye, vec3_t at, vec3_t up)
{

    vec3_t z = norm(at - eye);
    vec3_t x = norm(cross(z, up));
    vec3_t y = cross(x, z);

    m4x4 R = {
        { { x.x, x.y, x.z, -dot(x, eye) },
            { y.x, y.y, y.z, -dot(y, eye) },
            { z.x, z.y, z.z, -dot(z, eye) },
            { 0.0f, 0.0f, 0.0f, 1.0f } }
    };

    return R;
}

inline m4x4
Columns3x3(vec3_t x, vec3_t y, vec3_t z)
{
    m4x4 R = {
        { { x.x, y.x, z.x, 0.0f },
            { x.y, y.y, z.y, 0.0f },
            { x.z, y.z, z.z, 0.0f },
            { 0.0f, 0.0f, 0.0f, 1.0f } }
    };
    return R;
}

inline m4x4
Rows3x3(vec3_t x, vec3_t y, vec3_t z)
{
    m4x4 R = {
        { { x.x, x.y, x.z, 0.0f },
            { y.x, y.y, y.z, 0.0f },
            { z.x, z.y, z.z, 0.0f },
            { 0.0f, 0.0f, 0.0f, 1.0f } }
    };
    return R;
}

inline vec3_t
GetColumn(m4x4 a, u32 col)
{
    vec3_t R = { a.E[0][col], a.E[1][col], a.E[2][col] };
    return R;
}

inline vec3_t
GetRow(m4x4 a, u32 row)
{
    vec3_t R = { a.E[row][0], a.E[row][1], a.E[row][2] };
    return R;
}

inline m4x4
Translate(m4x4 a, vec3_t t)
{
    m4x4 R = a;
    R.E[0][3] += t.x;
    R.E[1][3] += t.y;
    R.E[2][3] += t.z;
    return R;
}

inline m4x4
CameraTransform(vec3_t x, vec3_t y, vec3_t z, vec3_t p)
{
    m4x4 R = Rows3x3(x, y, z);
    R = Translate(R, -(R * p));
    return R;
}

m4x4 Invert(const m4x4* input);