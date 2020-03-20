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

#define INIT_FREE_PAGECOUNT     16

static UINT32 sRamHeapLock(K2OS_RAMHEAP *apRamHeap)
{
    BOOL ok;
    ok = K2OS_CritSecEnter(&gData.KernVirtSec);
    K2_ASSERT(ok);
    return 0;
}

static void sRamHeapUnlock(K2OS_RAMHEAP *apRamHeap, UINT32 aDisp)
{
    BOOL ok;
    ok = K2OS_CritSecLeave(&gData.KernVirtSec);
    K2_ASSERT(ok);
}

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

typedef enum _KernPhysBuf_Disposition KernPhysBuf_Disposition;
enum _KernPhysBuf_Disposition
{
    KernPhysBuf_Disp_Uncached = 0,
    KernPhysBuf_Disp_Cached_WriteThrough,
    KernPhysBuf_Disp_Cached,

    // goes last
    KernPhysBuf_Disp_Count
};

K2OSKERN_OBJ_SEGMENT * KernMem_PhysBuf_AllocSegment(UINT32 aPageCount, KernPhysBuf_Disposition aDisp, UINT32 aAlign)
{
    BOOL                        intrDisp;
    K2TREE_NODE *               pTreeNode;
    K2OSKERN_PHYSTRACK_FREE *   pTrackFree;
    UINT32                      pageCount;
    UINT32                      pageIndex;
    UINT32                      alignIndex;
    K2OSKERN_OBJ_SEGMENT *      pSeg;
    UINT32                      virtAddr;

    K2_ASSERT(aPageCount > 0);
    K2_ASSERT(aPageCount < K2_VA32_PAGEFRAMES_FOR_2G);
    K2_ASSERT(aDisp < KernPhysBuf_Disp_Count);
    if (aAlign == 0)
        aAlign = 1;
    else
    {
        K2_ASSERT(aAlign >= K2_VA32_MEMPAGE_BYTES);
        aAlign /= K2_VA32_MEMPAGE_BYTES;
    }
    K2_ASSERT((aAlign & K2_VA32_MEMPAGE_OFFSET_MASK) == 0);

    pSeg = K2OS_HeapAlloc(sizeof(K2OSKERN_OBJ_SEGMENT));
    if (pSeg == NULL)
        return NULL;

    do {
        virtAddr = 0;
        if (!K2OS_VirtPagesAlloc(&virtAddr, aPageCount, 0, K2OS_VIRTCOMMIT_NOACCESS))
            break;
        K2_ASSERT(virtAddr != 0);

        do {
            pTrackFree = NULL;

            intrDisp = K2OSKERN_SeqIntrLock(&gData.FreePhysSeqLock);
            do {
                pTreeNode = K2TREE_FindOrAfter(&gData.FreePhysTree, (aPageCount << K2OSKERN_PHYSTRACK_FREE_COUNT_SHL));
                if (pTreeNode == NULL)
                {
                    break;
                }

                do {
                    K2_ASSERT(pTreeNode->mUserVal & K2OSKERN_PHYSTRACK_FREE_FLAG);

                    pageCount = pTreeNode->mUserVal >> K2OSKERN_PHYSTRACK_FREE_COUNT_SHL;
                    K2_ASSERT(pageCount >= aPageCount);

                    switch (aDisp)
                    {
                    case KernPhysBuf_Disp_Uncached:
                        //
                        // if region cannot be uncached then set pagecount = 0
                        //
                        if ((pTreeNode->mUserVal & K2OSKERN_PHYSTRACK_PROPMASK_UNCACHEABLE) == 0)
                            pageCount = 0;
                        break;

                    case KernPhysBuf_Disp_Cached_WriteThrough:
                        if ((pTreeNode->mUserVal & K2OSKERN_PHYSTRACK_PROP_WT_CAP) == 0)
                            pageCount = 0;
                        break;

                    case KernPhysBuf_Disp_Cached:
                        if ((pTreeNode->mUserVal & K2OSKERN_PHYSTRACK_PROP_WB_CAP) == 0)
                            pageCount = 0;
                        break;

                    default:
                        K2_ASSERT(0);
                        break;
                    }

                    if (pageCount > 0)
                    {
                        pageIndex = K2OS_PHYSTRACK_TO_PHYS32((UINT32)pTreeNode) >> K2_VA32_MEMPAGE_BYTES_POW2;

                        if (aAlign > 1)
                        {
                            alignIndex = ((pageIndex + (aAlign - 1)) / aAlign) * aAlign;
                            if ((alignIndex + aPageCount) > (pageIndex + pageCount))
                            {
                                //
                                // allocation won't fit in this chunk when aligned
                                //
                                alignIndex = 0;
                            }
                        }
                        else
                            alignIndex = pageIndex;

                        if (alignIndex != 0)
                        {
                            //
                            // take the allocation from this tree node
                            //
                            break;
                        }
                    }

                    pTreeNode = K2TREE_NextNode(&gData.FreePhysTree, pTreeNode);

                } while (pTreeNode != NULL);

                if (pTreeNode == NULL)
                    break;

                //
                // pageIndex is index of page that free space starts at
                // pageCount is number of pages starting at pageIndex that are free
                // alignIndex is the index of the page that we start allocating from
                // aPageCount is the number of pages starting at alignIndex that we are allocating
                //
                K2OSKERN_Debug("pageIndex = %d, pageCount = %d, alignIndex = %d, aPageCount = %d\n",
                    pageIndex, pageCount, alignIndex, aPageCount);

                K2TREE_Remove(&gData.FreePhysTree, pTreeNode);

                if (alignIndex != pageIndex)
                {
                    //
                    // re-add tree node that comes before allocation, and align-up 
                    //
                    pTreeNode->mUserVal &= ~K2OSKERN_PHYSTRACK_FREE_COUNT_MASK;
                    pTreeNode->mUserVal |= (alignIndex - pageIndex) << K2OSKERN_PHYSTRACK_FREE_COUNT_SHL;

                    K2TREE_Insert(&gData.FreePhysTree, pTreeNode->mUserVal, pTreeNode);

                    pTreeNode += (alignIndex - pageIndex);
                    pageCount -= (alignIndex - pageIndex);
                    pageIndex = alignIndex;
                }

                pTrackFree = (K2OSKERN_PHYSTRACK_FREE *)pTreeNode;

                if (pageCount != aPageCount)
                {
                    //
                    // add the tree node that comes after the allocation
                    //
                    pTreeNode += aPageCount;

                    pTreeNode->mUserVal = (pTrackFree->mFlags & K2OSKERN_PHYSTRACK_PROP_MASK) |
                        ((pageCount - aPageCount) << K2OSKERN_PHYSTRACK_FREE_COUNT_SHL) |
                        K2OSKERN_PHYSTRACK_FREE_FLAG;

                    K2TREE_Insert(&gData.FreePhysTree, pTreeNode->mUserVal, pTreeNode);
                }

                pTrackFree->mFlags &= ~(K2OSKERN_PHYSTRACK_FREE_COUNT_MASK | K2OSKERN_PHYSTRACK_FREE_FLAG);
                pTrackFree->mFlags |= (aPageCount << K2OSKERN_PHYSTRACK_FREE_COUNT_SHL) | K2OSKERN_PHYSTRACK_CONTIG_ALLOC_FLAG;
            } while (0);

            K2OSKERN_SeqIntrUnlock(&gData.FreePhysSeqLock, intrDisp);

            if (pTrackFree == NULL)
            {
                K2OS_ThreadSetStatus(K2STAT_ERROR_OUT_OF_MEMORY);
                break;
            }

            //
            // now we map memory at pTrackFree starting at virtAddr for aPageCount, using
            // the properties selected in aDisp
            //

        } while (0);

        if (pTrackFree == NULL)
        {
            K2OS_VirtPagesFree(virtAddr);
        }

    } while (0);

    if (pTrackFree == NULL)
    {
        K2OS_HeapFree(pSeg);
        return NULL;
    }

    return pSeg;
}

