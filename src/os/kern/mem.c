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

#define MAX_DLX_NAME_LEN    64

static
void
sDumpHeapNode(
    K2HEAP_ANCHOR * apHeap,
    K2HEAP_NODE *   apNode
)
{
    K2OSKERN_VMNODE *       pVmNode;
    K2OSKERN_OBJ_SEGMENT *  pSeg;
    UINT32                  nodeType;
    char const *            pStr;
    K2OSKERN_OBJ_DLX *      pDlxObj;
    char                    dlxName[MAX_DLX_NAME_LEN];
    K2STAT                  stat;

    if (K2HEAP_NodeIsFree(apNode))
    {
        K2OSKERN_Debug(
            "  %08X %08X FREE\n",
            apNode->AddrTreeNode.mUserVal,
            apNode->SizeTreeNode.mUserVal
        );
    }
    else
    {
        pVmNode = K2_GET_CONTAINER(K2OSKERN_VMNODE, apNode, HeapNode);
        nodeType = pVmNode->NonSeg.mType;

        if (nodeType != K2OSKERN_VMNODE_TYPE_SEGMENT)
        {
            if (nodeType == K2OSKERN_VMNODE_TYPE_RESERVED)
            {
                switch (pVmNode->NonSeg.mNodeInfo)
                {
                case K2OSKERN_VMNODE_RESERVED_ERROR:
                    pStr = "ERROR!";
                    break;
                case K2OSKERN_VMNODE_RESERVED_PUBLICAPI:
                    pStr = "PUBLICAPI";
                    break;
                case K2OSKERN_VMNODE_RESERVED_ARCHSPEC:
                    pStr = "ARCHSPEC";
                    break;
                case K2OSKERN_VMNODE_RESERVED_COREPAGES:
                    pStr = "COREPAGES";
                    break;
                case K2OSKERN_VMNODE_RESERVED_VAMAP:
                    pStr = "VAMAP";
                    break;
                case K2OSKERN_VMNODE_RESERVED_PHYSTRACK:
                    pStr = "PHYSTRACK";
                    break;
                case K2OSKERN_VMNODE_RESERVED_TRANSTAB:
                    pStr = "TRANSTAB";
                    break;
                case K2OSKERN_VMNODE_RESERVED_TRANSTAB_HOLE:
                    pStr = "TRANSTAB_HOLE";
                    break;
                case K2OSKERN_VMNODE_RESERVED_EFIMAP:
                    pStr = "EFIMAP";
                    break;
                case K2OSKERN_VMNODE_RESERVED_PTPAGECOUNT:
                    pStr = "PTPAGECOUNT";
                    break;
                case K2OSKERN_VMNODE_RESERVED_LOADER:
                    pStr = "LOADER";
                    break;
                case K2OSKERN_VMNODE_RESERVED_EFI_RUN_TEXT:
                    pStr = "EFI_RUN_TEXT";
                    break;
                case K2OSKERN_VMNODE_RESERVED_EFI_RUN_DATA:
                    pStr = "EFI_RUN_DATA";
                    break;
                case K2OSKERN_VMNODE_RESERVED_DEBUG:
                    pStr = "DEBUG";
                    break;
                case K2OSKERN_VMNODE_RESERVED_FW_TABLES:
                    pStr = "FW_TABLES";
                    break;
                case K2OSKERN_VMNODE_RESERVED_UNKNOWN:
                    pStr = "UNKNOWN";
                    break;
                case K2OSKERN_VMNODE_RESERVED_EFI_MAPPED_IO:
                    pStr = "EFI_MAPPED_IO";
                    break;
                case K2OSKERN_VMNODE_RESERVED_CORECLEARPAGES:
                    pStr = "CORECLEARPAGES";
                    break;
                default:
                    pStr = "******ERROR";
                    break;
                }
            }
            else
            {
                pStr = "******UNKNOWN!";
            }
            K2OSKERN_Debug(
                "  %08X %08X NONSEG(%d) %s\n",
                apNode->AddrTreeNode.mUserVal,
                apNode->SizeTreeNode.mUserVal,
                pVmNode->NonSeg.mType,
                pStr
            );
        }
        else
        {
            pSeg = pVmNode->mpSegment;
            K2_ASSERT(pSeg != NULL);
            if ((pSeg->mSegAndMemPageAttr & K2OS_SEG_ATTR_TYPE_MASK) == K2OS_SEG_ATTR_TYPE_DLX_PART)
            {
                pDlxObj = pSeg->Info.DlxPart.mpDlxObj;
                stat = DLX_GetIdent(pDlxObj->mpDlx, dlxName, MAX_DLX_NAME_LEN, NULL);
                K2_ASSERT(!K2STAT_IS_ERROR(stat));
                K2OSKERN_Debug(
                    "  %08X %08X SEGMENT %08X attr(%08X) DLX_PART %s SegIndex %d\n",
                    apNode->AddrTreeNode.mUserVal,
                    apNode->SizeTreeNode.mUserVal,
                    pSeg, pSeg->mSegAndMemPageAttr,
                    dlxName, pSeg->Info.DlxPart.mSegmentIndex
                );
            }
            else if ((pSeg->mSegAndMemPageAttr & K2OS_SEG_ATTR_TYPE_MASK) == K2OS_SEG_ATTR_TYPE_DLX_PAGE)
            {
                pDlxObj = pSeg->Info.DlxPage.mpDlxObj;
                stat = DLX_GetIdent(pDlxObj->mpDlx, dlxName, MAX_DLX_NAME_LEN, NULL);
                K2_ASSERT(!K2STAT_IS_ERROR(stat));
                K2OSKERN_Debug(
                    "  %08X %08X SEGMENT %08X attr(%08X) DLX_PAGE %s\n",
                    apNode->AddrTreeNode.mUserVal,
                    apNode->SizeTreeNode.mUserVal,
                    pSeg, pSeg->mSegAndMemPageAttr,
                    dlxName
                );
            }
            else
            {
                switch (pSeg->mSegAndMemPageAttr & K2OS_SEG_ATTR_TYPE_MASK)
                {
                case K2OS_SEG_ATTR_TYPE_PROCESS:
                    pStr = "PROCESS";
                    break;
                case K2OS_SEG_ATTR_TYPE_THREAD:
                    pStr = "THREAD";
                    break;
                case K2OS_SEG_ATTR_TYPE_HEAP_TRACK:
                    pStr = "HEAP_TRACK";
                    break;
                case K2OS_SEG_ATTR_TYPE_USER:
                    pStr = "USER";
                    break;
                case K2OS_SEG_ATTR_TYPE_DEVMAP:
                    pStr = "DEVMAP";
                    break;
                case K2OS_SEG_ATTR_TYPE_PHYSBUF:
                    pStr = "PHYSBUF";
                    break;
                case K2OS_SEG_ATTR_TYPE_SEG_SLAB:
                    pStr = "SEGSLAB";
                    break;
                default:
                    pStr = "UNKNOWN!";
                }
                K2OSKERN_Debug(
                    "  %08X %08X SEGMENT %08X attr(%08X) %s\n",
                    apNode->AddrTreeNode.mUserVal,
                    apNode->SizeTreeNode.mUserVal,
                    pSeg, pSeg->mSegAndMemPageAttr, pStr
                );
            }
        }
    }
}

