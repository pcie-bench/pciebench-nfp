/*
 * Copyright (C) 2015 Rolf Neugebauer. All rights reserved.
 * Copyright (C) 2015 Netronome Systems, Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

void __noinline exit(int status)
{
    __asm {
        nop
        nop
        nop
        ctx_arb[kill]
    }
}

unsigned int
_div_mod_32(unsigned int x, unsigned int y, unsigned int mod)
{
    int lo = x;
    unsigned int hi = 0;
    unsigned int i;

    if (y == 0)
        return 0xffffffff;

    for (i = 0; i < 32; i++) {
        hi <<= 1;
        if (lo < 0)
            hi |= 1;

        lo <<= 1;

        if (hi >= y) {
            hi -= y;
            lo |= 1;
        }
    }

    if (mod)
        return hi;
    else
        return lo;
}


int
_mod_u32(int x, int y)
{
    return _div_mod_32(x, y, 1);
}

int
_div_u32(int x, int y)
{
    return _div_mod_32(x, y, 0);
}

typedef union
{
    struct
    {
        int a;
        int b;
        int c;
        int d;
    };
    long long ll;
} fourlong;


typedef unsigned long long                  U64;
#define DECL_MEM(mtype,ctype)       __declspec(mtype)ctype
typedef DECL_MEM(sdram,void)                DRAM_VOID;
typedef DECL_MEM(sdram,U64)                 DRAM_U64;
void ua_set_64_dram(DRAM_VOID *q, unsigned int offset, unsigned long long val)
{
    fourlong v;
    unsigned int shift, a, b, c;

    DRAM_U64 *p = (DRAM_U64 *)((int)q + offset);

    v = *(__declspec(dram) fourlong *)p;

    shift = ((int)p & 7) << 3;

#if __BIGENDIAN

#if __NFP_PERMIT_DRAM_UNALIGNED

    *p =val;
    return;

#else
    if (shift)
    {
        a = val >> 32;
        b = val;

        if (shift == 32)
        {
            v.c = b;
            v.b = a;
        }
        else if (shift < 32)
        {
            c = b << (32 - shift);
            b = dbl_shr(a, b, shift);
            a = a >> shift;

            v.c = v.c & (0xffffffff >> shift) | c;
            v.b = b;
            v.a = v.a & (0xffffffff << (32 - shift)) | a;
        }
        else
        {
            c = b << (64 - shift);
            b = dbl_shr(a, b, shift - 32);
            a = a >> (shift - 32);

            v.d = v.d & (0xffffffff >> (shift - 32)) | c;
            v.c = b;
            v.b = v.b & (0xffffffff << (64 - shift)) | a;
        }
    }
    else
    {
        v.a = val >> 32;
        v.b = val;
    }
#endif
#else

    if (shift)
    {
        a = val;
        b = val >> 32;

        if (shift == 32)
        {
            v.c = b;
            v.b = a;
        }
        else if (shift < 32)
        {
            c = b >> (32 - shift);
            b = dbl_shl(b, a, shift);
            a = a << shift;

            v.c = v.c & (0xffffffff << shift) | c;
            v.b = b;
            v.a = v.a & (0xffffffff >> (32 - shift)) | a;
        }
        else
        {
            c = b >> (64 - shift);
            b = dbl_shl(b, a, shift - 32);
            a = a << (shift - 32);

            v.d = v.d & (0xffffffff << (shift - 32)) | c;
            v.c = b;
            v.b = v.b & (0xffffffff >> (64 - shift)) | a;
        }
    }
    else
    {
        v.a = val;
        v.b = val >> 32;
    }
#endif

    *(__declspec(dram) fourlong *)p = v;
}

/* -*-  Mode:C; c-basic-offset:4; tab-width:4 -*- */
