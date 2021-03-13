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

/*

At initialization:

...
|XXX-USED-XXX|  
+------------+  <<- gData.LoadInfo.mKernArenaHigh - used and mapped above this
|            |
+------------+
|            |  (unused but mappable virtual page frames)
...             
|            |
+------------+  <<- sgTrack.mTopPt, pagetable map boundary (4MB)
|            |
|            |
...             (unused and unmappable virtual page frames)
|            |
|            |
+------------+  <<- sgTrack.mBotPt, pagetable map boundary (4MB)
|            |
...             
|            |  (unused but mappable virtual page frames)
+------------+
|            |
+------------+  <<- gData.LoadInfo.mKernArenaLow - used and mapped below this
|XXX-USED-XXX|  
...

*/

#define HEAPNODES_PER_PAGE (K2_VA32_MEMPAGE_BYTES / sizeof(K2HEAP_NODE))

typedef struct _KERNVIRT_TRACK KERNVIRT_TRACK;
struct _KERNVIRT_TRACK
{
    UINT32  mTopPt;     // top-down address of where first page after last free pagetable ENDS
    UINT32  mBotPt;     // bottom-up address of where first byte after last free pagetable STARTS

    K2HEAP_ANCHOR       VirtHeap;
    K2LIST_ANCHOR       VirtHeapFreeNodeList;
    K2OSKERN_SEQLOCK    VirtHeapSeqLock;
    K2HEAP_NODE         StockNode[2];

    K2RAMHEAP           RamHeap;
    K2OSKERN_SEQLOCK    RamHeapSeqLock;
    UINT32              mRamHeapTlbFlushAddr;
    UINT32              mRamHeapTlbFlushPages;
};

static K2HEAP_NODE * sVirtHeapAcqNode(K2HEAP_ANCHOR *apHeap);
static void          sVirtHeapRelNode(K2HEAP_ANCHOR *apHeap, K2HEAP_NODE *apNode);

static UINT32  sKernRamHeap_Lock(K2RAMHEAP *apRamHeap);
static void    sKernRamHeap_Unlock(K2RAMHEAP *apRamHeap, UINT32 aDisp);
static K2STAT  sKernRamHeap_CreateRange(UINT32 aTotalPageCount, UINT32 aInitCommitPageCount, void **appRetRange, UINT32 *apRetStartAddr);
static K2STAT  sKernRamHeap_CommitPages(void *aRange, UINT32 aAddr, UINT32 aPageCount);
static K2STAT  sKernRamHeap_DecommitPages(void *aRange, UINT32 aAddr, UINT32 aPageCount);
static K2STAT  sKernRamHeap_DestroyRange(void *aRange);

static KERNVIRT_TRACK       sgTrack;
static K2RAMHEAP_SUPP const sgRamHeapSupp =
{
    sKernRamHeap_Lock,
    sKernRamHeap_Unlock,
    sKernRamHeap_CreateRange,
    sKernRamHeap_CommitPages,
    sKernRamHeap_DecommitPages,
    sKernRamHeap_DestroyRange
};

static UINT32   iVirtLocked_Alloc(UINT32 aPagesCount);
static void     iVirtLocked_Free(UINT32 aPagesAddr);

