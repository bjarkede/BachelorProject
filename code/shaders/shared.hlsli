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

#ifndef __SHARED_HLSL__
#define __SHARED_HLSL__

#include "shader_constants.hlsli"

#pragma warning(disable : 4000)

#define PI 3.14159265358979323846f

#define min3(a, b, c) (min(a, min(b, c)))
#define max3(a, b, c) (max(a, max(b, c)))

struct LightEntity
{
    float4 positionWS; // w = radius
    float4 dirWS; // w = cos umbra angle
    float4 color; // w = cos penumbra angle
    float4 params; // x = size in texture space, y = zNear
    float4x4 lightView;
    float4x4 lightProj;
};

struct AABB
{
    float3 min;
    float3 max;
};

static matrix Identity = {
    { 1, 0, 0, 0 },
    { 0, 1, 0, 0 },
    { 0, 0, 1, 0 },
    { 0, 0, 0, 1 }
};

struct Material
{
    float4 specular;
    uint flags; 
};

cbuffer VertexShaderBuffer
{
    float4x4 modelViewMatrix;
    float4x4 projectionMatrix;
};

// original code from Christian Schüler
// http://www.thetenthplanet.de/archives/1180
float3x3 CotangentFrame(float3 N, float3 p, float2 uv)
{
    float3 dp1 = ddx(p);
    float3 dp2 = ddy(p);
    float2 duv1 = ddx(uv);
    float2 duv2 = ddy(uv);

    // solve linear system
    float3 dp2perp = cross(dp2, N);
    float3 dp1perp = cross(N, dp1);
    float3 T = dp2perp * duv1.x + dp1perp * duv2.x; // cotangent
    float3 B = dp2perp * duv1.y + dp1perp * duv2.y; // co-bitangent

    // construct
    float invmax = rsqrt(max(dot(T, T), dot(B, B)));
    float3x3 result = float3x3(T * invmax, B * invmax, N);
    return result;
}

// BEGIN octahedron normal vector encoding
// original code from "A Survey of Efficient Representations for Independent Unit Vectors"
// further improved by Krzysztof Narkowicz and Rune Stubbe

float2 OctWrap(float2 v)
{
    return (1.0 - abs(v.yx)) * (v.xy >= 0.0 ? 1.0 : -1.0);
}

float2 OctEncode(float3 n)
{
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    n.xy = n.z >= 0.0 ? n.xy : OctWrap(n.xy);
    n.xy = n.xy * 0.5 + 0.5;
    return n.xy;
}

float3 OctDecode(float2 f)
{
    f = f * 2.0 - 1.0;
    float3 n = float3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = saturate(-n.z);
    n.xy += n.xy >= 0.0 ? -t : t;
    return normalize(n);
}

// END octahedron normal vector encoding

// @NOTE: untested
float LinearDepth(float depthZoverW, float near, float far)
{
    float linearDepth = (far * near) / (far - depthZoverW * (far - near));
    return linearDepth;
}

// formula 5.14 from RTR 4th ed.
// fwin = pow( max(1 - pow(d / r, 4), 0), 2 )
float PointLightFalloffWindow(float d, float r)
{
    float win = (d / r);
    win *= win;
    win *= win;
    win = max(1.0 - win, 0);
    win *= win;
    return win;
}

// point light fall-off using the inverse square law,
// makes sure no division by 0 can occur
float PointLightFalloffUnbounded(float d, float r)
{
    float falloff = r / max(d, 0.01);
    falloff *= falloff;
    return falloff;
}

// computes the final point light fall-off,
// if d > r, returns 0
float PointLightFalloff(float d, float r)
{
    float dist = PointLightFalloffUnbounded(d, r);
    float window = PointLightFalloffWindow(d, r);
    float falloff = dist * window;
    return falloff;
}

// computes the angular fall-off of a spotlight
// the formula is from "Moving Frostbite to Physically Based Rendering"
float SpotLightDirectionalFalloff(float cosThetaS, float umbraAngle, float penumbraAngle)
{
    float dirFalloff = saturate((cosThetaS - umbraAngle) / (penumbraAngle - umbraAngle));
    dirFalloff *= dirFalloff;
    return dirFalloff;
}

