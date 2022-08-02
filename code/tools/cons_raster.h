/* date = August 20th 2021 7:29 pm */

#ifndef CONS_RASTER_H
#define CONS_RASTER_H

#include <math.h>

struct float2
{
    union {
        struct
        {
            float x, y;
        };

        float v[2];
    };

    float2()
    {
    }

    explicit float2(float v)
    {
        x = v;
        y = v;
    }

    float2(float x2, float y2)
    {
        x = x2;
        y = y2;
    }

    float& operator[](int index)
    {
        return v[index];
    }

    float operator[](int index) const
    {
        return v[index];
    }

    float2& operator*=(float s)
    {
        x *= s;
        y *= s;
        return *this;
    }

    float2& operator/=(float s)
    {
        x /= s;
        y /= s;
        return *this;
    }

    float2& operator+=(float s)
    {
        x += s;
        y += s;
        return *this;
    }

    float2& operator-=(float s)
    {
        x -= s;
        y -= s;
        return *this;
    }

    float2& operator*=(float2 v)
    {
        x *= v.x;
        y *= v.y;
        return *this;
    }

    float2& operator/=(float2 v)
    {
        x /= v.x;
        y /= v.y;
        return *this;
    }

    float2& operator+=(float2 v)
    {
        x += v.x;
        y += v.y;
        return *this;
    }

    float2& operator-=(float2 v)
    {
        x -= v.x;
        y -= v.y;
        return *this;
    }
};

static float2 operator*(float2 a, float2 b)
{
    return float2(a.x * b.x, a.y * b.y);
}

static float2 operator/(float2 a, float2 b)
{
    return float2(a.x / b.x, a.y / b.y);
}

static float2 operator+(float2 a, float2 b)
{
    return float2(a.x + b.x, a.y + b.y);
}

static float2 operator-(float2 a, float2 b)
{
    return float2(a.x - b.x, a.y - b.y);
}

static float2 operator-(float2 v)
{
    return float2(-v.x, -v.y);
}

static float2 operator+(float2 v, float s)
{
    return float2(v.x + s, v.y + s);
}

static float2 operator-(float2 v, float s)
{
    return float2(v.x - s, v.y - s);
}

static float2 operator*(float2 v, float s)
{
    return float2(v.x * s, v.y * s);
}

static float2 operator/(float2 v, float s)
{
    return float2(v.x / s, v.y / s);
}

static float2 operator+(float s, float2 v)
{
    return float2(s + v.x, s + v.y);
}

static float2 operator-(float s, float2 v)
{
    return float2(s - v.x, s - v.y);
}

static float2 operator*(float s, float2 v)
{
    return float2(s * v.x, s * v.y);
}

static float2 operator/(float s, float2 v)
{
    return float2(s / v.x, s / v.y);
}

static float cross(float2 a, float2 b)
{
    return a.x * b.y - a.y * b.x;
}

static float dot(float2 a, float2 b)
{
    return a.x * b.x + a.y * b.y;
}

static float2 abs(float2 a)
{
    float2 result;
    for (int i = 0; i < 2; ++i)
        result[i] = fabsf(a[i]);
    return result;
}

static float distanceSquared(float2 a, float2 b)
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

static float distance(float2 a, float2 b)
{
    return sqrtf(distanceSquared(a, b));
}

static float lengthSquared(float2 v)
{
    return v.x * v.x + v.y * v.y;
}

static float length(float2 v)
{
    return sqrtf(lengthSquared(v));
}

static float2 normalize(float2 d)
{
    return d / length(d);
}

struct float3
{
    union {
        struct
        {
            float x, y, z;
        };

        float v[3];
    };

    float3()
    {
    }

    explicit float3(float v)
    {
        x = v;
        y = v;
        z = v;
    }

    float3(float x2, float y2, float z2)
    {
        x = x2;
        y = y2;
        z = z2;
    }

    float& operator[](int index)
    {
        return v[index];
    }

    float operator[](int index) const
    {
        return v[index];
    }

    float3& operator*=(float s)
    {
        x *= s;
        y *= s;
        z *= s;
        return *this;
    }

    float3& operator/=(float s)
    {
        x /= s;
        y /= s;
        z /= s;
        return *this;
    }

    float3& operator+=(float s)
    {
        x += s;
        y += s;
        z += s;
        return *this;
    }