K2STAT KernMem_PhysBuf_FreeSegment(UINT32 aVirtAddr)
{
    return K2STAT_ERROR_NOT_IMPL;
}


UINT32 KernMem_PhysToThread(UINT32 aPageCount, BOOL aTakeClean)
{
    return 0;
}

void KernMem_Start(void)
{
    K2TREE_NODE *               pTreeNode;
    K2TREE_NODE *               pLeftOver;
    K2OSKERN_PHYSTRACK_FREE *   pTrackFree;
    K2OSKERN_PHYSTRACK_PAGE *   pTrackPage;
    UINT32                      flags;
    UINT32                      pageCount;
    UINT32                      nodePages;
    UINT32                      takePages;
    UINT32                      physPageAddr;
    UINT32                      virtPageAddr;
//    UINT32                      pageTableVirtAddr;
    UINT32                      pageTableIndex;
    UINT32 *                    pPTPageCount;
//    UINT32                      pageTablePageTableIndex;
    BOOL                        ptIsPresent;
    UINT32                      pte;
    UINT32                      accessAttr;

    //
    // should be threaded
    //
    K2_ASSERT(gData.mKernInitStage >= KernInitStage_Threaded);

    K2OSKERN_SeqIntrInit(&gData.FreePhysSeqLock);
    K2OS_CritSecInit(&gData.KernVirtSec);
    K2OS_RAMHEAP_Init(&gData.RamHeap, sRamHeapLock, sRamHeapUnlock);

    K2OSKERN_Debug("\n\nPhysical Free Memory, sorted by size:\n");
    pTreeNode = K2TREE_FirstNode(&gData.FreePhysTree);
    do {
        pTrackFree = (K2OSKERN_PHYSTRACK_FREE *)pTreeNode;
        physPageAddr = K2OS_PHYSTRACK_TO_PHYS32((UINT32)pTrackFree);
        flags = pTrackFree->mFlags;
        K2_ASSERT(flags & K2OSKERN_PHYSTRACK_FREE_FLAG);
        K2OSKERN_Debug("  %08X - %8d Pages (%08X bytes) Flags %03X\n",
            physPageAddr,
            flags >> K2OSKERN_PHYSTRACK_FREE_COUNT_SHL,
            (flags >> K2OSKERN_PHYSTRACK_FREE_COUNT_SHL) * K2_VA32_MEMPAGE_BYTES,
            flags & K2OSKERN_PHYSTRACK_PROP_MASK);
        pTreeNode = K2TREE_NextNode(&gData.FreePhysTree, pTreeNode);
    } while (pTreeNode != NULL);

    K2OSKERN_Debug("\nVirtual Memory, sorted by address:\n");
    K2HEAP_Dump(&gData.KernVirtHeap, sDumpHeapNode);

    //
    // deposit some free pages into the dirty free list
    //
    pageCount = INIT_FREE_PAGECOUNT;
    do {
        pTreeNode = K2TREE_FindOrAfter(&gData.FreePhysTree, pageCount << K2OSKERN_PHYSTRACK_FREE_COUNT_SHL);
        
        K2_ASSERT(pTreeNode != NULL);
        
        K2TREE_Remove(&gData.FreePhysTree, pTreeNode);

        flags = pTreeNode->mUserVal & K2OSKERN_PHYSTRACK_PROP_MASK;

        nodePages = pTreeNode->mUserVal >> K2OSKERN_PHYSTRACK_FREE_COUNT_SHL;
        if (nodePages > pageCount)
        {
            pLeftOver = pTreeNode + pageCount;
            pLeftOver->mUserVal = flags | K2OSKERN_PHYSTRACK_FREE_FLAG | ((nodePages - pageCount) << K2OSKERN_PHYSTRACK_FREE_COUNT_SHL);
            K2TREE_Insert(&gData.FreePhysTree, pLeftOver->mUserVal, pLeftOver);
            takePages = pageCount;
        }
        else
        {
            takePages = nodePages;
        }
        pageCount -= takePages;

        //
        // add the pages just allocated to the free dirty list
        //
        pTrackPage = (K2OSKERN_PHYSTRACK_PAGE *)pTreeNode;
        do {
            pTrackPage->mFlags = (KernPageList_Free_Dirty << K2OSKERN_PHYSTRACK_PAGE_LIST_SHL) | flags;
            pTrackPage->mpOwnerObject = NULL;
            K2LIST_AddAtTail(&gData.PageList[KernPageList_Free_Dirty], &pTrackPage->ListLink);
            pTrackPage++;
        } while (--takePages);

    } while (pageCount > 0);

    //
    // now set up 'next free tracker'
    //
    virtPageAddr = K2HEAP_AllocAligned(&gData.KernVirtHeap, 1, K2_VA32_MEMPAGE_BYTES);
    K2_ASSERT(virtPageAddr != 0);
    K2OSKERN_Debug("Got nextFreeTrack at virt 0x%08X\n", virtPageAddr);

    //
    // ensure pagetable is mapped
    //
    pPTPageCount = (UINT32 *)K2OS_KVA_PTPAGECOUNT_BASE;
    pageTableIndex = virtPageAddr / K2_VA32_PAGETABLE_MAP_BYTES;
    KernArch_Translate(gpProc0, virtPageAddr, &ptIsPresent, &pte, &accessAttr);
    if (!ptIsPresent)
    {
        pTrackPage = K2_GET_CONTAINER(K2OSKERN_PHYSTRACK_PAGE, gData.PageList[KernPageList_Free_Dirty].mpHead, ListLink);
        K2LIST_Remove(&gData.PageList[KernPageList_Free_Dirty], &pTrackPage->ListLink);
        physPageAddr = K2OS_PHYSTRACK_TO_PHYS32((UINT32)pTrackPage);

        KernArch_MapPageTable(gpProc0, virtPageAddr, physPageAddr);

        K2_ASSERT(pPTPageCount[pageTableIndex] == 0);
    }
    else
    {
        //
        // pagetable for the page is already mapped.  assert that the page is not currently valid
        //
        K2_ASSERT((pte & K2OSKERN_PTE_PRESENT_BIT) == 0);
        K2_ASSERT(pPTPageCount[pageTableIndex] != 0);
    }

    pTrackPage = K2_GET_CONTAINER(K2OSKERN_PHYSTRACK_PAGE, gData.PageList[KernPageList_Free_Dirty].mpHead, ListLink);
    K2LIST_Remove(&gData.PageList[KernPageList_Free_Dirty], &pTrackPage->ListLink);
    physPageAddr = K2OS_PHYSTRACK_TO_PHYS32((UINT32)pTrackPage);

    KernMap_MakeOnePage(gpProc0->mVirtMapKVA, virtPageAddr, physPageAddr, K2OS_VIRTMAPTYPE_KERN_DATA);

    pPTPageCount[pageTableIndex]++;

    K2MEM_Zero((void *)virtPageAddr, K2_VA32_MEMPAGE_BYTES);

    //
    // this is a new segment
    //
#if 0
typedef struct _K2OSKERN_HEAPTRACKPAGE K2OSKERN_HEAPTRACKPAGE;

#define TRACK_BYTES     (K2_VA32_MEMPAGE_BYTES - (sizeof(K2OSKERN_OBJ_SEGMENT) + sizeof(K2OSKERN_HEAPTRACKPAGE *)))
#define TRACK_PER_PAGE  (TRACK_BYTES / sizeof(K2HEAP_NODE))

struct _K2OSKERN_HEAPTRACKPAGE
{
    K2OSKERN_OBJ_SEGMENT        SegObj;
    K2OSKERN_HEAPTRACKPAGE *    mpNext;
    UINT8                       mTrack[TRACK_BYTES];
};
K2_STATIC_ASSERT(sizeof(K2OSKERN_HEAPTRACKPAGE) == K2_VA32_MEMPAGE_BYTES);

K2OSKERN_HEAPTRACKPAGE * gpTrackPages = NULL;

K2OSKERN_HEAPTRACKPAGE * gpNextTrackPage;
#endif

}
