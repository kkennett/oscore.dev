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
X32_LDTENTRY __attribute__((aligned(16))) gX32Kern_UserLDT[K2OS_MAX_CPU_COUNT];

K2_STATIC_ASSERT((X32_DEVIRQ_MAX_COUNT + X32KERN_DEVVECTOR_BASE) < X32_NUM_IDT_ENTRIES);

UINT32 volatile *   gpX32Kern_PerCoreFS = (UINT32 volatile *)K2OS_KVA_PUBLICAPI_PERCORE_DATA;
UINT32              gX32Kern_KernelPageDirPhysAddr;

UINT64 *                                gpX32Kern_AcpiTablePtrs = NULL;
UINT32                                  gX32Kern_AcpiTableCount = 0;
ACPI_FADT *                             gpX32Kern_FADT = NULL;
ACPI_MADT *                             gpX32Kern_MADT = NULL;
ACPI_MADT_SUB_PROCESSOR_LOCAL_APIC *    gpX32Kern_MADT_LocApic[K2OS_MAX_CPU_COUNT];
ACPI_MADT_SUB_IO_APIC *                 gpX32Kern_MADT_IoApic[K2OS_X32_MAX_IOAPICS_COUNT];
ACPI_HPET *                             gpX32Kern_HPET = NULL;
BOOL                                    gX32Kern_ApicReady = FALSE;
X32_CPUID                               gX32Kern_CpuId01;
K2OSKERN_SEQLOCK                        gX32Kern_IntrSeqLock;
UINT16                                  gX32Kern_GlobalSystemIrqOverrideMap[X32_DEVIRQ_MAX_COUNT];
UINT16                                  gX32Kern_GlobalSystemIrqOverrideFlags[X32_DEVIRQ_MAX_COUNT];
UINT16                                  gX32Kern_VectorToBeforeAnyOverrideIrqMap[X32_NUM_IDT_ENTRIES];
UINT8                                   gX32Kern_IrqToIoApicIndexMap[X32_DEVIRQ_MAX_COUNT];
UINT32                                  gX32Kern_IoApicCount;

