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

float3 GridDimensions(Texture3D map)
{
    float3 gridDim;
    map.GetDimensions(gridDim.x, gridDim.y, gridDim.z);
    gridDim.x /= 6.0;
    return gridDim;
}

float3 GridDimensions(Texture3D<unorm float4> map)
{
    float3 gridDim;
    map.GetDimensions(gridDim.x, gridDim.y, gridDim.z);
    gridDim.x /= 6.0;
    return gridDim;
}

float SampleOpacityOneMip(Texture3D map, SamplerState samp, float lod, float3 spherePos, float3 dir)
{
    float3 tc = spherePos;
    float halfVoxelSizeX = 0.5 / GridDimensions(map).x;
    tc.x = clamp(tc.x, halfVoxelSizeX, 1.0 - halfVoxelSizeX);
    tc.x /= 6.0;
    float3 weights = dir * dir;
    float3 faceOffsets = dir >= 0.0 ? 0.0 : 1.0;
    float3 tcOffsets = (float3(0.0, 2.0, 4.0) + faceOffsets) / 6.0;
    float3 texData;
    texData.x = map.SampleLevel(samp, tc + float3(tcOffsets.x, 0.0, 0.0), lod).r;
    texData.y = map.SampleLevel(samp, tc + float3(tcOffsets.y, 0.0, 0.0), lod).r;
    texData.z = map.SampleLevel(samp, tc + float3(tcOffsets.z, 0.0, 0.0), lod).r;
    float result = dot(texData, weights);
    return result;
}

float4 SampleEmittanceOneMip(EmittanceTexture3D map, SamplerState samp, float lod, float3 spherePos, float3 dir)
{
    float3 tc = spherePos;
    float halfVoxelSizeX = 0.5 / GridDimensions(map).x;
    tc.x = clamp(tc.x, halfVoxelSizeX, 1.0 - halfVoxelSizeX);
    tc.x /= 6.0;
    float3 weights = dir * dir;
    float3 faceOffsets = dir >= 0.0 ? 0.0 : 1.0;
    float3 tcOffsets = (float3(0.0, 2.0, 4.0) + faceOffsets) / 6.0;
    float4 texDataX = map.SampleLevel(samp, tc + float3(tcOffsets.x, 0.0, 0.0), lod);
    float4 texDataY = map.SampleLevel(samp, tc + float3(tcOffsets.y, 0.0, 0.0), lod);
    float4 texDataZ = map.SampleLevel(samp, tc + float3(tcOffsets.z, 0.0, 0.0), lod);
    float4 result = texDataX * weights.x + texDataY * weights.y + texDataZ * weights.z;
    return result;
}

float SampleOpacity(Texture3D map, SamplerState samp, float lod, float3 spherePos, float3 dir)
{
    float lowMip = floor(lod);
    float lowValue = SampleOpacityOneMip(map, samp, lowMip, spherePos, dir);
    float result = lowValue;

    if (lowMip != lod)
    {
        float highMip = ceil(lod);
        float highValue = SampleOpacityOneMip(map, samp, highMip, spherePos, dir);
        result = lerp(lowValue, highValue, frac(lod));
    }

    return result;
}

float4 SampleEmittance(EmittanceTexture3D map, SamplerState samp, float lod, float3 spherePos, float3 dir)
{
    float lowMip = floor(lod);
    float4 lowValue = SampleEmittanceOneMip(map, samp, lowMip, spherePos, dir);
    float4 result = lowValue;

    if (lowMip != lod)
    {
        float highMip = ceil(lod);
        float4 highValue = SampleEmittanceOneMip(map, samp, highMip, spherePos, dir);
        result = lerp(lowValue, highValue, frac(lod));
    }

    return result;
}

float CorrectOpacity(float opacity, float factor)
{
    float result = 1.0 - pow(1.0 - saturate(opacity), factor);
    return result;
}

