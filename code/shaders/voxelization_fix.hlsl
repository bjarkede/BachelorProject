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

#include "shared.hlsli"

RWTexture3D<float4> emittanceMap : register(u0);

cbuffer CSData
{
    uint cst_maxDistance;
    uint cst_axis;
};

void GetTC(uint3 id, uint i, uint3 dim, out uint3 tcp, out uint3 tcn) 
{
    [flatten] if (cst_axis == 0)
    {
        tcp = uint3(i + (1 * dim.x), id.x, id.y);
        tcn = uint3(i + (0 * dim.x), id.x, id.y);     
    }
    else if (cst_axis == 1)
    {
        tcp = uint3(id.x + (3 * dim.x), i, id.y);
        tcn = uint3(id.x + (2 * dim.x), i, id.y);
    }
    else     
    {
        tcp = uint3(id.x + (5 * dim.x), id.y, i);
        tcn = uint3(id.x + (4 * dim.x), id.y, i);
    }
}

uint GetEnd(uint3 dim) 
{
    [flatten] if (cst_axis == 0)
    {
        return dim.x;
    }
    else if (cst_axis == 1)
    {
        return dim.y;
    }
    else
    {
        return dim.z;
    }
}

[numthreads(8, 8, 1)]
void cs_main(uint3 id : SV_DispatchThreadID)
{
    uint3 dim;
    emittanceMap.GetDimensions(dim.x, dim.y, dim.z);
    dim.x /= 6;
   
    uint start = 0;
    uint isFilling = 0;
    uint iEnd = GetEnd(dim);
    for (uint i = 0; i < iEnd; ++i)
    {
        uint3 tcp, tcn;
        GetTC(id, i, dim, tcp, tcn);
        if (isFilling)
        {
            if (i - start > cst_maxDistance)
            {
                isFilling = 0;
            }
            else if (emittanceMap[tcp].a > 0.0 && emittanceMap[tcn].a == 0.0)
            {
                uint end = i;
                for (uint i2 = start + 1; i2 < end; ++i2)
                {
                    uint3 tcp2, tcn2;
                    GetTC(id, i2, dim, tcp2, tcn2);
                    emittanceMap[tcp2] = float4(0.0, 0.0, 0.0, 1.0);
                    emittanceMap[tcn2] = float4(0.0, 0.0, 0.0, 1.0); 
                }
                uint3 tcStartp, tcStartn, tcEndp, tcEndn;
                GetTC(id, start, dim, tcStartp, tcStartn);
                GetTC(id, end, dim, tcEndp, tcEndn);
                emittanceMap[tcStartp] = float4(0.0, 0.0, 0.0, 1.0);
                emittanceMap[tcEndn] = float4(0.0, 0.0, 0.0, 1.0); 
                isFilling = 0;
            }
        }
        else
        {
            if (emittanceMap[tcn].a > 0.0 && emittanceMap[tcp].a == 0.0)
            {
                start = i;
                isFilling = 1;
            }
        }
    }
}