void 
KernArch_InitAtEntry(
    void
)
{
    UINT32 *            pCountPTPages;
    UINT32 *            pPDE;
    UINT32 *            pPTE;
    UINTN               pdeLeft;
    UINTN               pteLeft;
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
    // get CPUid leaf 1, confirm SYSENTER support
    //
    gX32Kern_CpuId01.EAX = 1;
    gX32Kern_CpuId01.EBX = 0xFFFFFFFF;
    gX32Kern_CpuId01.ECX = 0xFFFFFFFF;
    gX32Kern_CpuId01.EDX = 0xFFFFFFFF;
    X32_CallCPUID(&gX32Kern_CpuId01);
    K2_ASSERT((gX32Kern_CpuId01.EDX & X32_CPUID1_EDX_SEP) != 0);

    //
    // save the kernel physical page directory location
    // in a place that the assembly code can easily get to it
    //
    gX32Kern_KernelPageDirPhysAddr = gData.LoadInfo.mTransBasePhys;

    //
    // set up GDT and IDT
    //
    X32Kern_GDTSetup();
    X32Kern_IDTSetup();

    K2OSKERN_SeqInit(&gX32Kern_IntrSeqLock);

    //
    // count # of pagetables in use for each translation table base entry
    //
    pCountPTPages = (UINT32 *)K2OS_KVA_PROC1_PAGECOUNT;
    K2MEM_Zero(pCountPTPages, K2_VA32_MEMPAGE_BYTES);
    pPDE = (UINT32 *)K2OS_KVA_TRANSTAB_BASE;
    pdeLeft = K2_VA32_PAGETABLES_FOR_4G;
    virtBase = 0;
    do
    {
        if (((*pPDE) & X32_PDE_PRESENT) != 0)
        {
            pPTE = (UINT32 *)K2OS_KVA_TO_PTE_ADDR(virtBase);
            pteLeft = K2_VA32_ENTRIES_PER_PAGETABLE;
            do
            {
                if ((*pPTE) != 0)
                    (*pCountPTPages)++;
                pPTE++;
                virtBase += K2_VA32_MEMPAGE_BYTES;
            } while (--pteLeft);
        }
        else
            virtBase += K2_VA32_PAGETABLE_MAP_BYTES;
        pCountPTPages++;
        pPDE++;
    } while (--pdeLeft);

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
    for (left = 0; left < K2OS_X32_MAX_IOAPICS_COUNT; left++)
    {
        gpX32Kern_MADT_IoApic[left] = NULL;
    }

    virtBase = gData.LoadInfo.mFwTabPagesVirt;
    physBase = gData.LoadInfo.mFwTabPagesPhys;

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
            KernMap_MakeOnePresentPage(gpProc1, K2OS_KVA_X32_HPET, ixCpu, K2OS_MAPTYPE_KERN_DEVICEIO);
        } while (0);
    }

    //
    // init intr override map
    //
    for (left = 0; left < X32_DEVIRQ_LVT_LAST; left++)
    {
        gX32Kern_GlobalSystemIrqOverrideMap[left] = (UINT16)-1; // this means "not overridden"
        gX32Kern_GlobalSystemIrqOverrideFlags[left] = 0;
        gX32Kern_VectorToBeforeAnyOverrideIrqMap[left] = (UINT16)-1; // this means "not overridden"
    }
    do {
        gX32Kern_VectorToBeforeAnyOverrideIrqMap[left] = (UINT16)-1; // this means "not overridden"
    } while (++left < X32_NUM_IDT_ENTRIES);
    K2MEM_Set(gX32Kern_IrqToIoApicIndexMap, 0xFF, sizeof(UINT8) * X32_DEVIRQ_MAX_COUNT);

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

    if (workCount == 0)
    {
        //
        // No MADT!  Assume old PIC
        //
        gpX32Kern_MADT = NULL;
        X32Kern_PICInit();
    }
    else
    {
        gpX32Kern_MADT = (ACPI_MADT *)pHeader;
        //
        // MADT - check to see if there is a PIC and if so make it be quiet
        //
        if (gpX32Kern_MADT->Flags & ACPI_MADT_FLAGS_X32_DUAL_8259_PRESENT)
        {
            X32Kern_PICInit();
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
                K2_ASSERT(ixCpu < gData.LoadInfo.mCpuCoreCount);
                gpX32Kern_MADT_LocApic[ixCpu] = (ACPI_MADT_SUB_PROCESSOR_LOCAL_APIC *)pScan;
                ixCpu++;
            }
            else if (*pScan == ACPI_MADT_SUB_TYPE_IO_APIC)
            {
                K2_ASSERT(gX32Kern_IoApicCount < K2OS_X32_MAX_IOAPICS_COUNT);
                K2_ASSERT(gpX32Kern_MADT_IoApic[gX32Kern_IoApicCount] == NULL);
                gpX32Kern_MADT_IoApic[gX32Kern_IoApicCount] = (ACPI_MADT_SUB_IO_APIC *)pScan;
                gX32Kern_IoApicCount++;
            }
            else if (*pScan == ACPI_MADT_SUB_TYPE_INTERRUPT_SOURCE_OVERRIDE)
            {
                pIntrOver = (ACPI_MADT_SUB_INTERRUPT_SOURCE_OVERRIDE *)pScan;

                K2_ASSERT(pIntrOver->GlobalSystemInterrupt < (X32_NUM_IDT_ENTRIES - X32KERN_DEVVECTOR_BASE));

                gX32Kern_GlobalSystemIrqOverrideMap[pIntrOver->Source] = pIntrOver->GlobalSystemInterrupt;
                gX32Kern_GlobalSystemIrqOverrideFlags[pIntrOver->Source] = pIntrOver->Flags;

                // now that override is set, the KernArch_DevIrqToVector should work for it
                gX32Kern_VectorToBeforeAnyOverrideIrqMap[KernArch_DevIrqToVector(pIntrOver->Source)] = pIntrOver->Source;

                K2_ASSERT(KernArch_VectorToDevIrq(KernArch_DevIrqToVector(pIntrOver->Source)) == pIntrOver->Source);
            }
            K2_ASSERT(left >= pScan[1]);
            left -= pScan[1];
            pScan += pScan[1];
        } while (left > 0);
        K2_ASSERT(ixCpu == gData.LoadInfo.mCpuCoreCount);

        if (pOverride != NULL)
        {
            localApicAddress = (UINT32)pOverride->LocalApicAddress;
        }
        KernMap_MakeOnePresentPage(gpProc1, K2OS_KVA_X32_LOCAPIC, localApicAddress, K2OS_MAPTYPE_KERN_DEVICEIO);

        //
        // init and enable cpu 0 APIC 
        //
        X32Kern_APICInit(0);
        gX32Kern_ApicReady = TRUE;
        K2_CpuWriteBarrier();

        //
        // find and configure io apic 
        //
        if (gX32Kern_IoApicCount > 0)
        {
            for (left = 0; left < gX32Kern_IoApicCount; left++)
            {
                //
                // map and initialize this IO Apic
                //
                KernMap_MakeOnePresentPage(
                    gpProc1, 
                    K2OS_KVA_X32_IOAPICS + (left * K2_VA32_MEMPAGE_BYTES), 
                    gpX32Kern_MADT_IoApic[left]->IoApicAddress, 
                    K2OS_MAPTYPE_KERN_DEVICEIO);
                X32Kern_IoApicInit(left);
            }
        }
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

    //
    // now we can init the stall counter
    //
    X32Kern_InitStall();
}

