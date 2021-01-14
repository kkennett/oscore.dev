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

#ifndef __A32KERN_H
#define __A32KERN_H

#include "..\..\kern.h"
#include "a32kerndef.inc"

struct _A32_EXCEPTION_CONTEXT
{
    UINT32  SPSR;
    UINT32  R[16];  /* R15 must be last thing in context */
};
typedef struct _A32_EXCEPTION_CONTEXT A32_EXCEPTION_CONTEXT;

#define A32KERN_CORESTACKS_BYTES    ( \
            A32KERN_CORE_SVC_STACK_BYTES + \
            A32KERN_CORE_UND_STACK_BYTES + \
            A32KERN_CORE_ABT_STACK_BYTES + \
            A32KERN_CORE_IRQ_STACK_BYTES   \
            )

K2_STATIC_ASSERT(A32KERN_CORESTACKS_BYTES < K2OSKERN_COREPAGE_STACKS_BYTES);

void A32Kern_LaunchEntryPoint(UINT32 aCoreIx);
void A32Kern_ResumeInMonitor(UINT32 aStackPtr);
void A32Kern_ResumeThread(UINT32 aKernModeStackPtr, UINT32 aSvcScratch);
void A32Kern_InitStall(void);

void    A32Kern_IntrInitGicDist(void);
void    A32Kern_IntrInitGicPerCore(void);
UINT32  A32Kern_IntrAck(UINT32 *apRetAckVal);
void    A32Kern_IntrEoi(UINT32 aAckVal);
void    A32Kern_IntrSetEnable(UINT32 aIntrId, BOOL aSetEnable);
BOOL    A32Kern_IntrGetEnable(UINT32 aIntrId);
void    A32Kern_IntrClearPending(UINT32 aIntrId, BOOL aAlsoDisable);

#define SYM_NAME_MAX_LEN    80

extern UINT64 *                         gpA32Kern_AcpiTablePtrs;
extern UINT32                           gA32Kern_AcpiTableCount;
extern ACPI_MADT *                      gpA32Kern_MADT;
extern BOOL                             gA32Kern_IsMulticoreCapable;
extern ACPI_MADT_SUB_GIC *              gpA32Kern_MADT_GICC[K2OS_MAX_CPU_COUNT];
extern ACPI_MADT_SUB_GIC_DISTRIBUTOR *  gpA32Kern_MADT_GICD;
extern UINT32                           gA32Kern_GICCAddr;
extern UINT32                           gA32Kern_GICDAddr;
extern UINT32                           gA32Kern_IntrCount;

#endif // __A32KERN_H
