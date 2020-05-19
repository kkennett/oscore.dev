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

#include "kern.h"

#if TRACING_ON

void K2OSKERN_Trace(UINT32 aTime, UINT32 aCode, UINT32 aCount, ...)
{
    UINT32 dec;
    UINT32 cur;
    UINT32 v;
    VALIST argPtr;

    dec = (3 + aCount) * sizeof(UINT32);

    do {
        cur = gData.mTrace;
        v = cur - dec;
        if (v < gData.mTraceBottom)
            return;
    } while (cur != K2ATOMIC_CompareExchange(&gData.mTrace, v, cur));

    *((UINT32 *)cur) = aTime;
    cur -= sizeof(UINT32);

    *((UINT32 *)cur) = aCode;
    cur -= sizeof(UINT32);

    if (aCount > 0)
    {
        K2_VASTART(argPtr, aCount);
        do {
            *((UINT32 *)cur) = K2_VAARG(argPtr, UINT32);
            cur -= sizeof(UINT32);
        } while (--aCount);

        K2_VAEND(argPtr);
    }

    *((UINT32 *)cur) = dec;
}

#endif