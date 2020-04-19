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

static UINT64 volatile sgTickCounter;
static UINT32 volatile sgTicksLeft;

UINT32 sElapseQuanta(UINT64 *apMaxElapsed)
{
    //
    // current time is gData.Sched.mCurrentAbsTime.  elapse up to *apMaxElapsed
    // on all running threads.
    //
    K2LIST_LINK *           pScan;
    K2OSKERN_CPUCORE *      pCpuCore;
    K2OSKERN_OBJ_THREAD *   pRunningThread;
    BOOL                    quantumExpired;
    BOOL                    changedSomething;
    BOOL                    subChangedSomething;
    BOOL                    threadsWithNonZeroQuantumLeft;
    UINT64                  threadElapsed;
    UINT32                  actualElapsed;
    UINT32                  threadLeft;

    actualElapsed = (UINT32)(*apMaxElapsed);

    threadsWithNonZeroQuantumLeft = FALSE;

    //
    // find the actual amount of time to expire based on 
    // the remaining thread quanta for the threads that are running
    //
    pScan = gData.Sched.CpuCorePrioList.mpHead;
    K2_ASSERT(pScan != NULL);
    do {
        pCpuCore = K2_GET_CONTAINER(K2OSKERN_CPUCORE, pScan, Sched.PrioListLink);
        pScan = pScan->mpNext;

        pRunningThread = pCpuCore->Sched.mpRunThread;
        if (pRunningThread != NULL)
        {
            threadLeft = pRunningThread->Sched.mQuantumLeft;
            if (threadLeft > 0)
            {
                if (threadLeft < actualElapsed)
                    actualElapsed = threadLeft;
            }
        }
    } while (pScan != NULL);

    K2_ASSERT(actualElapsed > 0);

    //
    // update all quanta before firing events for their expiry
    //
    do {
        pCpuCore = K2_GET_CONTAINER(K2OSKERN_CPUCORE, pScan, Sched.PrioListLink);
        pScan = pScan->mpNext;

        pRunningThread = pCpuCore->Sched.mpRunThread;
        if (pRunningThread != NULL)
        {
            pRunningThread->Sched.mTotalRunTimeMs += actualElapsed;
            threadLeft = pRunningThread->Sched.mQuantumLeft;
            if (threadLeft > 0)
            {
                K2_ASSERT(actualElapsed <= threadLeft);
                pRunningThread->Sched.mQuantumLeft -= actualElapsed;
                if (actualElapsed != threadLeft)
                {
                    threadsWithNonZeroQuantumLeft = TRUE;
                }
            }
        }
    } while (pScan != NULL);

    *apMaxElapsed = (UINT64)actualElapsed;

    return threadsWithNonZeroQuantumLeft;
}

BOOL sSignalTimerItem(K2OSKERN_SCHED_TIMERITEM * apItem)
{
    BOOL changedSomething;

    changedSomething = FALSE;

    switch (apItem->mType)
    {
    case KernSchedTimerItemType_Wait:
        //
        // wait timed out
        //
        break;
    case KernSchedTimerItemType_Alarm:
        //
        // alarm expired
        //
        break;
    default:
        K2OSKERN_Panic("***Unknown timer item signalled\n");
        break;
    }

    return changedSomething;
}

