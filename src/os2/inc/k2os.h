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

#define K2OS_SYSCALL_ID_GET_THREAD_IX       0
#define K2OS_SYSCALL_ID_OUTPUT_DEBUG        1
#define K2OS_SYSCALL_ID_DEBUG_BREAK         2
#define K2OS_SYSCALL_ID_CRT_INITDLX         3
#define K2OS_SYSCALL_ID_SIGNAL_NOTIFY       4
#define K2OS_SYSCALL_ID_WAIT_FOR_NOTIFY     5
#define K2OS_SYSCALL_ID_WAIT_FOR_NOTIFY_NB  6

typedef UINT32 (K2_CALLCONV_REGS *K2OS_pf_SysCall)(UINT32 aArg1, UINT32 aArg2);

#define K2OS_SYSCALL ((K2OS_pf_SysCall)(K2OS_UVA_PUBLICAPI_SYSCALL))

//
//------------------------------------------------------------------------
//

UINT32 K2OS_Debug_OutputString(char const *apStr);
void   K2OS_Debug_Break(void);

//
//------------------------------------------------------------------------
//

K2STAT K2OS_Thread_GetLastStatus(void);
K2STAT K2OS_Thread_SetLastStatus(K2STAT aStatus);

//
//------------------------------------------------------------------------
//

#define K2OS_MAX_NUM_TLS_SLOTS  32

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