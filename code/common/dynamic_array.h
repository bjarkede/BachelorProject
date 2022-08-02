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

template <typename T>
class DynamicArray
{
private:
    T* b;

    size_t len;
    size_t cap;

    void Realloc(size_t new_len, size_t elem_size)
    {
        assert(cap <= (SIZE_MAX - 1) / 2);
        size_t new_cap = CLAMP_MIN(2 * cap, MAX(new_len, 16));
        assert(new_len <= new_cap);
        size_t new_size = new_cap * elem_size;
        T* new_b;
        if (b)
        {
            new_b = (T*)realloc(b, new_size);
        }
        else
        {
            new_b = (T*)malloc(new_size);
            len = 0;
        }
        cap = new_cap;
        b = new_b;
    }

public:
    DynamicArray()
    {
        b = NULL;
        len = cap = 0;
    }
    ~DynamicArray()
    {
        if (b)
        {
            free(b);
            b = NULL;
            len = cap = 0;
        }
    }

    void Fit(size_t num);
    void Reserve(size_t num);
    void Fill(T value);
    u32 Push(T const& obj);
    size_t Length() { return len; }
    size_t Capacity() { return cap; }
    size_t UsedBytes() { return b ? len * sizeof(T) : 0; }
    T* GetEnd() { return b + len; }
    T* GetStart() { return b; }

    void Clear()
    {
        len = 0;
    }

    T& operator[](u32 i)
    {
        assert(i < len);
        return b[i];
    }
};

template <typename T>
void DynamicArray<T>::Fit(size_t new_len)
{
    if (new_len <= cap)
        return;
    Realloc(new_len, sizeof(T));
}

template <typename T>
void DynamicArray<T>::Fill(T value)
{
    for (u32 i = 0; i < len; ++i)
    {
        b[i] = value;
    }
}

template <typename T>
void DynamicArray<T>::Reserve(size_t new_len)
{
    Realloc(new_len, sizeof(T));
    len = new_len;
}

template <typename T>
u32 DynamicArray<T>::Push(T const& obj)
{
    Fit(1 + len);
    b[len++] = obj;
    return len;
}