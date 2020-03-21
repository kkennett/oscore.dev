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
        if (!K2OS_VirtPagesAlloc(&virtAddr, aPageCount, 0, K2OS_PAGEATTR_NOACCESS))
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

                    case KernPhys_Disp_Cached_WriteCombine:
                        if ((pTreeNode->mUserVal & (K2OSKERN_PHYSTRACK_PROP_WB_CAP | K2OSKERN_PHYSTRACK_PROP_WC_CAP)) != (K2OSKERN_PHYSTRACK_PROP_WB_CAP | K2OSKERN_PHYSTRACK_PROP_WC_CAP))
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
    // set up next free tracker
    //
    virtPageAddr = K2HEAP_AllocAligned(&gData.KernVirtHeap, 1, K2_VA32_MEMPAGE_BYTES);
    K2_ASSERT(virtPageAddr != 0);



    pTrackPage = K2_GET_CONTAINER(K2OSKERN_PHYSTRACK_PAGE, gData.PageList[KernPageList_Free_Dirty].mpHead, ListLink);
    K2LIST_Remove(&gData.PageList[KernPageList_Free_Dirty], &pTrackPage->ListLink);

    //
    // this is a new segment
    //
    K2MEM_Zero(&tempSeg, sizeof(tempSeg));

    pCurThread = K2OSKERN_CURRENT_THREAD;

    K2MEM_Zero(&tempSeg, sizeof(K2OSKERN_OBJ_SEGMENT));
    tempSeg.Hdr.mObjType = K2OS_Obj_Segment;
    tempSeg.Hdr.mRefCount = 1;
    K2LIST_Init(&tempSeg.Hdr.WaitingThreadsPrioList);
    tempSeg.mPagesBytes = K2_VA32_MEMPAGE_BYTES;
    tempSeg.mAccessAttr = K2OS_PAGEATTR_READWRITE;
    tempSeg.mSegType = KernSeg_HeapTracking;
    tempSeg.SegTreeNode.mUserVal = virtPageAddr;
    
    pHeapTrack = (K2OSKERN_HEAPTRACKPAGE *)virtPageAddr; // !not mapped yet!

    pCurThread->mWorkVirt = virtPageAddr;
    K2LIST_AddAtTail(&pCurThread->WorkPages_Dirty, &pTrackPage->ListLink);

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

K2STAT KernMem_PhysAllocToThread(UINT32 aPageCount, KernPhys_Disp aDisp)
{
    return K2STAT_ERROR_NOT_IMPL;
}

void KernMem_PhysFreeFromThread(void)
{
}

K2STAT KernMem_CreateSegmentFromThread(K2OSKERN_OBJ_SEGMENT *apSegSrc, K2OSKERN_OBJ_SEGMENT *apSegTarget)
{
    return K2STAT_ERROR_NOT_IMPL;
}

K2STAT KernMem_DestroySegmentToThread(K2OSKERN_OBJ_SEGMENT *apSeg)
{
    return K2STAT_ERROR_NOT_IMPL;
}