// returns a new vector orthogonal to u,
// making sure the new value doesn't create precision issues
float3 Orthogonal(float3 u)
{
    float3 v = float3(1.0, 0.0, 0.0);
    float3 result = abs(dot(u, v)) > 0.75 ? cross(u, float3(0.0, 1.0, 0.0)) : cross(u, v);
    return result;
}

float3 DecodeTangentSpaceNormal(float2 xy01)
{
    // [0;1] -> [-1;1]
    float2 xy = xy01 * 2.0 - 1.0;
    float z = sqrt(1 - (xy.x * xy.x) - xy.y * xy.y);
    return float3(xy, z);
}

bool IsPointInBox(float3 p, float3 min, float3 max)
{
    return all(p >= min) && all(p <= max);
}

bool IsPointInRectangle(float2 p, float2 min, float2 max)
{
    return all(p >= min) && all(p <= max);
}

//============================================================
// adapted from source at:
// https://keithmaggio.wordpress.com/2011/02/15/math-magician-lerp-slerp-and-nlerp/
float3 slerp(float3 start, float3 end, float percent)
{
    // Dot product - the cosine of the angle between 2 vectors.
    float d = dot(start, end);
    // Clamp it to be in the range of Acos()
    // This may be unnecessary, but floating point
    // precision can be a fickle mistress.
    d = clamp(d, -1.0, 1.0);
    // Acos(dot) returns the angle between start and end,
    // And multiplying that by percent returns the angle between
    // start and the final result.
    float theta = acos(d) * percent;
    float3 RelativeVec = normalize(end - start * d); // Orthonormal basis
    // The final result.
    return ((start * cos(theta)) + (RelativeVec * sin(theta)));
}

// Interleaved Gradient Noise by Jorge Jimenez
// from "Next Generation Post Processing in Call of Duty: Advanced Warfare"
float InterleavedGradientNoise(float2 uv)
{
    float3 magic = float3(0.06711056, 0.00583715, 52.9829189);
    return frac(magic.z * frac(dot(uv, magic.xy)));
}

float InterleavedGradientNoise(float3 uv)
{
    float magic1 = InterleavedGradientNoise(uv.xy);
    float magic2 = InterleavedGradientNoise(float2(magic1, uv.z));
    return magic2;
}

float2x2 CreateRandomRotationMatrix(float3 position)
{
    float angle = InterleavedGradientNoise(position) * 2.0 * PI;
    float sin, cos;
    sincos(angle, sin, cos);
    float2x2 result = float2x2(cos, -sin, sin, cos);
    return result;
}

// Percentage Closer Filtering shadow mapping
float PCF_Visibility(Texture2DArray map, SamplerComparisonState compSampler, float3 positionWS, LightEntity light, float slice, float bias)
{
    float4 positionLCS = float4(positionWS, 1.0);
    positionLCS = mul(positionLCS, light.lightView);
    positionLCS = mul(positionLCS, light.lightProj);
    positionLCS /= positionLCS.w; // light clip space -> NDC
    if (!IsPointInBox(positionLCS.xyz, float3(-1, -1, 0), float3(1, 1, 1)))
    {
        return 1.0;
    }
    float2 shadowMapTC = positionLCS.xy * 0.5 + 0.5; // shadow map texture space
    shadowMapTC.y = 1.0 - shadowMapTC.y; // fix tc

    float result = map.SampleCmpLevelZero(compSampler, float3(shadowMapTC, slice), positionLCS.z + bias);
    return result;
}

// compute the final intensity of light at point positionWS
float ComputeLightFalloff(LightEntity light, float3 positionWS)
{
    float d = distance(light.positionWS.xyz, positionWS);
    float r = light.positionWS.w;
    float umbra = light.dirWS.w;
    float penumbra = light.color.w;
    float3 L = normalize(light.positionWS.xyz - positionWS);
    float cosThetaS = dot(light.dirWS.xyz, -L);

    float distFalloff = PointLightFalloff(d, r);
    float dirFalloff = SpotLightDirectionalFalloff(cosThetaS, umbra, penumbra);
    float falloff = distFalloff * dirFalloff;
    return falloff;
}

// Schlick's approximation of the Fresnel reflectance equation
float SchlickFresnel(float NL)
{
    const float F0 = 0.04; // good value for fabric, stone, plastic, ...
    const float F90 = 1.0;
    float u = NL;
    float u2 = u * u;
    float u3 = u2 * u;
    float u5 = u3 * u2;
    float f = F0 + (F90 - F0) * (1.0 - u5);
    return f;
}