struct ConeTrace
{
    EmittanceTexture3D emittanceMap;
    Texture3D opacityMap;
    SamplerState samp;
    float3 origin;
    float3 dir;
    float coneRatio; // 2 * tan(aperture / 2) = coneDiameter / coneHeight
    float3 normal;
    float stepScale;
    float mipBias;
    float maxDiameter; // 1.0 means mip 0 sized
    float opacityThreshold;
    float distanceScale;
};

float TraceOpacityCone(ConeTrace input)
{
    float3 gridDim = GridDimensions(input.opacityMap);
    float normalStepSize = VoxelSizeMip0(gridDim, input.dir);
    float invNormalStepSize = 1.0 / normalStepSize;
    float minDist = normalStepSize * 1.0625;
    float dist = minDist;
    float3 spherePos = input.origin + input.dir * minDist;
    float stepLength = normalStepSize; // so we don't correct the first voxel fetch

    float accOpacity = 0.0;
    float occlusion = 0.0;
    while (IsPointInBox(spherePos, float3(0, 0, 0), float3(1, 1, 1)) && accOpacity < input.opacityThreshold)
    {
        float sphereDiameter = max(dist * input.coneRatio, minDist);
        float step = sphereDiameter * input.stepScale;
        float lodLinear = sphereDiameter * invNormalStepSize;
        float lod = log2(lodLinear) + input.mipBias;
        float vxlOpacity = SampleOpacity(input.opacityMap, input.samp, lod, spherePos, input.dir);

        float correctionFactor = stepLength / normalStepSize;
        vxlOpacity = CorrectOpacity(vxlOpacity, correctionFactor);
        accOpacity += (1.0 - accOpacity) * vxlOpacity;

        float vxlOcclusion = vxlOpacity / (1.0 + dist * input.distanceScale);
        occlusion += (1.0 - occlusion) * vxlOcclusion;

        dist += step;
        spherePos += input.dir * step;
        stepLength = step;
    }

    return occlusion;
}

float ComputeAmbientOcclusion(ConeTrace input)
{
    const uint coneCount = 17;
    const float4 cones[coneCount] = {
        { 0.955278, 0.000000, 0.295708, 0.619103 },
        { 0.772836, 0.561499, 0.295708, 0.619103 },
        { 0.295197, 0.908524, 0.295708, 0.619103 },
        { -0.295197, 0.908524, 0.295708, 0.619103 },
        { -0.772836, 0.561498, 0.295708, 0.619103 },
        { -0.955278, -0.000000, 0.295708, 0.619103 },
        { -0.772836, -0.561499, 0.295708, 0.619103 },
        { -0.295197, -0.908524, 0.295708, 0.619103 },
        { 0.295197, -0.908524, 0.295708, 0.619103 },
        { 0.772837, -0.561498, 0.295708, 0.619103 },
        { 0.609550, 0.064066, 0.790155, 0.642130 },
        { 0.249292, 0.559918, 0.790155, 0.642130 },
        { -0.360258, 0.495852, 0.790155, 0.642130 },
        { -0.609550, -0.064066, 0.790155, 0.642130 },
        { -0.249292, -0.559918, 0.790155, 0.642130 },
        { 0.360258, -0.495852, 0.790155, 0.642130 },
        { -0.000000, -0.000000, 1.000000, 0.727940 }
    };
    const float weights[coneCount] = {
        0.033997, 0.033997, 0.033997, 0.033997,
        0.033997, 0.033997, 0.033997, 0.033997,
        0.033997, 0.033997, 0.090843, 0.090843,
        0.090843, 0.090843, 0.090843, 0.090843,
        0.114969
    };

    float3 tangent = normalize(Orthogonal(input.normal));
    float3 bitangent = cross(input.normal, tangent);
    float3x3 tangentToWorld = float3x3(tangent, bitangent, input.normal);

    ConeTrace trace;
    trace.opacityMap = input.opacityMap;
    trace.samp = input.samp;
    trace.origin = input.origin;
    trace.stepScale = input.stepScale;
    trace.mipBias = input.mipBias;
    trace.opacityThreshold = input.opacityThreshold;
    trace.distanceScale = input.distanceScale;

    float totalOcclusion = 0.0;
    for (uint i = 0; i < coneCount; ++i)
    {
        trace.dir = normalize(mul(cones[i].xyz, tangentToWorld));
        trace.coneRatio = cones[i].w;
        totalOcclusion += weights[i] * TraceOpacityCone(trace);
    }

    return totalOcclusion;
}

