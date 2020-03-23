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

static K2HEAP_NODE * sKernVirtHeapAcquireNode(K2HEAP_ANCHOR *apHeap)
{
    K2HEAP_NODE *pRet;

    K2_ASSERT(apHeap == &gData.KernVirtHeap);
    if (gData.HeapTrackFreeList.mNodeCount < 5)
    {
        //
        // replenish from next track
        //
        K2_ASSERT(0);

        //
        // refill next track
        //
    }

    pRet = (K2HEAP_NODE *)gData.HeapTrackFreeList.mpHead;
    K2_ASSERT(pRet != NULL);

    K2LIST_Remove(&gData.HeapTrackFreeList, gData.HeapTrackFreeList.mpHead);

    return pRet;
}

static void sKernVirtHeapReleaseNode(K2HEAP_ANCHOR *apHeap, K2HEAP_NODE *apNode)
{
    K2_ASSERT(apHeap == &gData.KernVirtHeap);

    K2LIST_AddAtTail(&gData.HeapTrackFreeList, (K2LIST_LINK *)apNode);
}


K2OSKERN_OBJ_SEGMENT * KernMem_PhysBuf_AllocSegment(UINT32 aPageCount, KernPhys_Disp aDisp, UINT32 aAlign)
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
    K2_ASSERT(aDisp < KernPhys_Disp_Count);
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
        if (!K2OS_VirtPagesAlloc(&virtAddr, aPageCount, 0, 0))
            break;
        K2_ASSERT(virtAddr != 0);

        do {
            pTrackFree = NULL;

            intrDisp = K2OSKERN_SeqIntrLock(&gData.SysPageListsLock);
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
                    case KernPhys_Disp_Uncached:
                        //
                        // if region cannot be uncached then set pagecount = 0
                        //
                        if ((pTreeNode->mUserVal & K2OSKERN_PHYSTRACK_PROPMASK_UNCACHEABLE) == 0)
                            pageCount = 0;
                        break;

                    case KernPhys_Disp_Cached_WriteThrough:
                        if ((pTreeNode->mUserVal & K2OSKERN_PHYSTRACK_PROP_WT_CAP) == 0)
                            pageCount = 0;
                        break;

                    case KernPhys_Disp_Cached:
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

            K2OSKERN_SeqIntrUnlock(&gData.SysPageListsLock, intrDisp);

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

    intrDisp = K2OSKERN_SeqIntrLock(&gData.SysPageListsLock);

    pTreeNode = K2TREE_FirstNode(&gData.FreePhysTree);
    do {
        flags = pTreeNode->mUserVal;
        K2_ASSERT(flags & K2OSKERN_PHYSTRACK_FREE_FLAG);
        pageCount = flags >> K2OSKERN_PHYSTRACK_FREE_COUNT_SHL;
        K2OSKERN_Debug("  %08X - %8d Pages (%08X bytes) Flags %03X\n",
            K2OS_PHYSTRACK_TO_PHYS32((UINT32)pTreeNode),
            pageCount,
            pageCount * K2_VA32_MEMPAGE_BYTES,
            flags & K2OSKERN_PHYSTRACK_PROP_MASK);
        pTreeNode = K2TREE_NextNode(&gData.FreePhysTree, pTreeNode);
    } while (pTreeNode != NULL);

    K2OSKERN_SeqIntrUnlock(&gData.SysPageListsLock, intrDisp);

    K2OSKERN_Debug("\nVirtual Memory, sorted by address:\n");

    intrDisp = K2OS_CritSecEnter(&gData.KernVirtSec);
    K2_ASSERT(intrDisp);

    K2HEAP_Dump(&gData.KernVirtHeap, sDumpHeapNode);

    intrDisp = K2OS_CritSecLeave(&gData.KernVirtSec);
    K2_ASSERT(intrDisp);
}