void
KernVirt_Init(
    void
)
{
    UINT32  chk;
    UINT32 *pPTE;

    K2MEM_Zero(&sgTrack, sizeof(sgTrack));

    //
    // verify bottom-up remaining page range is really free (no pages mapped)
    //
    if (0 != (gData.LoadInfo.mKernArenaLow & (K2_VA32_PAGETABLE_MAP_BYTES - 1)))
    {
        chk = gData.LoadInfo.mKernArenaLow;
        pPTE = (UINT32 *)K2OS_KVA_TO_PTE_ADDR(chk);
        do
        {
            // verify this page is not mapped
            K2_ASSERT(0 == ((*pPTE) & K2OSKERN_PTE_PRESENT_BIT));
            pPTE++;
            chk += K2_VA32_MEMPAGE_BYTES;
        } while (0 != (chk & (K2_VA32_PAGETABLE_MAP_BYTES - 1)));
        sgTrack.mBotPt = chk;
    }
    else
        sgTrack.mBotPt = gData.LoadInfo.mKernArenaLow;

    //
    // verify top-down remaining page range is really free (no pages mapped)
    //
    if (0 != (gData.LoadInfo.mKernArenaHigh & (K2_VA32_PAGETABLE_MAP_BYTES - 1)))
    {
        chk = gData.LoadInfo.mKernArenaHigh;
        pPTE = (UINT32 *)K2OS_KVA_TO_PTE_ADDR(chk);
        do
        {
            // verify this page is not mapped
            chk -= K2_VA32_MEMPAGE_BYTES;
            pPTE--;
            K2_ASSERT(0 == ((*pPTE) & K2OSKERN_PTE_PRESENT_BIT));
        } while (0 != (chk & (K2_VA32_PAGETABLE_MAP_BYTES - 1)));
        sgTrack.mTopPt = chk;
    }
    else
        sgTrack.mTopPt = gData.LoadInfo.mKernArenaHigh;

    //
    // verify all pagetables between bottom and top pt are not mapped
    //
    if (sgTrack.mBotPt != sgTrack.mTopPt)
    {
        chk = sgTrack.mBotPt;
        pPTE = (UINT32 *)K2OS_KVA_TO_PTE_ADDR(K2OS_KVA_TO_PT_ADDR(chk));
        do
        {
            K2_ASSERT(0 == ((*pPTE) & K2OSKERN_PTE_PRESENT_BIT));
            pPTE++;
            chk += K2_VA32_PAGETABLE_MAP_BYTES;
        } while (chk != sgTrack.mTopPt);
    }

    //
    // this heap is the kernel's object heap. it uses the range heap
    //
    K2OSKERN_SeqInit(&sgTrack.RamHeapSeqLock);
    K2RAMHEAP_Init(&sgTrack.RamHeap, &sgRamHeapSupp);

    //
    // this heap is the kernel's virtual range heap
    //
    K2OSKERN_SeqInit(&sgTrack.VirtHeapSeqLock);
    K2LIST_Init(&sgTrack.VirtHeapFreeNodeList);
    K2HEAP_Init(&sgTrack.VirtHeap, sVirtHeapAcqNode, sVirtHeapRelNode);

    //
    // set up the range heap with the ranges for calculated above.  this is why we
    // have stock nodes
    //
    if (gData.LoadInfo.mKernArenaHigh != sgTrack.mTopPt)
    {
        K2LIST_AddAtTail(&sgTrack.VirtHeapFreeNodeList, (K2LIST_LINK *)&sgTrack.StockNode[0]);
        K2HEAP_AddFreeSpaceNode(&sgTrack.VirtHeap, sgTrack.mTopPt, gData.LoadInfo.mKernArenaHigh - sgTrack.mTopPt, NULL);
    }
    if (gData.LoadInfo.mKernArenaLow != sgTrack.mBotPt)
    {
        K2LIST_AddAtTail(&sgTrack.VirtHeapFreeNodeList, (K2LIST_LINK *)&sgTrack.StockNode[1]);
        K2HEAP_AddFreeSpaceNode(&sgTrack.VirtHeap, gData.LoadInfo.mKernArenaLow, sgTrack.mBotPt - gData.LoadInfo.mKernArenaLow, NULL);
    }
}

static UINT32 
sKernRamHeap_Lock(
    K2RAMHEAP * apRamHeap
)
{
    return K2OSKERN_SeqLock(&sgTrack.RamHeapSeqLock);
}

static void 
sKernRamHeap_Unlock(
    K2RAMHEAP * apRamHeap, 
    UINT32      aDisp
)
{
    K2OSKERN_SeqUnlock(&sgTrack.RamHeapSeqLock, (BOOL)aDisp);
}

static K2STAT 
sKernRamHeap_CreateRange(
    UINT32      aTotalPageCount, 
    UINT32      aInitCommitPageCount, 
    void **     appRetRange, 
    UINT32 *    apRetStartAddr
)
{
    //
    // both sgTrack.RamHeapSeqLock and sgTrack.VirtHeapSeqLock are held
    //
    UINT32 ix;
    UINT32 virtSpace;
    UINT32 physPageAddr;

    K2_ASSERT(aInitCommitPageCount <= aTotalPageCount);

    virtSpace = iVirtLocked_Alloc(aTotalPageCount);
    if (0 == virtSpace)
        return K2STAT_ERROR_OUT_OF_MEMORY;

    //
    // pull physical pages for initial commit count
    //
    for (ix = 0; ix < aInitCommitPageCount; ix++)
    {
        physPageAddr = KernPhys_AllocOneKernelPage(NULL);
        if (0 == physPageAddr)
            break;
        KernMap_MakeOnePresentPage(gpProc1, virtSpace + (ix * K2_VA32_MEMPAGE_BYTES), physPageAddr, K2OS_MAPTYPE_KERN_DATA);
    }
    if (ix < aInitCommitPageCount)
    {
        if (ix > 0)
        {
            //
            // do not need to flush tlbs here since nobody ever used this mapping
            //
            do
            {
                --ix;
                physPageAddr = KernMap_BreakOnePage(gpProc1, virtSpace + (ix * K2_VA32_MEMPAGE_BYTES), 0);
                KernPhys_FreeOneKernelPage(physPageAddr);
            } while (ix > 0);
        }
        iVirtLocked_Free(virtSpace);
        return K2STAT_ERROR_OUT_OF_MEMORY;
    }

    *appRetRange = (void *)virtSpace;
    *apRetStartAddr = virtSpace;

    return K2STAT_NO_ERROR;
}

