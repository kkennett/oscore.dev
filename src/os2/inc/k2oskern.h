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

#include <lib/k2archx32.h>

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

//
//------------------------------------------------------------------------
//

typedef
UINT32
(*K2OSKERN_pf_Debug)(
    char const *apFormat,
    ...
);

UINT32
K2OSKERN_Debug(
    char const *apFormat,
    ...
);

//
//------------------------------------------------------------------------
//

typedef
void
(*K2OSKERN_pf_Panic)(
    char const *apFormat,
    ...
);

void
K2OSKERN_Panic(
    char const *apFormat,
    ...
);

//
//------------------------------------------------------------------------
//

typedef
void
(K2_CALLCONV_REGS *K2OSKERN_pf_MicroStall)(
    UINT32      aMicroseconds
    );

void
K2_CALLCONV_REGS
K2OSKERN_MicroStall(
    UINT32      aMicroseconds
);

//
//------------------------------------------------------------------------
//

typedef
BOOL
(K2_CALLCONV_REGS *K2OSKERN_pf_SetIntr)(
    BOOL aEnable
);

BOOL
K2_CALLCONV_REGS
K2OSKERN_SetIntr(
    BOOL aEnable
);

typedef
BOOL
(K2_CALLCONV_REGS *K2OSKERN_pf_GetIntr)(
    void
    );

BOOL
K2_CALLCONV_REGS
K2OSKERN_GetIntr(
    void
    );

//
//------------------------------------------------------------------------
//

typedef
void
(K2_CALLCONV_REGS *K2OSKERN_pf_SeqIntrInit)(
    K2OSKERN_SEQLOCK *  apLock
);

void
K2_CALLCONV_REGS
K2OSKERN_SeqIntrInit(
    K2OSKERN_SEQLOCK *  apLock
);

//
//------------------------------------------------------------------------
//

typedef 
BOOL
(K2_CALLCONV_REGS *K2OSKERN_pf_SeqIntrLock)(
    K2OSKERN_SEQLOCK *  apLock
);

BOOL
K2_CALLCONV_REGS
K2OSKERN_SeqIntrLock(
    K2OSKERN_SEQLOCK *  apLock
);

//
//------------------------------------------------------------------------
//

typedef 
void
(K2_CALLCONV_REGS *K2OSKERN_pf_SeqIntrUnlock)(
    K2OSKERN_SEQLOCK *  apLock,
    BOOL                aDisp
);

void
K2_CALLCONV_REGS
K2OSKERN_SeqIntrUnlock(
    K2OSKERN_SEQLOCK *  apLock,
    BOOL                aDisp
);

//
//------------------------------------------------------------------------
//

typedef 
UINT32
(K2_CALLCONV_REGS *K2OSKERN_pf_GetCpuIndex)(
    void
);

UINT32
K2_CALLCONV_REGS
K2OSKERN_GetCpuIndex(
    void
);

//
//------------------------------------------------------------------------
//

struct _K2OSKERN_IRQ_LINE_CONFIG
{
    BOOL    mIsEdgeTriggered;
    BOOL    mIsActiveLow;
    BOOL    mShareConfig;
    BOOL    mWakeConfig;
};
typedef struct _K2OSKERN_IRQ_LINE_CONFIG K2OSKERN_IRQ_LINE_CONFIG;

struct _K2OSKERN_IRQ_CONFIG
{
    UINT32                      mSourceIrq;
    UINT32                      mDestCoreIx;
    K2OSKERN_IRQ_LINE_CONFIG    Line;
};
typedef struct _K2OSKERN_IRQ_CONFIG K2OSKERN_IRQ_CONFIG;

//
// calling convention must match ACPI_SYSTEM_XFACE
//
typedef UINT32 (*K2OSKERN_pf_IntrHandler)(void *apContext);

//
//------------------------------------------------------------------------
//

#if __cplusplus
}
#endif


#endif // __K2OSKERN_H