// combination of Lambert diffuse BRDF and Phong specular BRDF
float3 Shade(LightEntity light, float3 positionWS, float3 N, float3 V, float3 albedoColor, float3 specColor, float specPower, float vis)
{
    float falloff = ComputeLightFalloff(light, positionWS);

    float3 L = normalize(light.positionWS.xyz - positionWS);
    float3 R = reflect(-L, N);
    float NL = max(0.0, dot(N, L));
    float RV = max(0.0, dot(R, V));

    float3 Li = vis * falloff * light.color.xyz;
    float3 diffuse = albedoColor;
    float3 specular = specColor * pow(RV, specPower);
    float fresnel = SchlickFresnel(NL);

    float3 result = ((1.0 - fresnel) * diffuse + fresnel * specular) * Li * NL;
    return result;
}

// Lambert diffuse BRDF only
float3 ShadeDiffuse(LightEntity light, float3 positionWS, float3 N, float3 albedoColor, float vis)
{
    float falloff = ComputeLightFalloff(light, positionWS);

    float3 L = normalize(light.positionWS.xyz - positionWS);
    float NL = max(0.0, dot(N, L));

    float3 Li = vis * falloff * light.color.xyz;
    float3 diffuse = albedoColor;

    float3 result = diffuse * Li * NL;
    return result;
}

float ConeAngleFromSpecularExponent(float exponent)
{
    float angle = sqrt(2.0 / (exponent + 2.0));
    return angle;
}

float ConeRatioFromAperture(float aperture)
{
    float ratio = 2.0 * tan(aperture / 2.0);
    return ratio;
}

// packs 4 normalized floats into an unsigned integer
uint PackUnorm4x8(float4 input)
{
    input = saturate(input); // [0;1]
    uint4 bytes = uint4(input * 255.0); // [0;255]
    uint result = (bytes.w << 24u) | (bytes.z << 16u) | (bytes.y << 8u) | bytes.x;
    return result;
}

// unpacks 4 normalized floats from an unsigned integer
float4 UnpackUnorm4x8(uint input)
{
    uint4 bytes = uint4(input, input >> 8u, input >> 16u, input >> 24u) & 0xFFu;
    float4 result = float4(bytes) / 255.0; // [0;1]
    return result;
}

// packs 4 non-normalized floats into an unsigned integer
uint PackUnorm4x8U(float4 input)
{
    uint4 bytes = uint4(input);
    uint result = (bytes.w << 24u) | (bytes.z << 16u) | (bytes.y << 8u) | bytes.x;
    return result;
}

// unpacks 4 non-normalized floats from an unsigned integer
float4 UnpackUnorm4x8U(uint input)
{
    uint4 bytes = uint4(input, input >> 8u, input >> 16u, input >> 24u) & 0xFFu;
    float4 result = float4(bytes);
    return result;
}

// packs a HDR RGB emittance value into our 32-bit compressed HDR format
uint PackCHDR(float3 input)
{
    float Ef = max3(input.r, input.g, input.b);
    Ef = ceil(Ef);
    Ef = clamp(Ef, 1.0, 127.0);
    float3 Cf = (input / Ef) * 255.0;
    Cf = clamp(Cf, 0.0, 255.0);
    uint3 Cu = uint3(Cf);
    uint Eu = uint(Ef);
    uint result = Cu.r | (Cu.g << 8u) | (Cu.b << 16u) | (Eu << 24u) | (1 << 31u);
    return result;
}

// unpacks a HDR RGB emittance value from our 32-bit compressed HDR format
float3 UnpackCHDR(uint input)
{
    uint3 Cu = uint3(input & 0xFFu, (input >> 8u) & 0xFFu, (input >> 16u) & 0xFFu);
    float3 Cf = float3(Cu) / 255.0;
    uint Eu = (input >> 24u) & 0x7Fu;
    float Ef = float(Eu);
    float3 result = Cf * Ef;
    return result;
}

// packs 2 16-bit float values into an unsigned integer
uint PackHDR(float2 input)
{
    uint2 d = f32tof16(input);
    uint result = d.x | (d.y << 16u);
    return result;
}

