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

BOOL sElapseQuanta(UINT64 *apMaxElapsed, BOOL *apRetSomeNowZero)
{
    //
    // current time is gData.Sched.mCurrentAbsTime.  elapse up to *apMaxElapsed
    // on all running threads.
    //
    K2LIST_LINK *           pScan;
    K2OSKERN_CPUCORE *      pCpuCore;
    K2OSKERN_OBJ_THREAD *   pRunningThread;
    UINT32                  actualElapsed;
    UINT32                  threadLeft;
    BOOL                    allHitZero;

    actualElapsed = (UINT32)(*apMaxElapsed);
    K2_ASSERT(actualElapsed > 0);

    allHitZero = TRUE;
    *apRetSomeNowZero = FALSE;

    //
    // find the actual amount of time to expire based on 
    // the remaining thread quanta for the threads that are running
    //
    pScan = gData.Sched.CpuCorePrioList.mpHead;
    K2_ASSERT(pScan != NULL);
    do {
        pCpuCore = K2_GET_CONTAINER(K2OSKERN_CPUCORE, pScan, Sched.CpuCoreListLink);
        pScan = pScan->mpNext;

        //
        // cores are in priority order.  as soon as we hit a core not running anything
        // then we are done, we won't hit any other ones
        //
        pRunningThread = pCpuCore->Sched.mpRunThread;
        if (pRunningThread == NULL)
            break;

        if (0 == (pCpuCore->Sched.mExecFlags & K2OSKERN_SCHED_CPUCORE_EXECFLAG_CHANGED))
        {
            allHitZero = FALSE;
            //
            // consider this threads remaining quantum
            //
            threadLeft = pRunningThread->Sched.mQuantumLeft;
            K2_ASSERT(threadLeft > 0);
            if (threadLeft < actualElapsed)
                actualElapsed = threadLeft;
        }
    } while (pScan != NULL);

    K2_ASSERT(actualElapsed > 0);
    *apMaxElapsed = (UINT64)actualElapsed;

    if (allHitZero)
        return TRUE;

    //
    // there is at least one core running a thread with a nonzero quantum
    //
    pScan = gData.Sched.CpuCorePrioList.mpHead;
    do {
        pCpuCore = K2_GET_CONTAINER(K2OSKERN_CPUCORE, pScan, Sched.CpuCoreListLink);
        pScan = pScan->mpNext;

        //
        // cores are in priority order.  as soon as we hit a core not running anything
        // then we are done, we won't hit any other ones
        //
        pRunningThread = pCpuCore->Sched.mpRunThread;
        if (pRunningThread == NULL)
            break;

        if (0 == (pCpuCore->Sched.mExecFlags & K2OSKERN_SCHED_CPUCORE_EXECFLAG_CHANGED))
        {
            pRunningThread->Sched.mTotalRunTimeMs += actualElapsed;
            threadLeft = pRunningThread->Sched.mQuantumLeft;
            K2_ASSERT(actualElapsed <= threadLeft);
            pRunningThread->Sched.mQuantumLeft -= actualElapsed;
            if (threadLeft == actualElapsed)
            {
                pCpuCore->Sched.mExecFlags = K2OSKERN_SCHED_CPUCORE_EXECFLAG_QUANTUM_ZERO;
                *apRetSomeNowZero = TRUE;
            }
        }
    } while (pScan != NULL);

    return FALSE;
}

BOOL sReschedQuantaNowZero(void)
{
    K2LIST_LINK *           pScan;
    K2OSKERN_CPUCORE *      pCpuCore;
    K2OSKERN_OBJ_THREAD *   pRunningThread;
    BOOL                    changedSomething;

    changedSomething = FALSE;

    pScan = gData.Sched.CpuCorePrioList.mpHead;
    K2_ASSERT(pScan != NULL);
    do {
        pCpuCore = K2_GET_CONTAINER(K2OSKERN_CPUCORE, pScan, Sched.CpuCoreListLink);
        pScan = pScan->mpNext;

        pRunningThread = pCpuCore->Sched.mpRunThread;

        if (pRunningThread == NULL)
            break;

        if (pCpuCore->Sched.mExecFlags & K2OSKERN_SCHED_CPUCORE_EXECFLAG_QUANTUM_ZERO)
        {
            K2_ASSERT(pRunningThread->Sched.mQuantumLeft == 0);

            if (KernSched_RunningThreadQuantumExpired(pCpuCore, pRunningThread))
                changedSomething = TRUE;
        }

    } while (pScan != NULL);

    return changedSomething;
}

