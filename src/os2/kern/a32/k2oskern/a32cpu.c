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

void A32Kern_EnterMonitorCore0OneTime(UINT32 aStackPtr);
void A32Kern_StackBridge(UINT32 aCpuIx, UINT32 aCoreStacksBase);

struct _A32AUXCPU_STARTDATA
{
    UINT32  mReg_DACR;
    UINT32  mReg_TTBR;
    UINT32  mReg_COPROC;
    UINT32  mReg_CONTROL;
    UINT32  mVirtContinue;
    UINT32  mReserved[3];
};
typedef struct _A32AUXCPU_STARTDATA A32AUXCPU_STARTDATA;

void A32Kern_AuxCpuStart(void);
void A32Kern_AuxCpuStart_END(void);

static volatile INT32 sgCpuCoresExecuting = 0;

void A32Kern_CpuLaunch2(UINT32 aCpuIx)
{
    A32_REG_SCTRL                   sctrl;
    UINT32                          v;
    K2OSKERN_COREPAGE volatile *    pThisCorePage;

    pThisCorePage = K2OSKERN_COREIX_TO_COREPAGE(aCpuIx);
        
    K2_ASSERT(pThisCorePage->CpuCore.mCoreIx == aCpuIx);

    //
    // set up SCTRL
    //
    sctrl.mAsUINT32 = A32_ReadSCTRL();
    sctrl.Bits.mAFE = 0;    // afe off 
    sctrl.Bits.mTRE = 0;    // tex remap off
    sctrl.Bits.mV = 1;      // high vectors
    A32_WriteSCTRL(sctrl.mAsUINT32);
    A32_DSB();
    A32_ISB();

    //
    // switch over to main TTB
    //
    A32_TLBInvalidateAll_MP();
    v = A32_ReadTTBR1();    // everything except address of table will be correct
    v = (v & ~A32_TTBASE_PHYS_MASK) | gData.mpShared->LoadInfo.mTransBasePhys;
    A32_WriteTTBR0(v);
    A32_WriteTTBR1(v);
    A32_WriteTTBCR(1);  // use 0x80000000 sized TTBR0
    A32_ISB();
    A32_TLBInvalidateAll_MP();

    //
    // enable caches now (will only affect cores > ix 0) 
    //
    K2OS_CacheOperation(K2OS_CACHEOP_Init, NULL, 0);

    /* we should be off all CPU startup goop now */
    KernSched_AddCurrentCore();

    A32Kern_IntrInitGicPerCore();

    K2OSKERN_Debug("PL310.100 %08X\n", *(UINT32*)(K2OS_KVA_A32_PL310 + 0x100));
    K2OSKERN_Debug("PL310.104 %08X\n", *(UINT32*)(K2OS_KVA_A32_PL310 + 0x104));

    K2OSKERN_Debug("-CPU %d-------------\n", pThisCorePage->CpuCore.mCoreIx);
    K2OSKERN_Debug("STACK = %08X\n", A32_ReadStackPointer());
    K2OSKERN_Debug("SCTLR = %08X\n", A32_ReadSCTRL());
    K2OSKERN_Debug("ACTLR = %08X\n", A32_ReadAUXCTRL());
    K2OSKERN_Debug("SCU   = %08X\n", *((UINT32 *)K2OS_KVA_A32_PERIPHBASE));
    K2OSKERN_Debug("SCR   = %08X\n", A32_ReadSCR());    // will fault in NS mode
    K2OSKERN_Debug("DACR  = %08X\n", A32_ReadDACR());
    K2OSKERN_Debug("TTBCR = %08X\n", A32_ReadTTBCR());
    K2OSKERN_Debug("TTBR0 = %08X\n", A32_ReadTTBR0());
    K2OSKERN_Debug("TTBR1 = %08X\n", A32_ReadTTBR1());
    K2OSKERN_Debug("%08X = %d\n", &pThisCorePage->CpuCore.mIsExecuting, pThisCorePage->CpuCore.mIsExecuting);
    K2OSKERN_Debug("--------------------\n");

    v = (UINT32)&pThisCorePage->mStacks[K2OSKERN_COREPAGE_STACKS_BYTES - 8];
    K2OSKERN_Debug("CPU %d Started. EntryStack @ %08X\n", pThisCorePage->CpuCore.mCoreIx, v);

    K2OSKERN_Debug("*0x81410010 = %08X\n", *(UINT32*)0x81410010);

    pThisCorePage->CpuCore.mIsExecuting = TRUE;
    pThisCorePage->CpuCore.mIsInMonitor = TRUE;
    K2_CpuWriteBarrier();

    A32Kern_ResumeInMonitor(v);

    //
    // should never return here
    //

    K2_ASSERT(0);
    while (1);
}