static K2STAT 
sKernRamHeap_CommitPages(
    void *  aRange, 
    UINT32  aAddr, 
    UINT32  aPageCount
)
{
    //
    // both sgTrack.RamHeapSeqLock and sgTrack.VirtHeapSeqLock are held
    //
    UINT32 ix;
    UINT32 physPageAddr;

    //
    // pull physical pages for initial commit count
    //
    for (ix = 0; ix < aPageCount; ix++)
    {
        physPageAddr = KernPhys_AllocOneKernelPage(NULL);
        if (0 == physPageAddr)
            break;
        KernMap_MakeOnePresentPage(gpProc1, aAddr + (ix * K2_VA32_MEMPAGE_BYTES), physPageAddr, K2OS_MAPTYPE_KERN_DATA);
    }
    if (ix < aPageCount)
    {
        if (ix > 0)
        {
            //
            // do not need to flush tlbs here since nobody ever used this mapping
            //
            do
            {
                --ix;
                physPageAddr = KernMap_BreakOnePage(gpProc1, aAddr + (ix * K2_VA32_MEMPAGE_BYTES), 0);
                KernPhys_FreeOneKernelPage(physPageAddr);
            } while (ix > 0);
        }
        return K2STAT_ERROR_OUT_OF_MEMORY;
    }

    return K2STAT_NO_ERROR;
}

static K2STAT 
sKernRamHeap_DecommitPages(
    void *  aRange, 
    UINT32  aAddr, 
    UINT32  aPageCount
)
{
    //
    // both sgTrack.RamHeapSeqLock and sgTrack.VirtHeapSeqLock are held
    //
    UINT32 ix;
    UINT32 physPageAddr;

    for (ix = 0; ix < aPageCount; ix++)
    {
        physPageAddr = KernMap_BreakOnePage(gpProc1, aAddr + (ix * K2_VA32_MEMPAGE_BYTES), 0);
        KernPhys_FreeOneKernelPage(physPageAddr);
    }
    sgTrack.mRamHeapTlbFlushAddr = aAddr;
    sgTrack.mRamHeapTlbFlushPages = aPageCount;

    return K2STAT_NO_ERROR;
}

static K2STAT 
sKernRamHeap_DestroyRange(
    void *aRange
)
{
    //
    // both sgTrack.RamHeapSeqLock and sgTrack.VirtHeapSeqLock are held
    //

    return K2STAT_ERROR_NOT_IMPL;
}

void *  
KernHeap_Alloc(
    UINT32 aBytes
)
{
    K2STAT  stat;
    void *  retPtr;
    BOOL    disp;
    UINT32  tlbFlushAddr;
    UINT32  tlbFlushPages;

    disp = K2OSKERN_SeqLock(&sgTrack.VirtHeapSeqLock);

    stat = K2RAMHEAP_Alloc(&sgTrack.RamHeap, aBytes, TRUE, &retPtr);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));

    tlbFlushAddr = sgTrack.mRamHeapTlbFlushAddr;
    if (tlbFlushAddr)
    {
        tlbFlushPages = sgTrack.mRamHeapTlbFlushPages;
        K2_ASSERT(0 != tlbFlushPages);
        sgTrack.mRamHeapTlbFlushAddr = 0;
    }
    
    K2OSKERN_SeqUnlock(&sgTrack.VirtHeapSeqLock, disp);

    if (0 != tlbFlushAddr)
    {
        KernMap_FlushTlb(gpProc1, tlbFlushAddr, tlbFlushPages);
    }

    return retPtr;
}

void    
KernHeap_Free(
    void *aPtr
)
{
    K2STAT  stat;
    BOOL    disp;
    UINT32  tlbFlushAddr;
    UINT32  tlbFlushPages;

    disp = K2OSKERN_SeqLock(&sgTrack.VirtHeapSeqLock);

    stat = K2RAMHEAP_Free(&sgTrack.RamHeap, aPtr);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));

    tlbFlushAddr = sgTrack.mRamHeapTlbFlushAddr;
    if (tlbFlushAddr)
    {
        tlbFlushPages = sgTrack.mRamHeapTlbFlushPages;
        K2_ASSERT(0 != tlbFlushPages);
        sgTrack.mRamHeapTlbFlushAddr = 0;
    }

    K2OSKERN_SeqUnlock(&sgTrack.VirtHeapSeqLock, disp);

    if (0 != tlbFlushAddr)
    {
        KernMap_FlushTlb(gpProc1, tlbFlushAddr, tlbFlushPages);
    }
}

