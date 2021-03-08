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
#ifndef __K2OS_H
#define __K2OS_H

#include "k2osbase.h"

#if __cplusplus
extern "C" {
#endif

//
//------------------------------------------------------------------------
//

typedef UINT32 K2OS_TOKEN;

//
//------------------------------------------------------------------------
//

static inline UINT64 K2OS_ReadTickCount(void)
{
    return *((UINT64 volatile *)K2OS_UVA_PUBLICAPI_MSTIMER64);
}

//
//------------------------------------------------------------------------
//


UINT32 K2OS_Debug_OutputString(char const *apStr);
void   K2OS_Debug_Break(void);

//
//------------------------------------------------------------------------
//

#define K2OS_NUM_TLS_SLOTS              64

typedef struct _K2OS_USER_THREAD_PAGE K2OS_USER_THREAD_PAGE;
struct _K2OS_USER_THREAD_PAGE
{
    UINT32  mTlsValue[K2OS_NUM_TLS_SLOTS];
#if !K2_TARGET_ARCH_IS_ARM
    UINT32  mSysCall_Arg1;  // r2 on ARM
    UINT32  mSysCall_Arg2;  // r3 on ARM
#endif
    UINT32  mSysCall_Arg3;
    UINT32  mSysCall_Arg4;
    UINT32  mSysCall_Arg5;
    UINT32  mSysCall_Arg6;
    UINT32  mSysCall_Arg7;
    K2STAT  mLastStatus;

    UINT32  mMirror_HeldPtPageCount;
    UINT32  mMirror_HeldPageCount;
    BOOL    mMirror_HeldContig;
};

K2STAT K2OS_Thread_GetLastStatus(void);
K2STAT K2OS_Thread_SetLastStatus(K2STAT aStatus);
K2STAT K2OS_Thread_WaitForNotify(K2OS_TOKEN aTokNotify);
K2STAT K2OS_Thread_TestForNotify(K2OS_TOKEN aTokNotify);

//
//------------------------------------------------------------------------
//

K2STAT K2OS_Notify_Signal(K2OS_TOKEN aTokNotify, UINT32 aSignalBits);

//
//------------------------------------------------------------------------
//

K2STAT K2OS_Tls_AllocSlot(UINT32 *apRetNewIndex);
K2STAT K2OS_Tls_FreeSlot(UINT32 aSlotIndex);
K2STAT K2OS_Tls_SetValue(UINT32 aSlotIndex, UINT32 aValue);
K2STAT K2OS_Tls_GetValue(UINT32 aSlotIndex, UINT32 *apRetValue);

//
//------------------------------------------------------------------------
//


#if __cplusplus
}
#endif

#endif // __K2OS_H