//   
//   BSD 3-Clause License
//   
//   Copyright (c) 2020, Kurt Kennett
//   All rights reserved.
//   
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions are met:
//   
//   1. Redistributions of source code must retain the above copyright notice, this
//      list of conditions and the following disclaimer.
//   
//   2. Redistributions in binary form must reproduce the above copyright notice,
//      this list of conditions and the following disclaimer in the documentation
//      and/or other materials provided with the distribution.
//   
//   3. Neither the name of the copyright holder nor the names of its
//      contributors may be used to endorse or promote products derived from
//      this software without specific prior written permission.
//   
//   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
//   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
//   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
//   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
//   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
//   SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
//   CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
//   OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
#include <lib/k2mem.h>

void 
K2MEM_Copy(
    void *          apTarget,
    void const *    apSrc,
    UINT32          aByteCount
    )
{
    UINT32 inc;

    if ((aByteCount == 0) || (apSrc == (void const*)apTarget))
        return;

    if (((((UINT32)apTarget) & 7) == 0) && (aByteCount > 7))
    {
        if ((((UINT32)apSrc) & 7) == 0)
        {
            // 64-bit target, 64-bit src
            inc = aByteCount & ~7;
            K2MEM_Copy64(apTarget, apSrc, inc);
            if (inc == aByteCount)
                return;
            apTarget = (void *)(((UINT32)apTarget) + inc);
            apSrc = (void const *)(((UINT32)apSrc) + inc);
            aByteCount &= 7;
        }
        else if ((((UINT32)apSrc) & 3) == 0)
        {
            // 64-bit target, 32-bit src
            inc = aByteCount & ~3;
            K2MEM_Copy32(apTarget, apSrc, inc);
            if (inc == aByteCount)
                return;
            apTarget = (void *)(((UINT32)apTarget) + inc);
            apSrc = (void const *)(((UINT32)apSrc) + inc);
            aByteCount &= 3;
        }
        else if ((((UINT32)apSrc) & 1) == 0)
        {
            // 64-bit target, 16-bit src
            inc = aByteCount & ~1;
            K2MEM_Copy16(apTarget, apSrc, inc);
            if (inc == aByteCount)
                return;
            apTarget = (void *)(((UINT32)apTarget) + inc);
            apSrc = (void const *)(((UINT32)apSrc) + inc);
            aByteCount &= 1;
        }
    }
    else if (((((UINT32)apTarget) & 3) == 0) && (aByteCount > 3))
    {
        if ((((UINT32)apSrc) & 3) == 0)
        {
            // 32-bit target, 32-bit src
            inc = aByteCount & ~3;
            K2MEM_Copy32(apTarget, apSrc, inc);
            if (inc == aByteCount)
                return;
            apTarget = (void *)(((UINT32)apTarget) + inc);
            apSrc = (void const *)(((UINT32)apSrc) + inc);
            aByteCount &= 3;
        }
        else if ((((UINT32)apSrc) & 1) == 0)
        {
            // 32-bit target, 16-bit src
            inc = aByteCount & ~1;
            K2MEM_Copy16(apTarget, apSrc, inc);
            if (inc == aByteCount)
                return;
            apTarget = (void *)(((UINT32)apTarget) + inc);
            apSrc = (void const *)(((UINT32)apSrc) + inc);
            aByteCount &= 1;
        }
    }
    else if (((((UINT32)apTarget) & 1) == 0) && (aByteCount > 1))
    {
        if ((((UINT32)apSrc) & 1) == 0)
        {
            // 16-bit target, 16-bit src
            inc = aByteCount & ~1;
            K2MEM_Copy16(apTarget, apSrc, inc);
            if (inc == aByteCount)
                return;
            apTarget = (void *)(((UINT32)apTarget) + inc);
            apSrc = (void const *)(((UINT32)apSrc) + inc);
            aByteCount &= 1;
        }
    }

    // whatever is left copies at byte level
    K2MEM_Copy8(apTarget, apSrc, aByteCount);
}

