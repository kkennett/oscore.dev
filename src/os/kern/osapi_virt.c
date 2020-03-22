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
    KernPhys_Disp           disp;
    K2OSKERN_OBJ_SEGMENT *  pSeg;
    K2OSKERN_OBJ_THREAD *   pCurThread;

    K2_ASSERT(apAddr != NULL);
    useAddr = *apAddr;
    K2_ASSERT((useAddr & K2_VA32_MEMPAGE_OFFSET_MASK) == 0);
    if (aPageAttrFlags & K2OS_PAGEATTR_NOCACHE)
    {
        K2_ASSERT((aPageAttrFlags & K2OS_PAGEATTR_WRITECOMBINE) == 0);
    }

    if (!(aPageAttrFlags & K2OS_VIRTALLOCFLAG_ALSO_COMMIT))
    {
        K2_ASSERT((aPageAttrFlags & ~K2OS_VIRTALLOCFLAG_TOP_DOWN) == 0);
    }

    stat = KernMem_VirtAllocToThread(useAddr, aPageCount, (aVirtAllocFlags & K2OS_VIRTALLOCFLAG_TOP_DOWN) ? TRUE : FALSE);
    if (K2STAT_IS_ERROR(stat))
    {
        K2OS_ThreadSetStatus(stat);
        return FALSE;
    }

    do {
        pSeg = K2OS_HeapAlloc(sizeof(K2OSKERN_OBJ_SEGMENT));
        if (pSeg == NULL)
            return FALSE;

        pCurThread = K2OSKERN_CURRENT_THREAD;

        K2MEM_Zero(pSeg, sizeof(K2OSKERN_OBJ_SEGMENT));
        pSeg->Hdr.mObjType = K2OS_Obj_Segment;
        pSeg->Hdr.mRefCount = 1;
        K2LIST_Init(&pSeg->Hdr.WaitingThreadsPrioList);
        pSeg->mPagesBytes = aPageCount * K2_VA32_MEMPAGE_BYTES;
        pSeg->mAccessAttr = aPageAttrFlags;
        pSeg->mSegType = KernSeg_LooseLeaf;
        pSeg->Info.LooseLeaf.mpProc = pCurThread->mpProc;
        pSeg->SegTreeNode.mUserVal = useAddr;

        if (aPageAttrFlags & K2OS_VIRTALLOCFLAG_ALSO_COMMIT)
        {
            if (aPageAttrFlags & K2OS_PAGEATTR_NOCACHE)
            {
                disp = KernPhys_Disp_Uncached;
            }
            else if (aPageAttrFlags & K2OS_PAGEATTR_WRITECOMBINE)
            {
                disp = KernPhys_Disp_Cached_WriteCombine;
            }
            else
            {
                disp = KernPhys_Disp_Cached;
            }

            stat = KernMem_PhysAllocToThread(aPageCount, disp);
            if (!K2STAT_IS_ERROR(stat))
            {
                K2_ASSERT(pCurThread->WorkPages_Dirty.mNodeCount + pCurThread->WorkPages_Clean.mNodeCount >= aPageCount);

                stat = KernMem_CreateSegmentFromThread(pSeg, NULL);

                if (K2STAT_IS_ERROR(stat))
                {
                    KernMem_PhysFreeFromThread();
                }
            }
        }
        else
        {
            K2_ASSERT(pCurThread->WorkPages_Dirty.mNodeCount == 0);
            K2_ASSERT(pCurThread->WorkPages_Clean.mNodeCount == 0);

            stat = KernMem_CreateSegmentFromThread(pSeg, NULL);
        }

        if (!K2STAT_IS_ERROR(stat))
        {
            K2OS_HeapFree(pSeg);
        }
        else
        {
            K2_ASSERT(pCurThread->WorkPages_Dirty.mNodeCount == 0);
            K2_ASSERT(pCurThread->WorkPages_Clean.mNodeCount == 0);
            K2_ASSERT(pCurThread->mWorkVirt == 0);
        }

    } while (0);

    if (K2STAT_IS_ERROR(stat))
    {
        KernMem_VirtFreeFromThread();
        K2OS_ThreadSetStatus(stat);
        return FALSE;
    }

    return TRUE;
}

BOOL K2_CALLCONV_CALLERCLEANS K2OS_VirtPagesCommit(UINT32 aPagesAddr, UINT32 aPageCount, UINT32 aPageAttrFlags)
{
    K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_IMPL);
    K2_ASSERT(0);
    return FALSE;
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