static void sDumpPhys(void)
{
    UINT32                      flags;
    UINT32                      pageCount;
    BOOL                        intrDisp;
    K2OSKERN_PHYSTRACK_PAGE *   pPhysPage;
    UINT32                      left;

    K2OSKERN_Debug("\n\nPhysical Memory:\n");

    left = K2_VA32_PAGEFRAMES_FOR_4G;
    pPhysPage = (K2OSKERN_PHYSTRACK_PAGE *)K2OS_PHYS32_TO_PHYSTRACK(0);
    intrDisp = K2OSKERN_SeqIntrLock(&gData.PhysMemSeqLock);
    do {
        flags = pPhysPage->mFlags;
        if (flags & K2OSKERN_PHYSTRACK_FREE_FLAG)
        {
            pageCount = flags >> K2OSKERN_PHYSTRACK_PAGE_COUNT_SHL;
            K2OSKERN_Debug("  FREE   %08X - %8d Pages (%08X bytes) Flags %08X\n",
                K2OS_PHYSTRACK_TO_PHYS32((UINT32)pPhysPage),
                pageCount,
                pageCount * K2_VA32_MEMPAGE_BYTES,
                flags);
            pPhysPage += pageCount;
            K2_ASSERT(pageCount <= left);
            left -= pageCount;
        }
        else if (flags != 0)
        {
            if (flags & K2OSKERN_PHYSTRACK_CONTIG_ALLOC_FLAG)
            {
                pageCount = flags >> K2OSKERN_PHYSTRACK_PAGE_COUNT_SHL;
                K2OSKERN_Debug("  CONTIG %08X - %8d Pages (%08X bytes) Flags %08X\n",
                    K2OS_PHYSTRACK_TO_PHYS32((UINT32)pPhysPage),
                    pageCount,
                    pageCount * K2_VA32_MEMPAGE_BYTES,
                    flags);
                pPhysPage += pageCount;
                K2_ASSERT(pageCount <= left);
                left -= pageCount;
            }
            else
            {
                K2OSKERN_Debug("  PAGE   %08X - Owner %08X, List %2d Flags %08X\n",
                    K2OS_PHYSTRACK_TO_PHYS32((UINT32)pPhysPage),
                    pPhysPage->mpOwnerObject,
                    (flags & K2OSKERN_PHYSTRACK_PAGE_LIST_MASK) >> K2OSKERN_PHYSTRACK_PAGE_LIST_SHL,
                    flags);
                pPhysPage++;
                left--;
            }
        }
        else
        {
            //
            // not populated with trackable memory
            //
            pPhysPage++;
            left--;
        }
    } while (left > 0);

    K2OSKERN_SeqIntrUnlock(&gData.PhysMemSeqLock, intrDisp);
}

static void sDumpVirt(void)
{
    BOOL intrDisp;

    K2OSKERN_Debug("\nVirtual Memory:\n");

    intrDisp = K2OS_CritSecEnter(&gData.KernVirtHeapSec);
    K2_ASSERT(intrDisp);

    K2HEAP_Dump(&gData.KernVirtHeap, sDumpHeapNode);

    intrDisp = K2OS_CritSecLeave(&gData.KernVirtHeapSec);
    K2_ASSERT(intrDisp);
}

static void sDumpSeg(void)
{
    BOOL                    intrDisp;
    K2TREE_NODE *           pTreeNode;
    K2OSKERN_OBJ_SEGMENT *  pSeg;

    intrDisp = K2OSKERN_SeqIntrLock(&gpProc0->SegTreeSeqLock);

    K2OSKERN_Debug("\nProc0 segment tree:\n");
    pTreeNode = K2TREE_FirstNode(&gpProc0->SegTree);
    K2_ASSERT(pTreeNode != NULL);
    do {
        pSeg = K2_GET_CONTAINER(K2OSKERN_OBJ_SEGMENT, pTreeNode, SegTreeNode);
        K2_ASSERT(pSeg->Hdr.mObjType == K2OS_Obj_Segment);
        K2OSKERN_Debug("  %08X %08X %08X\n", pSeg->SegTreeNode.mUserVal, pSeg->mPagesBytes, pSeg->mSegAndMemPageAttr);
        pTreeNode = K2TREE_NextNode(&gpProc0->SegTree, pTreeNode);
    } while (pTreeNode != NULL);

    K2OSKERN_SeqIntrUnlock(&gpProc0->SegTreeSeqLock, intrDisp);
}