float3 TraceEmittanceCone(ConeTrace input, out float occlusion)
{
    float3 gridDim = GridDimensions(input.emittanceMap);
    float normalStepSize = VoxelSizeMip0(gridDim, input.dir);
    float invNormalStepSize = 1.0 / normalStepSize;
    float minDist = normalStepSize * 1.0625;
    float dist = minDist;
    float3 spherePos = input.origin + input.dir * minDist;
    float stepLength = normalStepSize; // so we don't correct the first voxel fetch
    float maxDiameter = input.maxDiameter * normalStepSize;
    input.coneRatio = clamp(input.coneRatio, 0.08, 2.0);

    float4 acc = 0.0; // accumulated light + opacity
    occlusion = 0.0;
    while (IsPointInBox(spherePos, float3(0, 0, 0), float3(1, 1, 1)) && acc.a < input.opacityThreshold)
    {
        float sphereDiameter = clamp(dist * input.coneRatio, normalStepSize, maxDiameter);
        float step = sphereDiameter * input.stepScale;
        float lodLinear = sphereDiameter * invNormalStepSize;
        float lod = log2(lodLinear) + input.mipBias;
        float4 vxlData = SampleEmittance(input.emittanceMap, input.samp, lod, spherePos, input.dir);

        float correctionFactor = stepLength / normalStepSize;
        vxlData.rgb *= correctionFactor;
        vxlData.a = CorrectOpacity(vxlData.a, correctionFactor);

        float accVis = 1.0 - acc.a;
        acc += accVis * vxlData;

        float vxlOcclusion = vxlData.a / (1.0 + dist * input.distanceScale);
        occlusion += (1.0 - occlusion) * vxlOcclusion;

        dist += step;
        spherePos += input.dir * step;
        stepLength = step;
    }

    return acc.rgb;
}

