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

void 
KernCpu_Init(
    void
)
{
    UINT32                      coreIx;
    K2OSKERN_CPUCORE volatile * pCore;

    K2_ASSERT(gData.LoadInfo.mCpuCoreCount > 0);
    K2_ASSERT(gData.LoadInfo.mCpuCoreCount <= K2OS_MAX_CPU_COUNT);

    for (coreIx = 0; coreIx < gData.LoadInfo.mCpuCoreCount; coreIx++)
    {
        pCore = K2OSKERN_COREIX_TO_CPUCORE(coreIx);
        
        K2MEM_Zero((void *)pCore, sizeof(K2OSKERN_CPUCORE));
        
        pCore->mCoreIx = coreIx;

        K2LIST_Init((K2LIST_ANCHOR *)&pCore->RunList);

        K2LIST_Init((K2LIST_ANCHOR *)&pCore->IciOutList);
    }

    K2_CpuWriteBarrier();
}

static UINT32
sTryToSendIcis(
    K2OSKERN_CPUCORE volatile * apThisCore,
    UINT32                      aTargetCpuMask,
    KernIciCode                 aCodeToSend
)
{
    UINT32  coreIx;
    UINT32  workMask;
    UINT32  doneMask;
    UINT32  workBit;
    UINT32 volatile *pTarget;
    K2OSKERN_CPUCORE volatile * pOtherCore;

    doneMask = 0;
    workMask = aTargetCpuMask;
    workBit = 1;
    coreIx = 0;
    do
    {
        if (workMask & workBit)
        {
            pOtherCore = K2OSKERN_COREIX_TO_CPUCORE(coreIx);
            pTarget = &pOtherCore->mIciFromOtherCore[apThisCore->mCoreIx];
            // this is my slot on the other core, nobody else can set this but me
            // the other core will clear it when it has processed the ici
            if (0 == *pTarget)
            {
                *pTarget = (UINT32)aCodeToSend;
                K2_CpuWriteBarrier();
                doneMask |= workBit;
            }
            workMask &= ~workBit;
        }
        ++coreIx;
        workBit <<= 1;
    } while (workMask != 0);

    if (0 != doneMask)
    {
        //
        // issue the ICI(s) to wake up the other core(s) now
        //
        KernArch_SendIci(apThisCore, doneMask);
    }

    return doneMask;
}

void
KernCpu_ProcessOneIci(
    K2OSKERN_CPUCORE volatile * apThisCore,
    UINT32                      aSrcCore,
    UINT32                      aCode
)
{
    //
    // apThisCore->mIciFromOtherCore[aSrcCore] already set to KernIciCode_None
    //

    K2_ASSERT(0);
}

BOOL
KernCpu_ProcessIcis(
    K2OSKERN_CPUCORE volatile * apThisCore
)
{
    UINT32              otherCoreIx;
    UINT32              iciCount;
    KernIciCode         iciCode;
    K2LIST_LINK *       pListLink;
    K2OSKERN_OUT_ICI *  pIciOut;
    UINT32              maskDone;

    if (1 == gData.LoadInfo.mCpuCoreCount)
        return FALSE;

    iciCount = 0;

    //
    // process incoming ICIs
    //
    for (otherCoreIx = 0; otherCoreIx < gData.LoadInfo.mCpuCoreCount; otherCoreIx++)
    {
        iciCode = (KernIciCode)apThisCore->mIciFromOtherCore[otherCoreIx];
        if (KernIciCode_None != iciCode)
        {
            apThisCore->mIciFromOtherCore[otherCoreIx] = KernIciCode_None;
            K2_CpuWriteBarrier();
            KernCpu_ProcessOneIci(apThisCore, otherCoreIx, iciCode);
            iciCount++;
        }
    }

    // 
    // try to process outgoing ICIs (may be backed up)
    //
    pListLink = apThisCore->IciOutList.mpHead;
    if (NULL != pListLink)
    {
        ++iciCount;

        do
        {
            pIciOut = K2_GET_CONTAINER(K2OSKERN_OUT_ICI, pListLink, ListLink);
            pListLink = pListLink->mpNext;

            //
            // this returns the mask of the cpus we successfully sent the ici to
            //
            maskDone = sTryToSendIcis(apThisCore, pIciOut->mTargetCoreMask, pIciOut->mCode);

            if (maskDone == pIciOut->mTargetCoreMask)
            {
                //
                // all done with this one
                //
                K2LIST_Remove((K2LIST_ANCHOR *)&apThisCore->IciOutList, &pIciOut->ListLink);
                pIciOut->mTargetCoreMask = 0;
                K2_CpuWriteBarrier();
            }
            else
            {
                //
                // did not finish this one. will go around again
                //
                if (0 != maskDone)
                    pIciOut->mTargetCoreMask &= ~maskDone;
            }

        } while (NULL != pListLink);
    }

    //
    // returning TRUE will make Exec call this again
    //
    return (iciCount > 0) ? TRUE : FALSE;
}