static K2HEAP_NODE * 
sVirtHeapAcqNode(
    K2HEAP_ANCHOR *apHeap
)
{
    K2LIST_LINK * pListLink;

    K2_ASSERT(apHeap == &sgTrack.VirtHeap);

    pListLink = sgTrack.VirtHeapFreeNodeList.mpHead;
    if (NULL != pListLink)
    {
        K2LIST_Remove(&sgTrack.VirtHeapFreeNodeList, pListLink);
    }

    return (K2HEAP_NODE *)pListLink;
}

static void 
sVirtHeapRelNode(
    K2HEAP_ANCHOR * apHeap, 
    K2HEAP_NODE *   apNode
)
{
    K2_ASSERT(apHeap == &sgTrack.VirtHeap);
    K2LIST_AddAtTail(&sgTrack.VirtHeapFreeNodeList, (K2LIST_LINK *)apNode);
}

static BOOL
sReplenishTracking(
    void
)
{
    UINT32          physPageAddr;
    UINT32          ix;
    K2HEAP_NODE *   pHeapNode;
    K2STAT          stat;
    K2TREE_NODE *   pTreeNode;

    K2OSKERN_Debug("VirtHeap SizeTree nodecount = %d\n", sgTrack.VirtHeap.SizeTree.mNodeCount);
    if (sgTrack.VirtHeap.SizeTree.mNodeCount == 0)
    {
        //
        // need to add some free space to the heap
        //
        K2OSKERN_Debug("toppt = %08X\n", sgTrack.mTopPt);
        K2OSKERN_Debug("botpt = %08X\n", sgTrack.mBotPt);
        if (sgTrack.mTopPt == sgTrack.mBotPt)
        {
            //
            // virtual memory exhausted
            //
            K2_ASSERT(0);
            return FALSE;
        }

        physPageAddr = KernPhys_AllocOneKernelPage(NULL);
        K2OSKERN_Debug("Alloced phys page %08X\n", physPageAddr);
        if (0 == physPageAddr)
        {
            // physical memory exhausted
            K2_ASSERT(0);
            return FALSE;
        }

        K2OSKERN_Debug("Install PT for va %08X\n", sgTrack.mTopPt - K2_VA32_PAGETABLE_MAP_BYTES);
        KernArch_InstallPageTable(gpProc1, sgTrack.mTopPt - K2_VA32_PAGETABLE_MAP_BYTES, physPageAddr, TRUE);

        pHeapNode = (K2HEAP_NODE *)(sgTrack.mTopPt - K2_VA32_MEMPAGE_BYTES);
        K2OSKERN_Debug("pHeapNode = %08X\n", pHeapNode);

        sgTrack.mTopPt -= K2_VA32_PAGETABLE_MAP_BYTES;

        //
        // cant install this virtual space into the heap without tracking structures, so we need to
        // add a page of tracking, then add the free space, then alloc the tracking from the heap
        //
        physPageAddr = KernPhys_AllocOneKernelPage(NULL);
        K2OSKERN_Debug("Alloced phys page %08X\n", physPageAddr);
        if (0 == physPageAddr)
        {
            // physical memory exhausted
            K2_ASSERT(0);
            return FALSE;
        }

        K2OSKERN_Debug("Map v%08X -> p%08X\n", (UINT32)pHeapNode, physPageAddr);
        KernMap_MakeOnePresentPage(gpProc1, (UINT32)pHeapNode, physPageAddr, K2OS_MAPTYPE_KERN_DATA);
        for (ix = 0; ix < HEAPNODES_PER_PAGE; ix++)
        {
            K2LIST_AddAtTail(&sgTrack.VirtHeapFreeNodeList, (K2LIST_LINK *)&pHeapNode[ix]);
        }

        K2OSKERN_Debug("AddFreeSpace %08X (%08X)\n", sgTrack.mTopPt, K2_VA32_PAGETABLE_MAP_BYTES);
        stat = K2HEAP_AddFreeSpaceNode(&sgTrack.VirtHeap, sgTrack.mTopPt, K2_VA32_PAGETABLE_MAP_BYTES, NULL);
        K2_ASSERT(!K2STAT_IS_ERROR(stat));

        K2OSKERN_Debug("AllocAt(%08X,%08X)\n", (UINT32)pHeapNode, K2_VA32_MEMPAGE_BYTES);
        stat = K2HEAP_AllocAt(&sgTrack.VirtHeap, (UINT32)pHeapNode, K2_VA32_MEMPAGE_BYTES);
        K2_ASSERT(!K2STAT_IS_ERROR(stat));
    }
    else
    {
        //
        // there is space in the heap to alloc a page - take it from the start of the smallest size chunk
        //
        pTreeNode = K2TREE_LastNode(&sgTrack.VirtHeap.SizeTree);
        pHeapNode = K2_GET_CONTAINER(K2HEAP_NODE, pTreeNode, SizeTreeNode);
        K2OSKERN_Debug("Last node in size tree = addr %08X size %08X\n", pHeapNode->AddrTreeNode.mUserVal, pHeapNode->SizeTreeNode.mUserVal);

        physPageAddr = KernPhys_AllocOneKernelPage(NULL);
        K2OSKERN_Debug("Alloced phys page %08X\n", physPageAddr);
        if (0 == physPageAddr)
        {
            // physical memory exhausted
            K2_ASSERT(0);
            return FALSE;
        }

        pHeapNode = (K2HEAP_NODE *)pHeapNode->AddrTreeNode.mUserVal;
        K2OSKERN_Debug("pHeapNode = %08X\n", pHeapNode);

        K2OSKERN_Debug("Map v%08X -> p%08X\n", (UINT32)pHeapNode, physPageAddr);
        KernMap_MakeOnePresentPage(gpProc1, (UINT32)pHeapNode, physPageAddr, K2OS_MAPTYPE_KERN_DATA);
        for (ix = 0; ix < HEAPNODES_PER_PAGE; ix++)
        {
            K2LIST_AddAtTail(&sgTrack.VirtHeapFreeNodeList, (K2LIST_LINK *)&pHeapNode[ix]);
        }

        K2OSKERN_Debug("AllocAt(%08X,%08X)\n", (UINT32)pHeapNode, K2_VA32_MEMPAGE_BYTES);
        stat = K2HEAP_AllocAt(&sgTrack.VirtHeap, (UINT32)pHeapNode, K2_VA32_MEMPAGE_BYTES);
        K2_ASSERT(!K2STAT_IS_ERROR(stat));
    }
    //
    // should be lots of tracking structures
    //
    K2OSKERN_Debug("VirtHeap freelist nodecount = %d\n", sgTrack.VirtHeapFreeNodeList.mNodeCount);
    return TRUE;
}

