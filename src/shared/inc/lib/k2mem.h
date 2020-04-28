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
#ifndef __K2MEM_H
#define __K2MEM_H

#include <k2systype.h>

//
//------------------------------------------------------------------------
//

#ifdef __cplusplus
extern "C" {
#endif

void 
K2MEM_Copy8(
    void *          apTarget,
    void const *    apSrc,
    UINT32          aByteCount
    );

void 
K2MEM_Copy16(
    void *          apTarget,
    void const *    apSrc,
    UINT32          aByteCount
    );

void 
K2MEM_Copy32(
    void *          apTarget,
    void const *    apSrc,
    UINT32          aByteCount
    );

void 
K2MEM_Copy64(
    void *          apTarget,
    void const *    apSrc,
    UINT32          aByteCount
    );

void
K2MEM_Copy(
    void *          apTarget,
    void const *    apSrc,
    UINT32          aByteCount
    );

//
//------------------------------------------------------------------------
//

void
K2MEM_Swap8(
    void *  aPtr1,
    void *  aPtr2,
    UINT32  aByteCount
);

void
K2MEM_Swap16(
    void *  aPtr1,
    void *  aPtr2,
    UINT32  aByteCount
);

void
K2MEM_Swap32(
    void *  aPtr1,
    void *  aPtr2,
    UINT32  aByteCount
);

void
K2MEM_Swap64(
    void *  aPtr1,
    void *  aPtr2,
    UINT32  aByteCount
);

void
K2MEM_Swap(
    void *  aPtr1,
    void *  aPtr2,
    UINT32  aByteCount
);

//
//------------------------------------------------------------------------
//

void
K2MEM_Set(
    void *  apTarget,
    UINT8   aValue,
    UINT32  aByteCount
    );

#define K2MEM_Set8  K2MEM_Set

void
K2MEM_Set16(
    void *  apTarget,
    UINT16  aValue,
    UINT32  aByteCount
    );

void
K2MEM_Set32(
    void *  apTarget,
    UINT32  aValue,
    UINT32  aByteCount
    );

void
K2MEM_Set64(
    void *  apTarget,
    UINT64  aValue,
    UINT32  aByteCount
    );

static K2_INLINE
void
K2MEM_Zero(
    void *  apTarget,
    UINT32  aByteCount
    )
{
    K2MEM_Set(apTarget, 0, aByteCount);
}

//
//------------------------------------------------------------------------
//

BOOL
K2MEM_Verify(
    void const *    apTarget,
    UINT8           aValue,
    UINT32          aByteCount
    );

#define K2MEM_Verify8  Mem_Verify

BOOL
K2MEM_Verify16(
    void const *    apTarget,
    UINT16          aValue,
    UINT32          aByteCount
    );

BOOL
K2MEM_Verify32(
    void const *    apTarget,
    UINT32          aValue,
    UINT32          aByteCount
    );

BOOL
K2MEM_Verify64(
    void const *    apTarget,
    UINT64          aValue,
    UINT32          aByteCount
    );

static K2_INLINE
BOOL
K2MEM_VerifyZero(
    void const *    apTarget,
    UINT32          aByteCount
    )
{
    return K2MEM_Verify(apTarget, 0, aByteCount);
}

//
//------------------------------------------------------------------------
//

INT8
K2MEM_Compare8(
    void const *    aPtr1,
    void const *    aPtr2,
    UINT32          aByteCount
);

INT16
K2MEM_Compare16(
    void const *    aPtr1,
    void const *    aPtr2,
    UINT32          aByteCount
);

INT32
K2MEM_Compare32(
    void const *    aPtr1,
    void const *    aPtr2,
    UINT32          aByteCount
);

INT64
K2MEM_Compare64(
    void const *    aPtr1,
    void const *    aPtr2,
    UINT32          aByteCount
);

int
K2MEM_Compare(
    void const *    aPtr1,
    void const *    aPtr2,
    UINT32          aByteCount
    );

#ifdef __cplusplus
};  // extern "C"
#endif

//
//------------------------------------------------------------------------
//

#endif // __K2MEM_H