float3 ComputeIndirectDiffuse(ConeTrace input, out float occlusion)
{
#if CONE_COUNT == 40
    const uint coneCount = 40;
    const float4 cones[coneCount] = {
        { 0.981627, 0.000000, 0.190809, 0.388761 },
        { 0.906905, 0.375652, 0.190809, 0.388761 },
        { 0.694115, 0.694115, 0.190809, 0.388761 },
        { 0.375652, 0.906905, 0.190809, 0.388761 },
        { -0.000000, 0.981627, 0.190809, 0.388761 },
        { -0.375653, 0.906905, 0.190809, 0.388761 },
        { -0.694115, 0.694115, 0.190809, 0.388761 },
        { -0.906905, 0.375652, 0.190809, 0.388761 },
        { -0.981627, 0.000000, 0.190809, 0.388761 },
        { -0.906905, -0.375652, 0.190809, 0.388761 },
        { -0.694115, -0.694115, 0.190809, 0.388761 },
        { -0.375653, -0.906905, 0.190809, 0.388761 },
        { 0.000000, -0.981627, 0.190809, 0.388761 },
        { 0.375653, -0.906905, 0.190809, 0.388761 },
        { 0.694115, -0.694115, 0.190809, 0.388761 },
        { 0.906905, -0.375652, 0.190809, 0.388761 },
        { 0.836661, 0.013143, 0.547563, 0.396011 },
        { 0.734719, 0.400454, 0.547563, 0.396011 },
        { 0.464461, 0.696025, 0.547563, 0.396011 },
        { 0.087801, 0.832145, 0.547563, 0.396011 },
        { -0.308973, 0.777631, 0.547563, 0.396011 },
        { -0.634965, 0.544971, 0.547563, 0.396011 },
        { -0.815495, 0.187465, 0.547563, 0.396011 },
        { -0.809204, -0.212987, 0.547563, 0.396011 },
        { -0.617534, -0.564647, 0.547563, 0.396011 },
        { -0.284395, -0.786952, 0.547563, 0.396011 },
        { 0.113896, -0.828977, 0.547563, 0.396011 },
        { 0.486095, -0.681092, 0.547563, 0.396011 },
        { 0.746935, -0.377178, 0.547563, 0.396011 },
        { 0.564773, 0.014789, 0.825113, 0.396011 },
        { 0.423135, 0.374358, 0.825113, 0.396011 },
        { 0.083507, 0.558761, 0.825113, 0.396011 },
        { -0.295195, 0.481714, 0.825113, 0.396011 },
        { -0.535772, 0.179267, 0.825113, 0.396011 },
        { -0.525655, -0.207061, 0.825113, 0.396011 },
        { -0.269579, -0.496503, 0.825113, 0.396011 },
        { 0.112636, -0.553625, 0.825113, 0.396011 },
        { 0.442148, -0.351700, 0.825113, 0.396011 },
        { 0.199706, 0.035214, 0.979223, 0.406905 },
        { -0.199706, -0.035214, 0.979223, 0.406905 }
    };
    const float weights[coneCount] = {
        0.009757, 0.009757, 0.009757, 0.009757,
        0.009757, 0.009757, 0.009757, 0.009757,
        0.009757, 0.009757, 0.009757, 0.009757,
        0.009757, 0.009757, 0.009757, 0.009757,
        0.028000, 0.028000, 0.028000, 0.028000,
        0.028000, 0.028000, 0.028000, 0.028000,
        0.028000, 0.028000, 0.028000, 0.028000,
        0.028000, 0.042193, 0.042193, 0.042193,
        0.042193, 0.042193, 0.042193, 0.042193,
        0.042193, 0.042193, 0.050073, 0.050073
    };
#elif CONE_COUNT == 32
    const uint coneCount = 32;
    const float4 cones[coneCount] = {
        { 0.976296, 0.000000, 0.216440, 0.443389 },
        { 0.879612, 0.423599, 0.216440, 0.443389 },
        { 0.608711, 0.763299, 0.216440, 0.443389 },
        { 0.217246, 0.951818, 0.216440, 0.443389 },
        { -0.217246, 0.951818, 0.216440, 0.443389 },
        { -0.608711, 0.763299, 0.216440, 0.443389 },
        { -0.879612, 0.423599, 0.216440, 0.443389 },
        { -0.976296, 0.000000, 0.216440, 0.443389 },
        { -0.879612, -0.423599, 0.216440, 0.443389 },
        { -0.608711, -0.763299, 0.216440, 0.443389 },
        { -0.217246, -0.951818, 0.216440, 0.443389 },
        { 0.217246, -0.951818, 0.216440, 0.443389 },
        { 0.608711, -0.763299, 0.216440, 0.443389 },
        { 0.879613, -0.423599, 0.216440, 0.443389 },
        { 0.788911, 0.016525, 0.614285, 0.458061 },
        { 0.654740, 0.440420, 0.614285, 0.458061 },
        { 0.312694, 0.724484, 0.614285, 0.458061 },
        { -0.128631, 0.778529, 0.614285, 0.458061 },
        { -0.529116, 0.585397, 0.614285, 0.458061 },
        { -0.761610, 0.206406, 0.614285, 0.458061 },
        { -0.752299, -0.238118, 0.614285, 0.458061 },
        { -0.504138, -0.607041, 0.614285, 0.458061 },
        { -0.095917, -0.783233, 0.614285, 0.458061 },
        { 0.342757, -0.710754, 0.614285, 0.458061 },
        { 0.672608, -0.412616, 0.614285, 0.458061 },
        { 0.444142, 0.020945, 0.895712, 0.458061 },
        { 0.203932, 0.395110, 0.895712, 0.458061 },
        { -0.240210, 0.374165, 0.895712, 0.458061 },
        { -0.444142, -0.020945, 0.895712, 0.458061 },
        { -0.203932, -0.395110, 0.895712, 0.458061 },
        { 0.240210, -0.374165, 0.895712, 0.458061 },
        { -0.000000, -0.000000, 1.000000, 0.476467 }
    };
    const float weights[coneCount] = {
        0.013392, 0.013392, 0.013392, 0.013392,
        0.013392, 0.013392, 0.013392, 0.013392,
        0.013392, 0.013392, 0.013392, 0.013392,
        0.013392, 0.013392, 0.038009, 0.038009,
        0.038009, 0.038009, 0.038009, 0.038009,
        0.038009, 0.038009, 0.038009, 0.038009,
        0.038009, 0.055422, 0.055422, 0.055422,
        0.055422, 0.055422, 0.055422, 0.061875
    };
#elif CONE_COUNT == 17
    const uint coneCount = 17;
    const float4 cones[coneCount] = {
        { 0.955278, 0.000000, 0.295708, 0.619103 },
        { 0.772836, 0.561499, 0.295708, 0.619103 },
        { 0.295197, 0.908524, 0.295708, 0.619103 },
        { -0.295197, 0.908524, 0.295708, 0.619103 },
        { -0.772836, 0.561498, 0.295708, 0.619103 },
        { -0.955278, -0.000000, 0.295708, 0.619103 },
        { -0.772836, -0.561499, 0.295708, 0.619103 },
        { -0.295197, -0.908524, 0.295708, 0.619103 },
        { 0.295197, -0.908524, 0.295708, 0.619103 },
        { 0.772837, -0.561498, 0.295708, 0.619103 },
        { 0.609550, 0.064066, 0.790155, 0.642130 },
        { 0.249292, 0.559918, 0.790155, 0.642130 },
        { -0.360258, 0.495852, 0.790155, 0.642130 },
        { -0.609550, -0.064066, 0.790155, 0.642130 },
        { -0.249292, -0.559918, 0.790155, 0.642130 },
        { 0.360258, -0.495852, 0.790155, 0.642130 },
        { -0.000000, -0.000000, 1.000000, 0.727940 }
    };
    const float weights[coneCount] = {
        0.033997, 0.033997, 0.033997, 0.033997,
        0.033997, 0.033997, 0.033997, 0.033997,
        0.033997, 0.033997, 0.090843, 0.090843,
        0.090843, 0.090843, 0.090843, 0.090843,
        0.114969
    };
#elif CONE_COUNT == 15
    const uint coneCount = 15;
    const float4 cones[coneCount] = {
        { 0.951057, 0.000000, 0.309017, 0.649839 },
        { 0.728552, 0.611327, 0.309017, 0.649839 },
        { 0.165149, 0.936608, 0.309017, 0.649839 },
        { -0.475528, 0.823639, 0.309017, 0.649839 },
        { -0.893701, 0.325280, 0.309017, 0.649839 },
        { -0.893701, -0.325281, 0.309017, 0.649839 },
        { -0.475528, -0.823639, 0.309017, 0.649839 },
        { 0.165149, -0.936608, 0.309017, 0.649839 },
        { 0.728551, -0.611327, 0.309017, 0.649839 },
        { 0.586353, 0.041002, 0.809017, 0.649839 },
        { 0.142198, 0.570325, 0.809017, 0.649839 },
        { -0.498470, 0.311479, 0.809017, 0.649839 },
        { -0.450270, -0.377821, 0.809017, 0.649839 },
        { 0.220188, -0.544985, 0.809017, 0.649839 },
        { -0.000000, -0.000000, 1.000000, 0.649839 }
    };
    const float weights[coneCount] = {
        0.039485, 0.039485, 0.039485, 0.039485,
        0.039485, 0.039485, 0.039485, 0.039485,
        0.039485, 0.103372, 0.103372, 0.103372,
        0.103372, 0.103372, 0.127775
    };
#elif CONE_COUNT == 11
    const uint coneCount = 11;
    const float4 cones[coneCount] = {
        { 0.934204, 0.000000, 0.356738, 0.763726 },
        { 0.660582, 0.660582, 0.356738, 0.763726 },
        { -0.000000, 0.934204, 0.356738, 0.763726 },
        { -0.660582, 0.660582, 0.356738, 0.763726 },
        { -0.934204, -0.000000, 0.356738, 0.763726 },
        { -0.660582, -0.660582, 0.356738, 0.763726 },
        { 0.000000, -0.934204, 0.356738, 0.763726 },
        { 0.660582, -0.660582, 0.356738, 0.763726 },
        { 0.433065, 0.057014, 0.899558, 0.820260 },
        { -0.265908, 0.346538, 0.899558, 0.820260 },
        { -0.167157, -0.403552, 0.899558, 0.820260 }
    };
    const float weights[coneCount] = {
        0.064247, 0.064247, 0.064247, 0.064247,
        0.064247, 0.064247, 0.064247, 0.064247,
        0.162007, 0.162007, 0.162007
    };
#elif CONE_COUNT == 9
    // manually constructed, aperture = 90 degrees, elevation = 45 degrees
    const uint coneCount = 9;
    const float4 cones[coneCount] = {
        { 0.0, 0.0, 1.0, 2.0 },
        { 0.70710678118654752440084436210485, 0.0, 0.70710678118654752440084436210485, 2.0 },
        { -0.70710678118654752440084436210485, 0.0, 0.70710678118654752440084436210485, 2.0 },
        { 0.0, 0.70710678118654752440084436210485, 0.70710678118654752440084436210485, 2.0 },
        { 0.0, -0.70710678118654752440084436210485, 0.70710678118654752440084436210485, 2.0 },
        { 0.57735026918962576450914878050195, 0.57735026918962576450914878050195, 0.57735026918962576450914878050195, 2.0 },
        { 0.57735026918962576450914878050195, -0.57735026918962576450914878050195, 0.57735026918962576450914878050195, 2.0 },
        { -0.57735026918962576450914878050195, 0.57735026918962576450914878050195, 0.57735026918962576450914878050195, 2.0 },
        { -0.57735026918962576450914878050195, -0.57735026918962576450914878050195, 0.57735026918962576450914878050195, 2.0 }
    };
    const float weights[coneCount] = {
        0.15022110482233484500666951280125, 0.10622236189720814437416631089984, 0.10622236189720814437416631089984,
        0.10622236189720814437416631089984, 0.10622236189720814437416631089984, 0.10622236189720814437416631089984,
        0.10622236189720814437416631089984, 0.10622236189720814437416631089984, 0.10622236189720814437416631089984
    };
#elif CONE_COUNT == 8
    const uint coneCount = 8;
    const float4 cones[coneCount] = {
        { 0.917060, 0.000000, 0.398749, 0.869625 },
        { 0.571778, 0.716986, 0.398749, 0.869625 },
        { -0.204065, 0.894067, 0.398749, 0.869625 },
        { -0.826243, 0.397897, 0.398749, 0.869625 },
        { -0.826243, -0.397897, 0.398749, 0.869625 },
        { -0.204065, -0.894067, 0.398749, 0.869625 },
        { 0.571777, -0.716987, 0.398749, 0.869625 },
        { -0.000000, -0.000000, 1.000000, 1.865030 }
    };
    const float weights[coneCount] = {
        0.105176, 0.105176, 0.105176, 0.105176,
        0.105176, 0.105176, 0.105176, 0.263766
    };
#elif CONE_COUNT == 6
    const uint coneCount = 6;
    const float4 cones[coneCount] = {
        { 0.866025, 0.000000, 0.500000, 1.154701 },
        { 0.267617, 0.823639, 0.500000, 1.154701 },
        { -0.700629, 0.509037, 0.500000, 1.154701 },
        { -0.700629, -0.509037, 0.500000, 1.154701 },
        { 0.267617, -0.823639, 0.500000, 1.154701 },
        { -0.000000, -0.000000, 1.000000, 1.154701 }
    };
    const float weights[coneCount] = {
        0.142857, 0.142857, 0.142857, 0.142857,
        0.142857, 0.285714
    };
#elif CONE_COUNT == 5
    // manually constructed, aperture = 60 degrees, elevation = 45 degrees
    // it has more overlap but it should be more tolerant of incorrect average normals
    const uint coneCount = 5;
    const float4 cones[coneCount] = {
        { 0.0, 0.0, 1.0, 1.1547005383792515290182975610039 },
        { 0.70710678118654752440084436210485, 0.0, 0.70710678118654752440084436210485, 1.1547005383792515290182975610039 },
        { -0.70710678118654752440084436210485, 0.0, 0.70710678118654752440084436210485, 1.1547005383792515290182975610039 },
        { 0.0, 0.70710678118654752440084436210485, 0.70710678118654752440084436210485, 1.1547005383792515290182975610039 },
        { 0.0, -0.70710678118654752440084436210485, 0.70710678118654752440084436210485, 1.1547005383792515290182975610039 }
    };
    const float weights[coneCount] = {
        0.26120387496374144251476820691706, 0.18469903125906463937130794827074, 0.18469903125906463937130794827074,
        0.18469903125906463937130794827074, 0.18469903125906463937130794827074
    };
#elif CONE_COUNT == 5
    // manually constructed, aperture = 60 degrees, elevation = 30 degrees
    const uint coneCount = 5;
    const float4 cones[coneCount] = {
        { 0.0, 0.0, 1.0, 1.1547005383792515290182975610039 },
        { 0.86602540378443864676372317075294, 0.0, 0.5, 1.1547005383792515290182975610039 },
        { -0.86602540378443864676372317075294, 0.0, 0.5, 1.1547005383792515290182975610039 },
        { 0.0, 0.86602540378443864676372317075294, 0.5, 1.1547005383792515290182975610039 },
        { 0.0, -0.86602540378443864676372317075294, 0.5, 1.1547005383792515290182975610039 }
    };
    const float weights[coneCount] = {
        0.33333333333333333333333333333333, 0.16666666666666666666666666666667, 0.16666666666666666666666666666667,
        0.16666666666666666666666666666667, 0.16666666666666666666666666666667
    };
#endif

    float3 tangent = normalize(Orthogonal(input.normal));
    float3 bitangent = cross(input.normal, tangent);
    float3x3 tangentToWorld = float3x3(tangent, bitangent, input.normal);

    ConeTrace trace;
    trace.emittanceMap = input.emittanceMap;
    trace.samp = input.samp;
    trace.origin = input.origin;
    trace.stepScale = input.stepScale;
    trace.mipBias = input.mipBias;
    trace.maxDiameter = input.maxDiameter;
    trace.opacityThreshold = input.opacityThreshold;
    trace.distanceScale = input.distanceScale;

    occlusion = 0.0;
    float3 totalEmittance = float3(0.0, 0.0, 0.0);
    for (uint i = 0; i < coneCount; ++i)
    {
        trace.dir = normalize(mul(cones[i].xyz, tangentToWorld));
        trace.coneRatio = cones[i].w;
        float ao;
        totalEmittance += weights[i] * TraceEmittanceCone(trace, ao);
        occlusion += weights[i] * ao;
    }

    return totalEmittance;
}
