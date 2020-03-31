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

#include "x32kern.h"

X32_GDTENTRY __attribute__((aligned(16))) gX32Kern_GDT[X32_NUM_SEGMENTS];
X32_xDTPTR   __attribute__((aligned(16))) gX32Kern_GDTPTR;
X32_IDTENTRY __attribute__((aligned(16))) gX32Kern_IDT[X32_NUM_IDT_ENTRIES];
X32_xDTPTR   __attribute__((aligned(16))) gX32Kern_IDTPTR;
X32_LDTENTRY __attribute__((aligned(16))) gX32Kern_LDT[K2OS_MAX_CPU_COUNT];

UINT32 volatile                         gX32Kern_PerCoreFS[K2OS_MAX_CPU_COUNT];
UINT32                                  gX32Kern_KernelPageDirPhysAddr;
UINT64 *                                gpX32Kern_AcpiTablePtrs = NULL;
UINT32                                  gX32Kern_AcpiTableCount = 0;
ACPI_FADT *                             gpX32Kern_FADT = NULL;
ACPI_MADT *                             gpX32Kern_MADT = NULL;
ACPI_MADT_SUB_PROCESSOR_LOCAL_APIC *    gpX32Kern_MADT_LocApic[K2OS_MAX_CPU_COUNT];
ACPI_MADT_SUB_IO_APIC *                 gpX32Kern_MADT_IoApic = NULL;
ACPI_HPET *                             gpX32Kern_HPET = NULL;
BOOL                                    gX32Kern_ApicReady = FALSE;
UINT32                                  gX32Kern_IntrMap[256];
UINT16                                  gX32Kern_IntrFlags[256];
X32_CPUID                               gX32Kern_CpuId01;

static void sInit_AtDlxEntry(void)
{
    //
    // get CPUid leaf 1
    //
    gX32Kern_CpuId01.EAX = 1;
    gX32Kern_CpuId01.EBX = 0xFFFFFFFF;
    gX32Kern_CpuId01.ECX = 0xFFFFFFFF;
    gX32Kern_CpuId01.EDX = 0xFFFFFFFF;
    X32_CallCPUID(&gX32Kern_CpuId01);

    //
    // save the kernel physical page directory location
    // in a place that the assembly code can easily get to it
    //
    gX32Kern_KernelPageDirPhysAddr = gData.mpShared->LoadInfo.mTransBasePhys;

    //
    // set up GDT and IDT
    //
    X32Kern_GDTSetup();
    X32Kern_IDTSetup();
}

static void sInit_BeforeHal_SetupPtPageCount(void)
{
    UINT32 *    pCountPTPages;
    UINT32 *    pPDE;
    UINT32 *    pPTE;
    UINT32      virtAddr;
    UINTN       pdeLeft;
    UINTN       pteLeft;

    //
    // count # of pagetables in use for each translation table base entry
    //
    pCountPTPages = (UINT32 *)K2OS_KVA_PTPAGECOUNT_BASE;
    K2MEM_Zero(pCountPTPages, K2_VA32_MEMPAGE_BYTES);

    pPDE = (UINT32 *)K2OS_KVA_TRANSTAB_BASE;

    pdeLeft = K2_VA32_PAGETABLES_FOR_4G;
    virtAddr = 0;

    do
    {
        if (((*pPDE) & X32_PDE_PRESENT) != 0)
        {
            pPTE = (UINT32 *)K2OS_KVA_TO_PTE_ADDR(virtAddr);
            pteLeft = K2_VA32_ENTRIES_PER_PAGETABLE;
            do
            {
                if ((*pPTE) != 0)
                    (*pCountPTPages)++;
                pPTE++;
                virtAddr += K2_VA32_MEMPAGE_BYTES;
            } while (--pteLeft);
        }
        else
            virtAddr += K2_VA32_PAGETABLE_MAP_BYTES;
        pCountPTPages++;
        pPDE++;
    } while (--pdeLeft);
}