static void sDumpAll(void)
{
    sDumpPhys();
    sDumpVirt();
    sDumpSeg();
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

static K2HEAP_NODE * sKernVirtHeapAcquireNode(K2HEAP_ANCHOR *apHeap)
{
    K2HEAP_NODE * pRet;

    K2_ASSERT(apHeap == &gData.KernVirtHeap);

    K2_ASSERT(gData.HeapTrackFreeList.mNodeCount > 2);

    pRet = (K2HEAP_NODE *)gData.HeapTrackFreeList.mpHead;
    if (pRet != NULL)
    {
        K2LIST_Remove(&gData.HeapTrackFreeList, gData.HeapTrackFreeList.mpHead);
    }

    return pRet;
}

static void sKernVirtHeapReleaseNode(K2HEAP_ANCHOR *apHeap, K2HEAP_NODE *apNode)
{
    K2_ASSERT(apHeap == &gData.KernVirtHeap);

    K2LIST_AddAtTail(&gData.HeapTrackFreeList, (K2LIST_LINK *)apNode);
}

void KernMem_Start(void)
{
    K2STAT                      stat;
    K2OSKERN_OBJ_SEGMENT        tempSeg;
    K2OSKERN_OBJ_THREAD *       pCurThread;
    K2OSKERN_HEAPTRACKPAGE *    pHeapTrackPage;
    BOOL                        ok;
    K2OSKERN_OBJ_SEGMENT *      pSeg;
    K2TREE_NODE *               pTreeNode;

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

    //
    // Segment Slabs
    //
    ok = K2OS_CritSecInit(&gData.SegSec);
    K2_ASSERT(ok);
    K2LIST_Init(&gData.SegFreeList);
    gData.mpSegSlabs = NULL;

    //
    // every segment in the segment tree up to this point must be permanent
    //
    pTreeNode = K2TREE_FirstNode(&gpProc0->SegTree);
    K2_ASSERT(pTreeNode != NULL);
    do {
        pSeg = K2_GET_CONTAINER(K2OSKERN_OBJ_SEGMENT, pTreeNode, SegTreeNode);
        K2_ASSERT(pSeg->Hdr.mObjFlags & K2OSKERN_OBJ_FLAG_PERMANENT);
        pTreeNode = K2TREE_NextNode(&gpProc0->SegTree, pTreeNode);
    } while (pTreeNode != NULL);

//    sDumpAll();

    //
    // create the first heap tracking segment
    //
    pCurThread = K2OSKERN_CURRENT_THREAD;

    //
    // this virt alloc uses tracking from the initial static tracking memory
    //
    pCurThread->mWorkVirt_Range = K2HEAP_AllocAlignedHighest(&gData.KernVirtHeap, K2_VA32_MEMPAGE_BYTES, K2_VA32_MEMPAGE_BYTES);
    K2_ASSERT(pCurThread->mWorkVirt_Range != 0);
    pCurThread->mWorkVirt_PageCount = 1;

    stat = KernMem_PhysAllocToThread(1, KernPhys_Disp_Cached, FALSE);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));
    K2_ASSERT(pCurThread->WorkPages_Dirty.mNodeCount + pCurThread->WorkPages_Clean.mNodeCount > 0);

    K2MEM_Zero(&tempSeg, sizeof(tempSeg));
    tempSeg.Hdr.mObjType = K2OS_Obj_Segment;
    tempSeg.Hdr.mObjFlags = K2OSKERN_OBJ_FLAG_PERMANENT;
    tempSeg.Hdr.mRefCount = 0x7FFFFFFF;
    K2LIST_Init(&tempSeg.Hdr.WaitingThreadsPrioList);
    tempSeg.mPagesBytes = K2_VA32_MEMPAGE_BYTES;
    tempSeg.mSegAndMemPageAttr = K2OS_MAPTYPE_KERN_DATA | K2OS_SEG_ATTR_TYPE_HEAP_TRACK;
    tempSeg.SegTreeNode.mUserVal = pCurThread->mWorkVirt_Range;

    pHeapTrackPage = (K2OSKERN_HEAPTRACKPAGE *)pCurThread->mWorkVirt_Range; // !not mapped yet!

    stat = KernMem_CreateSegmentFromThread(&tempSeg, &pHeapTrackPage->SegObj);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));

    pHeapTrackPage->mpNextPage = NULL;

    gData.mpNextTrackPage = pHeapTrackPage;

    //
    // switch over the virtual heap off the init static list of tracks to the
    // new dynamic one
    //
    gData.KernVirtHeap.mfAcquireNode = sKernVirtHeapAcquireNode;
    gData.KernVirtHeap.mfReleaseNode = sKernVirtHeapReleaseNode;

    //
    // tracking list starts empty and first alloc will bump it
    //
    K2OSKERN_Debug("\n---------Mem Started---------\n\n");
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
    BOOL                    ok;
    K2STAT                  stat;
    K2OSKERN_OBJ_THREAD *   pCurThread;
    K2OSKERN_HEAPTRACKPAGE *pPage;
    UINT32                  left;
    K2OSKERN_VMNODE *       pNode;
    K2OSKERN_OBJ_SEGMENT    tempSeg;

    pCurThread = K2OSKERN_CURRENT_THREAD;

    K2_ASSERT(pCurThread->mWorkVirt_Range == 0);
    K2_ASSERT(pCurThread->mWorkVirt_PageCount == 0);

    stat = K2STAT_NO_ERROR;

    if (aUseAddr != 0)
    {
        K2_ASSERT((aUseAddr & K2_VA32_MEMPAGE_OFFSET_MASK) == 0);
    }

    ok = K2OS_CritSecEnter(&gData.KernVirtHeapSec);
    K2_ASSERT(ok);

    if (gData.HeapTrackFreeList.mNodeCount <= 3)
    {
        //
        // need more heap tracking!
        //
        pPage = gData.mpNextTrackPage;
        pNode = (K2OSKERN_VMNODE *)pPage->TrackSpace;
        left = TRACK_PER_PAGE;
        do {
            K2LIST_AddAtTail(&gData.HeapTrackFreeList, (K2LIST_LINK *)pNode);
            pNode++;
        } while (--left);
        pPage->mpNextPage = gData.mpTrackPages;
        gData.mpTrackPages = pPage;
        gData.mpNextTrackPage = NULL;

        //
        // guaranteed we can do this and we won't run out of tracking
        // if it fails then we ran out of heap space
        //
        pCurThread->mWorkVirt_Range = K2HEAP_AllocAlignedHighest(&gData.KernVirtHeap, K2_VA32_MEMPAGE_BYTES, K2_VA32_MEMPAGE_BYTES);
        if (0 == pCurThread->mWorkVirt_Range)
        {
            //
            // out of virtual heap space!   Undo the tracking add
            //
            gData.mpTrackPages = pPage->mpNextPage;
            gData.mpNextTrackPage = pPage;
            pNode = (K2OSKERN_VMNODE *)pPage->TrackSpace;
            left = TRACK_PER_PAGE;
            do {
                K2LIST_Remove(&gData.HeapTrackFreeList, (K2LIST_LINK *)pNode);
                pNode++;
            } while (--left);

            //
            // back to how we started
            //
            ok = K2OS_CritSecLeave(&gData.KernVirtHeapSec);
            K2_ASSERT(ok);

            return K2STAT_ERROR_OUT_OF_RESOURCES;
        }

        pCurThread->mWorkVirt_PageCount = 1;

        stat = KernMem_PhysAllocToThread(1, KernPhys_Disp_Cached, FALSE);
        K2_ASSERT(!K2STAT_IS_ERROR(stat));
        K2_ASSERT(pCurThread->WorkPages_Dirty.mNodeCount + pCurThread->WorkPages_Clean.mNodeCount > 0);

        K2MEM_Zero(&tempSeg, sizeof(tempSeg));
        tempSeg.Hdr.mObjType = K2OS_Obj_Segment;
        tempSeg.Hdr.mObjFlags = K2OSKERN_OBJ_FLAG_PERMANENT;
        tempSeg.Hdr.mRefCount = 0x7FFFFFFF;
        K2LIST_Init(&tempSeg.Hdr.WaitingThreadsPrioList);
        tempSeg.mPagesBytes = K2_VA32_MEMPAGE_BYTES;
        tempSeg.mSegAndMemPageAttr = K2OS_MAPTYPE_KERN_DATA | K2OS_SEG_ATTR_TYPE_HEAP_TRACK;
        tempSeg.SegTreeNode.mUserVal = pCurThread->mWorkVirt_Range;

        pPage = (K2OSKERN_HEAPTRACKPAGE *)pCurThread->mWorkVirt_Range; // !not mapped yet!

        stat = KernMem_CreateSegmentFromThread(&tempSeg, &pPage->SegObj);
        K2_ASSERT(!K2STAT_IS_ERROR(stat));

        pPage->mpNextPage = NULL;

        gData.mpNextTrackPage = pPage;

        K2_ASSERT(pCurThread->mWorkVirt_Range == 0);
        K2_ASSERT(pCurThread->mWorkVirt_PageCount == 0);
    }

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
        pCurThread->mWorkVirt_Range = aUseAddr;
        pCurThread->mWorkVirt_PageCount = aPageCount;
    }

    ok = K2OS_CritSecLeave(&gData.KernVirtHeapSec);
    K2_ASSERT(ok);

    return stat;
}

