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

UINT32 * sGetPTE(UINT32 aVirtMapBase, UINT32 aVirtAddr)
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

void KernMap_MakeOnePresentPage(UINT32 aVirtMapBase, UINT32 aVirtAddr, UINT32 aPhysAddr, UINT32 aPageMapAttr)
{
    UINT32 *    pPTE;
    UINT32 *    pPageCount;
    UINT32      pte;

    aPageMapAttr &= K2OS_MEMPAGE_ATTR_MASK;

    pPTE = sGetPTE(aVirtMapBase, aVirtAddr);

    pte = *pPTE;
    K2_ASSERT((pte & K2OSKERN_PTE_PRESENT_BIT) == 0);

    *pPTE = KernArch_MakePTE(aPhysAddr, aPageMapAttr);

    if (0 == (pte & K2OSKERN_PTE_NP_BIT))
    {
        if (aVirtAddr >= K2OS_KVA_KERN_BASE)
        {
            pPageCount = ((UINT32 *)K2OS_KVA_PTPAGECOUNT_BASE) + (aVirtAddr / K2_VA32_PAGETABLE_MAP_BYTES);
        }
        else
        {
            K2_ASSERT(0);
        }
        (*pPageCount)++;
        K2_ASSERT((*pPageCount) <= 1024);
    }

    K2_CpuWriteBarrier();
}

void KernMap_MakeOneNotPresentPage(UINT32 aVirtMapBase, UINT32 aVirtAddr, UINT32 aNpFlags, UINT32 aContent)
{
    UINT32 *    pPTE;
    UINT32 *    pPageCount;
    UINT32      pte;

    K2_ASSERT((aNpFlags & K2OSKERN_PTE_NP_CONTENT_MASK) == 0);
    K2_ASSERT((aContent & ~K2OSKERN_PTE_NP_CONTENT_MASK) == 0);
    K2_ASSERT(aNpFlags & K2OSKERN_PTE_NP_BIT);

    pPTE = sGetPTE(aVirtMapBase, aVirtAddr);

    pte = *pPTE;
    K2_ASSERT((pte & (K2OSKERN_PTE_PRESENT_BIT | K2OSKERN_PTE_NP_BIT)) == 0);

    *pPTE = aNpFlags | aContent;

    if (aVirtAddr >= K2OS_KVA_KERN_BASE)
    {
        pPageCount = ((UINT32 *)K2OS_KVA_PTPAGECOUNT_BASE) + (aVirtAddr / K2_VA32_PAGETABLE_MAP_BYTES);
    }
    else
    {
        K2_ASSERT(0);
    }
    (*pPageCount)++;
    K2_ASSERT((*pPageCount) <= 1024);

    K2_CpuWriteBarrier();
}

UINT32 KernMap_BreakOnePage(UINT32 aVirtMapBase, UINT32 aVirtAddr, UINT32 aNpFlags)
{
    UINT32 *    pPTE;
    UINT32 *    pPageCount;
    UINT32      result;

    pPTE = sGetPTE(aVirtMapBase, aVirtAddr);

    result = ((*pPTE) & K2_VA32_PAGEFRAME_MASK);

    if (aNpFlags & K2OSKERN_PTE_NP_BIT)
    {
        K2_ASSERT(0 == (aNpFlags & K2OSKERN_PTE_PRESENT_BIT));
        *pPTE = aNpFlags;
    }
    else
    {
        *pPTE = 0;

        if (aVirtAddr >= K2OS_KVA_KERN_BASE)
        {
            pPageCount = ((UINT32 *)K2OS_KVA_PTPAGECOUNT_BASE) + (aVirtAddr / K2_VA32_PAGETABLE_MAP_BYTES);
        }
        else
        {
            K2_ASSERT(0);
        }
        K2_ASSERT((*pPageCount) > 0);
        (*pPageCount)--;
    }

    K2_CpuWriteBarrier();

    return result;
}