void KernMem_Start(void)
{
    K2OSKERN_OBJ_SEGMENT        tempSeg;
    K2OSKERN_HEAPTRACKPAGE *    pHeapTrack;
    K2STAT                      stat;
    K2OSKERN_OBJ_THREAD *       pCurThread;

    //
    // should be threaded
    //
    K2_ASSERT(gData.mKernInitStage >= KernInitStage_Threaded);

    K2LIST_Init(&gData.HeapTrackFreeList);
    gData.mpTrackPages = NULL;
    gData.mpNextTrackPage = NULL;
    K2OSKERN_SeqIntrInit(&gData.SysPageListsLock);
    K2OS_CritSecInit(&gData.KernVirtSec);
    K2OS_RAMHEAP_Init(&gData.RamHeap, sRamHeapLock, sRamHeapUnlock);
    K2OS_CritSecInit(&gData.CleanerSec);

    sDumpAll();

    pCurThread = K2OSKERN_CURRENT_THREAD;

    //
    // create the first heap tracking segment
    //

    stat = KernMem_VirtAllocToThread(0, 1, FALSE);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));
    K2_ASSERT(pCurThread->mWorkVirt != 0);

    stat = KernMem_PhysAllocToThread(1, KernPhys_Disp_Cached, FALSE);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));
    K2_ASSERT(pCurThread->WorkPages_Dirty.mNodeCount + pCurThread->WorkPages_Clean.mNodeCount > 0);

    K2MEM_Zero(&tempSeg, sizeof(tempSeg));
    tempSeg.Hdr.mObjType = K2OS_Obj_Segment;
    tempSeg.Hdr.mRefCount = 1;
    K2LIST_Init(&tempSeg.Hdr.WaitingThreadsPrioList);
    tempSeg.mPagesBytes = K2_VA32_MEMPAGE_BYTES;
    tempSeg.mSegAndMemPageAttr = K2OS_MAPTYPE_KERN_DATA | K2OS_SEG_ATTR_TYPE_HEAP_TRACK;
    tempSeg.SegTreeNode.mUserVal = pCurThread->mWorkVirt;
    
    pHeapTrack = (K2OSKERN_HEAPTRACKPAGE *)pCurThread->mWorkVirt; // !not mapped yet!

    stat = KernMem_CreateSegmentFromThread(&tempSeg, &pHeapTrack->SegObj);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));

    pHeapTrack->mpNext = NULL;

    gData.mpNextTrackPage = pHeapTrack;

    //
    // switch over the virtual heap off the init static list of tracks to the
    // new dynamic one
    //
    gData.KernVirtHeap.mfAcquireNode = sKernVirtHeapAcquireNode;
    gData.KernVirtHeap.mfReleaseNode = sKernVirtHeapReleaseNode;

    //
    // tracking list starts empty and first alloc will bump it
    //
}

K2STAT KernMem_VirtAllocToThread(UINT32 aUseAddr, UINT32 aPageCount, BOOL aTopDown)
{
    BOOL                    ok;
    K2STAT                  stat;
    K2OSKERN_OBJ_THREAD *   pCurThread;

    if (aUseAddr != 0)
    {
        K2_ASSERT((aUseAddr & K2_VA32_MEMPAGE_OFFSET_MASK) == 0);
    }

    ok = K2OS_CritSecEnter(&gData.KernVirtSec);
    K2_ASSERT(ok);

    if (aUseAddr != 0)
    {
        stat = K2HEAP_AllocAt(&gData.KernVirtHeap, aUseAddr, aPageCount * K2_VA32_MEMPAGE_BYTES);
    }
    else
    {
        if (aTopDown)
        {
            aUseAddr = K2HEAP_AllocAlignedHighest(&gData.KernVirtHeap, aPageCount * K2_VA32_MEMPAGE_BYTES, K2_VA32_MEMPAGE_BYTES);
        }
        else
        {
            aUseAddr = K2HEAP_AllocAligned(&gData.KernVirtHeap, aPageCount * K2_VA32_MEMPAGE_BYTES, K2_VA32_MEMPAGE_BYTES);
        }
        if (aUseAddr == 0)
            stat = K2STAT_ERROR_OUT_OF_MEMORY;
    }

    if (!K2STAT_IS_ERROR(stat))
    {
        pCurThread = K2OSKERN_CURRENT_THREAD;
        pCurThread->mWorkVirt = aUseAddr;
    }

    ok = K2OS_CritSecLeave(&gData.KernVirtSec);
    K2_ASSERT(ok);

    return stat;
}

void KernMem_VirtFreeFromThread(void)
{
    BOOL                    ok;
    UINT32                  temp;
    K2OSKERN_OBJ_THREAD *   pCurThread;

    pCurThread = K2OSKERN_CURRENT_THREAD;
    K2_ASSERT(pCurThread->mWorkVirt != 0);

    ok = K2OS_CritSecEnter(&gData.KernVirtSec);
    K2_ASSERT(ok);

    temp = pCurThread->mWorkVirt;
    pCurThread->mWorkVirt = 0;

    ok = K2HEAP_Free(&gData.KernVirtHeap, temp);
    K2_ASSERT(ok);

    ok = K2OS_CritSecLeave(&gData.KernVirtSec);
    K2_ASSERT(ok);
}