BOOL KernSched_TimePassed(UINT64 aSchedAbsTime)
{
    UINT64                      elapsed;
    UINT64                      nextDelta;
    BOOL                        changedSomething;
    BOOL                        subChangedSomething;
    BOOL                        checkElapseQuanta;
    K2OSKERN_SCHED_TIMERITEM *  pItem;

    if (aSchedAbsTime == (UINT64)-1)
    {
        aSchedAbsTime = sgTickCounter;
        K2_CpuReadBarrier();
    }

    if (aSchedAbsTime <= gData.Sched.mCurrentAbsTime)
        return FALSE;

    changedSomething = FALSE;
    checkElapseQuanta = TRUE;

    elapsed = aSchedAbsTime - gData.Sched.mCurrentAbsTime;

    do {
        //
        // set nextDelta as the max amount of time to expire
        //
        if (gData.Sched.mpTimerItemQueue != NULL)
        {
            nextDelta = gData.Sched.mpTimerItemQueue->mDeltaT;
            if (nextDelta > elapsed)
            {
                nextDelta = elapsed;
            }
        }
        else
            nextDelta = elapsed;

        //
        // expire time on running threads, up to 'nextDelta' at 
        // max, but possibly less if some running thread expires 
        // its quantum 
        //
        if (checkElapseQuanta)
        {
            K2OSKERN_Debug("Check Elapsed(%d)\n", (UINT32)nextDelta);
            checkElapseQuanta = sElapseQuanta(&nextDelta);
            K2OSKERN_Debug("Act   Elapsed(%d)\n", (UINT32)nextDelta);
        }
        K2_ASSERT(nextDelta <= elapsed);

        gData.Sched.mCurrentAbsTime += nextDelta;
        elapsed -= nextDelta;

        //
        // nextDelta is the actual # of ticks that has expired
        // and have already been taken away from running thread
        // quanta. 
        //
        if (gData.Sched.mpTimerItemQueue != NULL)
        {
            do {
                if (nextDelta < gData.Sched.mpTimerItemQueue->mDeltaT)
                {
                    gData.Sched.mpTimerItemQueue->mDeltaT -= nextDelta;
                    break;
                }

                //
                // timer item at head of queue has expired
                //
                nextDelta -= gData.Sched.mpTimerItemQueue->mDeltaT;
                pItem = gData.Sched.mpTimerItemQueue;
                gData.Sched.mpTimerItemQueue = pItem->mpNext;
                pItem->mpNext = NULL;
                pItem->mOnQueue = FALSE;

                subChangedSomething = sSignalTimerItem(pItem);
                if (subChangedSomething)
                    changedSomething = TRUE;

            } while (nextDelta > 0);
        }

    } while (elapsed > 0);

    K2_ASSERT(gData.Sched.mCurrentAbsTime == aSchedAbsTime);

    return changedSomething;
}

UINT32 KernSched_SystemTickInterrupt(void *apContext)
{
    K2OSKERN_CPUCORE *                  pThisCore;
    K2OSKERN_CPUCORE_EVENT volatile *   pCoreEvent;
    UINT32                              v;

    K2_ASSERT(gData.Sched.mTokSysTickIntr == (*((K2OS_TOKEN *)apContext)));

    //
    // interrupts are off
    //
    sgTickCounter++;
    K2_CpuWriteBarrier();

    //
    // if scheduling timer expires, return TRUE and will enter scheduler, else return FALSE
    //
    do {
        v = sgTicksLeft;
        K2_CpuReadBarrier();

        if (v == 0)
            break;

        if (v == K2ATOMIC_CompareExchange(&sgTicksLeft, v - 1, v))
            break;

    } while (1);

    if (v != 1)
        return v;

    //
    // just decremented the last tick - queue the scheduling timer event
    //
    pThisCore = K2OSKERN_GET_CURRENT_CPUCORE;

    pCoreEvent = &gData.Sched.SchedTimerSchedItem.CpuCoreEvent;
    pCoreEvent->mEventType = KernCpuCoreEvent_SchedTimerFired;
    pCoreEvent->mEventAbsTimeMs = sgTickCounter;
    pCoreEvent->mSrcCoreIx = pThisCore->mCoreIx;

    KernIntr_QueueCpuCoreEvent(pThisCore, pCoreEvent);

    return 0;
}

void KernSched_StartSysTick(K2OSKERN_IRQ_CONFIG const * apConfig)
{
    K2STAT stat;

    sgTickCounter = 0;
    sgTicksLeft = 0;

    K2_CpuWriteBarrier();

    stat = K2OSKERN_InstallIntrHandler(apConfig, KernSched_SystemTickInterrupt, &gData.Sched.mTokSysTickIntr, &gData.Sched.mTokSysTickIntr);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));

    K2_ASSERT(gData.Sched.mTokSysTickIntr != NULL);
}

UINT64
K2_CALLCONV_REGS
K2OS_SysUpTimeMs(
    void
)
{
    return sgTickCounter;
}

void
KernSched_ArmSchedTimer(
    UINT32 aMsFromNow
)
{
    BOOL disp;

    //
    // interrupts should be on
    //
    disp = K2OSKERN_SetIntr(FALSE);

    K2ATOMIC_Exchange(&sgTicksLeft, aMsFromNow);

    K2OSKERN_SetIntr(disp);
}
