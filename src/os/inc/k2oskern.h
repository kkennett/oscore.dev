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

typedef
K2STAT
(*K2OSKERN_pf_MapDevice)(
    UINT32      aPhysDeviceAddr,
    UINT32      aPageCount,
    UINT32 *    apRetVirtAddr
    );

K2STAT
K2OSKERN_MapDevice(
    UINT32      aPhysDeviceAddr,
    UINT32      aPageCount,
    UINT32 *    apRetVirtAddr
);

typedef
K2STAT
(*K2OSKERN_pf_UnmapDevice)(
    UINT32      aVirtDeviceAddr
    );

K2STAT
K2OSKERN_UnmapDevice(
    UINT32      aVirtDeviceAddr
);

//
//------------------------------------------------------------------------
//

typedef
K2STAT
(*K2OSKERN_pf_Esc)(
    UINT32  aEscCode,
    void *  apParam
    );

K2STAT
K2OSKERN_Esc(
    UINT32  aEscCode,
    void *  apParam
);

#define K2OSKERN_ESC_GET_DEBUGPAGE  1

//
//------------------------------------------------------------------------
//

extern K2_CACHEINFO const * gpK2OSKERN_CacheInfo;

//
//------------------------------------------------------------------------
//

#define K2OS_SYSPROP_ID_KERN_FWTABLES_PHYS_ADDR     0x80000000
#define K2OS_SYSPROP_ID_KERN_FWTABLES_VIRT_ADDR     0x80000001
#define K2OS_SYSPROP_ID_KERN_FWTABLES_BYTES         0x80000002
#define K2OS_SYSPROP_ID_KERN_FWFACS_PHYS_ADDR       0x80000003
#define K2OS_SYSPROP_ID_KERN_XFWFACS_PHYS_ADDR      0x80000004

//
//------------------------------------------------------------------------
//


#endif // __K2OSKERN_H