#define PHYSPAGES_CHUNK 16

K2STAT KernMem_PhysAllocToThread(UINT32 aPageCount, KernPhys_Disp aDisp, BOOL aForPageTables)
{
    BOOL                        intrDisp;
    K2OSKERN_OBJ_THREAD *       pCurThread;
    K2LIST_LINK *               pListLink;
    K2OSKERN_PHYSTRACK_PAGE *   pPhysPage;
    K2LIST_ANCHOR *             pList;
    UINT32                      chunkLeft;
    UINT32                      tookDirty;
    UINT32                      tookClean;
    UINT32                      flags;
    K2TREE_NODE *               pTreeNode;
    K2TREE_NODE *               pNextNode;
    K2TREE_NODE *               pLeftOver;
    UINT32                      nodeCount;
    UINT32                      takePages;

    K2_ASSERT(aPageCount > 0);
    K2_ASSERT(aDisp < KernPhys_Disp_Count);

    pCurThread = K2OSKERN_CURRENT_THREAD;

    //
    // first try to pull pages off clean page list
    //
    tookClean = 0;
    do {
        intrDisp = K2OSKERN_SeqIntrLock(&gData.SysPageListsLock);

        pList = &gData.PageList[KernPageList_Free_Clean];

        if (pList->mNodeCount == 0)
        {
            //
            // system clean pages list is empty
            //
            break;
        }

        chunkLeft = PHYSPAGES_CHUNK;

        pListLink = pList->mpHead;
        do {
            pPhysPage = K2_GET_CONTAINER(K2OSKERN_PHYSTRACK_PAGE, pListLink, ListLink);
            pListLink = pListLink->mpNext;

            switch (aDisp)
            {
            case KernPhys_Disp_Uncached:
                //
                // if region cannot be uncached then set pPhysPage = NULL;
                //
                if ((pPhysPage->mFlags & K2OSKERN_PHYSTRACK_PROPMASK_UNCACHEABLE) == 0)
                    pPhysPage = NULL;
                break;

            case KernPhys_Disp_Cached_WriteThrough:
                if ((pPhysPage->mFlags & K2OSKERN_PHYSTRACK_PROP_WT_CAP) == 0)
                    pPhysPage = NULL;
                break;

            case KernPhys_Disp_Cached:
                if ((pPhysPage->mFlags & K2OSKERN_PHYSTRACK_PROP_WB_CAP) == 0)
                    pPhysPage = NULL;
                break;

            default:
                K2_ASSERT(0);
                break;
            }

            if (pPhysPage != NULL)
            {
                K2LIST_Remove(pList, &pPhysPage->ListLink);
                pPhysPage->mFlags &= ~(K2OSKERN_PHYSTRACK_PAGE_LIST_MASK | K2OSKERN_PHYSTRACK_FREE_FLAG);

                if (aForPageTables)
                {
                    pPhysPage->mFlags |= (KernPageList_Thread_PtClean << K2OSKERN_PHYSTRACK_PAGE_LIST_SHL);
                    K2LIST_AddAtTail(&pCurThread->PtPages_Clean, &pPhysPage->ListLink);
                }
                else
                {
                    pPhysPage->mFlags |= (KernPageList_Thread_Clean << K2OSKERN_PHYSTRACK_PAGE_LIST_SHL);
                    K2LIST_AddAtTail(&pCurThread->WorkPages_Clean, &pPhysPage->ListLink);
                }

                tookClean++;

                if (--aPageCount == 0)
                    break;

                if (--chunkLeft == 0)
                {
                    K2OSKERN_SeqIntrUnlock(&gData.SysPageListsLock, intrDisp);
                    //
                    // give interrupts a chance to fire here
                    //
                    intrDisp = K2OSKERN_SeqIntrLock(&gData.SysPageListsLock);
                    break;
                }
            }

        } while (pListLink != NULL);

        //
        // if clean pagelist is empty then stop now
        //
        if (pListLink == NULL)
            break;

        //
        // we allocated a chunk of clean pages while locked, and unlocked to check for interrupts
        // before we continue to allocate pages, unless pagecount is zero and we are done
        //

    } while (aPageCount > 0);

    if (aPageCount == 0)
    {
        K2OSKERN_SeqIntrUnlock(&gData.SysPageListsLock, intrDisp);
        return K2STAT_NO_ERROR;
    }

    //
    // next try to pull pages off dirty page list
    //
    do {
        pList = &gData.PageList[KernPageList_Free_Dirty];

        if (pList->mNodeCount == 0)
        {
            //
            // system dirty pages list is empty
            //
            break;
        }

        chunkLeft = PHYSPAGES_CHUNK;

        pListLink = pList->mpHead;
        do {
            pPhysPage = K2_GET_CONTAINER(K2OSKERN_PHYSTRACK_PAGE, pListLink, ListLink);
            pListLink = pListLink->mpNext;

            switch (aDisp)
            {
            case KernPhys_Disp_Uncached:
                //
                // if region cannot be uncached then set pPhysPage = NULL;
                //
                if ((pPhysPage->mFlags & K2OSKERN_PHYSTRACK_PROPMASK_UNCACHEABLE) == 0)
                    pPhysPage = NULL;
                break;

            case KernPhys_Disp_Cached_WriteThrough:
                if ((pPhysPage->mFlags & K2OSKERN_PHYSTRACK_PROP_WT_CAP) == 0)
                    pPhysPage = NULL;
                break;

            case KernPhys_Disp_Cached:
                if ((pPhysPage->mFlags & K2OSKERN_PHYSTRACK_PROP_WB_CAP) == 0)
                    pPhysPage = NULL;
                break;

            default:
                K2_ASSERT(0);
                break;
            }

            if (pPhysPage != NULL)
            {
                K2LIST_Remove(pList, &pPhysPage->ListLink);
                pPhysPage->mFlags &= ~(K2OSKERN_PHYSTRACK_PAGE_LIST_MASK | K2OSKERN_PHYSTRACK_FREE_FLAG);

                if (aForPageTables)
                {
                    pPhysPage->mFlags |= (KernPageList_Thread_PtDirty << K2OSKERN_PHYSTRACK_PAGE_LIST_SHL);
                    K2LIST_AddAtTail(&pCurThread->PtPages_Dirty, &pPhysPage->ListLink);
                }
                else
                {
                    pPhysPage->mFlags |= (KernPageList_Thread_Dirty << K2OSKERN_PHYSTRACK_PAGE_LIST_SHL);
                    K2LIST_AddAtTail(&pCurThread->WorkPages_Dirty, &pPhysPage->ListLink);
                }

                tookDirty++;

                if (--aPageCount == 0)
                    break;

                if (--chunkLeft == 0)
                {
                    K2OSKERN_SeqIntrUnlock(&gData.SysPageListsLock, intrDisp);
                    //
                    // give interrupts a chance to fire here
                    //
                    intrDisp = K2OSKERN_SeqIntrLock(&gData.SysPageListsLock);
                    break;
                }
            }

        } while (pListLink != NULL);

        //
        // if dirty pagelist is empty then stop now
        //
        if (pListLink == NULL)
            break;

        //
        // we allocated a chunk of dirty pages while locked, and unlocked to check for interrupts
        // before we continue to allocate pages, unless pagecount is zero and we are done
        //

    } while (aPageCount > 0);

    if (aPageCount == 0)
    {
        K2OSKERN_SeqIntrUnlock(&gData.SysPageListsLock, intrDisp);
        return K2STAT_NO_ERROR;
    }

    //
    // there were not enough clean and/or dirty free pages available in the system, so we 
    // allocate them directly to this thread from free space
    //

    //
    // first we have to see if the request can be satisfied before we start taking
    // hunks of physical memory
    //
    pTreeNode = K2TREE_FirstNode(&gData.FreePhysTree);
    chunkLeft = aPageCount;
    do {
        nodeCount = pTreeNode->mUserVal >> K2OSKERN_PHYSTRACK_FREE_COUNT_SHL;
        switch (aDisp)
        {
        case KernPhys_Disp_Uncached:
            //
            // if region cannot be uncached then set pPhysPage = NULL;
            //
            if ((pTreeNode->mUserVal & K2OSKERN_PHYSTRACK_PROPMASK_UNCACHEABLE) == 0)
                nodeCount = 0;
            break;

        case KernPhys_Disp_Cached_WriteThrough:
            if ((pTreeNode->mUserVal & K2OSKERN_PHYSTRACK_PROP_WT_CAP) == 0)
                nodeCount = 0;
            break;

        case KernPhys_Disp_Cached:
            if ((pTreeNode->mUserVal & K2OSKERN_PHYSTRACK_PROP_WB_CAP) == 0)
                nodeCount = 0;
            break;

        default:
            K2_ASSERT(0);
            break;
        }

        if (nodeCount >= chunkLeft)
        {
            chunkLeft = 0;
            break;
        }

        chunkLeft -= nodeCount;
        pTreeNode = K2TREE_NextNode(&gData.FreePhysTree, pTreeNode);

    } while (pTreeNode != NULL);

    if (chunkLeft > 0)
    {
        //
        // request cannot be satisfied because we rammed through all physical memory
        // and there isn't enough that matches the required disposition
        //
        K2_ASSERT(aPageCount > 0);

        //
        // transfer any pages we took back onto the free lists
        //
        if (tookDirty)
        {
            pList = &gData.PageList[KernPageList_Free_Dirty];
            pListLink = pCurThread->WorkPages_Dirty.mpTail;
            do {
                pPhysPage = K2_GET_CONTAINER(K2OSKERN_PHYSTRACK_PAGE, pListLink, ListLink);
                pListLink = pListLink->mpPrev;

                if (aForPageTables)
                {
                    K2LIST_Remove(&pCurThread->PtPages_Dirty, &pPhysPage->ListLink);
                }
                else
                {
                    K2LIST_Remove(&pCurThread->WorkPages_Dirty, &pPhysPage->ListLink);
                }
                pPhysPage->mFlags &= ~K2OSKERN_PHYSTRACK_PAGE_LIST_MASK;
                pPhysPage->mFlags |= ((KernPageList_Free_Dirty << K2OSKERN_PHYSTRACK_PAGE_LIST_SHL) | K2OSKERN_PHYSTRACK_FREE_FLAG);
                K2LIST_AddAtTail(pList, &pPhysPage->ListLink);
            } while (--tookDirty > 0);
        }

        if (tookClean)
        {
            pList = &gData.PageList[KernPageList_Free_Clean];
            pListLink = pCurThread->WorkPages_Clean.mpTail;
            do {
                pPhysPage = K2_GET_CONTAINER(K2OSKERN_PHYSTRACK_PAGE, pListLink, ListLink);
                pListLink = pListLink->mpPrev;

                if (aForPageTables)
                {
                    K2LIST_Remove(&pCurThread->PtPages_Clean, &pPhysPage->ListLink);
                }
                else
                {
                    K2LIST_Remove(&pCurThread->WorkPages_Clean, &pPhysPage->ListLink);
                }
                pPhysPage->mFlags &= ~K2OSKERN_PHYSTRACK_PAGE_LIST_MASK;
                pPhysPage->mFlags |= ((KernPageList_Free_Clean << K2OSKERN_PHYSTRACK_PAGE_LIST_SHL) | K2OSKERN_PHYSTRACK_FREE_FLAG);
                K2LIST_AddAtTail(pList, &pPhysPage->ListLink);
            } while (--tookClean > 0);
        }

        K2OSKERN_SeqIntrUnlock(&gData.SysPageListsLock, intrDisp);

        return K2STAT_ERROR_OUT_OF_MEMORY;
    }

    //
    // if we get here, we know the request can be satisfied
    //
    pTreeNode = K2TREE_FirstNode(&gData.FreePhysTree);
    chunkLeft = aPageCount;
    do {
        pNextNode = K2TREE_NextNode(&gData.FreePhysTree, pTreeNode);

        nodeCount = pTreeNode->mUserVal >> K2OSKERN_PHYSTRACK_FREE_COUNT_SHL;
        switch (aDisp)
        {
        case KernPhys_Disp_Uncached:
            //
            // if region cannot be uncached then set pPhysPage = NULL;
            //
            if ((pTreeNode->mUserVal & K2OSKERN_PHYSTRACK_PROPMASK_UNCACHEABLE) == 0)
                nodeCount = 0;
            break;

        case KernPhys_Disp_Cached_WriteThrough:
            if ((pTreeNode->mUserVal & K2OSKERN_PHYSTRACK_PROP_WT_CAP) == 0)
                nodeCount = 0;
            break;

        case KernPhys_Disp_Cached:
            if ((pTreeNode->mUserVal & K2OSKERN_PHYSTRACK_PROP_WB_CAP) == 0)
                nodeCount = 0;
            break;

        default:
            K2_ASSERT(0);
            break;
        }

        if (nodeCount > 0)
        {
            //
            // take this chunk and convert at least some of it into dirty pages for this thread
            //
            K2TREE_Remove(&gData.FreePhysTree, pTreeNode);

            flags = (pTreeNode->mUserVal & K2OSKERN_PHYSTRACK_PROP_MASK);

            if (nodeCount > aPageCount)
            {
                pLeftOver = pTreeNode + aPageCount;

                pLeftOver->mUserVal = 
                    flags | 
                    K2OSKERN_PHYSTRACK_FREE_FLAG | 
                    ((nodeCount - aPageCount) << K2OSKERN_PHYSTRACK_FREE_COUNT_SHL);
                K2TREE_Insert(&gData.FreePhysTree, pLeftOver->mUserVal, pLeftOver);
                takePages = nodeCount;
            }
            else
            {
                takePages = aPageCount;
            }

            pPhysPage = (K2OSKERN_PHYSTRACK_PAGE *)pTreeNode;

            if (aForPageTables)
            {
                do {
                    pPhysPage->mFlags = (KernPageList_Thread_PtDirty << K2OSKERN_PHYSTRACK_PAGE_LIST_SHL) | flags;
                    pPhysPage->mpOwnerObject = NULL;
                    K2LIST_AddAtTail(&pCurThread->PtPages_Dirty, &pPhysPage->ListLink);
                    pPhysPage++;
                    --aPageCount;
                } while (--takePages);
            }
            else
            {
                do {
                    pPhysPage->mFlags = (KernPageList_Thread_Dirty << K2OSKERN_PHYSTRACK_PAGE_LIST_SHL) | flags;
                    pPhysPage->mpOwnerObject = NULL;
                    K2LIST_AddAtTail(&pCurThread->WorkPages_Dirty, &pPhysPage->ListLink);
                    pPhysPage++;
                    --aPageCount;
                } while (--takePages);
            }

            if (aPageCount == 0)
                break;
        }

        pTreeNode = pNextNode;

    } while (pTreeNode != NULL);

    K2OSKERN_SeqIntrUnlock(&gData.SysPageListsLock, intrDisp);

//    sDumpAll();

    return K2STAT_NO_ERROR;
}