void A32Kern_CpuLaunch1(UINT32 aCpuIx)
{
    K2OSKERN_COREPAGE * pThisCorePage;
    pThisCorePage = K2OSKERN_COREIX_TO_COREPAGE(aCpuIx);
    A32Kern_StackBridge(aCpuIx, (UINT32)pThisCorePage->mStacks);
}

static void sStartAuxCpu(UINT32 aCpuIx, UINT32 transitPhys)
{
    UINT32 physAddr;
    UINT32 virtAddr;
    UINT32 chk;
    UINT32 volatile * pScuConfig;

    physAddr = gData.mpShared->LoadInfo.CpuInfo[aCpuIx].mAddrSet;
    virtAddr = A32KERN_APSTART_CPUINFO_PAGE_VIRT | (physAddr & K2_VA32_MEMPAGE_OFFSET_MASK);

    //
    // map set address page
    //
    KernMap_MakeOnePresentPage(K2OS_KVA_KERNVAMAP_BASE, A32KERN_APSTART_CPUINFO_PAGE_VIRT, physAddr & K2_VA32_PAGEFRAME_MASK, K2OS_MAPTYPE_KERN_DEVICEIO);

    //
    // tell CPU aCpuIx to jump to transit page + sizeof(A32AUXCPU_STARTDATA)
    //
    *((UINT32 *)virtAddr) = transitPhys + sizeof(A32AUXCPU_STARTDATA);
    K2_CpuWriteBarrier();
    KernMap_BreakOnePage(K2OS_KVA_KERNVAMAP_BASE, A32KERN_APSTART_CPUINFO_PAGE_VIRT, 0);

    K2OS_CacheOperation(K2OS_CACHEOP_FlushInvalidateData, NULL, 0);

    //
    // this fires the SGI that wakes up the aux core 
    //
    A32_ISB();
    MMREG_WRITE32(gA32Kern_GICDAddr, A32_PERIF_GICD_OFFSET_ICDSGIR, (1 << (aCpuIx + 16)));

    //
    // wait for the core to be participating in coherency before we continue
    //
    pScuConfig = (UINT32 volatile*)(K2OS_KVA_A32_PERIPHBASE + A32_PERIPH_OFFSET_SCU + A32_PERIF_SCU_OFFSET_CONFIG);
    do {
        chk = *pScuConfig;
    } while (0 == ((chk & A32_PERIF_SCU_CONFIG_SMPCPUS_MASK) & (1 << (A32_PERIF_SCU_CONFIG_SMPCPUS_SHIFT + aCpuIx))));
}