static void sInit_BeforeHal_ScanAcpi(void)
{
    UINT32              virtBase;
    UINT32              physBase;
    ACPI_RSDP *         pAcpiRdsp;
    UINT32              workAddr;

    ACPI_DESC_HEADER *  pHeader;
    UINT64 *            pTabAddr;
    UINT32              workCount;

    UINT8 *             pScan;
    UINT32              left;
    ACPI_MADT_SUB_LOCAL_APIC_ADDRESS_OVERRIDE *pOverride;
    UINT32              localApicAddress;
    UINT32              ixCpu;

    ACPI_MADT_SUB_INTERRUPT_SOURCE_OVERRIDE * pIntrOver;

    //
    // confirm local apic support
    //
    K2_ASSERT((gX32Kern_CpuId01.EDX & X32_CPUID1_EDX_APIC) != 0);

    //
    // initialize MADT apic pointers
    //
    for (left = 0; left < K2OS_MAX_CPU_COUNT; left++)
    {
        gpX32Kern_MADT_LocApic[left] = NULL;
    }
    gpX32Kern_MADT_IoApic = NULL;

    virtBase = gData.mpShared->LoadInfo.mFwTabPagesVirt;
    physBase = gData.mpShared->LoadInfo.mFwTabPagesPhys;

    //
    // set up to scan xsdt
    //
    pAcpiRdsp = (ACPI_RSDP *)virtBase;
    workAddr = (UINT32)pAcpiRdsp->XsdtAddress;
    workAddr = workAddr - physBase + virtBase;

    pHeader = (ACPI_DESC_HEADER *)workAddr;
    gX32Kern_AcpiTableCount = pHeader->Length - sizeof(ACPI_DESC_HEADER);
    gX32Kern_AcpiTableCount /= sizeof(UINT64);
    K2_ASSERT(gX32Kern_AcpiTableCount > 0);
    gpX32Kern_AcpiTablePtrs = (UINT64 *)(workAddr + sizeof(ACPI_DESC_HEADER));

    //
    // find HPET and map it if it is there
    //
    pTabAddr = gpX32Kern_AcpiTablePtrs;
    workCount = gX32Kern_AcpiTableCount;
    do {
        workAddr = (UINT32)(*pTabAddr);
        workAddr = workAddr - physBase + virtBase;
        pHeader = (ACPI_DESC_HEADER *)workAddr;
        if (pHeader->Signature == ACPI_HPET_SIGNATURE)
            break;
        pTabAddr++;
    } while (--workCount);
    if (workCount > 0)
    {
        //
        // HPET exists and pHeader points to it 
        //
        gpX32Kern_HPET = (ACPI_HPET *)pHeader;
        do {
            if (gpX32Kern_HPET->Address.AddressSpaceId != ACPI_ASID_SYSTEM_MEMORY)
            {
                gpX32Kern_HPET = NULL;
                break;
            }
            if ((gpX32Kern_HPET->Address.Address & 0xFFFFFFFF00000000ull) != 0)
            {
                gpX32Kern_HPET = NULL;
                break;
            }
            ixCpu = (UINT32)gpX32Kern_HPET->Address.Address;
            ixCpu &= K2_VA32_PAGEFRAME_MASK;
            KernMap_MakeOnePresentPage(gpProc0->mVirtMapKVA, K2OSKERN_X32_HPET_KVA, ixCpu, K2OS_MAPTYPE_KERN_DEVICEIO);
        } while (0);
    }

    //
    // find MADT
    //
    pTabAddr = gpX32Kern_AcpiTablePtrs;
    workCount = gX32Kern_AcpiTableCount;
    do {
        workAddr = (UINT32)(*pTabAddr);
        workAddr = workAddr - physBase + virtBase;
        pHeader = (ACPI_DESC_HEADER *)workAddr;
        if (pHeader->Signature == ACPI_MADT_SIGNATURE)
            break;
        pTabAddr++;
    } while (--workCount);
    K2_ASSERT(workCount > 0);

    gpX32Kern_MADT = (ACPI_MADT *)pHeader;

    //
    // MADT - check to see if there is a PIC and if so make it be quiet
    //
    if (gpX32Kern_MADT->Flags & ACPI_MADT_FLAGS_X32_DUAL_8259_PRESENT)
    {
        X32Kern_PICInit();
    }

    //
    // init interrupt mapping
    //
    for (left = 0; left < 256; left++)
    {
        gX32Kern_IntrMap[left] = left;
        gX32Kern_IntrFlags[left] = 0;
    }

    //
    // MADT - first pass - discover stuff
    //
    ixCpu = 0;
    localApicAddress = (UINT32)gpX32Kern_MADT->LocalApicAddress;
    pScan = ((UINT8 *)gpX32Kern_MADT) + sizeof(ACPI_MADT);
    left = gpX32Kern_MADT->Header.Length - sizeof(ACPI_MADT);
    K2_ASSERT(left > 0);
    pOverride = NULL;
    do {
        if (*pScan == ACPI_MADT_SUB_TYPE_LOCAL_APIC_ADDRESS_OVERRIDE)
        {
            if (pOverride == NULL)
                pOverride = (ACPI_MADT_SUB_LOCAL_APIC_ADDRESS_OVERRIDE *)pScan;
        }
        else if (*pScan == ACPI_MADT_SUB_TYPE_PROCESSOR_LOCAL_APIC)
        {
            K2_ASSERT(ixCpu < gData.mCpuCount);
            gpX32Kern_MADT_LocApic[ixCpu] = (ACPI_MADT_SUB_PROCESSOR_LOCAL_APIC *)pScan;
            ixCpu++;
        }
        else if (*pScan == ACPI_MADT_SUB_TYPE_IO_APIC)
        {
            K2_ASSERT(gpX32Kern_MADT_IoApic == NULL);
            gpX32Kern_MADT_IoApic = (ACPI_MADT_SUB_IO_APIC *)pScan;
        }
        else if (*pScan == ACPI_MADT_SUB_TYPE_INTERRUPT_SOURCE_OVERRIDE)
        {
            pIntrOver = (ACPI_MADT_SUB_INTERRUPT_SOURCE_OVERRIDE *)pScan;
            gX32Kern_IntrMap[pIntrOver->Source] = pIntrOver->GlobalSystemInterrupt;
            gX32Kern_IntrFlags[pIntrOver->Source] = pIntrOver->Flags;
        }
        K2_ASSERT(left >= pScan[1]);
        left -= pScan[1];
        pScan += pScan[1];
    } while (left > 0);
    K2_ASSERT(ixCpu == gData.mCpuCount);

    if (pOverride != NULL)
    {
        localApicAddress = (UINT32)pOverride->LocalApicAddress;
    }
    KernMap_MakeOnePresentPage(gpProc0->mVirtMapKVA, K2OSKERN_X32_LOCAPIC_KVA, localApicAddress, K2OS_MAPTYPE_KERN_DEVICEIO);

    //
    // init and enable cpu 0 APIC 
    //
    X32Kern_APICInit(0);
    gX32Kern_ApicReady = TRUE;
    K2_CpuWriteBarrier();

    //
    // find and configure io apic 
    //
    if (gpX32Kern_MADT_IoApic != NULL)
    {
        K2_ASSERT(gpX32Kern_MADT_IoApic->GlobalSystemInterruptBase == 0);

        KernMap_MakeOnePresentPage(gpProc0->mVirtMapKVA, K2OSKERN_X32_IOAPIC_KVA, gpX32Kern_MADT_IoApic->IoApicAddress, K2OS_MAPTYPE_KERN_DEVICEIO);

        //
        // initialize IO Apic
        //
        X32Kern_IoApicInit();
    }

    //
    // find FADT
    //
    pTabAddr = gpX32Kern_AcpiTablePtrs;
    workCount = gX32Kern_AcpiTableCount;
    do {
        workAddr = (UINT32)(*pTabAddr);
        workAddr = workAddr - physBase + virtBase;
        pHeader = (ACPI_DESC_HEADER *)workAddr;
        if (pHeader->Signature == ACPI_FADT_SIGNATURE)
            break;
        pTabAddr++;
    } while (--workCount);
    K2_ASSERT(workCount > 0);

    gpX32Kern_FADT = (ACPI_FADT *)pHeader;
}

static void sInit_BeforeHal(void)
{
    sInit_BeforeHal_SetupPtPageCount();

    sInit_BeforeHal_ScanAcpi();

    X32Kern_InitStall();
}

static void sInit_AfterHal(void)
{
    K2OSKERN_Debug("\n\nK2OS Kernel(X32) " __DATE__ " " __TIME__ "\n");

    //
    // confirm SYSENTER support
    //
    K2_ASSERT((gX32Kern_CpuId01.EDX & X32_CPUID1_EDX_SEP) != 0);
}

void KernInit_Arch(void)
{
    // stage is in gData.mKernInitStage
    switch (gData.mKernInitStage)
    {
    case KernInitStage_dlx_entry:
        sInit_AtDlxEntry();
        break;
    case KernInitStage_Before_Hal:
        sInit_BeforeHal();
        break;
    case KernInitStage_After_Hal:
        sInit_AfterHal();
        break;
    default:
        break;
    }
}
