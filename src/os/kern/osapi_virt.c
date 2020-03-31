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

BOOL K2_CALLCONV_CALLERCLEANS K2OS_VirtPagesAlloc(UINT32 *apAddr, UINT32 aPageCount, UINT32 aVirtAllocFlags, UINT32 aPageAttrFlags)
{
    K2STAT                  stat;
    UINT32                  useAddr;
    K2OSKERN_OBJ_SEGMENT *  pSeg;
    K2OSKERN_OBJ_THREAD *   pCurThread;
    KernPhys_Disp           physDisp;

    aPageAttrFlags &= K2OS_MEMPAGE_ATTR_MASK;

    K2_ASSERT(apAddr != NULL);
    useAddr = *apAddr;

    if (useAddr == 0)
        useAddr = 0xC0000000;

    K2_ASSERT((useAddr & K2_VA32_MEMPAGE_OFFSET_MASK) == 0);

    pSeg = NULL;
    stat = KernMem_SegAlloc(&pSeg);
    if (K2STAT_IS_ERROR(stat))
    {
        K2OS_ThreadSetStatus(stat);
        return FALSE;
    }
    K2_ASSERT(pSeg != NULL);

    do {
        pCurThread = K2OSKERN_CURRENT_THREAD;

        stat = KernMem_VirtAllocToThread(pCurThread, useAddr, aPageCount, (aVirtAllocFlags & K2OS_VIRTALLOCFLAG_TOP_DOWN) ? TRUE : FALSE);
        if (K2STAT_IS_ERROR(stat))
            break;

        do {
            K2MEM_Zero(pSeg, sizeof(K2OSKERN_OBJ_SEGMENT));
            pSeg->Hdr.mObjType = K2OS_Obj_Segment;
            pSeg->Hdr.mRefCount = 1;
            K2LIST_Init(&pSeg->Hdr.WaitingThreadsPrioList);
            pSeg->SegTreeNode.mUserVal = pCurThread->mWorkVirt_Range;
            pSeg->mPagesBytes = pCurThread->mWorkVirt_PageCount * K2_VA32_MEMPAGE_BYTES;
            if (pSeg->SegTreeNode.mUserVal >= K2OS_KVA_KERN_BASE)
                aPageAttrFlags |= K2OS_MEMPAGE_ATTR_KERNEL;
            else
                aPageAttrFlags &= ~K2OS_MEMPAGE_ATTR_KERNEL;
            pSeg->mSegAndMemPageAttr = K2OS_SEG_ATTR_TYPE_USER | aPageAttrFlags;
            pSeg->Info.User.mpProc = pCurThread->mpProc;

            if (aVirtAllocFlags & K2OS_VIRTALLOCFLAG_ALSO_COMMIT)
            {
                if (aPageAttrFlags & K2OS_MEMPAGE_ATTR_UNCACHED)
                {
                    physDisp = KernPhys_Disp_Uncached;
                }
                else if ((aPageAttrFlags & (K2OS_MEMPAGE_ATTR_WRITEABLE | K2OS_MEMPAGE_ATTR_WRITE_THRU)) == (K2OS_MEMPAGE_ATTR_WRITEABLE | K2OS_MEMPAGE_ATTR_WRITE_THRU))
                {
                    physDisp = KernPhys_Disp_Cached_WriteThrough;
                }
                else
                {
                    physDisp = KernPhys_Disp_Cached;
                }

                stat = KernMem_PhysAllocToThread(pCurThread, aPageCount, physDisp, FALSE);
                if (!K2STAT_IS_ERROR(stat))
                {
                    K2_ASSERT(pCurThread->WorkPages_Dirty.mNodeCount + pCurThread->WorkPages_Clean.mNodeCount >= aPageCount);

                    stat = KernMem_CreateSegmentFromThread(pCurThread, pSeg, NULL);

                    if (K2STAT_IS_ERROR(stat))
                    {
                        KernMem_PhysFreeFromThread(pCurThread);
                    }
                }
            }
            else
            {
                K2_ASSERT(pCurThread->WorkPages_Dirty.mNodeCount == 0);
                K2_ASSERT(pCurThread->WorkPages_Clean.mNodeCount == 0);

                stat = KernMem_CreateSegmentFromThread(pCurThread, pSeg, NULL);
            }

            if (!K2STAT_IS_ERROR(stat))
            {
                *apAddr = pSeg->SegTreeNode.mUserVal;
                K2_ASSERT(pCurThread->WorkPages_Dirty.mNodeCount == 0);
                K2_ASSERT(pCurThread->WorkPages_Clean.mNodeCount == 0);
                K2_ASSERT(pCurThread->mWorkVirt_Range == 0);
                K2_ASSERT(pCurThread->mWorkVirt_PageCount == 0);
            }

        } while (0);

        if (K2STAT_IS_ERROR(stat))
        {
            KernMem_VirtFreeFromThread(pCurThread);
            K2OS_ThreadSetStatus(stat);
            return FALSE;
        }

    } while (0);

    if (K2STAT_IS_ERROR(stat))
    {
        KernMem_SegFree(pSeg);
        K2OS_ThreadSetStatus(stat);
        return FALSE;
    }

    return TRUE;
}