void KernMem_VirtFreeFromThread(void)
{
    BOOL                    ok;
    K2STAT                  stat;
    K2HEAP_NODE *           pHeapNode;
    K2OSKERN_OBJ_THREAD *   pCurThread;

    pCurThread = K2OSKERN_CURRENT_THREAD;

    K2_ASSERT(pCurThread->mWorkVirt_Range != 0);
    K2_ASSERT(pCurThread->mWorkVirt_PageCount != 0);

    ok = K2OS_CritSecEnter(&gData.KernVirtHeapSec);
    K2_ASSERT(ok);

    pHeapNode = K2HEAP_FindNodeContainingAddr(&gData.KernVirtHeap, pCurThread->mWorkVirt_Range);
    K2_ASSERT(pHeapNode != NULL);
    K2_ASSERT(K2HEAP_NodeAddr(pHeapNode) == pCurThread->mWorkVirt_Range);
    K2_ASSERT(K2HEAP_NodeSize(pHeapNode) == pCurThread->mWorkVirt_PageCount * K2_VA32_MEMPAGE_BYTES);
    stat = K2HEAP_FreeNode(&gData.KernVirtHeap, pHeapNode);

    if (!K2STAT_IS_ERROR(stat))
    {
        pCurThread->mWorkVirt_Range = 0;
        pCurThread->mWorkVirt_PageCount = 0;
    }

    ok = K2OS_CritSecLeave(&gData.KernVirtHeapSec);
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

    if (aForPageTables)
    {
        K2_ASSERT(pCurThread->WorkPtPages_Dirty.mNodeCount + pCurThread->WorkPtPages_Clean.mNodeCount == 0);
    }
    else
    {
        K2_ASSERT(pCurThread->WorkPages_Dirty.mNodeCount + pCurThread->WorkPages_Clean.mNodeCount == 0);
    }

    //
    // first try to pull pages off clean page list
    //
    tookClean = 0;
    do {
        intrDisp = K2OSKERN_SeqIntrLock(&gData.PhysMemSeqLock);

        pList = &gData.PhysPageList[KernPhysPageList_Free_Clean];

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
                    pPhysPage->mFlags |= (KernPhysPageList_Thread_PtClean << K2OSKERN_PHYSTRACK_PAGE_LIST_SHL);
                    K2LIST_AddAtTail(&pCurThread->WorkPtPages_Clean, &pPhysPage->ListLink);
                }
                else
                {
                    pPhysPage->mFlags |= (KernPhysPageList_Thread_Clean << K2OSKERN_PHYSTRACK_PAGE_LIST_SHL);
                    K2LIST_AddAtTail(&pCurThread->WorkPages_Clean, &pPhysPage->ListLink);
                }

                tookClean++;

                if (--aPageCount == 0)
                    break;

                if (--chunkLeft == 0)
                {
                    K2OSKERN_SeqIntrUnlock(&gData.PhysMemSeqLock, intrDisp);
                    //
                    // give interrupts a chance to fire here
                    //
                    intrDisp = K2OSKERN_SeqIntrLock(&gData.PhysMemSeqLock);
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
        K2OSKERN_SeqIntrUnlock(&gData.PhysMemSeqLock, intrDisp);
        return K2STAT_NO_ERROR;
    }

    //
    // next try to pull pages off dirty page list
    //
    do {
        pList = &gData.PhysPageList[KernPhysPageList_Free_Dirty];
        
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
                    pPhysPage->mFlags |= (KernPhysPageList_Thread_PtDirty << K2OSKERN_PHYSTRACK_PAGE_LIST_SHL);
                    K2LIST_AddAtTail(&pCurThread->WorkPtPages_Dirty, &pPhysPage->ListLink);
                }
                else
                {
                    pPhysPage->mFlags |= (KernPhysPageList_Thread_Dirty << K2OSKERN_PHYSTRACK_PAGE_LIST_SHL);
                    K2LIST_AddAtTail(&pCurThread->WorkPages_Dirty, &pPhysPage->ListLink);
                }

                tookDirty++;

                if (--aPageCount == 0)
                    break;

                if (--chunkLeft == 0)
                {
                    K2OSKERN_SeqIntrUnlock(&gData.PhysMemSeqLock, intrDisp);
                    //
                    // give interrupts a chance to fire here
                    //
                    intrDisp = K2OSKERN_SeqIntrLock(&gData.PhysMemSeqLock);
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
        K2OSKERN_SeqIntrUnlock(&gData.PhysMemSeqLock, intrDisp);
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
    pTreeNode = K2TREE_FirstNode(&gData.PhysFreeTree);
    if (pTreeNode != NULL)
    {
        chunkLeft = aPageCount;
        do {
            nodeCount = pTreeNode->mUserVal >> K2OSKERN_PHYSTRACK_PAGE_COUNT_SHL;
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
            pTreeNode = K2TREE_NextNode(&gData.PhysFreeTree, pTreeNode);

        } while (pTreeNode != NULL);
    }

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
            pList = &gData.PhysPageList[KernPhysPageList_Free_Dirty];
            pListLink = pCurThread->WorkPages_Dirty.mpTail;
            do {
                pPhysPage = K2_GET_CONTAINER(K2OSKERN_PHYSTRACK_PAGE, pListLink, ListLink);
                pListLink = pListLink->mpPrev;

                if (aForPageTables)
                {
                    K2LIST_Remove(&pCurThread->WorkPtPages_Dirty, &pPhysPage->ListLink);
                }
                else
                {
                    K2LIST_Remove(&pCurThread->WorkPages_Dirty, &pPhysPage->ListLink);
                }
                pPhysPage->mFlags &= ~K2OSKERN_PHYSTRACK_PAGE_LIST_MASK;
                pPhysPage->mFlags |= (KernPhysPageList_Free_Dirty << K2OSKERN_PHYSTRACK_PAGE_LIST_SHL);
                K2LIST_AddAtTail(pList, &pPhysPage->ListLink);
            } while (--tookDirty > 0);
        }

        if (tookClean)
        {
            pList = &gData.PhysPageList[KernPhysPageList_Free_Clean];
            pListLink = pCurThread->WorkPages_Clean.mpTail;
            do {
                pPhysPage = K2_GET_CONTAINER(K2OSKERN_PHYSTRACK_PAGE, pListLink, ListLink);
                pListLink = pListLink->mpPrev;

                if (aForPageTables)
                {
                    K2LIST_Remove(&pCurThread->WorkPtPages_Clean, &pPhysPage->ListLink);
                }
                else
                {
                    K2LIST_Remove(&pCurThread->WorkPages_Clean, &pPhysPage->ListLink);
                }
                pPhysPage->mFlags &= ~K2OSKERN_PHYSTRACK_PAGE_LIST_MASK;
                pPhysPage->mFlags |= (KernPhysPageList_Free_Clean << K2OSKERN_PHYSTRACK_PAGE_LIST_SHL);
                K2LIST_AddAtTail(pList, &pPhysPage->ListLink);
            } while (--tookClean > 0);
        }

        K2OSKERN_SeqIntrUnlock(&gData.PhysMemSeqLock, intrDisp);

        return K2STAT_ERROR_OUT_OF_MEMORY;
    }

    //
    // if we get here, we know the request can be satisfied
    //
    pTreeNode = K2TREE_FirstNode(&gData.PhysFreeTree);
    chunkLeft = aPageCount;
    do {
        pNextNode = K2TREE_NextNode(&gData.PhysFreeTree, pTreeNode);

        nodeCount = pTreeNode->mUserVal >> K2OSKERN_PHYSTRACK_PAGE_COUNT_SHL;
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
            K2TREE_Remove(&gData.PhysFreeTree, pTreeNode);

            flags = (pTreeNode->mUserVal & K2OSKERN_PHYSTRACK_PROP_MASK);

            if (nodeCount > aPageCount)
            {
                pLeftOver = pTreeNode + aPageCount;

                pLeftOver->mUserVal =
                    flags |
                    K2OSKERN_PHYSTRACK_FREE_FLAG |
                    ((nodeCount - aPageCount) << K2OSKERN_PHYSTRACK_PAGE_COUNT_SHL);
                K2TREE_Insert(&gData.PhysFreeTree, pLeftOver->mUserVal, pLeftOver);
                takePages = aPageCount;
            }
            else
            {
                takePages = nodeCount;
            }

            pPhysPage = (K2OSKERN_PHYSTRACK_PAGE *)pTreeNode;

            if (aForPageTables)
            {
                do {
                    pPhysPage->mFlags = (KernPhysPageList_Thread_PtDirty << K2OSKERN_PHYSTRACK_PAGE_LIST_SHL) | flags;
                    pPhysPage->mpOwnerObject = NULL;
                    K2LIST_AddAtTail(&pCurThread->WorkPtPages_Dirty, &pPhysPage->ListLink);
                    pPhysPage++;
                    --aPageCount;
                } while (--takePages);
            }
            else
            {
                do {
                    pPhysPage->mFlags = (KernPhysPageList_Thread_Dirty << K2OSKERN_PHYSTRACK_PAGE_LIST_SHL) | flags;
                    pPhysPage->mpOwnerObject = NULL;
                    K2LIST_AddAtTail(&pCurThread->WorkPages_Dirty, &pPhysPage->ListLink);
                    pPhysPage++;
                    --aPageCount;
                } while (--takePages);
            }

            if (aPageCount == 0)
            {
                break;
            }
        }

        pTreeNode = pNextNode;

    } while (pTreeNode != NULL);

    K2OSKERN_SeqIntrUnlock(&gData.PhysMemSeqLock, intrDisp);

    return K2STAT_NO_ERROR;
}

