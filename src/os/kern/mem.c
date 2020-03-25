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

static
void
sDumpHeapNode(
    K2HEAP_ANCHOR * apHeap,
    K2HEAP_NODE *   apNode
)
{
    K2OSKERN_Debug(
        "  %08X %08X %s\n",
        apNode->AddrTreeNode.mUserVal,
        apNode->SizeTreeNode.mUserVal,
        K2HEAP_NodeIsFree(apNode) ? "FREE" : "USED"
    );
}

static void sDumpAll(void)
{
    K2TREE_NODE *   pTreeNode;
    UINT32          flags;
    UINT32          pageCount;
    BOOL            intrDisp;

    K2OSKERN_Debug("\n\nPhysical Free Memory, sorted by size:\n");

    intrDisp = K2OSKERN_SeqIntrLock(&gData.PhysMemSeqLock);

    pTreeNode = K2TREE_FirstNode(&gData.PhysFreeTree);
    do {
        flags = pTreeNode->mUserVal;
        K2_ASSERT(flags & K2OSKERN_PHYSTRACK_FREE_FLAG);
        pageCount = flags >> K2OSKERN_PHYSTRACK_FREE_COUNT_SHL;
        K2OSKERN_Debug("  %08X - %8d Pages (%08X bytes) Flags %03X\n",
            K2OS_PHYSTRACK_TO_PHYS32((UINT32)pTreeNode),
            pageCount,
            pageCount * K2_VA32_MEMPAGE_BYTES,
            flags & K2OSKERN_PHYSTRACK_PROP_MASK);
        pTreeNode = K2TREE_NextNode(&gData.PhysFreeTree, pTreeNode);
    } while (pTreeNode != NULL);

    K2OSKERN_SeqIntrUnlock(&gData.PhysMemSeqLock, intrDisp);

    K2OSKERN_Debug("\nVirtual Memory, sorted by address:\n");

    intrDisp = K2OS_CritSecEnter(&gData.KernVirtHeapSec);
    K2_ASSERT(intrDisp);

    K2HEAP_Dump(&gData.KernVirtHeap, sDumpHeapNode);

    intrDisp = K2OS_CritSecLeave(&gData.KernVirtHeapSec);
    K2_ASSERT(intrDisp);
}

static UINT32 sRamHeapLock(K2OS_RAMHEAP *apRamHeap)
{
    BOOL ok;
    ok = K2OS_CritSecEnter(&gData.KernVirtHeapSec);
    K2_ASSERT(ok);
    return 0;
}

static void sRamHeapUnlock(K2OS_RAMHEAP *apRamHeap, UINT32 aDisp)
{
    BOOL ok;
    ok = K2OS_CritSecLeave(&gData.KernVirtHeapSec);
    K2_ASSERT(ok);
}

void KernMem_Start(void)
{
    //
    // should be threaded
    //
    K2_ASSERT(gData.mKernInitStage >= KernInitStage_Threaded);

    //
    // ramheap and tracking
    //
    K2OS_RAMHEAP_Init(&gData.RamHeap, sRamHeapLock, sRamHeapUnlock);
    K2LIST_Init(&gData.HeapTrackFreeList);
    gData.mpTrackPages = NULL;
    gData.mpNextTrackPage = NULL;

    sDumpAll();

    while (1);
}

UINT32 KernMem_CountPT(UINT32 aVirtAddr, UINT32 aPageCount)
{
    UINT32 workAddr;
    UINT32 workPages;
    UINT32 needPT;

    //
    // always need to allocate at least one pagetable page 
    //
    needPT = 1;

    //
    // workAddr = virtAddr rounded up to next pagetable boundary
    //
    workAddr = aVirtAddr + K2_VA32_PAGETABLE_MAP_BYTES;
    workAddr = (workAddr / K2_VA32_PAGETABLE_MAP_BYTES) * K2_VA32_PAGETABLE_MAP_BYTES;

    //
    // workpages = pages in area left to the rounded-up boundary
    //
    workPages = (workAddr - aVirtAddr) / K2_VA32_MEMPAGE_BYTES;
    if (aPageCount > workPages)
    {
        aPageCount -= workPages;
        do {
            needPT++;
            if (aPageCount <= K2_VA32_ENTRIES_PER_PAGETABLE)
                break;
            aPageCount -= K2_VA32_ENTRIES_PER_PAGETABLE;
        } while (1);
    }

    return needPT;
}

K2STAT KernMem_VirtAllocToThread(UINT32 aUseAddr, UINT32 aPageCount, BOOL aTopDown)
{
    return K2STAT_ERROR_NOT_IMPL;
}

void KernMem_VirtFreeFromThread(void)
{

}

K2STAT KernMem_PhysAllocToThread(UINT32 aPageCount, KernPhys_Disp aDisp, BOOL aForPageTables)
{
    return K2STAT_ERROR_NOT_IMPL;
}

void KernMem_PhysFreeFromThread(void)
{

}

K2STAT KernMem_CreateSegmentFromThread(K2OSKERN_OBJ_SEGMENT *apSrc, K2OSKERN_OBJ_SEGMENT *apDst)
{
    return K2STAT_ERROR_NOT_IMPL;
}
