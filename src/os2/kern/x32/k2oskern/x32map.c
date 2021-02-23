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

UINT32 
KernArch_MakePTE(
    UINT32 aPhysAddr, 
    UINT32 aPageMapAttr
)
{
    K2OSKERN_PHYSTRACK_PAGE *   pTrack;
    UINT32                      pte;

    pTrack = (K2OSKERN_PHYSTRACK_PAGE *)K2OS_PHYS32_TO_PHYSTRACK(aPhysAddr);
    K2_ASSERT(0 == (pTrack->mFlags & K2OSKERN_PHYSTRACK_UNALLOC_FLAG));

    pte = X32_PTE_PRESENT | aPhysAddr;

    if (aPageMapAttr & K2OS_MEMPAGE_ATTR_WRITEABLE)
        pte |= X32_PTE_WRITEABLE;

    if (aPageMapAttr & K2OS_MEMPAGE_ATTR_UNCACHED)
        pte |= X32_PTE_CACHEDISABLE;

    if (aPageMapAttr & K2OS_MEMPAGE_ATTR_WRITE_THRU)
        pte |= X32_PTE_WRITETHROUGH;

    if (aPageMapAttr & K2OS_MEMPAGE_ATTR_USER)
        pte |= X32_PTE_USER;
    else
        pte |= X32_PTE_GLOBAL;

    return pte;
}

void 
KernArch_BreakMapTransitionPageTable(
    UINT32 *apRetVirtAddrPT, 
    UINT32 *apRetPhysAddrPT
)
{
    UINT32      virtAddrPT;
    UINT32 *    pPDE;
    UINT32 *    pPTE;
    UINT32      pdePTAddr;

    pPDE = ((UINT32 *)K2OS_KVA_TRANSTAB_BASE) + (gData.LoadInfo.mTransitionPageAddr / K2_VA32_PAGETABLE_MAP_BYTES);
    pdePTAddr = (*pPDE) & K2_VA32_PAGEFRAME_MASK;
    *pPDE = 0;

    *apRetVirtAddrPT = virtAddrPT = K2OS_KVA_TO_PT_ADDR(gData.LoadInfo.mTransitionPageAddr);

    pPTE = (UINT32 *)K2OS_KVA_TO_PTE_ADDR(virtAddrPT);
    *apRetPhysAddrPT = (*pPTE) & K2_VA32_PAGEFRAME_MASK;
    K2_ASSERT((*apRetPhysAddrPT) == pdePTAddr);
    *pPTE = 0;

    K2_CpuWriteBarrier();
}

void 
KernArch_InvalidateTlbPageOnThisCore(
    UINT32 aVirtAddr
)
{
    X32_TLBInvalidatePage(aVirtAddr);
}

UINT32 * 
KernArch_Translate(
    K2OSKERN_OBJ_PROCESS *apProc, 
    UINT32 aVirtAddr, 
    UINT32* apRetPDE, 
    BOOL *apRetPtPresent, 
    UINT32 *apRetPte, 
    UINT32 *apRetMemPageAttr
)
{
    UINT32      transBase;
    UINT32 *    pPDE;
    UINT32 *    pPTE;
    UINT32      pte;

    K2_ASSERT(apProc != NULL);

    transBase = (apProc->mTransTableKVA & K2_VA32_PAGEFRAME_MASK);

    pPDE = ((UINT32 *)transBase) + (aVirtAddr / K2_VA32_PAGETABLE_MAP_BYTES);

    if (NULL != apRetPDE)
        *apRetPDE = *pPDE;

    if (!((*pPDE) & X32_PDE_PRESENT))
    {
        if (NULL != apRetPtPresent)
            *apRetPtPresent = FALSE;

        return NULL;
    }

    if (NULL != apRetPtPresent)
        *apRetPtPresent = TRUE;

    pPTE = ((UINT32*)K2_VA32_TO_PTE_ADDR(apProc->mVirtMapKVA, aVirtAddr));

    pte = *pPTE;

    if (NULL != apRetPte)
        *apRetPte = pte;

    if (NULL != apRetMemPageAttr)
    {
        if (pte & X32_PTE_WRITEABLE)
        {
            *apRetMemPageAttr |= K2OS_MEMPAGE_ATTR_WRITEABLE;
        }
        else
        {
            *apRetMemPageAttr |= K2OS_MEMPAGE_ATTR_EXEC;
        }

        if (pte & X32_PTE_CACHEDISABLE)
            *apRetMemPageAttr |= K2OS_MEMPAGE_ATTR_UNCACHED;

        if (pte & X32_PTE_WRITETHROUGH)
            *apRetMemPageAttr |= K2OS_MEMPAGE_ATTR_WRITE_THRU;

        if (pte & X32_PTE_USER)
            *apRetMemPageAttr |= K2OS_MEMPAGE_ATTR_USER;
    }

    return pPTE;
}