void KernMem_PhysFreeFromThread(void)
{
    K2_ASSERT(0);
}

static void sCleanPage(UINT32 aPhysPage)
{
    BOOL                disp;
    K2OSKERN_CPUCORE *  pThisCore;
    UINT32              virtAddr;

    //
    // interrupts off so thread does not migrate to another core or get stopped
    // while it is clearing the page
    //
    disp = K2OSKERN_SetIntr(FALSE);

    pThisCore = K2OSKERN_GET_CURRENT_CPUCORE;

    virtAddr = K2OS_KVA_CORECLEARPAGES_BASE + (pThisCore->mCoreIx * K2_VA32_MEMPAGE_BYTES);

    KernMap_MakeOnePage(K2OS_KVA_KERNVAMAP_BASE, virtAddr, aPhysPage, K2OS_MAPTYPE_KERN_WRITETHRU_CACHED);

    K2MEM_Zero((void *)virtAddr, K2_VA32_MEMPAGE_BYTES);

    KernMap_BreakOnePage(K2OS_KVA_KERNVAMAP_BASE, virtAddr);

    KernArch_InvalidateTlbPageOnThisCore(virtAddr);

    K2OSKERN_SetIntr(disp);
}

K2STAT KernMem_CreateSegmentFromThread(K2OSKERN_OBJ_SEGMENT *apSegSrc, K2OSKERN_OBJ_SEGMENT *apDst)
{
    K2OSKERN_OBJ_THREAD *       pCurThread;
    UINT32                      pageCount;
    UINT32                      virtAddr;
    UINT32                      needPT;
    K2STAT                      stat;
    UINT32                      segType;
    BOOL                        disp;
    BOOL                        cleanAfter;
    BOOL                        lockStatus;
    UINT32                      targetPageList;
    K2OSKERN_PHYSTRACK_PAGE *   pPhysPage;

    K2_ASSERT(apSegSrc != NULL);
    K2_ASSERT(apSegSrc->Hdr.mObjType == K2OS_Obj_Segment);
    K2_ASSERT(apSegSrc->Hdr.mRefCount > 0);
    K2_ASSERT(apSegSrc->mPagesBytes > 0);
    K2_ASSERT((apSegSrc->mPagesBytes & K2_VA32_MEMPAGE_OFFSET_MASK) == 0);
    pageCount = apSegSrc->mPagesBytes / K2_VA32_MEMPAGE_BYTES;
    segType = (apSegSrc->mSegAndMemPageAttr & K2OS_SEG_ATTR_TYPE_MASK);
    K2_ASSERT(segType < K2OS_SEG_ATTR_TYPE_COUNT);
    K2_ASSERT((apSegSrc->SegTreeNode.mUserVal & K2_VA32_MEMPAGE_OFFSET_MASK) == 0);
    virtAddr = apSegSrc->SegTreeNode.mUserVal;

    //
    // get pagetable pages we may need when mapping the segment
    //
    needPT = KernMem_CountPT(virtAddr, pageCount);

    //
    // pull pages that can be used as pagetables to thread
    //
    stat = KernMem_PhysAllocToThread(needPT, KernPhys_Disp_Cached_WriteThrough, TRUE);
    if (K2STAT_IS_ERROR(stat))
        return stat;

    //
    // cannot fail from this point forward
    //

    pCurThread = K2OSKERN_CURRENT_THREAD;

    if (pCurThread->WorkPtPages_Dirty.mNodeCount > 0)
    {
        //
        // clean the pagetable pages - these need to be clean before we start using them
        // otherwise there will be garbage in them mapping stuff randomly before they are
        // initialized
        //
        do {
            pPhysPage = K2_GET_CONTAINER(K2OSKERN_PHYSTRACK_PAGE, pCurThread->WorkPtPages_Dirty.mpHead, ListLink);
            K2_ASSERT((pPhysPage->mFlags & K2OSKERN_PHYSTRACK_FREE_FLAG) == 0);

            sCleanPage(K2OS_PHYSTRACK_TO_PHYS32((UINT32)pPhysPage));

            disp = K2OSKERN_SetIntr(FALSE);

            K2LIST_Remove(&pCurThread->WorkPtPages_Dirty, &pPhysPage->ListLink);
            pPhysPage->mFlags &= ~K2OSKERN_PHYSTRACK_PAGE_LIST_MASK;
            pPhysPage->mFlags |= (KernPhysPageList_Thread_PtClean << K2OSKERN_PHYSTRACK_PAGE_LIST_SHL);
            K2LIST_AddAtTail(&pCurThread->WorkPtPages_Clean, &pPhysPage->ListLink);

            K2OSKERN_SetIntr(disp);
        } while (pCurThread->WorkPtPages_Dirty.mNodeCount > 0);
    }

    K2_ASSERT(pCurThread->WorkPtPages_Clean.mNodeCount == needPT);

    //
    // load first pagetable page into mapper args
    //
    disp = K2OSKERN_SetIntr(FALSE);

    pPhysPage = K2_GET_CONTAINER(K2OSKERN_PHYSTRACK_PAGE, pCurThread->WorkPtPages_Clean.mpHead, ListLink);
    K2LIST_Remove(&pCurThread->WorkPtPages_Clean, &pPhysPage->ListLink);
    pPhysPage->mFlags &= ~K2OSKERN_PHYSTRACK_PAGE_LIST_MASK;
    pPhysPage->mFlags |= (KernPhysPageList_Thread_PtWorking << K2OSKERN_PHYSTRACK_PAGE_LIST_SHL);
    pCurThread->mpWorkPtPage = pPhysPage;

    K2OSKERN_SetIntr(disp);

    switch (segType)
    {
    case K2OS_SEG_ATTR_TYPE_PROCESS:
    case K2OS_SEG_ATTR_TYPE_THREAD:
        targetPageList = KernPhysPageList_Proc;
        break;

    case K2OS_SEG_ATTR_TYPE_HEAP_TRACK:
        K2_ASSERT((apSegSrc->Hdr.mObjFlags & K2OSKERN_OBJ_FLAG_PERMANENT) != 0);
        targetPageList = KernPhysPageList_Non_KData;
        break;

    case K2OS_SEG_ATTR_TYPE_USER:
        targetPageList = KernPhysPageList_General;
        break;

    case K2OS_SEG_ATTR_TYPE_DLX_PART:
        if (apSegSrc->mSegAndMemPageAttr & K2OS_MEMPAGE_ATTR_KERNEL)
        {
            if (apSegSrc->mSegAndMemPageAttr & K2OS_MEMPAGE_ATTR_EXEC)
            {
                targetPageList = KernPhysPageList_Non_KText;
            }
            else if (apSegSrc->mSegAndMemPageAttr & K2OS_MEMPAGE_ATTR_WRITEABLE)
            {
                targetPageList = KernPhysPageList_Non_KData;
            }
            else
            {
                targetPageList = KernPhysPageList_Non_KRead;
            }
        }
        else
        {
            if (apSegSrc->mSegAndMemPageAttr & K2OS_MEMPAGE_ATTR_EXEC)
            {
                targetPageList = KernPhysPageList_Non_UText;
            }
            else if (apSegSrc->mSegAndMemPageAttr & K2OS_MEMPAGE_ATTR_WRITEABLE)
            {
                targetPageList = KernPhysPageList_Non_UData;
            }
            else
            {
                targetPageList = KernPhysPageList_Non_URead;
            }
        }
        break;

    case K2OS_SEG_ATTR_TYPE_DEVMAP:
        targetPageList = KernPhysPageList_DeviceU;
        break;

    case K2OS_SEG_ATTR_TYPE_PHYSBUF:
        targetPageList = KernPhysPageList_General;
        break;

    case K2OS_SEG_ATTR_TYPE_DLX_PAGE:
        targetPageList = KernPhysPageList_Non_KData;
        break;

    case K2OS_SEG_ATTR_TYPE_SEG_SLAB:
        targetPageList = KernPhysPageList_Non_KData;
        break;

    default:
        K2_ASSERT(0);
    }

    //
    // if this is a thread segment then the first page in the range is a guard page
    //
    if (segType == K2OS_SEG_ATTR_TYPE_THREAD)
    {
        pCurThread->mWorkMapAttr = K2OS_MEMPAGE_ATTR_GUARD | K2OS_MEMPAGE_ATTR_KERNEL;
        pCurThread->mWorkMapAddr = virtAddr;
        pCurThread->mpWorkPage = NULL;
        KernMap_MakeOnePageFromThread(pCurThread, NULL, KernPhysPageList_Error);

        virtAddr += K2_VA32_MEMPAGE_BYTES;
        pageCount--;
        K2_ASSERT(pageCount > 0);
    }

    pCurThread->mWorkMapAttr = apSegSrc->mSegAndMemPageAttr;

    lockStatus = FALSE;

    cleanAfter = (pCurThread->mWorkMapAttr & K2OS_MEMPAGE_ATTR_WRITEABLE) ? TRUE : FALSE;

    do {
        pCurThread->mWorkMapAddr = virtAddr;

        //
        // try to load a regular page
        //
        if (pCurThread->mpWorkPage == NULL)
        {
            if ((pCurThread->WorkPages_Clean.mNodeCount > 0) || (pCurThread->WorkPages_Dirty.mNodeCount > 0))
            {
                if (pCurThread->WorkPages_Clean.mNodeCount > 0)
                {
                    disp = K2OSKERN_SetIntr(FALSE);
                    lockStatus = TRUE;

                    pPhysPage = K2_GET_CONTAINER(K2OSKERN_PHYSTRACK_PAGE, pCurThread->WorkPages_Clean.mpHead, ListLink);
                    K2LIST_Remove(&pCurThread->WorkPages_Clean, &pPhysPage->ListLink);
                }
                else
                {
                    //
                    // clean a dirty page if the page will not be mapped as writeable
                    //
                    pPhysPage = K2_GET_CONTAINER(K2OSKERN_PHYSTRACK_PAGE, pCurThread->WorkPages_Dirty.mpHead, ListLink);

                    if (!cleanAfter)
                    {
                        sCleanPage(K2OS_PHYSTRACK_TO_PHYS32((UINT32)pPhysPage));
                    }

                    disp = K2OSKERN_SetIntr(FALSE);
                    lockStatus = TRUE;

                    K2LIST_Remove(&pCurThread->WorkPages_Dirty, &pPhysPage->ListLink);
                }

                pPhysPage->mFlags &= ~K2OSKERN_PHYSTRACK_PAGE_LIST_MASK;
                pPhysPage->mFlags |= (KernPhysPageList_Thread_Working << K2OSKERN_PHYSTRACK_PAGE_LIST_SHL);
                pCurThread->mpWorkPage = pPhysPage;
            }
        }

        if (pCurThread->mpWorkPtPage == NULL)
        {
            //
            // try to load a pagetable page
            //
            if (pCurThread->WorkPtPages_Clean.mNodeCount > 0)
            {
                if (!lockStatus)
                {
                    disp = K2OSKERN_SetIntr(FALSE);
                    lockStatus = TRUE;
                }
                pPhysPage = K2_GET_CONTAINER(K2OSKERN_PHYSTRACK_PAGE, pCurThread->WorkPtPages_Clean.mpHead, ListLink);
                K2LIST_Remove(&pCurThread->WorkPtPages_Clean, &pPhysPage->ListLink);
                pPhysPage->mFlags &= ~K2OSKERN_PHYSTRACK_PAGE_LIST_MASK;
                pPhysPage->mFlags |= (KernPhysPageList_Thread_PtWorking << K2OSKERN_PHYSTRACK_PAGE_LIST_SHL);
                pCurThread->mpWorkPtPage = pPhysPage;
            }
        }

        if (lockStatus)
        {
            K2OSKERN_SetIntr(disp);
            lockStatus = FALSE;
        }

        if (pCurThread->mpWorkPage != NULL)
        {
            KernMap_MakeOnePageFromThread(pCurThread, apDst, targetPageList);
            if (cleanAfter)
            {
                K2MEM_Zero((void *)virtAddr, K2_VA32_MEMPAGE_BYTES);
            }
        }

        virtAddr += K2_VA32_MEMPAGE_BYTES;
    } while (--pageCount > 0);

    K2_ASSERT(pCurThread->WorkPages_Clean.mNodeCount + pCurThread->WorkPages_Dirty.mNodeCount == 0);

    if ((apDst != NULL) && (apDst != apSegSrc))
    {
        //
        // target segment was most probably inside area just mapped!
        //
        K2MEM_Copy(apDst, apSegSrc, sizeof(K2OSKERN_OBJ_SEGMENT));
        K2MEM_Zero(apSegSrc, sizeof(K2OSKERN_OBJ_SEGMENT));
        apSegSrc = apDst;
    }

    //
    // free any unused pagetables pages back to system free clean list
    //
    if ((pCurThread->mpWorkPtPage != NULL) || (pCurThread->WorkPtPages_Clean.mNodeCount > 0))
    {
        disp = K2OSKERN_SetIntr(FALSE);

        if (pCurThread->mpWorkPtPage != NULL)
        {
            pPhysPage = pCurThread->mpWorkPtPage;
            pPhysPage->mFlags &= ~K2OSKERN_PHYSTRACK_PAGE_LIST_MASK;
            pPhysPage->mFlags |= (KernPhysPageList_Thread_PtWorking << K2OSKERN_PHYSTRACK_PAGE_LIST_SHL);
            K2LIST_AddAtHead(&pCurThread->WorkPtPages_Clean, &pPhysPage->ListLink);
            pCurThread->mpWorkPtPage = NULL;
        }

        do {
            pPhysPage = K2_GET_CONTAINER(K2OSKERN_PHYSTRACK_PAGE, pCurThread->WorkPtPages_Clean.mpHead, ListLink);

            K2LIST_Remove(&pCurThread->WorkPtPages_Clean, &pPhysPage->ListLink);
            pPhysPage->mFlags &= ~K2OSKERN_PHYSTRACK_PAGE_LIST_MASK;
            pPhysPage->mFlags |= (KernPhysPageList_Free_Clean << K2OSKERN_PHYSTRACK_PAGE_LIST_SHL);
            K2LIST_AddAtTail(&gData.PhysPageList[KernPhysPageList_Free_Clean], &pPhysPage->ListLink);

        } while (pCurThread->WorkPtPages_Clean.mNodeCount > 0);

        K2OSKERN_SetIntr(disp);
    }
    
    //
    // latch the segment into the segment tree
    //
    if (apSegSrc->SegTreeNode.mUserVal >= K2OS_KVA_KERN_BASE)
    {
        disp = K2OSKERN_SeqIntrLock(&gpProc0->SegTreeSeqLock);
        K2TREE_Insert(&gpProc0->SegTree, apSegSrc->SegTreeNode.mUserVal, &apSegSrc->SegTreeNode);
        K2OSKERN_SeqIntrUnlock(&gpProc0->SegTreeSeqLock, disp);
    }
    else
    {
        disp = K2OSKERN_SeqIntrLock(&pCurThread->mpProc->SegTreeSeqLock);
        K2TREE_Insert(&pCurThread->mpProc->SegTree, apSegSrc->SegTreeNode.mUserVal, &apSegSrc->SegTreeNode);
        K2OSKERN_SeqIntrUnlock(&pCurThread->mpProc->SegTreeSeqLock, disp);
    }

    //
    // clean up
    //
    pCurThread->mWorkVirt_Range = 0;
    pCurThread->mWorkVirt_PageCount = 0;
    pCurThread->mWorkMapAttr = 0;
    pCurThread->mWorkMapAddr = 0;
    K2_ASSERT(pCurThread->mpWorkPage == NULL);
    K2_ASSERT(pCurThread->mpWorkPtPage == NULL);

    return K2STAT_NO_ERROR;
}

