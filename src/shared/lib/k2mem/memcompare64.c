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

INT64
K2MEM_Compare64(
    void const *    aPtr1,
    void const *    aPtr2,
    UINT32          aByteCount
    )
{
    INT64 diff;

    K2_ASSERT((((UINT32)aPtr1) & 7) == 0);
    K2_ASSERT((((UINT32)aPtr2) & 7) == 0);
    K2_ASSERT((((UINT32)aByteCount) & 7) == 0);
    if ((aByteCount == 0) || (aPtr1 == aPtr2))
        return 0;
    do {
        diff = *((INT64 const *)aPtr1) - *((INT64 const *)aPtr2);
        if (diff != 0)
            break;
        aPtr1 = (void const *)(((UINT32)aPtr1) + sizeof(UINT64));
        aPtr2 = (void const *)(((UINT32)aPtr2) + sizeof(UINT64));
        aByteCount -= sizeof(UINT64);
    } while (aByteCount != 0);

    return diff;
}
