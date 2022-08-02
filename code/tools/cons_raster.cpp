#include "cons_raster.h"
#include <stdio.h>

#define SIGN(v) (v > 0) ? 1 : ((v < 0) ? -1 : 0)

int main(int argc, char** argv)
{
#if 0
    float3 posCSxyw[3] =
    {
        {1.0f, 0.0f, 1.0f},
        {2.0f, 1.0f, 1.0f},
        {3.0f, 0.0f, 1.0f},
    };
    
    float3 posCSxyz[3] =
    {
        {1.0f, 0.0f, 0.0f},
        {2.0f, 1.0f, 0.0f},
        {3.0f, 0.0f, 0.0f},
    };
#else
    float3 posCSxyw[3] = {
        { 3.0f, 0.0f, 1.0f },
        { 2.0f, 1.0f, 1.0f },
        { 1.0f, 0.0f, 1.0f },
    };

    float3 posCSxyz[3] = {
        { 3.0f, 0.0f, 1.0f },
        { 2.0f, 1.0f, 1.0f },
        { 1.0f, 0.0f, 1.0f },
    };
#endif

    float2 hPixel = { 0.1f, 0.1f };

    float3 normalCS = cross(posCSxyz[1] - posCSxyz[0], posCSxyz[2] - posCSxyz[0]);
    float signFlip = SIGN(normalCS.z);

    float3 planes[3];
    float3 outPosCSxyw[3];

    for (int i = 0; i < 3; ++i)
    {
        planes[i] = cross(posCSxyw[i] - posCSxyw[(i + 2) % 3], posCSxyw[(i + 2) % 3]);
        float2 planesXY = { planes[i].x, planes[i].y };
        planes[i].z -= signFlip * dot(hPixel, abs(planesXY));
    }

    for (int i = 0; i < 3; ++i)
    {
        outPosCSxyw[i] = cross(planes[i], planes[(i + 1) % 3]);
        float2 finalPos = { outPosCSxyw[i].x, outPosCSxyw[i].y };
        finalPos /= outPosCSxyw[i].z;
        printf("x: %f, y: %f\n", finalPos.x, finalPos.y);
    }
}