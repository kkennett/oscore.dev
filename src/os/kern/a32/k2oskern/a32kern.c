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

#include "a32kern.h"

UINT64 *                        gpA32Kern_AcpiTablePtrs = NULL;
UINT32                          gA32Kern_AcpiTableCount = 0;
BOOL                            gA32Kern_IsMulticoreCapable = FALSE;
ACPI_MADT *                     gpA32Kern_MADT = NULL;
ACPI_MADT_SUB_GIC *             gpA32Kern_MADT_GICC[K2OS_MAX_CPU_COUNT];
ACPI_MADT_SUB_GIC_DISTRIBUTOR * gpA32Kern_MADT_GICD = NULL;

UINT32                          gA32Kern_GICCAddr = 0;
UINT32                          gA32Kern_GICDAddr = 0;
UINT32                          gA32Kern_IntrCount = 255;

void A32Kern_ResetVector(void);
void A32Kern_UndExceptionVector(void);
void A32Kern_SvcExceptionVector(void);
void A32Kern_PrefetchAbortExceptionVector(void);
void A32Kern_DataAbortExceptionVector(void);
void A32Kern_IRQExceptionVector(void);
void A32Kern_FIQExceptionVector(void);

static void sInit_BeforeHal_SetupPtPageCount(void)
{
    UINT32 *        pCountPTPages;
    A32_TTBEQUAD *  pQuad;
    UINT32 *        pPTE;
    UINT32          virtAddr;
    UINTN           quadLeft;
    UINTN           pteLeft;

    //
    // count # of pagetables in use for each translation table base entry
    //
    pCountPTPages = (UINT32*)K2OS_KVA_PTPAGECOUNT_BASE;
    K2MEM_Zero(pCountPTPages, K2_VA32_MEMPAGE_BYTES);

    pQuad = (A32_TTBEQUAD *)K2OS_KVA_TRANSTAB_BASE;

    quadLeft = K2_VA32_PAGETABLES_FOR_4G;
    virtAddr = 0;

    do
    {
        if (pQuad->Quad[0].PTBits.mPresent != 0)
        {
            pPTE = (UINT32*)K2OS_KVA_TO_PTE_ADDR(virtAddr);
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
        pQuad++;
    } while (--quadLeft);
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
    ACPI_MADT_SUB_GIC * pSubGic;

    virtBase = gData.mpShared->LoadInfo.mFwTabPagesVirt;
    physBase = gData.mpShared->LoadInfo.mFwTabPagesPhys;

    //
    // set up to scan xsdt
    //
    pAcpiRdsp = (ACPI_RSDP *)virtBase;
    workAddr = (UINT32)pAcpiRdsp->XsdtAddress;
    workAddr = workAddr - physBase + virtBase;

    pHeader = (ACPI_DESC_HEADER *)workAddr;
    gA32Kern_AcpiTableCount = pHeader->Length - sizeof(ACPI_DESC_HEADER);
    gA32Kern_AcpiTableCount /= sizeof(UINT64);
    K2_ASSERT(gA32Kern_AcpiTableCount > 0);
    gpA32Kern_AcpiTablePtrs = (UINT64 *)(workAddr + sizeof(ACPI_DESC_HEADER));

    //
    // find MADT
    //
    pTabAddr = gpA32Kern_AcpiTablePtrs;
    workCount = gA32Kern_AcpiTableCount;
    do {
        workAddr = (UINT32)(*pTabAddr);
        workAddr = workAddr - physBase + virtBase;
        pHeader = (ACPI_DESC_HEADER *)workAddr;
        if (pHeader->Signature == ACPI_MADT_SIGNATURE)
            break;
        pTabAddr++;
    } while (--workCount);
    K2_ASSERT(workCount > 0);

    gpA32Kern_MADT = (ACPI_MADT *)pHeader;

    //
    // scan for GICD and GICC
    //
    K2MEM_Zero(gpA32Kern_MADT_GICC, sizeof(ACPI_MADT_SUB_GIC *) * K2OS_MAX_CPU_COUNT);

    pScan = ((UINT8 *)gpA32Kern_MADT) + sizeof(ACPI_MADT);
    left = gpA32Kern_MADT->Header.Length - sizeof(ACPI_MADT);
    K2_ASSERT(left > 0);
    do {
        if (*pScan == ACPI_MADT_SUB_TYPE_GIC)
        {
            pSubGic = (ACPI_MADT_SUB_GIC *)pScan;
            K2_ASSERT(pSubGic->GicId < K2OS_MAX_CPU_COUNT);
            K2_ASSERT(gpA32Kern_MADT_GICC[pSubGic->GicId] == NULL);
            if (pSubGic->Flags & ACPI_MADT_SUB_GIC_FLAGS_ENABLED)
            {
                gpA32Kern_MADT_GICC[pSubGic->GicId] = pSubGic;
            }
        }
        else if (*pScan == ACPI_MADT_SUB_TYPE_GICD)
        {
            K2_ASSERT(gpA32Kern_MADT_GICD == NULL);
            gpA32Kern_MADT_GICD = (ACPI_MADT_SUB_GIC_DISTRIBUTOR *)pScan;
        }
        K2_ASSERT(left >= pScan[1]);
        left -= pScan[1];
        pScan += pScan[1];
    } while (left > 0);
}

void sInit_BeforeHal_InitIntr(void)
{
    UINT32  physCBAR;
    UINT32  virtBase;
    UINT32  physBase;
    UINT32  left;

    K2_ASSERT(gpA32Kern_MADT_GICD != NULL);
    K2_ASSERT(gpA32Kern_MADT_GICC[0] != NULL);

    //
    // try to get the Configuration Base Address Register 
    //
    physCBAR = A32_ReadCBAR() & A32_MPCORE_CBAR_LOW32_PERIPH_MASK;

    if (physCBAR == 0)
    {
        //
        // must be uniprocessor
        //
        K2_ASSERT(gData.mCpuCount == 1);

        //
        // find GICD and GICC via ACPI
        //
        physBase = (UINT32)gpA32Kern_MADT_GICD->PhysicalBaseAddress;

        KernMap_MakeOnePresentPage(K2OS_KVA_KERNVAMAP_BASE, A32KERN_UP_GICD_PAGE_VIRT, physBase & K2_VA32_PAGEFRAME_MASK, K2OS_VIRTMAPTYPE_KERN_DEVICE);

        gA32Kern_GICDAddr = A32KERN_UP_GICD_PAGE_VIRT | (physBase & K2_VA32_MEMPAGE_OFFSET_MASK);

        left = physBase;
        physBase = (UINT32)gpA32Kern_MADT_GICC[0]->PhysicalBaseAddress;

        if ((left & K2_VA32_PAGEFRAME_MASK) != (physBase & K2_VA32_PAGEFRAME_MASK))
        {
            //
            // gicc is on different page than gicd
            //
            KernMap_MakeOnePresentPage(K2OS_KVA_KERNVAMAP_BASE, A32KERN_UP_GICC_PAGE_VIRT, physBase & K2_VA32_PAGEFRAME_MASK, K2OS_VIRTMAPTYPE_KERN_DEVICE);
            gA32Kern_GICCAddr = A32KERN_UP_GICC_PAGE_VIRT | (physBase & K2_VA32_MEMPAGE_OFFSET_MASK);
        }
        else
        {
            //
            // gicc is on same page as gicd
            //
            gA32Kern_GICCAddr = A32KERN_UP_GICD_PAGE_VIRT | (physBase & K2_VA32_MEMPAGE_OFFSET_MASK);
        }
    }
    else
    {
        //
        // sanity check we match ACPI with the CBAR
        //
        physBase = (UINT32)gpA32Kern_MADT_GICC[0]->PhysicalBaseAddress;
        K2_ASSERT((physBase & K2_VA32_MEMPAGE_OFFSET_MASK) + (A32KERN_MP_CONFIGBASE_MAP_VIRT) == A32KERN_MP_GICC_VIRT);

        physBase = (UINT32)gpA32Kern_MADT_GICD->PhysicalBaseAddress;
        K2_ASSERT((physBase & K2_VA32_MEMPAGE_OFFSET_MASK) + (A32KERN_MP_CONFIGBASE_MAP_VIRT + K2_VA32_MEMPAGE_BYTES) == A32KERN_MP_GICD_VIRT);

        //
        // map the pages from A32KERN_CONFIGBASE_MAP_VIRT for A32KERN_CONFIGBASE_MAP_PAGES
        // this gives us the global timer and SCU as well as the GICC and GICD
        //
        virtBase = A32KERN_MP_CONFIGBASE_MAP_VIRT;
        physBase = physCBAR;
        left = A32_PERIPH_PAGES;
        do {
            KernMap_MakeOnePresentPage(K2OS_KVA_KERNVAMAP_BASE, virtBase, physBase, K2OS_VIRTMAPTYPE_KERN_DEVICE);
            virtBase += K2_VA32_MEMPAGE_BYTES;
            physBase += K2_VA32_MEMPAGE_BYTES;
        } while (--left);

        gA32Kern_GICCAddr = A32KERN_MP_GICC_VIRT;
        gA32Kern_GICDAddr = A32KERN_MP_GICD_VIRT;
    }

    A32Kern_IntrInitGicDist();
}

static void sInit_BeforeHal(void)
{
    sInit_BeforeHal_SetupPtPageCount();

    sInit_BeforeHal_ScanAcpi();

    sInit_BeforeHal_InitIntr();

    A32Kern_InitStall();
}

static void sInit_AfterHal(void)
{
    K2OSKERN_Debug("\n\nK2OS Kernel(A32) " __DATE__ " " __TIME__ "\n");

    // 
    // confirm any other architectual support here
    //

}

static void sInit_BeforeLaunchCores(void)
{
    UINT32 *    pOut;
    UINT32      val32;

    //
    // install vector page
    //
    K2_ASSERT(A32_HIGH_VECTORS_ADDRESS == K2OS_KVA_ARCHSPEC_BASE);

    KernMap_MakeOnePresentPage(K2OS_KVA_KERNVAMAP_BASE, K2OS_KVA_ARCHSPEC_BASE, gData.mA32VectorPagePhys, K2OS_VIRTMAPTYPE_KERN_DATA);

    val32 = K2OS_KVA_ARCHSPEC_BASE + 8;  /* actual PC at point of jump */
    pOut = (UINT32 *)K2OS_KVA_ARCHSPEC_BASE;

    *pOut = 0xEA000000 + (0xFFFFFF & ((((UINT32)A32Kern_ResetVector) - val32)) >> 2);
    pOut++;
    val32 += 4;

    *pOut = 0xEA000000 + (0xFFFFFF & ((((UINT32)A32Kern_UndExceptionVector) - val32)) >> 2);
    pOut++;
    val32 += 4;

    *pOut = 0xEA000000 + (0xFFFFFF & ((((UINT32)A32Kern_SvcExceptionVector) - val32)) >> 2);
    pOut++;
    val32 += 4;

    *pOut = 0xEA000000 + (0xFFFFFF & ((((UINT32)A32Kern_PrefetchAbortExceptionVector) - val32)) >> 2);
    pOut++;
    val32 += 4;

    *pOut = 0xEA000000 + (0xFFFFFF & ((((UINT32)A32Kern_DataAbortExceptionVector) - val32)) >> 2);
    pOut++;
    val32 += 4;

    *pOut = 0xEAFFFFFE;
    pOut++;
    val32 += 4;

    *pOut = 0xEA000000 + (0xFFFFFF & ((((UINT32)A32Kern_IRQExceptionVector) - val32)) >> 2);
    pOut++;
    val32 += 4;

    *pOut = 0xEA000000 + (0xFFFFFF & ((((UINT32)A32Kern_FIQExceptionVector) - val32)) >> 2);
    pOut++;

    //
    // flush and invalidate the vector area
    //
    K2OS_CacheOperation(K2OS_CACHEOP_FlushData, (UINT32 *)K2OS_KVA_ARCHSPEC_BASE, A32_VECTORS_LENGTH_BYTES);
    K2OS_CacheOperation(K2OS_CACHEOP_InvalidateInstructions, (UINT32 *)K2OS_KVA_ARCHSPEC_BASE, A32_VECTORS_LENGTH_BYTES);

    // 
    // break the mapping
    //
    KernMap_BreakOnePage(K2OS_KVA_KERNVAMAP_BASE, K2OS_KVA_ARCHSPEC_BASE);
    KernArch_InvalidateTlbPageOnThisCore(K2OS_KVA_ARCHSPEC_BASE);

    //
    // re-make the mapping as code
    //
    KernMap_MakeOnePresentPage(K2OS_KVA_KERNVAMAP_BASE, K2OS_KVA_ARCHSPEC_BASE, gData.mA32VectorPagePhys, K2OS_VIRTMAPTYPE_KERN_TEXT);

    //
    // turn on high vectors 
    //
    val32 = A32_ReadSCTRL();
    val32 |= A32_SCTRL_V_HIGHVECTORS;
    A32_WriteSCTRL(val32);
    A32_ISB();
}

void KernInit_Arch(void)
{
    switch (gData.mKernInitStage)
    {
    case KernInitStage_dlx_entry:
        gA32Kern_IsMulticoreCapable = (A32_ReadMPIDR() & 0x80000000) ? TRUE : FALSE;
        break;
    case KernInitStage_Before_Hal:
        sInit_BeforeHal();
        break;
    case KernInitStage_After_Hal:
        sInit_AfterHal();
        break;
    case KernInitStage_Before_Launch_Cores:
        sInit_BeforeLaunchCores();
        break;
    default:
        break;
    }
}