    float3& operator-=(float s)
    {
        x -= s;
        y -= s;
        z -= s;
        return *this;
    }

    float3& operator*=(float3 v)
    {
        x *= v.x;
        y *= v.y;
        z *= v.z;
        return *this;
    }

    float3& operator/=(float3 v)
    {
        x /= v.x;
        y /= v.y;
        z /= v.z;
        return *this;
    }

    float3& operator+=(float3 v)
    {
        x += v.x;
        y += v.y;
        z += v.z;
        return *this;
    }

    float3& operator-=(float3 v)
    {
        x -= v.x;
        y -= v.y;
        z -= v.z;
        return *this;
    }
};

static float3 operator*(float3 a, float3 b)
{
    return float3(a.x * b.x, a.y * b.y, a.z * b.z);
}

static float3 operator/(float3 a, float3 b)
{
    return float3(a.x / b.x, a.y / b.y, a.z / b.z);
}

static float3 operator+(float3 a, float3 b)
{
    return float3(a.x + b.x, a.y + b.y, a.z + b.z);
}

static float3 operator-(float3 a, float3 b)
{
    return float3(a.x - b.x, a.y - b.y, a.z - b.z);
}

static float3 operator-(float3 v)
{
    return float3(-v.x, -v.y, -v.z);
}

static float3 operator+(float3 v, float s)
{
    return float3(v.x + s, v.y + s, v.z + s);
}

static float3 operator-(float3 v, float s)
{
    return float3(v.x - s, v.y - s, v.z - s);
}

static float3 operator*(float3 v, float s)
{
    return float3(v.x * s, v.y * s, v.z * s);
}

static float3 operator/(float3 v, float s)
{
    return float3(v.x / s, v.y / s, v.z / s);
}

static float3 operator+(float s, float3 v)
{
    return float3(s + v.x, s + v.y, s + v.z);
}

static float3 operator-(float s, float3 v)
{
    return float3(s - v.x, s - v.y, s - v.z);
}

static float3 operator*(float s, float3 v)
{
    return float3(s * v.x, s * v.y, s * v.z);
}

static float3 operator/(float s, float3 v)
{
    return float3(s / v.x, s / v.y, s / v.z);
}

