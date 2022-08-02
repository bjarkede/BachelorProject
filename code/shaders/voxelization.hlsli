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

void VoxelizeGeometryShader(triangle VOut input[3], inout TriangleStream<GOut> os)
{
    float3 posCS[3];
    float3 pos01[3];

    for (uint j = 0; j < 3; ++j)
    {
        posCS[j] = input[j].position.xyz;

        // world-space -> [0;1]
        posCS[j] = ((posCS[j].xyz - g_bounding_min) / (g_bounding_max - g_bounding_min));
        pos01[j] = posCS[j];
        // [0;1] -> [-1;1]
        posCS[j] = (posCS[j].xyz * 2) - 1;
    }

    // Get normal to the triangle
    const float3 A = posCS[1].xyz - posCS[0].xyz;
    const float3 B = posCS[2].xyz - posCS[0].xyz;
    const float3 N = cross(A, B);
    const float3 n = abs(N);
    float3 normalScale = 1.0 / (g_bounding_max - g_bounding_min);
    float3 outputNormal = normalize(N * normalScale);

    GOut output[3];

    float3 lineDir;
    float3x2 inverseSwizzle;
    uint viewportAxis;
    for (uint i = 0; i < 3; ++i)
    {
        // Dominant axis selection
        if (n.x > n.y && n.x > n.z)
        {
            output[i].posCS = float4(posCS[i].yz, 0.0, 1.0);
            viewportAxis = 0;
            lineDir = float3(1.0, 0.0, 0.0);
            inverseSwizzle = float3x2(0, 0, 1, 0, 0, 1);
        }
        else if (n.y > n.x && n.y > n.z)
        {
            output[i].posCS = float4(posCS[i].xz, 0.0, 1.0);
            viewportAxis = 1;
            lineDir = float3(0.0, 1.0, 0.0);
            inverseSwizzle = float3x2(1, 0, 0, 0, 0, 1);
        }
        else if (n.z > n.x && n.z > n.y)
        {
            output[i].posCS = float4(posCS[i].xy, 0.0, 1.0);
            viewportAxis = 2;
            lineDir = float3(0.0, 0.0, 1.0);
            inverseSwizzle = float3x2(1, 0, 0, 1, 0, 0);
        }

        output[i].pos01 = pos01[i];
        output[i].normal = outputNormal;
#ifdef EMITTANCE
        output[i].posWS = input[i].position.xyz;
        output[i].tc = input[i].tc;
#endif
    }

    output[0].viewportAxis = viewportAxis;
    output[1].viewportAxis = viewportAxis;
    output[2].viewportAxis = viewportAxis;

#ifdef CONS_RASTER
    float4 aabb;
    aabb.xy = min(output[0].posCS.xy, min(output[1].posCS.xy, output[2].posCS.xy)) - halfPixelSizeCS[viewportAxis];
    aabb.zw = max(output[0].posCS.xy, max(output[1].posCS.xy, output[2].posCS.xy)) + halfPixelSizeCS[viewportAxis];
    output[0].aabbCS = aabb;
    output[1].aabbCS = aabb;
    output[2].aabbCS = aabb;

    float3 normalCS = cross(output[1].posCS.xyz - output[0].posCS.xyz, output[2].posCS.xyz - output[0].posCS.xyz);
    float signFlip = sign(normalCS.z);

    float3 planes[3];
    for (uint k = 0; k < 3; ++k)
    {
        // compute the planes
        planes[k] = cross(output[k].posCS.xyw - output[(k + 2) % 3].posCS.xyw, output[(k + 2) % 3].posCS.xyw);
        // push back plane
        planes[k].z -= signFlip * dot(halfPixelSizeCS[viewportAxis], abs(planes[k].xy));
    }

    // calculate intersections and update
    float4 finalPos;
    for (uint z = 0; z < 3; ++z)
    {
        finalPos.xyw = cross(planes[z], planes[(z + 1) % 3]);
        finalPos.xyw /= finalPos.w;
        finalPos.z = 0;
        output[z].posCS = finalPos;
    }

    // update 01 position
    float3 normal01 = cross(pos01[1] - pos01[0], pos01[2] - pos01[0]);
    float3 planePoint = pos01[0];

    for (int p = 0; p < 3; ++p)
    {
        float3 linePoint = mul(inverseSwizzle, output[p].posCS.xy) * 0.5 + 0.5;
        output[p].pos01 = IntersectLineAndPlane(linePoint, lineDir, planePoint, normal01);
    }
#endif
    os.Append(output[0]);
    os.Append(output[1]);
    os.Append(output[2]);

    os.RestartStrip();
}

bool IsPointInTriangle(float2 posCS, float2 inverseViewportSize, float4 aabbCS)
{
    // [0;viewportSize] (window-space) -> [0;1] -> [-1;1]
    float2 posNDC = (((posCS.xy * inverseViewportSize) * 2) - 1);
    posNDC.y *= -1; // flip y
    return IsPointInRectangle(posNDC, aabbCS.xy, aabbCS.zw);
}

void ComputeTC(uint3 texDim, float3 pos01, float3 normal, out uint3 tc, out uint3 tcOffsets)
{
    texDim.x /= 6;
    uint3 faceOffsets = normal >= 0.0 ? 1 : 0;
    uint3 faceIndexes = uint3(0, 2, 4) + faceOffsets;
    tcOffsets = texDim.x * faceIndexes;
    tc = uint3(pos01 * texDim);
}