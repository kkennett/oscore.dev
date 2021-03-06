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
#ifndef __CRT_H
#define __CRT_H

#include <k2os.h>

#include <lib/k2dlxsupp.h>
#include <lib/k2elf32.h>
#include <lib/k2bit.h>
#include <lib/k2rofshelp.h>
#include <lib/k2heap.h>
#include "..\..\kern\kerniface.h"

#if K2_TARGET_ARCH_IS_ARM
#define CRT_GET_CURRENT_THREAD_INDEX A32_ReadTPIDRURO()

typedef UINT32 (K2_CALLCONV_REGS *K2OS_pf_SysCall)(UINT32 aId, UINT32 aArg1, UINT32 aArg2, UINT32 aArg3);
#define K2OS_SYSCALL ((K2OS_pf_SysCall)(K2OS_UVA_PUBLICAPI_SYSCALL))

#define CrtKern_SysCall1(x,y)     K2OS_SYSCALL((x),(y))
#define CrtKern_SysCall2(x,y,z)   K2OS_SYSCALL((x),(y),(z))
#define CrtKern_SysCall3(x,y,z,w) K2OS_SYSCALL((x),(y),(z),(w))

#elif K2_TARGET_ARCH_IS_INTEL

UINT32 K2_CALLCONV_REGS X32_GetThreadIndex(void);
#define CRT_GET_CURRENT_THREAD_INDEX X32_GetThreadIndex()

typedef UINT32 (K2_CALLCONV_REGS *K2OS_pf_SysCall)(UINT32 aId, UINT32 aArg0);
#define K2OS_SYSCALL ((K2OS_pf_SysCall)(K2OS_UVA_PUBLICAPI_SYSCALL))

#define CrtKern_SysCall1(x,y) K2OS_SYSCALL((x),(y))

UINT32 CrtKern_SysCall2(UINT32 aId, UINT32 aArg0, UINT32 aArg1);
UINT32 CrtKern_SysCall3(UINT32 aId, UINT32 aArg0, UINT32 aArg1, UINT32 aArg2);

#else
#error !!!Unsupported Architecture
#endif

UINT32 CrtKern_SysCall4(UINT32 aId, UINT32 aArg0, UINT32 aArg1, UINT32 aArg2, UINT32 aArg3);
UINT32 CrtKern_SysCall5(UINT32 aId, UINT32 aArg0, UINT32 aArg1, UINT32 aArg2, UINT32 aArg3, UINT32 aArg4);
UINT32 CrtKern_SysCall6(UINT32 aId, UINT32 aArg0, UINT32 aArg1, UINT32 aArg2, UINT32 aArg3, UINT32 aArg4, UINT32 aArg5);
UINT32 CrtKern_SysCall7(UINT32 aId, UINT32 aArg0, UINT32 aArg1, UINT32 aArg2, UINT32 aArg3, UINT32 aArg4, UINT32 aArg5, UINT32 aArg6);
UINT32 CrtKern_SysCall8(UINT32 aId, UINT32 aArg0, UINT32 aArg1, UINT32 aArg2, UINT32 aArg3, UINT32 aArg4, UINT32 aArg5, UINT32 aArg6, UINT32 aArg7);

void   CrtKern_InitProc1(void);

typedef void(*__vfpv)(void *);

extern UINT32 gProcessId;
extern UINT32 gCrtMemEnd;

UINT32  CrtDbg_Printf(char const *apFormat, ...);

void    CrtDlx_Init(K2ROFS const * apROFS);

void    CrtMem_Init(void);
K2STAT  CrtMem_AllocPhysToThread(UINT32 aPageCount, UINT32 aDisposition);

#endif // __CRT_H