void
KernCpu_LatchIciToSend(
    K2OSKERN_CPUCORE volatile * apThisCore,
    K2OSKERN_OUT_ICI *          apIciOut
)
{
    K2_ASSERT(0 != apIciOut->mTargetCoreMask);
    K2_ASSERT(0 != apIciOut->mCode);
    K2LIST_AddAtTail((K2LIST_ANCHOR *)&apThisCore->IciOutList, &apIciOut->ListLink);
}

void
KernCpu_Reschedule(
    K2OSKERN_CPUCORE volatile * apThisCore
)
{
    K2_ASSERT(0);
}

void __attribute__((noreturn)) 
KernCpu_Exec(
    K2OSKERN_CPUCORE volatile * apThisCore,
    KernCpuExecReason           aReason
)
{
    K2OSKERN_OBJ_THREAD *   pNextThread;
    BOOL                    wasIdle;
    UINT32                  activity;

    wasIdle = apThisCore->mIsIdle;
    apThisCore->mIsIdle = FALSE;

    if (wasIdle)
    {
        K2OSKERN_Debug("Core %d exit idle\n", apThisCore->mCoreIx);
    }

    if ((aReason == KernCpuExecReason_Exception) ||
        (aReason == KernCpuExecReason_SystemCall))
    {
        if (aReason == KernCpuExecReason_Exception)
            KernThread_Exception(apThisCore);
        else
            KernThread_SystemCall(apThisCore);
    }

    // 
    // run next thread or go idle
    //
    do
    {
        do
        {
            activity = 0;
            if (KernCpu_ProcessIcis(apThisCore))
                ++activity;
            if (KernArch_PollIrq(apThisCore))
                ++activity;
        } while (0 != activity);

        //
        // resume next activity
        //
        pNextThread = K2_GET_CONTAINER(K2OSKERN_OBJ_THREAD, apThisCore->RunList.mpHead, CpuRunListLink);
        K2_ASSERT(KernThreadState_OnCoreRunList == pNextThread->mState);
        if (pNextThread == &apThisCore->IdleThread)
        {
            if (1 == apThisCore->RunList.mNodeCount)
            {
                //
                // next thread is idle thread and there is only one thread on the list
                // so this core needs to go idle
                //
                apThisCore->mIsIdle = TRUE;
                K2OSKERN_Debug("Core %d enter idle\n", apThisCore->mCoreIx);
                K2_CpuWriteBarrier();
                KernArch_CpuIdle(apThisCore);
                K2OSKERN_Panic("KernArch_CpuIdle() returned\n");
            }
            else
            {
                //
                // reschedule here, move idle thread to end of run list
                //
                KernCpu_Reschedule(apThisCore);
            }
        }
        else
        {
            KernArch_ResumeThread(apThisCore);
            K2OSKERN_Panic("KernArch_ResumeThread() returned\n");
        }

    } while (1);

    //
    // should never get here
    //
    K2OSKERN_Panic("CPU exec broke\n");
}

void
KernCpu_MigrateThread(
    UINT32                  aTargetCoreIx,
    K2OSKERN_OBJ_THREAD *   apThread
)
{
    K2_ASSERT(0);
#if 0
    SKCpu *pTarget;
    SKCpu *pThisCpu = GetCurrentCpu();

    if (((DWORD)-1) == aTargetCpu)
    {
        //
        // choose target cpu
        //
        aTargetCpu = ((DWORD)rand()) % mCpuCount;
    }
    pTarget = &mpCpus[aTargetCpu];

    K2_ASSERT(NULL == apThread->mpCurrentCpu);
    K2_ASSERT(apThread->mState == SKThreadState_Migrating);

    if (pThisCpu == pTarget)
    {
        // adding this at the end will place it after the idle thread, which guarantees a reschedule calc
        // before it executes
        apThread->mState = SKThreadState_OnRunList;
        apThread->mpCurrentCpu = pThisCpu;
        K2LIST_AddAtTail(&pThisCpu->RunningThreadList, &apThread->CpuThreadListLink);
    }
    else
    {
        SKThread * pLast;
        SKThread * pOld;
        do
        {
            pLast = pTarget->mpMigratedHead;
            K2_CpuWriteBarrier();
            apThread->mpCpuMigratedNext = pLast;
            K2_CpuReadBarrier();
            pOld = (SKThread *)InterlockedCompareExchangePointer((PVOID volatile *)&pTarget->mpMigratedHead, apThread, (PVOID)pLast);
        } while (pOld != pLast);

        pThisCpu->NB_SendIci(pTarget->mCpuIndex, SKICI_CODE_MIGRATED_THREAD);
    }
#endif
}

void
KernCpu_SetNextThread(
    K2OSKERN_CPUCORE volatile * apThisCore
)
{
    K2_ASSERT(0);
#if 0
    SKThread *pThread = K2_GET_CONTAINER(SKThread, RunningThreadList.mpHead, CpuThreadListLink);
    K2_ASSERT(pThread != mpCurrentThread);
    K2_ASSERT(pThread->mState == SKThreadState_OnRunList);
    K2_ASSERT(pThread->mpCurrentCpu == this);
    if (pThread == mpIdleThread)
        mSchedTimeout.QuadPart = 0;
    else
        mpSystem->MsToCpuTime(pThread->mQuantum, &mSchedTimeout);
    mpCurrentThread = pThread;
    mThreadChanged = TRUE;
#endif
}