K2STAT KernMem_SegAlloc(K2OSKERN_OBJ_SEGMENT **apRetSeg)
{
    K2STAT                  stat;
    K2OSKERN_OBJ_SEGMENT    tempSeg;
    K2OSKERN_OBJ_THREAD *   pCurThread;
    K2OSKERN_SEGSLAB *      pSlab;
    K2OSKERN_OBJ_SEGMENT *  pSeg;
    BOOL                    ok;
    UINT32                  left;

    K2_ASSERT(apRetSeg != NULL);

    ok = K2OS_CritSecEnter(&gData.SegSec);
    K2_ASSERT(ok);

    pCurThread = K2OSKERN_CURRENT_THREAD;

    stat = K2STAT_NO_ERROR;

    if (gData.SegFreeList.mNodeCount == 0)
    {
        do {
            stat = KernMem_VirtAllocToThread(0xC0000000, 1, TRUE);
            if (K2STAT_IS_ERROR(stat))
                break;

            K2OSKERN_Debug("Seg slab is at %08X\n", pCurThread->mWorkVirt_Range);

            do {
                stat = KernMem_PhysAllocToThread(1, KernPhys_Disp_Cached, FALSE);
                if (K2STAT_IS_ERROR(stat))
                    break;

                K2_ASSERT(pCurThread->WorkPages_Dirty.mNodeCount + pCurThread->WorkPages_Clean.mNodeCount > 0);

                K2MEM_Zero(&tempSeg, sizeof(tempSeg));
                tempSeg.Hdr.mObjType = K2OS_Obj_Segment;
                tempSeg.Hdr.mRefCount = 1;
                K2LIST_Init(&tempSeg.Hdr.WaitingThreadsPrioList);
                tempSeg.mPagesBytes = K2_VA32_MEMPAGE_BYTES;
                tempSeg.mSegAndMemPageAttr = K2OS_MAPTYPE_KERN_DATA | K2OS_SEG_ATTR_TYPE_SEG_SLAB;
                tempSeg.SegTreeNode.mUserVal = pCurThread->mWorkVirt_Range;

                pSlab = (K2OSKERN_SEGSLAB *)pCurThread->mWorkVirt_Range; // !not mapped yet!

                stat = KernMem_CreateSegmentFromThread(&tempSeg, &pSlab->This);
                if (K2STAT_IS_ERROR(stat))
                {
                    KernMem_PhysFreeFromThread();
                    break;
                }

                pSeg = (K2OSKERN_OBJ_SEGMENT *)pSlab->SegStore;
                left = SEGSTORE_OBJ_COUNT;
                do {
                    K2LIST_AddAtTail(&gData.SegFreeList, (K2LIST_LINK *)pSeg);
                    pSeg++;
                } while (--left);

                pSlab->mpNextSlab = gData.mpSegSlabs;
                pSlab->mUseMask = 1;
                gData.mpSegSlabs = pSlab;

                K2_ASSERT(pCurThread->mWorkVirt_Range == 0);
                K2_ASSERT(pCurThread->mWorkVirt_PageCount == 0);

            } while (0);

            if (K2STAT_IS_ERROR(stat))
            {
                KernMem_VirtFreeFromThread();
            }

        } while (0);
    }

    if (!K2STAT_IS_ERROR(stat))
    {
        K2_ASSERT(gData.SegFreeList.mNodeCount > 0);

        *apRetSeg = pSeg = (K2OSKERN_OBJ_SEGMENT *)gData.SegFreeList.mpHead;

        K2LIST_Remove(&gData.SegFreeList, gData.SegFreeList.mpHead);

        //
        // slab address of the allocated segment is the page address it is within
        //
        pSlab = (K2OSKERN_SEGSLAB *)(((UINT32)pSeg) & K2_VA32_PAGEFRAME_MASK);

        //
        // index within the slab is calculated from the offset into the page
        //
        left = (((UINT32)(*apRetSeg)) & K2_VA32_MEMPAGE_OFFSET_MASK) / sizeof(K2OSKERN_OBJ_SEGMENT);

        pSlab->mUseMask |= (1ull << left);
    }

    ok = K2OS_CritSecLeave(&gData.SegSec);
    K2_ASSERT(ok);

    return stat;
}