void 
KernArch_InstallPageTable(
    K2OSKERN_OBJ_PROCESS *  apProc, 
    UINT32                  aVirtAddrPtMaps, 
    UINT32                  aPhysPageAddr
)
{
    UINT32                  ptIndex;
    UINT32                  virtKernPT;
    UINT32 *                pPDE;
    UINT32 *                pPageCount;
    UINT32                  pde;
    K2LIST_LINK *           pListLink;
    K2OSKERN_OBJ_PROCESS *  pOtherProc;
    UINT32                  mapAttr;

    K2_ASSERT(apProc != NULL);

    K2_ASSERT(0 == (aVirtAddrPtMaps & (K2_VA32_PAGETABLE_MAP_BYTES - 1)));

    ptIndex = aVirtAddrPtMaps / K2_VA32_PAGETABLE_MAP_BYTES;

    if (ptIndex >= K2_VA32_PAGETABLES_FOR_2G)
    {
        //
        // pagetable maps kernel space. can only install in proc 1
        //
        K2_ASSERT(apProc == gpProc1);
        mapAttr = X32_KERN_PAGETABLE_PROTO;
    }
    else
    {
        //
        // pagetable maps user mode
        //
        mapAttr = X32_USER_PAGETABLE_PROTO;
    }

    //
    // verify there are no pages there
    //
    pPageCount = (UINT32 *)(((UINT8 *)apProc) + (K2_VA32_MEMPAGE_BYTES * K2OS_PROC_PAGES_OFFSET_PAGECOUNT));
    K2_ASSERT(pPageCount[ptIndex] == 0);

    //
    // pagetable virtual address will always be in kernel space (virtKernPT)
    // because that's where process KVAs live
    //
    virtKernPT = K2_VA32_TO_PT_ADDR(apProc->mVirtMapKVA, aVirtAddrPtMaps);
    K2_ASSERT(((*((UINT32 *)K2OS_KVA_TO_PTE_ADDR(virtKernPT))) & K2OSKERN_PTE_PRESENT_BIT) == 0);
    KernMap_MakeOnePresentPage(apProc, virtKernPT, aPhysPageAddr, K2OS_MAPTYPE_KERN_PAGETABLE);

    //
    // now install the PDE that points to this new pagetable
    //
    pde = (aPhysPageAddr & K2_VA32_PAGEFRAME_MASK) | mapAttr;

    // these should be optimized out by the compiler as they are not variable comparisons
    if (K2OS_MAPTYPE_KERN_PAGEDIR & K2OS_MEMPAGE_ATTR_UNCACHED)
        pde |= X32_PDE_CACHEDISABLE;
    if (K2OS_MAPTYPE_KERN_PAGEDIR & K2OS_MEMPAGE_ATTR_WRITE_THRU)
        pde |= X32_PDE_WRITETHROUGH;

    if (ptIndex >= K2_VA32_PAGETABLES_FOR_2G)
    {
        //
        // pde is for the kernel, so goes into all process pagetables
        //
        K2OSKERN_SeqLock(&gData.ProcListSeqLock);

        pListLink = gData.ProcList.mpHead;
        K2_ASSERT(pListLink != NULL);
        do {
            pOtherProc = K2_GET_CONTAINER(K2OSKERN_OBJ_PROCESS, pListLink, ProcListLink);

            pPDE = (((UINT32 *)pOtherProc->mTransTableKVA) + (ptIndex & 0x3FF));

            K2_ASSERT(((*pPDE) & K2OSKERN_PDE_PRESENT_BIT) == 0);

            *pPDE = pde;

            pListLink = pListLink->mpNext;
        } while (pListLink != NULL);

        K2OSKERN_SeqUnlock(&gData.ProcListSeqLock);
    }
    else
    {
        //
        // pde is for user mode, so only goes into this process
        //
        pPDE = (((UINT32 *)apProc->mTransTableKVA) + ptIndex);
        K2_ASSERT(((*pPDE) & K2OSKERN_PDE_PRESENT_BIT) == 0);
        *pPDE = pde;
    }
}