BOOL sSignalTimerItem(K2OSKERN_SCHED_TIMERITEM * apItem)
{
    BOOL    changedSomething;

    changedSomething = FALSE;

    switch (apItem->mType)
    {
    case KernSchedTimerItemType_Wait:
        KernSched_EndThreadWait(
            K2_GET_CONTAINER(K2OSKERN_SCHED_MACROWAIT, apItem, SchedTimerItem),
            K2STAT_ERROR_TIMEOUT);
        changedSomething = TRUE;
        break;
    case KernSchedTimerItemType_Alarm:
        //
        // alarm expired
        //
        K2_ASSERT(0);
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
    UINT64                      timerQueueItemTicks;
    BOOL                        changedSomething;
    BOOL                        allHitZero;
    BOOL                        someHitZero;
    K2OSKERN_SCHED_TIMERITEM *  pItem;

    if (aSchedAbsTime == (UINT64)-1)
    {
        aSchedAbsTime = sgTickCounter;
        K2_CpuReadBarrier();
    }

    if (aSchedAbsTime <= gData.Sched.mCurrentAbsTime)
        return FALSE;

    changedSomething = FALSE;

    allHitZero = FALSE;

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

        if (!allHitZero)
        {
            allHitZero = sElapseQuanta(&nextDelta, &someHitZero);
        }

        if (gData.Sched.mpTimerItemQueue != NULL)
        {
            do {
                timerQueueItemTicks = gData.Sched.mpTimerItemQueue->mDeltaT;
                if (nextDelta < timerQueueItemTicks)
                {
                    gData.Sched.mpTimerItemQueue->mDeltaT = timerQueueItemTicks - nextDelta;
                    gData.Sched.mCurrentAbsTime += nextDelta;
                    elapsed -= nextDelta;
                    break;
                }

                //
                // timer item at head of queue has expired
                //
                nextDelta -= timerQueueItemTicks;
                gData.Sched.mCurrentAbsTime += timerQueueItemTicks;
                elapsed -= timerQueueItemTicks;

                pItem = gData.Sched.mpTimerItemQueue;
                gData.Sched.mpTimerItemQueue = pItem->mpNext;
                pItem->mpNext = NULL;
                pItem->mOnQueue = FALSE;

                if (sSignalTimerItem(pItem))
                    changedSomething = TRUE;

            } while (nextDelta > 0);
        }
        else
        {
            gData.Sched.mCurrentAbsTime += nextDelta;
            elapsed -= nextDelta;
        }

        if (someHitZero)
        {
            if (sReschedQuantaNowZero())
                changedSomething = TRUE;
        }

    } while (elapsed > 0);

    K2_ASSERT(gData.Sched.mCurrentAbsTime == aSchedAbsTime);

    return changedSomething;
}

BOOL KernSched_AddTimerItem(K2OSKERN_SCHED_TIMERITEM *apItem, UINT64 aAbsWaitStartTime, UINT64 aWaitMs)
{
    //
    // if the timeritem is already expired, then we need to signal it immediately and
    // return TRUE.
    //
    K2OSKERN_SCHED_TIMERITEM *  pPrev;
    K2OSKERN_SCHED_TIMERITEM *  pLook;
    UINT64                      lostTime;

    if (aAbsWaitStartTime < gData.Sched.mCurrentAbsTime)
    {
        lostTime = gData.Sched.mCurrentAbsTime - aAbsWaitStartTime;
        if (lostTime >= aWaitMs)
        {
            return sSignalTimerItem(apItem);
        }

        aWaitMs -= lostTime;
    }
    else
    {
        aWaitMs += (aAbsWaitStartTime - gData.Sched.mCurrentAbsTime);
    }

    //
    // insert the iterm into the timer queue
    //
    apItem->mDeltaT = aWaitMs;
    apItem->mpNext = NULL;

    pPrev = NULL;
    pLook = gData.Sched.mpTimerItemQueue;
    if (pLook != NULL)
    {
        do {
            if (pLook->mDeltaT >= apItem->mDeltaT)
                break;
            apItem->mDeltaT -= pLook->mDeltaT;
            pPrev = pLook;
            pLook = pLook->mpNext;
        } while (pLook);
    }
    apItem->mpNext = pLook;
    if (pLook != NULL)
        pLook->mDeltaT -= apItem->mDeltaT;
    if (pPrev != NULL)
        pPrev->mpNext = apItem;
    else
        gData.Sched.mpTimerItemQueue = apItem;

    apItem->mOnQueue = TRUE;

    return FALSE;
}

void KernSched_DelTimerItem(K2OSKERN_SCHED_TIMERITEM *apItem)
{
    K2OSKERN_SCHED_TIMERITEM * pLook;
    K2OSKERN_SCHED_TIMERITEM * pPrev;
    K2OSKERN_SCHED_TIMERITEM * pNext;

    K2_ASSERT(apItem->mOnQueue);

    pPrev = NULL;
    pLook = gData.Sched.mpTimerItemQueue;
    do {
        if (pLook == apItem)
            break;
        pPrev = pLook;
        pLook = pLook->mpNext;
    } while (pLook);
    K2_ASSERT(pLook != NULL);

    pNext = apItem->mpNext;
    if (pNext != NULL)
        pNext->mDeltaT += apItem->mDeltaT;
    if (pPrev != NULL)
        pPrev->mpNext = pNext;
    else
        gData.Sched.mpTimerItemQueue = pNext;

    apItem->mOnQueue = FALSE;
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
