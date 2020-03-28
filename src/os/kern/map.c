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

#include "kern.h"

static UINT32 * sGetPTE(UINT32 aVirtMapBase, UINT32 aVirtAddr)
{
    UINT32* pPTE;

    if (aVirtAddr >= K2OS_KVA_KERN_BASE)
    {
        K2_ASSERT(aVirtMapBase == K2OS_KVA_KERNVAMAP_BASE);
        pPTE = (UINT32*)K2OS_KVA_TO_PTE_ADDR(aVirtAddr);
    }
    else
    {
        K2_ASSERT(aVirtMapBase != K2OS_KVA_KERNVAMAP_BASE);
        pPTE = (UINT32*)K2_VA32_TO_PTE_ADDR(aVirtMapBase, aVirtAddr);
    }

    return pPTE;
}

UINT32 KernMap_MakeOnePage(UINT32 aVirtMapBase, UINT32 aVirtAddr, UINT32 aPhysAddr, UINT32 aPageMapAttr)
{
    UINT32* pPTE;

    aPageMapAttr &= K2OS_MEMPAGE_ATTR_MASK;

    pPTE = sGetPTE(aVirtMapBase, aVirtAddr);

    *pPTE = KernArch_MakePTE(aPhysAddr, aPageMapAttr);

    K2_CpuWriteBarrier();

    return (UINT32)pPTE;
}

UINT32 KernMap_BreakOnePage(UINT32 aVirtMapBase, UINT32 aVirtAddr)
{
    UINT32* pPTE;

    pPTE = sGetPTE(aVirtMapBase, aVirtAddr);

    *pPTE = 0;

    K2_CpuWriteBarrier();

    return (UINT32)pPTE;
}