static UINT32
iVirtLocked_Alloc(
    UINT32 aPagesCount
)
{
    K2OSKERN_Debug("VirtHeap.Alloc freelist nodecount = %d\n", sgTrack.VirtHeapFreeNodeList.mNodeCount);
    if (sgTrack.VirtHeapFreeNodeList.mNodeCount < 4)
    {
        if (!sReplenishTracking())
        {
            return 0;
        }
    }
    
    return K2HEAP_Alloc(&sgTrack.VirtHeap, aPagesCount * K2_VA32_MEMPAGE_BYTES);
}

UINT32 
KernVirt_Alloc(
    UINT32 aPagesCount
)
{
    UINT32  result;
    BOOL    disp;

    disp = K2OSKERN_SeqLock(&sgTrack.VirtHeapSeqLock);

    result = iVirtLocked_Alloc(aPagesCount);

    K2OSKERN_SeqUnlock(&sgTrack.VirtHeapSeqLock, disp);

    return result;
}

void
iVirtLocked_Free(
    UINT32 aPagesAddr
)
{
    BOOL result;

    K2OSKERN_Debug("VirtHeap.Free freelist nodecount = %d\n", sgTrack.VirtHeapFreeNodeList.mNodeCount);
    if (sgTrack.VirtHeapFreeNodeList.mNodeCount < 2)
    {
        if (!sReplenishTracking())
            return;
    }
    
    result = K2HEAP_Free(&sgTrack.VirtHeap, aPagesAddr);
    K2_ASSERT(result);
}

void 
KernVirt_Free(
    UINT32 aPagesAddr
)
{
    BOOL disp;

    K2_ASSERT(0 == (aPagesAddr & K2_VA32_MEMPAGE_OFFSET_MASK));

    disp = K2OSKERN_SeqLock(&sgTrack.VirtHeapSeqLock);

    iVirtLocked_Free(aPagesAddr);
   
    K2OSKERN_SeqUnlock(&sgTrack.VirtHeapSeqLock, disp);
}