void KernMap_MakeOneKernPageFromThread(K2OSKERN_OBJ_THREAD *apCurThread, void *apPhysPageOwner, KernPhysPageList aTargetList)
{
    UINT32 *                pPDE;
    UINT32                  pde;
    BOOL                    disp;
    BOOL                    lockStatus;
    UINT32                  virtAddr;
    UINT32                  virtPT;
    UINT32                  physPageAddr;
    UINT32                  ptIndex;
    BOOL                    usedPT;
    UINT32 *                pPtPageCount;

#if !K2_TARGET_ARCH_IS_ARM
    K2LIST_LINK *           pListLink;
    K2OSKERN_OBJ_PROCESS *  pProc;
#endif

    virtAddr = apCurThread->mWorkMapAddr;

    ptIndex = virtAddr / K2_VA32_PAGETABLE_MAP_BYTES;

    virtPT = K2OS_KVA_TO_PT_ADDR(virtAddr);

#if K2_TARGET_ARCH_IS_ARM
    pPDE = (((UINT32 *)K2OS_KVA_TRANSTAB_BASE) + ((ptIndex & 0x3FF) * 4));
#else
    pPDE = (((UINT32 *)K2OS_KVA_TRANSTAB_BASE) + (ptIndex & 0x3FF));
#endif

    pPtPageCount = (UINT32 *)K2OS_KVA_PTPAGECOUNT_BASE;

    disp = K2OSKERN_SeqIntrLock(&gData.KernVirtMapLock);

    if ((*pPDE) & K2OSKERN_PDE_PRESENT_BIT)
    {
        //
        // pageTable is already mapped
        //
        lockStatus = FALSE;
        usedPT = FALSE;
        K2_ASSERT(pPtPageCount[ptIndex] != 0);
    }
    else
    {
        K2_ASSERT(pPtPageCount[ptIndex] == 0);

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
        K2_ASSERT(((*((UINT32 *)K2OS_KVA_TO_PTE_ADDR(virtPT))) & K2OSKERN_PTE_PRESENT_BIT) == 0);
        physPageAddr = K2OS_PHYSTRACK_TO_PHYS32(((UINT32)apCurThread->mpWorkPtPage));
//        K2OSKERN_Debug("MAKE PT %08X -> %08X\n", virtPT, physPageAddr);
        KernMap_MakeOnePresentPage(K2OS_KVA_KERNVAMAP_BASE, virtPT, physPageAddr, K2OS_MAPTYPE_KERN_PAGETABLE);

#if K2_TARGET_ARCH_IS_ARM
        //
        // a32 - install it into the system common page table
        //
        pde = (physPageAddr & K2_VA32_PAGEFRAME_MASK) | A32_TTBE_PAGETABLE_PROTO;
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

        pListLink = gData.ProcList.mpHead;
        K2_ASSERT(pListLink != NULL);
        do {
            pProc = K2_GET_CONTAINER(K2OSKERN_OBJ_PROCESS, pListLink, ProcListLink);

            pPDE = (((UINT32 *)pProc->mTransTableKVA) + (ptIndex & 0x3FF));

            K2_ASSERT(((*pPDE) & K2OSKERN_PDE_PRESENT_BIT) == 0);

            *pPDE = pde;

            pListLink = pListLink->mpNext;
        } while (pListLink != NULL);
#endif
    }

    //
    // pagetable now exists. just map the page appropriately
    //
    K2_ASSERT(((*((UINT32 *)K2OS_KVA_TO_PTE_ADDR(virtAddr))) & K2OSKERN_PTE_PRESENT_BIT) == 0);
    if (apCurThread->mpWorkPage != NULL)
    {
        physPageAddr = K2OS_PHYSTRACK_TO_PHYS32(((UINT32)apCurThread->mpWorkPage));
//        K2OSKERN_Debug("MAKE PG %08X -> %08X\n", virtAddr, physPageAddr);
        KernMap_MakeOnePresentPage(K2OS_KVA_KERNVAMAP_BASE, virtAddr, physPageAddr, apCurThread->mWorkMapAttr);
    }
    else
    {
//        K2OSKERN_Debug("MAKE Px %08X (attr %08X)\n", virtAddr, apCurThread->mWorkMapAttr);
        KernMap_MakeOneNotPresentPage(K2OS_KVA_KERNVAMAP_BASE, virtAddr, apCurThread->mWorkMapAttr, 0);
    }

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
        apCurThread->mpWorkPage->mpOwnerObject = apPhysPageOwner;
        apCurThread->mpWorkPage->mFlags = (apCurThread->mpWorkPage->mFlags & ~K2OSKERN_PHYSTRACK_PAGE_LIST_MASK) | (aTargetList << K2OSKERN_PHYSTRACK_PAGE_LIST_SHL);
        K2LIST_AddAtTail(&gData.PhysPageList[aTargetList], &apCurThread->mpWorkPage->ListLink);
        apCurThread->mpWorkPage = NULL;
    }

    if (lockStatus)
        K2OSKERN_SeqIntrUnlock(&gData.PhysMemSeqLock, disp);
}