BOOL K2_CALLCONV_CALLERCLEANS K2OS_VirtPagesCommit(UINT32 aPagesAddr, UINT32 aPageCount, UINT32 aPageAttrFlags)
{
    K2OSKERN_OBJ_SEGMENT *  pSeg;
    K2TREE_NODE *           pTreeNode;
    K2OSKERN_OBJ_PROCESS *  pUseProc;
    K2OSKERN_OBJ_THREAD *   pCurThread;
    UINT32                  segPageCount;
    UINT32                  segOffset;
    BOOL                    disp;
    K2STAT                  stat;
    KernPhys_Disp           physDisp;

    pCurThread = K2OSKERN_CURRENT_THREAD;

    //
    // find the segment containing the address
    //
    if (aPagesAddr >= K2OS_KVA_KERN_BASE)
    {
        pUseProc = gpProc0;
    }
    else
    {
        pUseProc = pCurThread->mpProc;
    }

    disp = K2OSKERN_SeqIntrLock(&pUseProc->SegTreeSeqLock);

    pTreeNode = K2TREE_FindOrAfter(&pUseProc->SegTree, aPagesAddr);
    if (pTreeNode == NULL)
    {
        pTreeNode = K2TREE_LastNode(&pUseProc->SegTree);
        pTreeNode = K2TREE_PrevNode(&pUseProc->SegTree, pTreeNode);
        if (pTreeNode != NULL)
        {
            pSeg = K2_GET_CONTAINER(K2OSKERN_OBJ_SEGMENT, pTreeNode, SegTreeNode);
            K2_ASSERT(pSeg->SegTreeNode.mUserVal < aPagesAddr);
        }
        else
        {
            K2_ASSERT(pUseProc->SegTree.mNodeCount == 0);
            pSeg = NULL;
        }
    }
    else
    {
        if (pTreeNode->mUserVal != aPagesAddr)
        {
            pTreeNode = K2TREE_PrevNode(&pUseProc->SegTree, pTreeNode);
            if (pTreeNode != NULL)
            {
                pSeg = K2_GET_CONTAINER(K2OSKERN_OBJ_SEGMENT, pTreeNode, SegTreeNode);
                K2_ASSERT(pSeg->SegTreeNode.mUserVal < aPagesAddr);
            }
            else
            {
                pSeg = NULL;
            }
        }
        else
        {
            pSeg = K2_GET_CONTAINER(K2OSKERN_OBJ_SEGMENT, pTreeNode, SegTreeNode);
        }
    }

    if (pSeg != NULL)
    {
        //
        // seg will start at or before page range
        //
        segPageCount = pSeg->mPagesBytes / K2_VA32_MEMPAGE_BYTES;
        segOffset = (aPagesAddr - pSeg->SegTreeNode.mUserVal) / K2_VA32_MEMPAGE_BYTES;
        if (segOffset < segPageCount)
        {
            if ((segPageCount - segOffset) >= aPageCount)
            {
                KernObj_AddRef(&pSeg->Hdr);
            }
            else
            {
                pSeg = NULL;
            }
        }
        else
        {
            pSeg = NULL;
        }
    }

    K2OSKERN_SeqIntrUnlock(&pUseProc->SegTreeSeqLock, disp);

    if (pSeg == NULL)
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_FOUND);
        return FALSE;
    }

    if ((pSeg->mSegAndMemPageAttr & K2OS_SEG_ATTR_TYPE_MASK) != K2OS_SEG_ATTR_TYPE_USER)
        aPageAttrFlags = pSeg->mSegAndMemPageAttr;
    
    if (pSeg->SegTreeNode.mUserVal >= K2OS_KVA_KERN_BASE)
        aPageAttrFlags |= K2OS_MEMPAGE_ATTR_KERNEL;
    else
        aPageAttrFlags &= ~K2OS_MEMPAGE_ATTR_KERNEL;

    aPageAttrFlags &= K2OS_MEMPAGE_ATTR_MASK;