// unpacks 2 16-bit float values from an unsigned integer
float2 UnpackHDR(uint input)
{
    uint2 d = uint2(input & 0xFFFFu, input >> 16u);
    float2 result = f16tof32(d);
    return result;
}

// writes the atomic average of value into dest at tc, trying maxIterations times at most
void AtomicAverage(RWTexture3D<uint> dest, uint3 tc, float3 value, uint maxIterations)
{
    value = saturate(value) * 255.0;
    uint newValue = PackUnorm4x8U(float4(value, 1.0));
    uint oldValue = 0;
    for (uint i = 0; i < maxIterations; ++i)
    {
        uint curValue;
        InterlockedCompareExchange(dest[tc], oldValue, newValue, curValue);
        if (curValue == oldValue || ((curValue >> 24) == 255))
            break;

        oldValue = curValue;
        float4 oldValueF = UnpackUnorm4x8U(oldValue);
        float4 newValueF;
        newValueF.rgb = oldValueF.rgb * oldValueF.a;
        newValueF.rgb += value.rgb;
        newValueF.rgb /= oldValueF.a + 1.0;
        newValueF.a = oldValueF.a + 1.0;
        newValue = PackUnorm4x8U(newValueF);
    }
}

// writes the atomic addition of value into dest at tc, trying maxIterations times at most
bool AtomicAddHDR(RWTexture3D<uint> dest, uint3 tc, float2 value, uint maxIterations)
{
    uint newValue = PackHDR(value);
    uint oldValue = 0;
    for (uint i = 0; i < maxIterations; ++i)
    {
        uint curValue;
        InterlockedCompareExchange(dest[tc], oldValue, newValue, curValue);
        if (curValue == oldValue)
        {
            return true;
        }

        oldValue = curValue;
        float2 oldValueF = UnpackHDR(oldValue);
        float2 newValueF = value + oldValueF;
        newValue = PackHDR(newValueF);
    }

    return false;
}

// writes the atomic addition of value into rg and ba at tc and atomically increments counter at tc
void AtomicAddHDR(RWTexture3D<uint> rg, RWTexture3D<uint> ba, RWTexture3D<uint> counter, uint3 tc, float4 value)
{
    if (AtomicAddHDR(rg, tc, value.rg, uint(20)))
    {
        AtomicAddHDR(ba, tc, value.ba, uint(20));
        InterlockedAdd(counter[tc], 1);
    }
}

float3 IntersectLineAndPlane(float3 linePoint, float3 lineDir, float3 planePoint, float3 planeNormal)
{
    float d = dot(planePoint - linePoint, planeNormal) / dot(lineDir, planeNormal);
    return linePoint + lineDir * d;
}

float3 GetWorldSpacePositionFromDepth(float4x4 invProjectionMatrix, float z, float2 uv)
{
    float x = uv.x * 2.0 - 1.0;
    float y = (1.0 - uv.y) * 2.0 - 1.0;

    float4 position = mul(float4(x, y, z, 1.0), invProjectionMatrix);
    float3 result = position.xyz / position.w;
    return result;
}

float VoxelSizeMip0(float3 gridDim, float3 dir)
{
    const float epsilon = 1.0 / float(1 << 20);
    float3 voxelDimOverDir = 1.0 / (gridDim * max(abs(dir), epsilon));
    float result = min3(voxelDimOverDir.x, voxelDimOverDir.y, voxelDimOverDir.z);
    return result;
}

float2 ClipLineSegmentToBox(float2 boxMin, float2 boxMax, float2 segStart, float2 segEnd)
{
    const float eps = 1 << 20;
    float2 delta = segEnd - segStart;
    float2 bMin = boxMin - segStart - eps;
    float2 bMax = boxMax - segStart + eps;
    if (delta.x > bMax.x)
    {
        delta *= bMax.x / delta.x;
    }
    if (delta.y > bMax.y)
    {
        delta *= bMax.y / delta.y;
    }

    if (delta.x < bMin.x)
    {
        delta *= bMin.x / delta.x;
    }
    if (delta.y < bMin.y)
    {
        delta *= bMin.y / delta.y;
    }
    return segStart + delta;
}

float2 NDCToTC(float2 ndc)
{
    float2 result = ndc.xy * 0.5 + 0.5;
    result.y = 1.0 - result.y;
    return result;
}

#endif