void KernMap_MakeOneUserPageFromThread(K2OSKERN_OBJ_THREAD *apCurThread, void *apPhysPageOwner, KernPhysPageList aTargetList)
{
    K2_ASSERT(0);
}

void KernMap_MakeOnePageFromThread(K2OSKERN_OBJ_THREAD *apCurThread, void *apPhysPageOwner, KernPhysPageList aTargetList)
{
    K2_ASSERT(apCurThread->mWorkMapAddr != 0);

    if (apCurThread->mpWorkPage == NULL)
    {
        K2_ASSERT(apCurThread->mWorkMapAttr & K2OSKERN_PTE_NP_BIT);
        K2_ASSERT(apPhysPageOwner == NULL);
        K2_ASSERT(aTargetList == KernPhysPageList_Error);
    }

    if (apCurThread->mWorkMapAddr >= K2OS_KVA_KERN_BASE)
    {
        if (apCurThread->mpWorkPage != NULL)
        {
            K2_ASSERT((apCurThread->mWorkMapAttr & K2OS_MEMPAGE_ATTR_KERNEL) != 0);
        }
        KernMap_MakeOneKernPageFromThread(apCurThread, apPhysPageOwner, aTargetList);
    }
    else
    {
        if (apCurThread->mpWorkPage != NULL)
        {
            K2_ASSERT((apCurThread->mWorkMapAttr & K2OS_MEMPAGE_ATTR_KERNEL) == 0);
        }
        KernMap_MakeOneUserPageFromThread(apCurThread, apPhysPageOwner, aTargetList);
    }
}

UINT32 KernMap_BreakOneKernPageToThread(K2OSKERN_OBJ_THREAD *apCurThread, void *apMatchPhysPageOwner, KernPhysPageList aMatchList, UINT32 aNpFlags)
{
    UINT32 *                    pPtPageCount;
    UINT32                      ptIndex;
    UINT32                      virtAddr;
    BOOL                        disp;
    BOOL                        disp2;
    UINT32                      physPageAddr;
    UINT32                      virtPt;
    BOOL                        emptyPt;
    K2OSKERN_PHYSTRACK_PAGE *   pPhysPage;

    pPtPageCount = (UINT32 *)K2OS_KVA_PTPAGECOUNT_BASE;

    virtAddr = apCurThread->mWorkMapAddr;

    ptIndex = virtAddr / K2_VA32_PAGETABLE_MAP_BYTES;

    disp = K2OSKERN_SeqIntrLock(&gData.KernVirtMapLock);

    if (0 == ((*((UINT32 *)K2OS_KVA_TO_PTE_ADDR(virtAddr))) & K2OSKERN_PTE_PRESENT_BIT))
    {
        K2_ASSERT((*((UINT32 *)K2OS_KVA_TO_PTE_ADDR(virtAddr))) & K2OSKERN_PTE_NP_BIT);
        K2_ASSERT(apMatchPhysPageOwner == NULL);
        K2_ASSERT(aMatchList == KernPhysPageList_Error);
        physPageAddr = KernMap_BreakOnePage(K2OS_KVA_KERNVAMAP_BASE, virtAddr, aNpFlags);
        K2_ASSERT(physPageAddr == 0);
    }
    else
    {
        physPageAddr = KernMap_BreakOnePage(K2OS_KVA_KERNVAMAP_BASE, virtAddr, aNpFlags);

        if (aMatchList != KernPhysPageList_Error)
        {
            pPhysPage = (K2OSKERN_PHYSTRACK_PAGE *)K2OS_PHYS32_TO_PHYSTRACK(physPageAddr);
//            K2OSKERN_Debug("BRAK PG %08X (was -> %08X)\n", virtAddr, physPageAddr);
            K2_ASSERT(pPhysPage->mpOwnerObject == apMatchPhysPageOwner);
            K2_ASSERT(((pPhysPage->mFlags & K2OSKERN_PHYSTRACK_PAGE_LIST_MASK) >> K2OSKERN_PHYSTRACK_PAGE_LIST_SHL) == aMatchList);

            disp2 = K2OSKERN_SeqIntrLock(&gData.PhysMemSeqLock);
            K2LIST_Remove(&gData.PhysPageList[aMatchList], &pPhysPage->ListLink);
            apCurThread->mpWorkPage = pPhysPage;
            K2OSKERN_SeqIntrUnlock(&gData.PhysMemSeqLock, disp2);
        }
    }

    emptyPt = (pPtPageCount[ptIndex] == 0) ? TRUE : FALSE;
    if (emptyPt)
    {
        //
        // ensure PT is clean
        //
        virtPt = K2OS_KVA_TO_PT_ADDR(virtAddr);
        K2_ASSERT(0 != K2MEM_VerifyZero((void *)virtPt, K2_VA32_MEMPAGE_BYTES));
#if K2_TARGET_ARCH_IS_ARM
        //
        // do not have to go into scheduler to unmap a pagetable or flush the TLB
        //
        K2_ASSERT(0);

        // set apCurThread->mpWorkPtPage

        apCurThread->mTlbFlushNeeded = FALSE;
        apCurThread->mTlbFlushBase = 0;
        apCurThread->mTlbFlushPages = 0;
#endif
    }

    K2OSKERN_SeqIntrUnlock(&gData.KernVirtMapLock, disp);

#if K2_TARGET_ARCH_IS_INTEL
    if (emptyPt)
    {
        //
        // try to unmap the pagetable and flush the TLB here via the scheduler
        //
        apCurThread->Sched.Item.mSchedItemType = KernSchedItem_PurgePT;
        apCurThread->Sched.Item.Args.PurgePt.mPtIndex = ptIndex;
        KernArch_ThreadCallSched();
        apCurThread->mpWorkPtPage = (K2OSKERN_PHYSTRACK_PAGE *)K2OS_PHYS32_TO_PHYSTRACK(apCurThread->Sched.Item.Args.PurgePt.mPtPhysOut);
//        K2OSKERN_Debug("BRAK PT %08X (was -> %08X)\n", virtPt, physPageAddr);
        apCurThread->mTlbFlushNeeded = FALSE;
        apCurThread->mTlbFlushBase = 0;
        apCurThread->mTlbFlushPages = 0;
    }
#endif

    if (emptyPt)
    {
        K2_ASSERT(apCurThread->mpWorkPtPage != NULL);
    }

    return physPageAddr;
}

