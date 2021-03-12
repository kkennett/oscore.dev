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
#ifndef __K2OSKERN_H
#define __K2OSKERN_H

#include "k2os.h"

#if K2_TARGET_ARCH_IS_INTEL
#include <lib/k2archx32.h>
#endif
#if K2_TARGET_ARCH_IS_ARM
#include <lib/k2archa32.h>
#endif

#if __cplusplus
extern "C" {
#endif

//
//------------------------------------------------------------------------
//

struct _K2OSKERN_SEQLOCK
{
    UINT32 volatile mSeqIn;
    UINT32 volatile mSeqOut;
};
typedef struct _K2OSKERN_SEQLOCK K2OSKERN_SEQLOCK;

UINT32 K2OSKERN_Debug(char const *apFormat, ...);
void   K2OSKERN_Panic(char const *apFormat, ...);

void    K2_CALLCONV_REGS K2OSKERN_MicroStall(UINT32 aMicroseconds);
BOOL    K2_CALLCONV_REGS K2OSKERN_SetIntr(BOOL aEnable);
BOOL    K2_CALLCONV_REGS K2OSKERN_GetIntr(void);
void    K2_CALLCONV_REGS K2OSKERN_SeqInit(K2OSKERN_SEQLOCK * apLock);
BOOL    K2_CALLCONV_REGS K2OSKERN_SeqLock(K2OSKERN_SEQLOCK * apLock);
void    K2_CALLCONV_REGS K2OSKERN_SeqUnlock(K2OSKERN_SEQLOCK * apLock, BOOL aLockDisp);
UINT32  K2_CALLCONV_REGS K2OSKERN_GetCpuIndex(void);

//
//------------------------------------------------------------------------
//

#if __cplusplus
}
#endif


#endif // __K2OSKERN_H