void KernMem_PhysFreeFromThread(void)
{
    K2_ASSERT(0);
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

static void sCleanPage(UINT32 aPhysPage)
{
    BOOL ok;

    K2OSKERN_Debug("Clean Physical Page 0x%08X\n", aPhysPage);

    //
    // using a section gives thread priority a chance to work
    //
    ok = K2OS_CritSecEnter(&gData.CleanerSec);
    K2_ASSERT(ok);

    K2_ASSERT(K2OSKERN_GetIntr() != FALSE);

    //
    // interrupts off so thread does not migrate to another core or get stopped
    // while it is clearing the page
    //
    K2OSKERN_SetIntr(FALSE);

    KernMap_MakeOnePage(K2OS_KVA_KERNVAMAP_BASE, K2OS_KVA_CLEANERPAGE_BASE, aPhysPage, K2OS_MAPTYPE_KERN_WRITETHRU_CACHED);

    K2MEM_Zero((void *)K2OS_KVA_CLEANERPAGE_BASE, K2_VA32_MEMPAGE_BYTES);

    KernMap_BreakOnePage(K2OS_KVA_KERNVAMAP_BASE, K2OS_KVA_CLEANERPAGE_BASE);

    KernArch_InvalidateTlbPageOnThisCore(K2OS_KVA_CLEANERPAGE_BASE);

    K2OSKERN_SetIntr(TRUE);

    K2OS_CritSecLeave(&gData.CleanerSec);
}

K2STAT KernMem_CreateSegmentFromThread(K2OSKERN_OBJ_SEGMENT *apSegSrc, K2OSKERN_OBJ_SEGMENT *apSegTarget)
{
    UINT32                  pageCount;
    UINT32                  virtAddr;
    UINT32                  workAddr;
    UINT32                  workPages;
    UINT32                  needPT;
    K2OSKERN_OBJ_THREAD *   pCurThread;
    BOOL                    ptPresent;
    UINT32                  pte;
    UINT32                  accessAttr;
    K2OSKERN_PHYSTRACK_PAGE *   pPhysPage;
    BOOL                        intrDisp;
    K2STAT                      stat;
    UINT32                  workPhys;

    K2_ASSERT(apSegSrc != NULL);
    K2_ASSERT(apSegSrc->Hdr.mObjType == K2OS_Obj_Segment);
    K2_ASSERT(apSegSrc->Hdr.mRefCount > 0);
    K2_ASSERT(apSegSrc->mPagesBytes > 0);
    K2_ASSERT((apSegSrc->mPagesBytes & K2_VA32_MEMPAGE_OFFSET_MASK) == 0);
    pageCount = apSegSrc->mPagesBytes / K2_VA32_MEMPAGE_BYTES;
    K2_ASSERT((apSegSrc->mSegAndMemPageAttr & K2OS_SEG_ATTR_TYPE_MASK) < K2OS_SEG_ATTR_TYPE_COUNT);
    K2_ASSERT(apSegSrc->SegTreeNode.mUserVal >= K2OS_KVA_KERN_BASE);
    K2_ASSERT((apSegSrc->SegTreeNode.mUserVal & K2_VA32_MEMPAGE_OFFSET_MASK) == 0);
    virtAddr = apSegSrc->SegTreeNode.mUserVal;

    //
    // get pagetable pages we may need when mapping the segment
    //
    needPT = KernMem_CountPT(virtAddr, pageCount);

    K2OSKERN_Debug("MAX of %d pagetables needed to map %d pages at 0x%08X\n", needPT, pageCount, virtAddr);

    //
    // pull pages that can be used as pagetables to thread
    //
    stat = KernMem_PhysAllocToThread(needPT, KernPhys_Disp_Cached_WriteThrough, TRUE);
    if (K2STAT_IS_ERROR(stat))
        return stat;

    pCurThread = K2OSKERN_CURRENT_THREAD;

    //
    // clean the pagetable pages - these need to be clean before we start using them
    // otherwise there will be garbage in them mapping stuff randomly before they are
    // initialized
    //
    while (pCurThread->PtPages_Dirty.mNodeCount > 0)
    {
        pPhysPage = K2_GET_CONTAINER(K2OSKERN_PHYSTRACK_PAGE, pCurThread->PtPages_Dirty.mpHead, ListLink);
        K2_ASSERT((pPhysPage->mFlags & K2OSKERN_PHYSTRACK_FREE_FLAG) == 0);
        sCleanPage(K2OS_PHYSTRACK_TO_PHYS32((UINT32)pPhysPage));
        K2LIST_Remove(&pCurThread->PtPages_Dirty, &pPhysPage->ListLink);
        pPhysPage->mFlags &= ~K2OSKERN_PHYSTRACK_PAGE_LIST_MASK;
        pPhysPage->mFlags |= (KernPageList_Thread_PtClean << K2OSKERN_PHYSTRACK_PAGE_LIST_SHL);
        K2LIST_AddAtTail(&pCurThread->PtPages_Clean, &pPhysPage->ListLink);
    }

    K2_ASSERT(pCurThread->PtPages_Dirty.mNodeCount == 0);
    K2_ASSERT(pCurThread->PtPages_Clean.mNodeCount == needPT);

    //
    // ready to go (map).  we have the physical pages for PT,
    // the virt space, and the physical pages to map to that space (if there are any)
    //
    workAddr = virtAddr;
    workPages = needPT;
    do {
        ptPresent = FALSE;
        pte = 0;
        accessAttr = 0;
        KernArch_Translate(pCurThread->mpProc, workAddr, &ptPresent, &pte, &accessAttr);
        if (!ptPresent)
        {
            //
            // need to create the pagetable for 'workAddr'
            //
            pPhysPage = K2_GET_CONTAINER(K2OSKERN_PHYSTRACK_PAGE, pCurThread->PtPages_Clean.mpHead, ListLink);

            intrDisp = K2OSKERN_SeqIntrLock(&gData.SysPageListsLock);

            K2LIST_Remove(&pCurThread->PtPages_Clean, &pPhysPage->ListLink);

            KernArch_MapPageTable(pCurThread->mpProc, workAddr, K2OS_PHYSTRACK_TO_PHYS32((UINT32)pPhysPage));

            pPhysPage->mFlags &= ~K2OSKERN_PHYSTRACK_PAGE_LIST_MASK;
            pPhysPage->mFlags |= (KernPageList_Paging << K2OSKERN_PHYSTRACK_PAGE_LIST_SHL);

            K2LIST_AddAtTail(&gData.PageList[KernPageList_Paging], &pPhysPage->ListLink);

            K2OSKERN_SeqIntrUnlock(&gData.SysPageListsLock, intrDisp);
        }        
        workAddr += K2_VA32_PAGETABLE_MAP_BYTES;
    } while (--workPages);

    //
    // free any unused pagetable pages back to the free list
    //
    if (pCurThread->PtPages_Clean.mNodeCount > 0)
    {
        intrDisp = K2OSKERN_SeqIntrLock(&gData.SysPageListsLock);

        do {
            pPhysPage = K2_GET_CONTAINER(K2OSKERN_PHYSTRACK_PAGE, pCurThread->PtPages_Clean.mpHead, ListLink);

            K2LIST_Remove(&pCurThread->PtPages_Clean, &pPhysPage->ListLink);

            pPhysPage->mFlags &= ~K2OSKERN_PHYSTRACK_PAGE_LIST_MASK;
            pPhysPage->mFlags |= (KernPageList_Free_Clean << K2OSKERN_PHYSTRACK_PAGE_LIST_SHL);

            K2LIST_AddAtTail(&gData.PageList[KernPageList_Free_Clean], &pPhysPage->ListLink);

        } while (pCurThread->PtPages_Clean.mNodeCount > 0);

        K2OSKERN_SeqIntrUnlock(&gData.SysPageListsLock, intrDisp);
    }

    //
    //
    if ((pCurThread->WorkPages_Dirty.mNodeCount == 0) ||
        (pCurThread->WorkPages_Clean.mNodeCount == 0))
    {
        //
        // no memory to map.  access attribute should be no access
        //
        K2_ASSERT((apSegSrc->mSegAndMemPageAttr & K2OS_MEMPAGE_ATTR_MASK) == 0);
        K2_ASSERT(apSegTarget == NULL);
    }
    else
    {
        workAddr = virtAddr;

        //
        // number of pages available in the thread must be equal to the size of the segment
        //
        K2_ASSERT(pageCount <= (pCurThread->WorkPages_Dirty.mNodeCount + pCurThread->WorkPages_Clean.mNodeCount));

        //
        // pages that need cleaning must either be cleaned before they are mapped if the map type
        // is not writeable.  otherwise they can be cleaned when they are mapped in
        //
        if (apSegSrc->mSegAndMemPageAttr & K2OS_MEMPAGE_ATTR_WRITEABLE)
        {
            //
            // can clean pages as we map them at their target location
            //


        }
        else
        {
            //
            // some pages are dirty.  we must clean them before we map them.
            //
            do {
                if (pCurThread->WorkPages_Dirty.mNodeCount > 0)
                {
                    pPhysPage = K2_GET_CONTAINER(K2OSKERN_PHYSTRACK_PAGE, pCurThread->WorkPages_Dirty.mpHead, ListLink);
                    workPhys = K2OS_PHYSTRACK_TO_PHYS32((UINT32)pPhysPage);
                    sCleanPage(workPhys);
                    intrDisp = K2OSKERN_SeqIntrLock(&gData.SysPageListsLock);
                    K2LIST_Remove(&pCurThread->WorkPages_Dirty, &pPhysPage->ListLink);
                }
                else
                {
                    pPhysPage = K2_GET_CONTAINER(K2OSKERN_PHYSTRACK_PAGE, pCurThread->WorkPages_Clean.mpHead, ListLink);
                    workPhys = K2OS_PHYSTRACK_TO_PHYS32((UINT32)pPhysPage);
                    intrDisp = K2OSKERN_SeqIntrLock(&gData.SysPageListsLock);
                    K2LIST_Remove(&pCurThread->WorkPages_Clean, &pPhysPage->ListLink);
                }

                pPhysPage->mFlags &= ~K2OSKERN_PHYSTRACK_PAGE_LIST_MASK;
                pPhysPage->mFlags |= (KernPageList_Free_Clean << K2OSKERN_PHYSTRACK_PAGE_LIST_SHL);
                K2LIST_AddAtTail(&gData.PageList[KernPageList_General], &pPhysPage->ListLink);

                K2OSKERN_SeqIntrUnlock(&gData.SysPageListsLock, intrDisp);

                KernMap_MakeOnePage(pCurThread->mpProc->mVirtMapKVA, workAddr, workPhys, apSegSrc->mSegAndMemPageAttr & K2OS_MEMPAGE_ATTR_MASK);

                workAddr += K2_VA32_MEMPAGE_BYTES;
            } while (--pageCount);
        }
    }

    //
    // now latch the segment object into the system
    //

    return K2STAT_ERROR_NOT_IMPL;
}

K2STAT KernMem_DestroySegmentToThread(K2OSKERN_OBJ_SEGMENT *apSeg)
{
    K2_ASSERT(0);
    return K2STAT_ERROR_NOT_IMPL;
}