UINT32 KernMap_BreakOneUserPageToThread(K2OSKERN_OBJ_THREAD *apCurThread, void *apMatchPhysPageOwner, KernPhysPageList aMatchList, UINT32 aNpFlags)
{
    K2_ASSERT(0);
    return (UINT32)-1;
}

UINT32 KernMap_BreakOnePageToThread(K2OSKERN_OBJ_THREAD *apCurThread, void *apMatchPhysPageOwner, KernPhysPageList aMatchList, UINT32 aNpFlags)
{
    K2_ASSERT(apCurThread->mpWorkPage == NULL);
    K2_ASSERT(apCurThread->mpWorkPtPage == NULL);

    K2_ASSERT(apCurThread->mWorkMapAddr != 0);

    if (apCurThread->mWorkMapAttr & K2OS_MEMPAGE_ATTR_SPEC_NP)
    {
        K2_ASSERT(apMatchPhysPageOwner == NULL);
        K2_ASSERT(aMatchList == KernPhysPageList_Error);
    }

    if (apCurThread->mWorkMapAddr >= K2OS_KVA_KERN_BASE)
    {
        return KernMap_BreakOneKernPageToThread(apCurThread, apMatchPhysPageOwner, aMatchList, aNpFlags);
    }

    return KernMap_BreakOneUserPageToThread(apCurThread, apMatchPhysPageOwner, aMatchList, aNpFlags);
}

BOOL KernMap_SegRangeNotMapped(K2OSKERN_OBJ_THREAD *apCurThread, K2OSKERN_OBJ_SEGMENT *apSeg, UINT32 aPageOffset, UINT32 aPageCount)
{
    BOOL    disp;
    UINT32 *pPTE;
    UINT32  pte;
    UINT32  segPageCount;
    UINT32  virtAddr;
    UINT32  mapBase;

    segPageCount = (apSeg->mPagesBytes / K2_VA32_MEMPAGE_BYTES);

    K2_ASSERT((aPageOffset + aPageCount) <= segPageCount);

    virtAddr = apSeg->ProcSegTreeNode.mUserVal + (aPageOffset * K2_VA32_MEMPAGE_BYTES);

    if (virtAddr >= K2OS_KVA_KERN_BASE)
        mapBase = gpProc0->mVirtMapKVA;
    else
        mapBase = apCurThread->mpProc->mVirtMapKVA;

    pPTE = (UINT32 *)K2_VA32_TO_PTE_ADDR(mapBase, virtAddr);

    disp = K2OSKERN_SeqIntrLock(&gData.KernVirtMapLock);

    do {
        pte = *pPTE;
        if ((0 == (pte & K2OSKERN_PTE_NP_BIT)) ||
            (0 != (pte & K2OSKERN_PTE_PRESENT_BIT)))
        {
            K2OSKERN_Debug("Check SegRangeNotMapped - ERROR %08X pte %08X\n", virtAddr, pte);
            break;
        }
        pPTE++;
        virtAddr += K2_VA32_MEMPAGE_BYTES;
    } while (--aPageCount);

    K2OSKERN_SeqIntrUnlock(&gData.KernVirtMapLock, disp);

    return (aPageCount == 0) ? TRUE : FALSE;
}

