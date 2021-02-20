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

#ifndef __X32KERN_H
#define __X32KERN_H

#include "..\..\kern.h"
#include "x32kerndef.inc"
#include <spec/x32pcdef.inc>

/* --------------------------------------------------------------------------------- */

K2_STATIC_ASSERT(K2OS_KVA_X32_ARCHSPEC_END <= (K2OS_KVA_ARCHSPEC_BASE + K2OS_KVA_ARCHSPEC_SIZE));

/* --------------------------------------------------------------------------------- */

#define X32_SYM_NAME_MAX_LEN            80

/* --------------------------------------------------------------------------------- */

#define X32_DEVIRQ_LVT_BASE             80 
#define X32_DEVIRQ_LVT_CMCI             (X32_DEVIRQ_LVT_BASE + 0)
#define X32_DEVIRQ_LVT_TIMER            (X32_DEVIRQ_LVT_BASE + 1)
#define X32_DEVIRQ_LVT_THERM            (X32_DEVIRQ_LVT_BASE + 2)
#define X32_DEVIRQ_LVT_PERF             (X32_DEVIRQ_LVT_BASE + 3)
#define X32_DEVIRQ_LVT_LINT0            (X32_DEVIRQ_LVT_BASE + 4)
#define X32_DEVIRQ_LVT_LINT1            (X32_DEVIRQ_LVT_BASE + 5)
#define X32_DEVIRQ_LVT_ERROR            (X32_DEVIRQ_LVT_BASE + 6)
#define X32_DEVIRQ_LVT_LAST             X32_DEVIRQ_LVT_ERROR
#define X32_DEVIRQ_MAX_COUNT            (X32_DEVIRQ_LVT_LAST + 1)

// interrupt vectors 0-31 reserved
#define X32KERN_DEVVECTOR_BASE          32    // MUST BE THIS VALUE. fits with vectors from classic PIC
#define X32KERN_DEVVECTOR_LAST_ALLOWED  (X32KERN_DEVVECTOR_BASE + X32_DEVIRQ_MAX_COUNT - 1)

#define X32KERN_DEVVECTOR_LVT_BASE      (X32_DEVIRQ_LVT_BASE + X32KERN_DEVVECTOR_BASE) 
#define X32KERN_DEVVECTOR_LVT_CMCI      (X32KERN_DEVVECTOR_LVT_BASE + 0)
#define X32KERN_DEVVECTOR_LVT_TIMER     (X32KERN_DEVVECTOR_LVT_BASE + 1)
#define X32KERN_DEVVECTOR_LVT_THERM     (X32KERN_DEVVECTOR_LVT_BASE + 2)
#define X32KERN_DEVVECTOR_LVT_PERF      (X32KERN_DEVVECTOR_LVT_BASE + 3)
#define X32KERN_DEVVECTOR_LVT_LINT0     (X32KERN_DEVVECTOR_LVT_BASE + 4)
#define X32KERN_DEVVECTOR_LVT_LINT1     (X32KERN_DEVVECTOR_LVT_BASE + 5)
#define X32KERN_DEVVECTOR_LVT_ERROR     (X32KERN_DEVVECTOR_LVT_BASE + 6)
#define X32KERN_DEVVECTOR_LVT_RESERVED  (X32KERN_DEVVECTOR_LVT_BASE + 7)

#define X32KERN_VECTOR_ICI_BASE         (X32KERN_DEVVECTOR_LVT_BASE + 8)
#define X32KERN_VECTOR_ICI_LAST         (X32KERN_VECTOR_ICI_BASE + K2OS_MAX_CPU_COUNT - 1)

K2_STATIC_ASSERT(X32KERN_DEVVECTOR_BASE == 32); // no really it has to be this
K2_STATIC_ASSERT(X32KERN_VECTOR_ICI_LAST < 128);

/* --------------------------------------------------------------------------------- */

struct _X32_KERNELMODE_EXCEPTION_STACKHEADER
{
    UINT32 EIP;
    UINT32 CS;
    UINT32 EFLAGS;
};
typedef struct _X32_KERNELMODE_EXCEPTION_STACKHEADER X32_KERNELMODE_EXCEPTION_STACKHEADER;

struct _X32_USERMODE_EXCEPTION_STACKHEADER
{
    UINT32 EIP;
    UINT32 CS;
    UINT32 EFLAGS;
    UINT32 ESP;
    UINT32 SS;
};
typedef struct _X32_USERMODE_EXCEPTION_STACKHEADER X32_USERMODE_EXCEPTION_STACKHEADER;