//    K2OSKERN_Debug("Commit %d pages at page offset %d within segment starting at 0x%08X that has %d pages in it.\n",
//        aPageCount, segOffset, pSeg->SegTreeNode.mUserVal, segPageCount);

    do {
        //
        // range must not already be mapped
        //
        if (!KernMap_SegRangeNotMapped(pCurThread, pSeg, segOffset, aPageCount))
        {
            stat = K2STAT_ERROR_ALREADY_MAPPED;
            break;
        }

        if (aPageAttrFlags & K2OS_MEMPAGE_ATTR_UNCACHED)
        {
            physDisp = KernPhys_Disp_Uncached;
        }
        else if ((aPageAttrFlags & (K2OS_MEMPAGE_ATTR_WRITEABLE | K2OS_MEMPAGE_ATTR_WRITE_THRU)) == (K2OS_MEMPAGE_ATTR_WRITEABLE | K2OS_MEMPAGE_ATTR_WRITE_THRU))
        {
            physDisp = KernPhys_Disp_Cached_WriteThrough;
        }
        else
        {
            physDisp = KernPhys_Disp_Cached;
        }

        stat = KernMem_PhysAllocToThread(pCurThread, aPageCount, physDisp, FALSE);
        if (K2STAT_IS_ERROR(stat))
            break;

        stat = KernMem_MapSegPagesFromThread(pCurThread, pSeg, segOffset, aPageCount, aPageAttrFlags);
        if (K2STAT_IS_ERROR(stat))
        {
            KernMem_PhysFreeFromThread(pCurThread);
        }

    } while (0);

    KernObj_Release(&pSeg->Hdr);

    if (K2STAT_IS_ERROR(stat))
    {
        K2OS_ThreadSetStatus(stat);
        return FALSE;
    }

    return TRUE;
}

BOOL K2_CALLCONV_CALLERCLEANS K2OS_VirtPagesDecommit(UINT32 aPagesAddr, UINT32 aPageCount)
{
    K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_IMPL);
    K2_ASSERT(0);
    return FALSE;
}

BOOL K2_CALLCONV_CALLERCLEANS K2OS_VirtPagesSetAttr(UINT32 aPagesAddr, UINT32 aPageCount, UINT32 aPageAttrFlags) 
{
    K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_IMPL);
    K2_ASSERT(0);
    return FALSE;
}

BOOL K2_CALLCONV_CALLERCLEANS K2OS_VirtPagesGetAttr(UINT32 aPagesAddr, UINT32 *apRetPageCount, UINT32 *apRetPagesAttrFlags)
{
    K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_IMPL);
    K2_ASSERT(0);
    return FALSE;
}

BOOL K2_CALLCONV_CALLERCLEANS K2OS_VirtPagesSetLock(UINT32 aPagesAddr, UINT32 aPageCount, BOOL aSetLocked)
{
    K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_IMPL);
    K2_ASSERT(0);
    return FALSE;
}

BOOL K2_CALLCONV_CALLERCLEANS K2OS_VirtPagesFree(UINT32 aPagesAllocAddr)
{
    K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_IMPL);
    K2_ASSERT(0);
    return FALSE;
}