BOOL KernMap_SegRangeMapped(K2OSKERN_OBJ_THREAD *apCurThread, K2OSKERN_OBJ_SEGMENT *apSeg, UINT32 aPageOffset, UINT32 aPageCount)
{
    BOOL    disp;
    UINT32 *pPTE;
    UINT32  pte;
    UINT32  segPageCount;
    UINT32  virtAddr;
    UINT32  mapBase;

    segPageCount = (apSeg->mPagesBytes / K2_VA32_MEMPAGE_BYTES);

    K2_ASSERT((aPageOffset + aPageCount) <= segPageCount);

    virtAddr = apSeg->ProcSegTreeNode.mUserVal + (aPageOffset * K2_VA32_MEMPAGE_BYTES);

    if (virtAddr >= K2OS_KVA_KERN_BASE)
        mapBase = gpProc0->mVirtMapKVA;
    else
        mapBase = apCurThread->mpProc->mVirtMapKVA;

    pPTE = (UINT32 *)K2_VA32_TO_PTE_ADDR(mapBase, virtAddr);

    disp = K2OSKERN_SeqIntrLock(&gData.KernVirtMapLock);

    do {
        pte = *pPTE;
        if (0 == (pte & K2OSKERN_PTE_PRESENT_BIT))
        {
            K2OSKERN_Debug("Check SegRangeMapped - ERROR %08X pte %08X\n", virtAddr, pte);
            break;
        }
        pPTE++;
        virtAddr += K2_VA32_MEMPAGE_BYTES;
    } while (--aPageCount);

    K2OSKERN_SeqIntrUnlock(&gData.KernVirtMapLock, disp);

    return (aPageCount == 0) ? TRUE : FALSE;
}

void KernMap_FindMapped(K2OSKERN_OBJ_THREAD *apCurThread, UINT32 aVirtAddr, UINT32 aPageCount, UINT32 *apScanIx, UINT32 *apFoundCount)
{
    BOOL        disp;
    UINT32 *    pPTE;
    UINT32      pte;
    UINT32      mapBase;

    K2_ASSERT(*apScanIx < aPageCount);
    *apFoundCount = 0;

    if (aVirtAddr >= K2OS_KVA_KERN_BASE)
        mapBase = gpProc0->mVirtMapKVA;
    else
        mapBase = apCurThread->mpProc->mVirtMapKVA;

    aVirtAddr += (*apScanIx) * K2_VA32_MEMPAGE_BYTES;
    aPageCount -= (*apScanIx);

    pPTE = (UINT32 *)K2_VA32_TO_PTE_ADDR(mapBase, aVirtAddr);

    disp = K2OSKERN_SeqIntrLock(&gData.KernVirtMapLock);

    //
    // look for starting mapped entry 
    //
    do {
        pte = *pPTE;
        if (0 != (pte & K2OSKERN_PTE_PRESENT_BIT))
            break;
        (*apScanIx)++;
        pPTE++;
        aVirtAddr += K2_VA32_MEMPAGE_BYTES;
    } while (--aPageCount);

    if (aPageCount > 0)
    {
        //
        // found something before we hit the end of the range
        //
        (*apFoundCount)++;
        pPTE++;
        aVirtAddr += K2_VA32_MEMPAGE_BYTES;
        if (--aPageCount > 0)
        {
            do {
                pte = *pPTE;
                if (0 == (pte & K2OSKERN_PTE_PRESENT_BIT))
                    break;
                (*apFoundCount)++;
                pPTE++;
                aVirtAddr += K2_VA32_MEMPAGE_BYTES;
            } while (--aPageCount);
        }
    }

    K2OSKERN_SeqIntrUnlock(&gData.KernVirtMapLock, disp);
}