void KernMap_KernFromThread(K2OSKERN_OBJ_THREAD *apCurThread, void *apPhysPageOwner, KernPhysPageList aTargetList)
{
    UINT32 *                pPDE;
    UINT32 *                pPTE;
    UINT32                  pde;
    BOOL                    disp;
    BOOL                    lockStatus;
    UINT32                  virtAddr;
    UINT32                  virtPT;
    UINT32                  physPageAddr;
    UINT32                  ptIndex;
    BOOL                    usedPT;

    K2LIST_LINK *           pListLink;
    K2OSKERN_OBJ_PROCESS *  pProc;

    virtAddr = apCurThread->mWorkMapAddr;
    K2OSKERN_Debug("map - v%08X\n", virtAddr);

    ptIndex = virtAddr / K2_VA32_PAGETABLE_MAP_BYTES;
    K2OSKERN_Debug("map - ptIndex %d\n", ptIndex);

    virtPT = K2OS_KVA_TO_PT_ADDR(virtAddr);
    K2OSKERN_Debug("map - virtPT %08X\n", virtPT);

#if K2_TARGET_ARCH_IS_ARM
    pPDE = (((UINT32 *)K2OS_KVA_TRANSTAB_BASE) + ((ptIndex & 0x3FF) * 4));
#else
    pPDE = (((UINT32 *)K2OS_KVA_TRANSTAB_BASE) + (ptIndex & 0x3FF));;
#endif
    K2OSKERN_Debug("map - pPDE %08X\n", pPDE);

    disp = K2OSKERN_SeqIntrLock(&gData.KernVirtMapLock);

    if ((*pPDE) & K2OSKERN_PDE_PRESENT_BIT)
    {
        //
        // pageTable is already mapped
        //
        lockStatus = FALSE;
        usedPT = FALSE;
    }
    else
    {
        K2OSKERN_Debug("map - PDE NOT PRESENT\n");

        K2_ASSERT(apCurThread->mpWorkPtPage != NULL);

        usedPT = TRUE;
#if !K2_TARGET_ARCH_IS_ARM
        //
        // need to map pagetable into all process' page directories
        //
        K2OSKERN_SeqIntrUnlock(&gData.KernVirtMapLock, FALSE);
        K2OSKERN_SeqIntrLock(&gData.ProcListSeqLock);
        lockStatus = TRUE;
        K2OSKERN_SeqIntrLock(&gData.KernVirtMapLock);
#endif

        //
        // map the pagetable page
        //
        pPTE = (UINT32 *)K2OS_KVA_TO_PTE_ADDR(virtPT);
        K2OSKERN_Debug("map - ptpte at %08X\n", pPTE);
        K2_ASSERT(((*pPTE) & K2OSKERN_PTE_PRESENT_BIT) == 0);
        physPageAddr = K2OS_PHYSTRACK_TO_PHYS32(((UINT32)apCurThread->mpWorkPtPage));
        K2OSKERN_Debug("map - ptphys is %08X\n", physPageAddr);
        KernMap_MakeOnePage(K2OS_KVA_KERNVAMAP_BASE, virtPT, physPageAddr, K2OS_MAPTYPE_KERN_PAGETABLE);

#if K2_TARGET_ARCH_IS_ARM
        //
        // a32 - install it into the system common page table
        //
        pde = (physPageAddr & K2VMAP32_PAGEPHYS_MASK) | A32_TTBE_PAGETABLE_PROTO;
        pPDE[0] = pde;
        pPDE[1] = (pde + 0x400);
        pPDE[2] = (pde + 0x800);
        pPDE[3] = (pde + 0xC00);
#else
        //
        // x32 - install it into the page directories of all processes
        //
        pde = (physPageAddr & K2_VA32_PAGEFRAME_MASK) | X32_KERN_PAGETABLE_PROTO;
        if (K2OS_MAPTYPE_KERN_PAGEDIR & K2OS_MEMPAGE_ATTR_UNCACHED)
            pde |= X32_PDE_CACHEDISABLE;
        if (K2OS_MAPTYPE_KERN_PAGEDIR & K2OS_MEMPAGE_ATTR_WRITE_THRU)
            pde |= X32_PDE_WRITETHROUGH;
        K2OSKERN_Debug("map - new PDE is %08X\n", pde);

        pListLink = gData.ProcList.mpHead;
        K2_ASSERT(pListLink != NULL);
        do {
            pProc = K2_GET_CONTAINER(K2OSKERN_OBJ_PROCESS, pListLink, ProcListLink);

            pPDE = (((UINT32 *)pProc->mTransTableKVA) + (ptIndex & 0x3FF));
            K2OSKERN_Debug("map - proc %d PDE at 0x%08X\n", pProc->mId, pPDE);

            K2_ASSERT(((*pPDE) & K2OSKERN_PDE_PRESENT_BIT) == 0);

            *pPDE = pde;

            pListLink = pListLink->mpNext;
        } while (pListLink != NULL);
#endif
    }

    //
    // pagetable exists. just map the page appropriately
    //
    pPTE = (UINT32 *)K2OS_KVA_TO_PTE_ADDR(virtAddr);
    K2OSKERN_Debug("map - page pte at %08X\n", pPTE);
    K2_ASSERT(((*pPTE) & K2OSKERN_PTE_PRESENT_BIT) == 0);
    if (apCurThread->mpWorkPage != NULL)
    {
        physPageAddr = K2OS_PHYSTRACK_TO_PHYS32(((UINT32)apCurThread->mpWorkPage));
    }
    else
        physPageAddr = 0;
    KernMap_MakeOnePage(K2OS_KVA_KERNVAMAP_BASE, virtAddr, physPageAddr, apCurThread->mWorkMapAttr);

    if (lockStatus)
    {
        K2OSKERN_SeqIntrUnlock(&gData.KernVirtMapLock, FALSE);
        K2OSKERN_SeqIntrUnlock(&gData.ProcListSeqLock, disp);
        lockStatus = FALSE;
    }
    else
    {
        K2OSKERN_SeqIntrUnlock(&gData.KernVirtMapLock, disp);
    }

    //
    // move physical pages we just used to their lists
    //
    K2_ASSERT(lockStatus == FALSE);

    if (usedPT)
    {
        disp = K2OSKERN_SeqIntrLock(&gData.PhysMemSeqLock);
        lockStatus = TRUE;

        // move phys pt page onto system tracking list
        K2OSKERN_Debug("map - move pagetable page to paging list\n");
        apCurThread->mpWorkPtPage->mpOwnerObject = NULL;
        K2LIST_AddAtTail(&gData.PhysPageList[KernPhysPageList_Paging], &apCurThread->mpWorkPtPage->ListLink);
        apCurThread->mpWorkPtPage = NULL;
    }

    if (apCurThread->mpWorkPage != NULL)
    {
        if (!lockStatus)
        {
            disp = K2OSKERN_SeqIntrLock(&gData.PhysMemSeqLock);
            lockStatus = TRUE;
        }
        K2OSKERN_Debug("map - move page to list %d\n", aTargetList);
        apCurThread->mpWorkPage->mpOwnerObject = apPhysPageOwner;
        apCurThread->mpWorkPage->mFlags = (apCurThread->mpWorkPage->mFlags & ~K2OSKERN_PHYSTRACK_PAGE_LIST_MASK) | (aTargetList << K2OSKERN_PHYSTRACK_PAGE_LIST_SHL);
        K2LIST_AddAtTail(&gData.PhysPageList[aTargetList], &apCurThread->mpWorkPage->ListLink);
        apCurThread->mpWorkPage = NULL;
    }

    if (lockStatus)
        K2OSKERN_SeqIntrUnlock(&gData.PhysMemSeqLock, disp);
}

void KernMap_UserFromThread(K2OSKERN_OBJ_THREAD *apCurThread, void *apPhysPageOwner, KernPhysPageList aTargetList)
{
    K2_ASSERT(0);
}

void KernMap_FromThread(K2OSKERN_OBJ_THREAD *apCurThread, void *apPhysPageOwner, KernPhysPageList aTargetList)
{
    K2_ASSERT(apCurThread->mWorkMapAddr != 0);

    if (apCurThread->mWorkMapAttr & K2OS_MEMPAGE_ATTR_GUARD)
    {
        K2_ASSERT(apCurThread->mpWorkPage == NULL);
        K2_ASSERT(apPhysPageOwner == NULL);
        K2_ASSERT(aTargetList == KernPhysPageList_Error);
    }
    else 
    {
        K2_ASSERT(apCurThread->mpWorkPage != NULL);
    }

    K2_ASSERT(apCurThread->mpWorkPtPage != NULL);

    if (apCurThread->mWorkMapAddr >= K2OS_KVA_KERN_BASE)
    {
        K2_ASSERT((apCurThread->mWorkMapAttr & K2OS_MEMPAGE_ATTR_KERNEL) != 0);
        KernMap_KernFromThread(apCurThread, apPhysPageOwner, aTargetList);
    }
    else
    {
        K2_ASSERT((apCurThread->mWorkMapAttr & K2OS_MEMPAGE_ATTR_KERNEL) == 0);
        KernMap_UserFromThread(apCurThread, apPhysPageOwner, aTargetList);
    }
}