K2STAT KernMem_SegFree(K2OSKERN_OBJ_SEGMENT *apSeg)
{
    K2OSKERN_SEGSLAB *          pSlab;
    BOOL                        ok;
    UINT32                      ix;
    K2OSKERN_OBJ_THREAD *       pCurThread;
    K2OSKERN_PHYSTRACK_PAGE *   pPhysPage;
    K2OSKERN_SEGSLAB *          pPrev;
    K2OSKERN_SEGSLAB *          pLook;

    K2_ASSERT((apSeg->Hdr.mObjFlags & K2OSKERN_OBJ_FLAG_PERMANENT) == 0);

    ok = K2OS_CritSecEnter(&gData.SegSec);
    K2_ASSERT(ok);

    pSlab = (K2OSKERN_SEGSLAB *)(((UINT32)apSeg) & K2_VA32_PAGEFRAME_MASK);

    ix = (((UINT32)apSeg) & K2_VA32_MEMPAGE_OFFSET_MASK) / sizeof(K2OSKERN_OBJ_SEGMENT);

    K2_ASSERT(ix != 0);

    K2MEM_Zero(apSeg, sizeof(K2OSKERN_OBJ_SEGMENT));
    K2LIST_AddAtTail(&gData.SegFreeList, (K2LIST_LINK *)apSeg);
    pSlab->mUseMask &= ~(1ull << ix);
    if (pSlab->mUseMask == 1)
    {
        //
        // slab is empty. remove segments in it from the free list and deallocate it
        //
        apSeg = (K2OSKERN_OBJ_SEGMENT *)pSlab->SegStore;
        ix = SEGSTORE_OBJ_COUNT;
        do {
            K2LIST_Remove(&gData.SegFreeList, (K2LIST_LINK *)apSeg);
            apSeg++;
        } while (--ix);

        pPrev = NULL;
        pLook = gData.mpSegSlabs;
        do {
            if (pLook == pSlab)
                break;
            pPrev = pLook;
            pLook = pLook->mpNextSlab;
        } while (pLook != NULL);
        K2_ASSERT(pLook != NULL);
        if (pPrev != NULL)
        {
            pPrev->mpNextSlab = pSlab->mpNextSlab;
        }
        else
        {
            gData.mpSegSlabs = pSlab->mpNextSlab;
        }

        //
        // slab page should be empty and completely unlinked now. zero it so
        // we can add its physical page back to the physical clean page list
        //
        K2MEM_Zero(pSlab, K2_VA32_MEMPAGE_BYTES);

        pCurThread = K2OSKERN_CURRENT_THREAD;

        pCurThread->mWorkMapAddr = (UINT32)pSlab;
        K2_ASSERT(pCurThread->mpWorkPage == NULL);
        K2_ASSERT(pCurThread->mpWorkPtPage == NULL);

        apCurThread->mTlbFlushNeeded = TRUE;
        apCurThread->mTlbFlushBase = (UINT32)pSlab;
        apCurThread->mTlbFlushPages = 1;

        KernMap_BreakOnePageToThread(pCurThread, &pSlab->This, KernPhysPageList_Non_KData);

        //
        // pSlab is gone here (unmapped), as is apSeg
        //
        pPhysPage = pCurThread->mpWorkPage;
        K2_ASSERT(pPhysPage != NULL);

        ok = K2OSKERN_SetIntr(FALSE);

        pPhysPage->mpOwnerObject = NULL;
        pPhysPage->mFlags &= ~K2OSKERN_PHYSTRACK_PAGE_LIST_MASK;
        pPhysPage->mFlags |= (KernPhysPageList_Thread_Clean << K2OSKERN_PHYSTRACK_PAGE_LIST_SHL);
        K2LIST_AddAtTail(&pCurThread->WorkPages_Clean, &pPhysPage->ListLink);
        pCurThread->mpWorkPage = NULL;

        pPhysPage = pCurThread->mpWorkPtPage;
        if (pPhysPage != NULL)
        {
            //
            // pagetable was unmapped too
            //
            pPhysPage->mpOwnerObject = NULL;
            pPhysPage->mFlags &= ~K2OSKERN_PHYSTRACK_PAGE_LIST_MASK;
            pPhysPage->mFlags |= (KernPhysPageList_Thread_PtClean << K2OSKERN_PHYSTRACK_PAGE_LIST_SHL);
            K2LIST_AddAtTail(&pCurThread->WorkPtPages_Clean, &pPhysPage->ListLink);
            pCurThread->mpWorkPtPage = NULL;
        }

        K2OSKERN_SetIntr(ok);

        KernMem_PhysFreeFromThread();

        if (pCurThread->mTlbFlushNeeded)
        {
            K2_ASSERT(0);
        }

        pCurThread->mWorkVirt_Range = (UINT32)pSlab;
        pCurThread->mWorkVirt_PageCount = 1;
        KernMem_VirtFreeFromThread();
    }

    ok = K2OS_CritSecLeave(&gData.SegSec);
    K2_ASSERT(ok);

    return K2STAT_NO_ERROR;
}
