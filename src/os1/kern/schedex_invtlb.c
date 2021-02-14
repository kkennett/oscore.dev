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

void KernSched_PerCpuTlbInvEvent(K2OSKERN_CPUCORE volatile *apThisCore)
{
    UINT32 left;
    UINT32 virtAddr;

    //
    // called from monitor on the core in response to Ici
    //
//    K2OSKERN_Debug("PerCpuTlbInv(%d)\n", apThisCore->mCoreIx);

    if ((gData.Sched.mTlbInv1_Base >= K2OS_KVA_KERN_BASE) ||
        (apThisCore->mpActiveProc == gData.Sched.mpTlbInvProc))
    {
        left = gData.Sched.mTlbInv1_PageCount;
        virtAddr = gData.Sched.mTlbInv1_Base;
        do {
//            K2OSKERN_Debug("Invalidate Tlb Cpu %d Virt 0x%08X\n", apThisCore->mCoreIx, virtAddr);
            KernArch_InvalidateTlbPageOnThisCore(virtAddr);
            virtAddr += K2_VA32_MEMPAGE_BYTES;
        } while (--left);
    }

    if (gData.Sched.mTlbInv2_PageCount > 0)
    {
        if ((gData.Sched.mTlbInv1_Base >= K2OS_KVA_KERN_BASE) ||
            (apThisCore->mpActiveProc == gData.Sched.mpTlbInvProc))
        {
            left = gData.Sched.mTlbInv2_PageCount;
            virtAddr = gData.Sched.mTlbInv2_Base;
            do {
//                K2OSKERN_Debug("Invalidate Tlb Cpu %d Virt 0x%08X\n", apThisCore->mCoreIx, virtAddr);
                KernArch_InvalidateTlbPageOnThisCore(virtAddr);
                virtAddr += K2_VA32_MEMPAGE_BYTES;
            } while (--left);
        }
    }

    K2ATOMIC_Inc(&gData.Sched.mTlbCores);
}

void KernSched_TlbInvalidateAcrossCores(void)
{
    K2_ASSERT(gData.Sched.mTlbInv1_PageCount > 0);

    gData.Sched.mTlbCores = 0;
    
    K2_CpuWriteBarrier();

    KernCpuCore_SendIciToAllOtherCores(gData.Sched.mpSchedulingCore, KernCpuCoreEvent_Ici_TlbInv);

    KernSched_PerCpuTlbInvEvent(gData.Sched.mpSchedulingCore);

    do {
        K2_CpuReadBarrier();
    } while (gData.Sched.mTlbCores < gData.mCpuCount);

    gData.Sched.mpTlbInvProc = NULL;
    gData.Sched.mTlbInv1_Base = 0;
    gData.Sched.mTlbInv1_PageCount = 0;
    gData.Sched.mTlbInv2_Base = 0;
    gData.Sched.mTlbInv2_PageCount = 0;
}

BOOL KernSched_Exec_InvalidateTlb(void)
{
    gData.Sched.mpTlbInvProc = gData.Sched.mpActiveItem->Args.InvalidateTlb.mpProc;
    gData.Sched.mTlbInv1_Base = gData.Sched.mpActiveItem->Args.InvalidateTlb.mVirtAddr;
    gData.Sched.mTlbInv1_PageCount = gData.Sched.mpActiveItem->Args.InvalidateTlb.mPageCount;
    gData.Sched.mTlbInv2_PageCount = 0;

    KernSched_TlbInvalidateAcrossCores();

    return FALSE;
}