void KernArch_LaunchCores(void)
{
    A32_TRANSTBL *                  pTTB;
    A32_PAGETABLE *                 pPageTable;
    UINT32                          workVal;
    UINT32                          ttbPhys;
    UINT32                          ptPhys;
    UINT32                          transitPhys;
    UINT32                          doneFlags;
    UINT32                          uncachedPhys;
    A32_TTBEQUAD *                  pQuad;
    A32AUXCPU_STARTDATA *           pStartData;
    K2OSKERN_COREPAGE volatile *    pAuxCorePage;

    if (gData.mCpuCount > 100)
    {
        //
        // vector page has been installed already
        //

        //
        // figure out the physical addresses from the block of physical space we got
        //
        workVal = gData.mA32StartAreaPhys;      // here, for K2OS_A32_APSTART_PHYS_PAGECOUNT
        doneFlags = 0;
        for (doneFlags=0;doneFlags!=0xF;)
        {
            if ((doneFlags & 1) == 0)
            {
                if ((workVal & 0x3FFF) == 0)
                {
                    ttbPhys = workVal;      // 4 pages 16kb-aligned
                    workVal += 0x4000;
                    doneFlags |= 1;
                    continue;
                }
            }
            if ((doneFlags & 2) == 0)
            {
                ptPhys = workVal;           // 1 page
                workVal += 0x1000;
                doneFlags |= 2;
                continue;
            }
            if ((doneFlags & 4) == 0)
            {
                transitPhys = workVal;      // 1 page
                workVal += 0x1000;
                doneFlags |= 4;
                continue;
            }
            if ((doneFlags & 8) == 0)
            {
                uncachedPhys = workVal;     // 1 page
                workVal += 0x1000;
                doneFlags |= 8;
                continue;
            }
            workVal += 0x1000;
        }

        //
        // map the pages so we can write to them, then clear them
        //
        KernMap_MakeOnePresentPage(K2OS_KVA_KERNVAMAP_BASE, A32KERN_APSTART_TTB_VIRT         , ttbPhys, K2OS_MAPTYPE_KERN_PAGEDIR);
        KernMap_MakeOnePresentPage(K2OS_KVA_KERNVAMAP_BASE, A32KERN_APSTART_TTB_VIRT + 0x1000, ttbPhys + 0x1000, K2OS_MAPTYPE_KERN_PAGEDIR);
        KernMap_MakeOnePresentPage(K2OS_KVA_KERNVAMAP_BASE, A32KERN_APSTART_TTB_VIRT + 0x2000, ttbPhys + 0x2000, K2OS_MAPTYPE_KERN_PAGEDIR);
        KernMap_MakeOnePresentPage(K2OS_KVA_KERNVAMAP_BASE, A32KERN_APSTART_TTB_VIRT + 0x3000, ttbPhys + 0x3000, K2OS_MAPTYPE_KERN_PAGEDIR);
        pTTB = (A32_TRANSTBL *)A32KERN_APSTART_TTB_VIRT;
        K2MEM_Zero(pTTB, sizeof(A32_TRANSTBL));

        KernMap_MakeOnePresentPage(K2OS_KVA_KERNVAMAP_BASE, A32KERN_APSTART_PAGETABLE_VIRT, ptPhys, K2OS_MAPTYPE_KERN_PAGETABLE);
        pPageTable = (A32_PAGETABLE *)A32KERN_APSTART_PAGETABLE_VIRT;
        K2MEM_Zero(pPageTable, sizeof(A32_PAGETABLE));

        KernMap_MakeOnePresentPage(K2OS_KVA_KERNVAMAP_BASE, A32KERN_APSTART_TRANSIT_PAGE_VIRT, transitPhys, K2OS_MAPTYPE_KERN_DATA);
        K2MEM_Zero((void *)A32KERN_APSTART_TRANSIT_PAGE_VIRT, K2_VA32_MEMPAGE_BYTES);

        KernMap_MakeOnePresentPage(K2OS_KVA_KERNVAMAP_BASE, A32KERN_APSTART_UNCACHED_PAGE_VIRT, uncachedPhys, K2OS_MAPTYPE_KERN_UNCACHED);
        K2MEM_Zero((void *)A32KERN_APSTART_UNCACHED_PAGE_VIRT, K2_VA32_MEMPAGE_BYTES);

        // 
        // put the pagetable for the transition page into the AUX TTB
        //
        workVal = (transitPhys >> K2_VA32_PAGETABLE_MAP_BYTES_POW2) & (K2_VA32_PAGETABLES_FOR_4G - 1);
        pQuad = &pTTB->QuadEntry[workVal];
        workVal = A32_TTBE_PT_PRESENT | ptPhys;
        pQuad->Quad[0].mAsUINT32 = workVal;
        pQuad->Quad[1].mAsUINT32 = workVal + 0x400;
        pQuad->Quad[2].mAsUINT32 = workVal + 0x800;
        pQuad->Quad[3].mAsUINT32 = workVal + 0xC00;

        //
        // put the transition page into the aux pagetable
        //
        workVal = (transitPhys >> K2_VA32_MEMPAGE_BYTES_POW2) & (K2_VA32_ENTRIES_PER_PAGETABLE - 1);
        pPageTable->Entry[workVal].mAsUINT32 =
            transitPhys | 
            A32_PTE_PRESENT | A32_MMU_PTE_REGIONTYPE_UNCACHED | A32_MMU_PTE_PERMIT_KERN_RW_USER_NONE;

        //
        // put all pagetables for kernel into the TTB as well
        // this copy is why the transition page can't be in the high 2GB
        //
        K2MEM_Copy(
            &pTTB->QuadEntry[K2_VA32_PAGETABLES_FOR_4G / 2],
            (void *)(K2OS_KVA_TRANSTAB_BASE + 0x2000),
            0x2000
        );

        workVal = ((UINT32)A32Kern_AuxCpuStart_END) - ((UINT32)A32Kern_AuxCpuStart) + 4;
        K2MEM_Copy((void *)A32KERN_APSTART_TRANSIT_PAGE_VIRT, (void *)A32Kern_AuxCpuStart, workVal);

        //
        // copy start code to transition page
        //
        pStartData = (A32AUXCPU_STARTDATA *)A32KERN_APSTART_TRANSIT_PAGE_VIRT;
        pStartData->mReg_DACR = A32_ReadDACR();
        pStartData->mReg_TTBR = (A32_ReadTTBR1() & ~A32_TTBASE_PHYS_MASK) | ttbPhys;   // use AUX startup TTB not the master one
        pStartData->mReg_COPROC = A32_ReadCPACR();
        pStartData->mReg_CONTROL = A32_ReadSCTRL();
        pStartData->mVirtContinue = (UINT32)A32Kern_LaunchEntryPoint;

        //
        // unmap all the pages except the uncached page since we are done using them
        //
        KernMap_BreakOnePage(K2OS_KVA_KERNVAMAP_BASE, A32KERN_APSTART_TRANSIT_PAGE_VIRT, 0);
        KernMap_BreakOnePage(K2OS_KVA_KERNVAMAP_BASE, A32KERN_APSTART_PAGETABLE_VIRT, 0);
        KernMap_BreakOnePage(K2OS_KVA_KERNVAMAP_BASE, A32KERN_APSTART_TTB_VIRT, 0);
        KernMap_BreakOnePage(K2OS_KVA_KERNVAMAP_BASE, A32KERN_APSTART_TTB_VIRT + 0x1000, 0);
        KernMap_BreakOnePage(K2OS_KVA_KERNVAMAP_BASE, A32KERN_APSTART_TTB_VIRT + 0x2000, 0);
        KernMap_BreakOnePage(K2OS_KVA_KERNVAMAP_BASE, A32KERN_APSTART_TTB_VIRT + 0x3000, 0);

        //
        // go tell the CPUs to start up.  
        //
        for (workVal = 1; workVal < gData.mCpuCount; workVal++)
        {
            pAuxCorePage = K2OSKERN_COREIX_TO_COREPAGE(workVal);

            K2_ASSERT(pAuxCorePage->CpuCore.mIsExecuting == FALSE);

            sStartAuxCpu(workVal, transitPhys);

            while (pAuxCorePage->CpuCore.mIsExecuting == FALSE);
        }
    }

    /* go to entry point for core 0 */
    A32Kern_LaunchEntryPoint(0);

    /* should never return */
    K2_ASSERT(0);
    while (1);
}