static float3 cross(float3 a, float3 b)
{
    return float3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

static float dot(float3 a, float3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static float distanceSquared(float3 a, float3 b)
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

static float distance(float3 a, float3 b)
{
    return sqrtf(distanceSquared(a, b));
}

static float lengthSquared(float3 v)
{
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

static float length(float3 v)
{
    return sqrtf(lengthSquared(v));
}

static float3 normalize(float3 d)
{
    return d / length(d);
}

struct float4
{
    union {
        struct
        {
            float x, y, z, w;
        };

        float v[3];
    };

    float4()
    {
    }

    explicit float4(float v)
    {
        x = v;
        y = v;
        z = v;
        w = v;
    }

    float4(float x2, float y2, float z2, float w2)
    {
        x = x2;
        y = y2;
        z = z2;
        w = w2;
    }

    float& operator[](int index)
    {
        return v[index];
    }

    float operator[](int index) const
    {
        return v[index];
    }

    float4& operator*=(float s)
    {
        x *= s;
        y *= s;
        z *= s;
        w *= s;
        return *this;
    }

    float4& operator/=(float s)
    {
        x /= s;
        y /= s;
        z /= s;
        w /= s;
        return *this;
    }

    float4& operator+=(float s)
    {
        x += s;
        y += s;
        z += s;
        w += s;
        return *this;
    }

    float4& operator-=(float s)
    {
        x -= s;
        y -= s;
        z -= s;
        w -= s;
        return *this;
    }

    float4& operator*=(float4 v)
    {
        x *= v.x;
        y *= v.y;
        z *= v.z;
        w *= v.w;
        return *this;
    }

    float4& operator/=(float4 v)
    {
        x /= v.x;
        y /= v.y;
        z /= v.z;
        w /= v.w;
        return *this;
    }

    float4& operator+=(float4 v)
    {
        x += v.x;
        y += v.y;
        z += v.z;
        w += v.w;
        return *this;
    }

    float4& operator-=(float4 v)
    {
        x -= v.x;
        y -= v.y;
        z -= v.z;
        w -= v.w;
        return *this;
    }
};

static float4 operator*(float4 a, float4 b)
{
    return float4(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w);
}

static float4 operator/(float4 a, float4 b)
{
    return float4(a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w);
}

static float4 operator+(float4 a, float4 b)
{
    return float4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
}

static float4 operator-(float4 a, float4 b)
{
    return float4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
}

static float4 operator-(float4 v)
{
    return float4(-v.x, -v.y, -v.z, -v.w);
}

static float4 operator+(float4 v, float s)
{
    return float4(v.x + s, v.y + s, v.z + s, v.w + s);
}

static float4 operator-(float4 v, float s)
{
    return float4(v.x - s, v.y - s, v.z - s, v.w - s);
}

static float4 operator*(float4 v, float s)
{
    return float4(v.x * s, v.y * s, v.z * s, v.w * s);
}

static float4 operator/(float4 v, float s)
{
    return float4(v.x / s, v.y / s, v.z / s, v.w / s);
}

static float4 operator+(float s, float4 v)
{
    return float4(s + v.x, s + v.y, s + v.z, s + v.w);
}

static float4 operator-(float s, float4 v)
{
    return float4(s - v.x, s - v.y, s - v.z, s - v.w);
}

static float4 operator*(float s, float4 v)
{
    return float4(s * v.x, s * v.y, s * v.z, s * v.w);
}

static float4 operator/(float s, float4 v)
{
    return float4(s / v.x, s / v.y, s / v.z, s / v.w);
}

static float dot(float4 a, float4 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

float2 GetNormal(float2 dir)
{
    return normalize(float2(-dir.y, dir.x));
}

float2 GetFixedNormal(float2 normal, float2 center, float2 linePoint)
{
    float far = distance(linePoint + normal, center);
    float near = distance(linePoint, center);
    return far >= near ? normal : -normal;
}

float GetEdgeDistance(float2 normal, float2 halfPixelSize)
{
    // return the length of the worst-case half-diagonal
    return fabsf(dot(normal, halfPixelSize));
}

float2 GetLineIntersection(float2 L0P0, float2 L0P1, float2 L1P0, float2 L1P1)
{
    float x1 = L0P0.x;
    float y1 = L0P0.y;
    float x2 = L0P1.x;
    float y2 = L0P1.y;
    float x3 = L1P0.x;
    float y3 = L1P0.y;
    float x4 = L1P1.x;
    float y4 = L1P1.y;
    float D = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
    float t1 = x1 * y2 - y1 * x2;
    float t2 = x3 * y4 - y3 * x4;
    float x = t1 * (x3 - x4) - (x1 - x2) * t2;
    float y = t1 * (y3 - y4) - (y1 - y2) * t2;
    return float2(x, y) / D;
}

void ExtendTriangle(float2& V0x, float2& V1x, float2& V2x, float2 V0, float2 V1, float2 V2, float2 halfPixelSize)
{
    float2 N01 = GetNormal(V1 - V0);
    float2 N12 = GetNormal(V2 - V1);
    float2 N20 = GetNormal(V0 - V2);
    N01 *= GetEdgeDistance(N01, halfPixelSize);
    N12 *= GetEdgeDistance(N12, halfPixelSize);
    N20 *= GetEdgeDistance(N20, halfPixelSize);
    N01 = GetFixedNormal(N01, V2, 0.5 * (V0 + V1));
    N12 = GetFixedNormal(N12, V0, 0.5 * (V1 + V2));
    N20 = GetFixedNormal(N20, V1, 0.5 * (V2 + V0));
    float2 L0P0 = V0 + N01;
    float2 L0P1 = V1 + N01;
    float2 L1P0 = V1 + N12;
    float2 L1P1 = V2 + N12;
    float2 L2P0 = V2 + N20;
    float2 L2P1 = V0 + N20;
    V0x = GetLineIntersection(L0P0, L0P1, L1P0, L1P1);
    V1x = GetLineIntersection(L1P0, L1P1, L2P0, L2P1);
    V2x = GetLineIntersection(L2P0, L2P1, L0P0, L0P1);
}

void ExtendTriangleV2(float4& V0x, float4& V1x, float4& V2x, float4 V0, float4 V1, float4 V2, float2 halfPixelSize)
{
}

#endif //CONS_RASTER_H