struct _X32_EXCEPTION_CONTEXT
{
    UINT32      DS;
    X32_PUSHA   REGS;                   // 8 32-bit words
    UINT32      Exception_Vector;
    UINT32      Exception_ErrorCode;
    union
    {
        X32_USERMODE_EXCEPTION_STACKHEADER   UserMode;
        X32_KERNELMODE_EXCEPTION_STACKHEADER KernelMode;
    };
};
typedef struct _X32_EXCEPTION_CONTEXT X32_EXCEPTION_CONTEXT;

#define X32KERN_SIZEOF_USERMODE_EXCEPTION_CONTEXT       sizeof(X32_EXCEPTION_CONTEXT)
#define X32KERN_SIZEOF_KERNELMODE_EXCEPTION_CONTEXT     (sizeof(X32_EXCEPTION_CONTEXT)-(2*4))

K2_STATIC_ASSERT(X32KERN_SIZEOF_KERNELMODE_EXCEPTION_CONTEXT == X32_SIZEOF_KERNEL_EXCEPTION_CONTEXT);

/* --------------------------------------------------------------------------------- */

extern X32_GDTENTRY __attribute__((aligned(16))) gX32Kern_GDT[X32_NUM_SEGMENTS];
extern X32_xDTPTR   __attribute__((aligned(16))) gX32Kern_GDTPTR;
extern X32_IDTENTRY __attribute__((aligned(16))) gX32Kern_IDT[X32_NUM_IDT_ENTRIES];
extern X32_xDTPTR   __attribute__((aligned(16))) gX32Kern_IDTPTR;
extern X32_LDTENTRY __attribute__((aligned(16))) gX32Kern_LDT[K2OS_MAX_CPU_COUNT];

extern UINT32 volatile gX32Kern_PerCoreFS[K2OS_MAX_CPU_COUNT];
extern UINT32 gX32Kern_KernelPageDirPhysAddr;

extern UINT64 *                             gpX32Kern_AcpiTablePtrs;
extern UINT32                               gX32Kern_AcpiTableCount;
extern ACPI_FADT *                          gpX32Kern_FADT;
extern ACPI_MADT *                          gpX32Kern_MADT;
extern ACPI_MADT_SUB_PROCESSOR_LOCAL_APIC * gpX32Kern_MADT_LocApic[K2OS_MAX_CPU_COUNT];
extern ACPI_MADT_SUB_IO_APIC *              gpX32Kern_MADT_IoApic[K2OS_X32_MAX_IOAPICS_COUNT];
extern ACPI_HPET *                          gpX32Kern_HPET;
extern BOOL                                 gX32Kern_ApicReady;
extern X32_CPUID                            gX32Kern_CpuId01;
extern K2OSKERN_SEQLOCK                     gX32Kern_IntrSeqLock;
extern UINT16                               gX32Kern_GlobalSystemIrqOverrideMap[X32_DEVIRQ_MAX_COUNT];
extern UINT16                               gX32Kern_GlobalSystemIrqOverrideFlags[X32_DEVIRQ_MAX_COUNT];
extern UINT16                               gX32Kern_VectorToBeforeAnyOverrideIrqMap[X32_NUM_IDT_ENTRIES];
extern UINT8                                gX32Kern_IrqToIoApicIndexMap[X32_DEVIRQ_MAX_COUNT];
extern UINT32                               gX32Kern_IoApicCount;

/* --------------------------------------------------------------------------------- */

void X32Kern_GDTSetup(void);
void X32Kern_GDTFlush(void);
void X32Kern_IDTSetup(void);
void X32Kern_TSSSetup(X32_TSS *apTSS, UINT32 aESP0);

void X32Kern_PICInit(void);
void X32Kern_APICInit(UINT32 aCpuIx);
void X32Kern_IoApicInit(UINT32 aIoApicIx);
void X32Kern_ConfigDevIrq(K2OSKERN_IRQ_CONFIG const *apConfig);
void X32Kern_MaskDevIrq(UINT8 aIrqIx);
void X32Kern_UnmaskDevIrq(UINT8 aIrqIx);
void X32Kern_EOI(UINT32 aVector);
void K2_CALLCONV_REGS X32Kern_InterruptReturn(UINT32 aESP, UINT32 aFSSel);

void X32Kern_InitStall(void);

void X32Kern_DumpStackTrace(K2OSKERN_OBJ_PROCESS *apProc, UINT32 aEIP, UINT32 aEBP, UINT32 aESP,char * apBuffer);
void X32Kern_DumpUserModeExceptionContext(X32_EXCEPTION_CONTEXT *apContext);
void X32Kern_DumpKernelModeExceptionContext(X32_EXCEPTION_CONTEXT * apContext);

/* --------------------------------------------------------------------------------- */

#endif // __X32KERN_H
