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

BOOL KernSched_Exec_PurgePT(void)
{
    UINT32 *                pPtPageCount;
    UINT32 *                pPDE;
    BOOL                    disp;
    UINT32                  ptIndex;
    UINT32                  virtPtAddr;
    UINT32                  physPtAddr;
    UINT32                  virtAddrInPtRange;
    K2OSKERN_OBJ_PROCESS *  pUseProc;

    pPtPageCount = (UINT32 *)K2OS_KVA_PTPAGECOUNT_BASE;

    ptIndex = gData.Sched.mpActiveItemThread->Sched.Item.Args.PurgePt.mPtIndex;

    virtAddrInPtRange = ptIndex * K2_VA32_PAGETABLE_MAP_BYTES;

    if ((ptIndex >= K2_VA32_PAGEFRAMES_FOR_2G))
    {
        //
        // pagetable maps somewhere in kernel space
        //
        pUseProc = gpProc0;
    }
    else
    {
        //
        // pagetable maps somewhere in user space
        //
        pUseProc = gData.Sched.mpActiveItemThread->mpProc;
    }

#if K2_TARGET_ARCH_IS_ARM
    pPDE = (((UINT32 *)pUseProc->mTransTableKVA) + ((ptIndex & 0x3FF) * 4));
#else
    pPDE = (((UINT32 *)pUseProc->mTransTableKVA) + (ptIndex & 0x3FF));
#endif

    virtPtAddr = K2_VA32_TO_PT_ADDR(pUseProc->mVirtMapKVA, virtAddrInPtRange);

    K2OSKERN_Debug("SchedExec:PurgePT index %d\n", ptIndex);
    K2OSKERN_Debug("PageTable Virtual Address = %08X\n", virtPtAddr);
    K2OSKERN_Debug("VirtAddr in Pt range = %08X\n", virtAddrInPtRange);

    disp = K2OSKERN_SeqIntrLock(&gData.KernVirtMapLock);

    do {
        if (pPtPageCount[ptIndex] != 0)
        {
            gData.Sched.mpActiveItemThread->Sched.Item.mResult = K2STAT_ERROR_IN_USE;
            break;
        }

        gData.Sched.mpActiveItemThread->Sched.Item.mResult = K2STAT_NO_ERROR;

        physPtAddr = KernMap_BreakOnePage(pUseProc->mVirtMapKVA, virtPtAddr);

        gData.Sched.mpActiveItemThread->Sched.Item.Args.PurgePt.mPtPhysOut = physPtAddr;

        K2_ASSERT(((*pPDE) & K2_VA32_PAGEFRAME_MASK) == physPtAddr);

        *pPDE = 0;
#if K2_TARGET_ARCH_IS_ARM
        pPDE[1] = 0;
        pPDE[2] = 0;
        pPDE[3] = 0;
#endif

    } while (0);

    K2OSKERN_SeqIntrUnlock(&gData.KernVirtMapLock, disp);

    //
    // invalidate virtPt for 1 page across all cores
    // invalidate virtAddrInPtRange for 1 page across all cores, which should clear
    //      out the tlb for all pages in that pagetable range
    //
    gData.Sched.mpTlbInvProc = pUseProc;
    gData.Sched.mTlbInv1_Base = virtPtAddr;
    gData.Sched.mTlbInv1_PageCount = 1;
    gData.Sched.mTlbInv2_Base = virtAddrInPtRange;
    gData.Sched.mTlbInv2_PageCount = 1;

    KernSched_TlbInvalidateAcrossCores();

    return FALSE;  // if something changes scheduling-wise, return true
}

