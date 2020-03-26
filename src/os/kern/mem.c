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


#if 0
#define K2OSKERN_VMNODE_TYPE_SEGMENT    0       // MUST BE TYPE ZERO
#define K2OSKERN_VMNODE_TYPE_RESERVED   1
#define K2OSKERN_VMNODE_TYPE_UNKNOWN    2
#define K2OSKERN_VMNODE_TYPE_SPARE      3

#define K2OSKERN_VMNODE_RESERVED_ERROR          0
#define K2OSKERN_VMNODE_RESERVED_PUBLICAPI      1
#define K2OSKERN_VMNODE_RESERVED_ARCHSPEC       2
#define K2OSKERN_VMNODE_RESERVED_COREPAGES      3
#define K2OSKERN_VMNODE_RESERVED_VAMAP          4
#define K2OSKERN_VMNODE_RESERVED_PHYSTRACK      5
#define K2OSKERN_VMNODE_RESERVED_TRANSTAB       6
#define K2OSKERN_VMNODE_RESERVED_TRANSTAB_HOLE  7
#define K2OSKERN_VMNODE_RESERVED_EFIMAP         8
#define K2OSKERN_VMNODE_RESERVED_PTPAGECOUNT    9
#define K2OSKERN_VMNODE_RESERVED_LOADER         10
#define K2OSKERN_VMNODE_RESERVED_DLX_CRT        11
#define K2OSKERN_VMNODE_RESERVED_DLX_KERN       12
#define K2OSKERN_VMNODE_RESERVED_DLX_HAL        13
#define K2OSKERN_VMNODE_RESERVED_EFI_RUN_TEXT   14
#define K2OSKERN_VMNODE_RESERVED_EFI_RUN_DATA   15
#define K2OSKERN_VMNODE_RESERVED_DEBUG          16
#define K2OSKERN_VMNODE_RESERVED_FW_TABLES      17
#define K2OSKERN_VMNODE_RESERVED_UNKNOWN        18
#define K2OSKERN_VMNODE_RESERVED_EFI_MAPPED_IO  19
#define K2OSKERN_VMNODE_RESERVED_DLX_ACPI       20
#define K2OSKERN_VMNODE_RESERVED_CORECLEARPAGES 21

typedef struct _K2OSKERN_VMNODE_NONSEG K2OSKERN_VMNODE_NONSEG;
struct _K2OSKERN_VMNODE_NONSEG
{
    UINT32  mType : 2;      // 0 means segment and the mpSegment pointer will be valid
    UINT32  mNodeInfo : 30;     // depends on value of mNodeIsNotSegment;
};
K2_STATIC_ASSERT(sizeof(K2OSKERN_VMNODE_NONSEG) == sizeof(UINT32));

typedef struct _K2OSKERN_VMNODE K2OSKERN_VMNODE;
struct _K2OSKERN_VMNODE
{
    K2HEAP_NODE     HeapNode;
    union {
        K2OSKERN_OBJ_SEGMENT *  mpSegment;
        K2OSKERN_VMNODE_NONSEG  NonSeg;
    };
};
#endif

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

static void sDumpAll(void)
{
    K2TREE_NODE *               pTreeNode;
    UINT32                      flags;
    UINT32                      pageCount;
    BOOL                        intrDisp;
    K2OSKERN_OBJ_SEGMENT *      pSeg;
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

    K2OSKERN_Debug("\nVirtual Memory:\n");

    intrDisp = K2OS_CritSecEnter(&gData.KernVirtHeapSec);
    K2_ASSERT(intrDisp);

    K2HEAP_Dump(&gData.KernVirtHeap, sDumpHeapNode);

    intrDisp = K2OS_CritSecLeave(&gData.KernVirtHeapSec);
    K2_ASSERT(intrDisp);


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

void KernMem_Start(void)
{
    K2STAT                      stat;
    K2OSKERN_OBJ_SEGMENT        tempSeg;
    K2OSKERN_OBJ_THREAD *       pCurThread;
    K2OSKERN_HEAPTRACKPAGE *    pHeapTrackPage;

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

    pCurThread = K2OSKERN_CURRENT_THREAD;

    //
    // create the first heap tracking segment
    //

    stat = KernMem_VirtAllocToThread(0, 1, FALSE);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));
    K2_ASSERT(pCurThread->mWorkVirt_Range != 0);

    stat = KernMem_PhysAllocToThread(1, KernPhys_Disp_Cached, FALSE);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));
    K2_ASSERT(pCurThread->WorkPages_Dirty.mNodeCount + pCurThread->WorkPages_Clean.mNodeCount > 0);

    K2MEM_Zero(&tempSeg, sizeof(tempSeg));
    tempSeg.Hdr.mObjType = K2OS_Obj_Segment;
    tempSeg.Hdr.mRefCount = 1;
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
    K2OSKERN_OBJ_THREAD * pCurThread;

    pCurThread = K2OSKERN_CURRENT_THREAD;

    K2_ASSERT(pCurThread->mWorkVirt_Range == 0);
    K2_ASSERT(pCurThread->mWorkVirt_PageCount == 0);

    if (aUseAddr != 0)
    {
        K2_ASSERT((aUseAddr & K2_VA32_MEMPAGE_OFFSET_MASK) == 0);
    }

    ok = K2OS_CritSecEnter(&gData.KernVirtHeapSec);
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
                pPhysPage->mFlags |= ((KernPhysPageList_Free_Dirty << K2OSKERN_PHYSTRACK_PAGE_LIST_SHL) | K2OSKERN_PHYSTRACK_FREE_FLAG);
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
                pPhysPage->mFlags |= ((KernPhysPageList_Free_Clean << K2OSKERN_PHYSTRACK_PAGE_LIST_SHL) | K2OSKERN_PHYSTRACK_FREE_FLAG);
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
                break;
        }

        pTreeNode = pNextNode;

    } while (pTreeNode != NULL);

    K2OSKERN_SeqIntrUnlock(&gData.PhysMemSeqLock, intrDisp);

    return K2STAT_NO_ERROR;
}

void KernMem_PhysFreeFromThread(void)
{

}

K2STAT KernMem_CreateSegmentFromThread(K2OSKERN_OBJ_SEGMENT *apSrc, K2OSKERN_OBJ_SEGMENT *apDst)
{




    return K2STAT_ERROR_NOT_IMPL;
}
