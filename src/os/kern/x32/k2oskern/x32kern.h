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

#define K2OSKERN_X32_LOCAPIC_KVA            K2OS_KVA_ARCHSPEC_BASE
#define K2OSKERN_X32_LOCAPIC_SIZE           K2_VA32_MEMPAGE_BYTES

#define K2OSKERN_X32_IOAPIC_KVA             (K2OSKERN_X32_LOCAPIC_KVA + K2OSKERN_X32_LOCAPIC_SIZE)
#define K2OSKERN_X32_IOAPIC_SIZE            K2_VA32_MEMPAGE_BYTES

#define K2OSKERN_X32_HPET_KVA               (K2OSKERN_X32_IOAPIC_KVA + K2OSKERN_X32_IOAPIC_SIZE)
#define K2OSKERN_X32_HPET_SIZE              K2_VA32_MEMPAGE_BYTES

#define K2OSKERN_X32_AP_TRANSITION_KVA      (K2OSKERN_X32_HPET_KVA + K2OSKERN_X32_HPET_SIZE)
#define K2OSKERN_X32_AP_TRANSITION_SIZE     K2_VA32_MEMPAGE_BYTES

#define K2OSKERN_X32_AP_PAGEDIR_KVA         (K2OSKERN_X32_AP_TRANSITION_KVA + K2OSKERN_X32_AP_TRANSITION_SIZE)
#define K2OSKERN_X32_AP_PAGEDIR_SIZE        K2_VA32_MEMPAGE_BYTES

#define K2OSKERN_X32_AP_PAGETABLE_KVA       (K2OSKERN_X32_AP_PAGEDIR_KVA + K2OSKERN_X32_AP_PAGEDIR_SIZE)
#define K2OSKERN_X32_AP_PAGETABLE_SIZE      K2_VA32_MEMPAGE_BYTES

#define K2OSKERN_X32_ARCHSPEC_END           (K2OSKERN_X32_AP_PAGETABLE_KVA + K2OSKERN_X32_AP_PAGETABLE_SIZE)

K2_STATIC_ASSERT(K2OSKERN_X32_ARCHSPEC_END <= (K2OS_KVA_ARCHSPEC_BASE + K2OS_KVA_ARCHSPEC_SIZE));

/* --------------------------------------------------------------------------------- */

// interrupt vectors 0-31 reserved
#define X32KERN_INTR_ICI_BASE           32
#define X32KERN_INTR_DEV_BASE           (X32KERN_INTR_ICI_BASE + K2OS_MAX_CPU_COUNT)

#define X32KERN_INTR_LVT_CMCI           (X32KERN_INTR_DEV_BASE + 0)
#define X32KERN_INTR_LVT_TIMER          (X32KERN_INTR_DEV_BASE + 1)
#define X32KERN_INTR_LVT_THERM          (X32KERN_INTR_DEV_BASE + 2)
#define X32KERN_INTR_LVT_PERF           (X32KERN_INTR_DEV_BASE + 3)
#define X32KERN_INTR_LVT_LINT0          (X32KERN_INTR_DEV_BASE + 4)
#define X32KERN_INTR_LVT_LINT1          (X32KERN_INTR_DEV_BASE + 5)
#define X32KERN_INTR_LVT_ERROR          (X32KERN_INTR_DEV_BASE + 6)

#define X32KERN_INTR_DEV_SCHED_TIMER    (X32KERN_INTR_DEV_BASE + 7)

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
    UINT32      Exception_IntNum;
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
extern ACPI_MADT_SUB_IO_APIC *              gpX32Kern_MADT_IoApic;
extern ACPI_HPET *                          gpX32Kern_HPET;
extern BOOL                                 gX32Kern_ApicReady;
extern UINT32                               gX32Kern_BusClockRate;
extern UINT32                               gX32Kern_IntrMap[256];
extern UINT16                               gX32Kern_IntrFlags[256];
extern X32_CPUID                            gX32Kern_CpuId01;

/* --------------------------------------------------------------------------------- */

void X32Kern_GDTSetup(void);
void X32Kern_GDTFlush(void);
void X32Kern_IDTSetup(void);
void X32Kern_TSSSetup(X32_TSS *apTSS, UINT32 aESP0);

void X32Kern_PICInit(void);
void X32Kern_APICInit(UINT32 aCpuIx);
void X32Kern_IoApicInit(void);

void K2_CALLCONV_REGS X32Kern_EnterMonitor(UINT32 aESP);
void K2_CALLCONV_REGS X32Kern_MonitorMainLoop(void);
void K2_CALLCONV_REGS X32Kern_MonitorOneTimeInit(void);

void K2_CALLCONV_REGS X32Kern_InterruptReturn(UINT32 aESP, UINT32 aFSSel);

typedef void (K2_CALLCONV_REGS *pf_X32Kern_LaunchEntryPoint)(UINT32 aCoreIx);
void K2_CALLCONV_REGS X32Kern_LaunchEntryPoint(UINT32 aCoreIx);

void X32Kern_InitStall(void);
void X32Kern_StartTime(void);
BOOL X32Kern_IntrTimerTick(void);

void X32Kern_ConfigDevIntr(K2OSKERN_INTR_CONFIG const *apConfig);
void X32Kern_MaskDevIntr(UINT8 aIntrId);
void X32Kern_UnmaskDevIntr(UINT8 aIntrId);

/* --------------------------------------------------------------------------------- */

#endif // __X32KERN_H
