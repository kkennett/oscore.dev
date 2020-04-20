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

static
const
KernSched_pf_Handler
sgSchedHandlers[KernSchedItemType_Count] =
{
    NULL,                               // KernSchedItem_Invalid
    NULL,                               // KernSchedItem_SchedTimer - not done through pointer call
    KernSched_Exec_EnterDebug,          // KernSchedItem_EnterDebug
    KernSched_Exec_ThreadExit,          // KernSchedItem_ThreadExit
    KernSched_Exec_ThreadWaitAny,       // KernSchedItem_ThreadWaitAny
    KernSched_Exec_Contended_CritSec,   // KernSchedItem_Contended_Critsec
    KernSched_Exec_Destroy_CritSec,     // KernSchedItem_Destroy_Critsec
    KernSched_Exec_PurgePT,             // KernSchedItem_PurgePT
    KernSched_Exec_InvalidateTlb,       // KernSchedItem_InvalidateTlb
    KernSched_Exec_SemRelease,          // KernSchedItem_SemRelease
    KernSched_Exec_ThreadCreate         // KernSchedItem_ThreadCreate

};

static K2OSKERN_SCHED_ITEM * sgpItemList;
static K2OSKERN_SCHED_ITEM * sgpItemListEnd;

void KernSched_InsertCore(K2OSKERN_CPUCORE *apCore, BOOL aInsertAtTailOfSamePrio)
{
    K2LIST_LINK *       pFind;
    K2OSKERN_CPUCORE *  pOther;

    pFind = gData.Sched.CpuCorePrioList.mpHead;
    if (pFind == NULL)
    {
        K2LIST_AddAtHead(&gData.Sched.CpuCorePrioList, &apCore->Sched.CpuCoreListLink);

        return;
    }

    if (aInsertAtTailOfSamePrio)
    {
        do {
            pOther = K2_GET_CONTAINER(K2OSKERN_CPUCORE, pFind, Sched.CpuCoreListLink);
            if (apCore->Sched.mActivePrio <= pOther->Sched.mActivePrio)
            {
                K2LIST_AddBefore(&gData.Sched.CpuCorePrioList, &apCore->Sched.CpuCoreListLink, pFind);
                return;
            }
            pFind = pFind->mpNext;
        } while (pFind != NULL);

        K2LIST_AddAtTail(&gData.Sched.CpuCorePrioList, &apCore->Sched.CpuCoreListLink);

        return;
    }

    do {
        pOther = K2_GET_CONTAINER(K2OSKERN_CPUCORE, pFind, Sched.CpuCoreListLink);
        if (apCore->Sched.mActivePrio < pOther->Sched.mActivePrio)
        {
            K2LIST_AddBefore(&gData.Sched.CpuCorePrioList, &apCore->Sched.CpuCoreListLink, pFind);
            return;
        }
        pFind = pFind->mpNext;
    } while (pFind != NULL);

    K2LIST_AddAtTail(&gData.Sched.CpuCorePrioList, &apCore->Sched.CpuCoreListLink);
}

void KernSched_AddCurrentCore(void)
{
    UINT32                  coreIndex;
    K2OSKERN_CPUCORE *      pThisCore;
    K2OSKERN_OBJ_THREAD *   pThread;

    coreIndex = K2OSKERN_GetCpuIndex();

    pThisCore = K2OSKERN_COREIX_TO_CPUCORE(coreIndex);

    if (coreIndex != 0)
    {
        pThisCore->Sched.mActivePrio = K2OS_THREADPRIO_IDLE;
        KernSched_InsertCore(pThisCore, TRUE);
        K2_CpuWriteBarrier();
        return;
    }

    //
    // core 0 is always last, and gets thread 0
    //

    //
    // manually insert thread as running on this core
    //
    pThread = gpThread0;

    K2_ASSERT(pThread->Info.CreateInfo.Attr.mFieldMask == K2OS_THREADATTR_VALID_MASK);

    pThread->Sched.Attr = pThread->Info.CreateInfo.Attr;
    pThread->Sched.mBasePrio = pThread->Sched.Attr.mPriority;
    pThread->Sched.mQuantumLeft = pThread->Sched.Attr.mQuantum;
    pThread->Sched.mActivePrio = pThread->Sched.mBasePrio;

    pThread->Sched.State.mLifeStage = KernThreadLifeStage_Instantiated;
    pThread->Sched.State.mRunState = KernThreadRunState_None;
    pThread->Sched.State.mStopFlags = KERNTHREAD_STOP_FLAG_NONE;

    K2LIST_Init(&pThread->Sched.OwnedCritSecList);

    KernArch_PrepareThread(pThread);

    gData.Sched.mSysWideThreadCount = 1;

    K2_ASSERT(pThread->Sched.State.mRunState == KernThreadRunState_Transition);
    pThread->Sched.State.mRunState = KernThreadRunState_Running;
    pThisCore->Sched.mpRunThread = pThread;

    pThisCore->Sched.mActivePrio = pThread->Sched.mActivePrio;
    KernSched_InsertCore(pThisCore, TRUE);

    pThisCore->mpActiveThread = pThread;
    K2_CpuWriteBarrier();
}

static void sInit_AfterVirt(void)
{
    UINT32 ix;

    K2LIST_Init(&gData.Sched.CpuCorePrioList);

    for (ix = 0; ix < K2OS_THREADPRIO_LEVELS; ix++)
    {
        K2LIST_Init(&gData.Sched.ReadyThreadsByPrioList[ix]);
    }

    //
    // this is only set valid by an actual sched timer expiry
    //
    gData.Sched.SchedTimerSchedItem.mSchedItemType = KernSchedItem_Invalid;
}

void KernInit_Sched(void)
{
    switch (gData.mKernInitStage)
    {
    case KernInitStage_dlx_entry:
        break;

    case KernInitStage_After_Virt:
        sInit_AfterVirt();
        break;

    default:
        break;
    }
}

static void sInsertToItemList(K2OSKERN_SCHED_ITEM *apNewItems)
{
    K2OSKERN_SCHED_ITEM *pItem;
    K2OSKERN_SCHED_ITEM *pInsAfter;
    K2OSKERN_SCHED_ITEM *pNext;

    K2_ASSERT(apNewItems != NULL);

    if (sgpItemList == NULL)
    {
        /* nothing on trap list. put first item on incoming list as the first trap */
        sgpItemList = apNewItems;
        apNewItems = sgpItemList->mpNext;
        sgpItemListEnd = sgpItemList;
        sgpItemList->mpPrev = NULL;
        sgpItemList->mpNext = NULL;
        if (apNewItems == NULL)
            return;
    }

    /* new traps almost always go at the end of the list
       since the incoming list is usually reversed
       chronologically.  we search from the end of the list
       up linearly.  usually we won't have to look more than
       one link to insert into the right place */
    do {
        pItem = apNewItems;
        apNewItems = pItem->mpNext;

        pInsAfter = sgpItemListEnd;
        K2_ASSERT(pInsAfter != NULL);

        do {
            if (pItem->CpuCoreEvent.mEventAbsTimeMs >= pInsAfter->CpuCoreEvent.mEventAbsTimeMs)
                break;
            pInsAfter = pInsAfter->mpPrev;
        } while (pInsAfter);

        if (!pInsAfter)
        {
            /* goes at start of list */
            pItem->mpNext = sgpItemList;
            sgpItemList->mpPrev = pItem;
            pItem->mpPrev = NULL;
            sgpItemList = pItem;
        }
        else
        {
            /* goes after pInsAfter */
            pNext = pInsAfter->mpNext;
            pItem->mpPrev = pInsAfter;
            pItem->mpNext = pNext;
            pInsAfter->mpNext = pItem;
            if (!pNext)
                sgpItemListEnd = pItem;
            else
                pNext->mpPrev = pItem;
        }

    } while (apNewItems != NULL);
}

static BOOL sExecItems(void)
{
    K2OSKERN_SCHED_ITEM *   pPendNew;
    BOOL                    changedSomething;
    BOOL                    subChangedSomething;
    BOOL                    processThreadItem;

    changedSomething = FALSE;

    do
    {
        pPendNew = (K2OSKERN_SCHED_ITEM *)K2ATOMIC_Exchange((volatile UINT32 *)&gData.Sched.mpPendingItemListHead, 0);
        if (pPendNew == NULL)
            break;

        sInsertToItemList(pPendNew);

        do
        {
            gData.Sched.mpActiveItem = sgpItemList;
            sgpItemList = sgpItemList->mpNext;
            if (sgpItemList == NULL)
                sgpItemListEnd = NULL;
            else
                sgpItemList->mpPrev = NULL;

            K2ATOMIC_Dec((INT32 volatile *)&gData.Sched.mReq);

            K2_ASSERT(gData.Sched.mpActiveItem->mSchedItemType != KernSchedItem_Invalid);

            K2_ASSERT(gData.Sched.mpActiveItem->mSchedItemType < KernSchedItemType_Count);

            processThreadItem = (gData.Sched.mpActiveItem->mSchedItemType != KernSchedItem_SchedTimer) ? TRUE : FALSE;

            if (!processThreadItem)
            {
                //
                // setting this to invalid allows it to be used again. there
                // should only ever be one of these in flight in the whole
                // system
                //
                K2_ASSERT(gData.Sched.mpActiveItem == &gData.Sched.SchedTimerSchedItem);
                gData.Sched.SchedTimerSchedItem.mSchedItemType = KernSchedItem_Invalid;
            }
            
            subChangedSomething = KernSched_TimePassed(gData.Sched.mpActiveItem->CpuCoreEvent.mEventAbsTimeMs);
            if (subChangedSomething)
                changedSomething = TRUE;

            if (processThreadItem)
            {
                gData.Sched.mpActiveItemThread = K2_GET_CONTAINER(K2OSKERN_OBJ_THREAD, gData.Sched.mpActiveItem, Sched.Item);

                K2_ASSERT(sgSchedHandlers[gData.Sched.mpActiveItem->mSchedItemType] != NULL);

                subChangedSomething = sgSchedHandlers[gData.Sched.mpActiveItem->mSchedItemType]();
                if (subChangedSomething)
                    changedSomething = TRUE;

                gData.Sched.mpActiveItemThread = NULL;
            }

            gData.Sched.mpActiveItem = NULL;

        } while (sgpItemList != NULL);

    } while (1);

    subChangedSomething = KernSched_TimePassed((UINT64)-1);
    if (subChangedSomething)
        changedSomething = TRUE;

    return changedSomething;
}

void sScheduleThreadsToCores(void)
{
    UINT64                  lowestQuantumLeft;
    UINT32                  workPrio;
    K2LIST_LINK *           pThreadListLink;
    K2LIST_LINK *           pCoreListLink;
    K2OSKERN_OBJ_THREAD *   pWorkThread;
    K2OSKERN_CPUCORE *      pWorkCore;

    //
    // scheduling timer has been disabled
    //

    lowestQuantumLeft = gData.Sched.mpTimerItemQueue ? gData.Sched.mpTimerItemQueue->mDeltaT : 0;
    do {
        if (0 == gData.Sched.mReadyThreadCount)
            break;

        //
        // something to check on for scheduling
        //
        workPrio = 0;
        do {
            pThreadListLink = gData.Sched.ReadyThreadsByPrioList[workPrio].mpHead;
            if (pThreadListLink != NULL)
                break;
            workPrio++;
        } while (workPrio < K2OS_THREADPRIO_LEVELS);
        K2_ASSERT(workPrio < K2OS_THREADPRIO_LEVELS);

        //
        // skip cores that are already running something higher than this priority
        //
        pCoreListLink = gData.Sched.CpuCorePrioList.mpHead;
        do {
            pWorkCore = K2_GET_CONTAINER(K2OSKERN_CPUCORE, pCoreListLink, CpuCoreListLink);

            if (pWorkCore->Sched.mActivePrio > workPrio)
                break;

            if (pWorkCore->Sched.mActivePrio == workPrio)
            {
                //
                // if currently running thread's quantum is expired then stop here
                // otherwise keep looking
                //
                if (pWorkCore->Sched.mpRunThread == NULL)
                    break;

                if (pWorkCore->Sched.mpRunThread->Sched.mQuantumLeft == 0)
                    break;

                //
                // core is running something of the same prio but there
                // is quantum left on the thread so we are not going to 
                // preempt it
                //
            }
            //
            // else we are letting what is running on this core continue to run
            // because there is nothing of higher priority
            //

            pCoreListLink = pCoreListLink->mpNext;

        } while (pCoreListLink != NULL);

        if (pCoreListLink == NULL)
        {
            //
            // there are no cores we want to preempt
            //
            break;
        }

        //
        // we are going to change what is running somehow
        //
        // 'pWorkCore' is first core we can preempt
        // 'pThreadListLink' is first ready thread and is
        //      set at priority level 'workPrio'
        pWorkThread = K2_GET_CONTAINER(K2OSKERN_OBJ_THREAD, pThreadListLink, Sched.ReadyListLink);
        K2_ASSERT(pWorkThread->Sched.mActivePrio == workPrio);








    } while (0);

    if (lowestQuantumLeft != 0)
        KernSched_ArmSchedTimer(lowestQuantumLeft);



            //
            // remove the thread from the ready list because we are going
            // to stick it onto a core
            //
            pThread = K2_GET_CONTAINER(K2OSKERN_OBJ_THREAD, pThreadListLink, Sched.ReadyListLink);
            pThreadListLink = pThreadListLink->mpNext;
            K2LIST_Remove(&gData.Sched.ReadyThreadsByPrioList[workPrio], &pThread->Sched.ReadyListLink);
            gData.Sched.mReadyThreadCount--;
            pThread->Sched.State.mRunState = KernThreadRunState_Transition;


}

void KernSched_Exec(void)
{
    K2LIST_LINK *           pLink;
    K2OSKERN_CPUCORE *      pCpuCore;
    K2OSKERN_OBJ_THREAD *   pThread;
    BOOL                    somethingChanged;
    BOOL                    localChange;
    BOOL                    disarmed;

    /* execute any scheduler items queued and give debugger a chance to run */
    sgpItemList = sgpItemListEnd = NULL;

    somethingChanged = FALSE;

    if (!gData.mDebuggerActive)
    {
        if (sExecItems())
        {
            somethingChanged = TRUE;
            if (gData.Sched.mpTimerItemQueue != NULL)
            {
                KernSched_ArmSchedTimer(0);
                sExecItems();
            }
        }
    }
    else
    {
        //
        // debugger is active
        //
        disarmed = FALSE;
        do {
            if (sExecItems())
            {
                somethingChanged = TRUE;
                localChange = TRUE;
                if (gData.Sched.mpTimerItemQueue != NULL)
                {
                    disarmed = TRUE;
                    KernSched_ArmSchedTimer(0);
                    sExecItems();
                }
            }

            if (!KernDebug_Service(localChange))
                break;

            localChange = FALSE;
            somethingChanged = TRUE;
            if ((!disarmed) && (gData.Sched.mpTimerItemQueue != NULL))
                KernSched_ArmSchedTimer(0);
        } while (1);
    }

    /* put threads onto cores */
    if (somethingChanged)
        sScheduleThreadsToCores();

    /* release cores that are due to run something they are not running */
    pLink = gData.Sched.CpuCorePrioList.mpHead;
    do
    {
        pCpuCore = K2_GET_CONTAINER(K2OSKERN_CPUCORE, pLink, Sched.CpuCoreListLink);

        if (pCpuCore->mpActiveThread == NULL)
        {
            pThread = pCpuCore->Sched.mpRunThread;
            if (pThread != NULL)
            {
                K2_ASSERT(pThread->Sched.State.mRunState == KernThreadRunState_Running);

                pCpuCore->mpActiveThread = pThread;
                K2_CpuWriteBarrier();

                if (pCpuCore != gData.Sched.mpSchedulingCore)
                    KernCpuCore_SendIciToOneCore(gData.Sched.mpSchedulingCore, pCpuCore->mCoreIx, KernCpuCoreEvent_Ici_Wakeup);
            }
        }

        pLink = pLink->mpNext;

    } while (pLink != NULL);
}

void KernSched_Check(K2OSKERN_CPUCORE *apThisCore)
{
    UINT32  v;
    UINT32  old;
    UINT32  reqVal;

    do {
        //
        // check to see if somebody else is scheduling already 
        //
        reqVal = gData.Sched.mReq;
        if ((reqVal & 0xF0000000) != 0)
        {
            // some other core is scheduling 
            return;
        }

        //
        // nobody is scheduling - check to see if there is work to do 
        //
        if (0 == (reqVal & 0x0FFFFFFF))
        {
            //
            // there is no work to do
            //
            return;
        }

        //
        // nobody is scheduling and there is work to do,
        // to try to install this core as the scheduling core
        //
        v = reqVal | ((apThisCore->mCoreIx + 1) << 28);

        old = K2ATOMIC_CompareExchange(&gData.Sched.mReq, v, reqVal);
    } while (old != reqVal);

    //
    // if we get here we are the scheduling core 
    //
//    K2OSKERN_Debug("!Core %d: Entered Scheduler\n", apThisCore->mCoreIx);

    gData.Sched.mpSchedulingCore = apThisCore;

    v = (apThisCore->mCoreIx + 1) << 28;

    do {
        //
        // execute scheduler
        //
        KernSched_Exec();

        //
        // try to leave the scheduler
        //
        gData.Sched.mpSchedulingCore = NULL;
        K2_CpuWriteBarrier();
        old = K2ATOMIC_CompareExchange(&gData.Sched.mReq, 0, v);
        if (old == v)
            break;

        //
        // we were not able to get out of the scheduler
        // before more work came in for us to do
        //
        gData.Sched.mpSchedulingCore = apThisCore;
        K2_CpuWriteBarrier();
    } while (1);

    // 
    // if we got here we left the scheduler
    //
//    K2OSKERN_Debug("!Core %d: Left Scheduler\n", apThisCore->mCoreIx);
}

static
void
sQueueSchedItem(
    K2OSKERN_SCHED_ITEM *apItem
)
{
    K2OSKERN_SCHED_ITEM * pHead;
    K2OSKERN_SCHED_ITEM * pOld;

    apItem->mResult = K2STAT_ERROR_UNKNOWN;

    //
    // lockless add to sched item list
    //
    do {
        pHead = gData.Sched.mpPendingItemListHead;
        apItem->mpNext = pHead;
        pOld = (K2OSKERN_SCHED_ITEM *)K2ATOMIC_CompareExchange((UINT32 volatile *)&gData.Sched.mpPendingItemListHead, (UINT32)apItem, (UINT32)pHead);
    } while (pOld != pHead);

    K2ATOMIC_Inc((INT32 volatile *)&gData.Sched.mReq);
}

void KernSched_RespondToCallFromThread(K2OSKERN_CPUCORE *apThisCore)
{
    K2OSKERN_OBJ_THREAD *   pThread;
    UINT64                  absTime;

    pThread = apThisCore->mpActiveThread;
    K2_ASSERT(pThread != NULL);

    absTime = pThread->Sched.Item.CpuCoreEvent.mEventAbsTimeMs;

    sQueueSchedItem(&pThread->Sched.Item);

    //
    // althought not practically possible, thread may have disappeared completely here
    // so we can no longer reference it
    //
    apThisCore->Sched.mLastStopAbsTimeMs = absTime;
    apThisCore->mpActiveThread = NULL;
    K2_CpuWriteBarrier();
}

void KernSched_TimerFired(K2OSKERN_CPUCORE *apThisCore)
{
    //
    // timer fire time is in the gData.Sched.SchedTimerSchedItem.CpuCoreEvent.mEventAbsTime
    //
    K2_ASSERT(gData.Sched.SchedTimerSchedItem.mSchedItemType == KernSchedItem_Invalid);
    gData.Sched.SchedTimerSchedItem.mSchedItemType = KernSchedItem_SchedTimer;
    sQueueSchedItem(&gData.Sched.SchedTimerSchedItem);
}

void KernSched_MakeThreadReady(K2OSKERN_OBJ_THREAD *apThread, BOOL aEndOfListAtPrio)
{
    UINT32          activePrio;
    K2LIST_ANCHOR * pAnchor;
    K2LIST_LINK *   pListLink;

    activePrio = apThread->Sched.mActivePrio;
    K2_ASSERT(activePrio < K2OS_THREADPRIO_LEVELS);

    K2_ASSERT(apThread->Sched.State.mLifeStage == KernThreadLifeStage_Run);
    K2_ASSERT(apThread->Sched.State.mRunState == KernThreadRunState_Transition);
    K2_ASSERT(apThread->Sched.State.mStopFlags == 0);

    pAnchor = &gData.Sched.ReadyThreadsByPrioList[activePrio];
    pListLink = &apThread->Sched.ReadyListLink;

    apThread->Sched.State.mRunState = KernThreadRunState_Ready;
    if (aEndOfListAtPrio)
        K2LIST_AddAtTail(pAnchor, pListLink);
    else
        K2LIST_AddAtHead(pAnchor, pListLink);

    gData.Sched.mReadyThreadCount++;
}

void KernSched_MakeThreadInactive(K2OSKERN_OBJ_THREAD *apThread, KernThreadRunState aNewRunState)
{
    K2LIST_LINK *           pScan;
    K2OSKERN_CPUCORE *      pCpuCore;
    K2OSKERN_OBJ_THREAD *   pCheck;

    K2_ASSERT(apThread->Sched.State.mLifeStage == KernThreadLifeStage_Run);

    K2_ASSERT(aNewRunState != KernThreadRunState_None);
    K2_ASSERT(aNewRunState != KernThreadRunState_Ready);
    K2_ASSERT(aNewRunState != KernThreadRunState_Running);

    if (apThread->Sched.State.mRunState == KernThreadRunState_Running)
    {
        //
        // take thread off of its core
        //
        pScan = gData.Sched.CpuCorePrioList.mpHead;
        K2_ASSERT(pScan != NULL);
        do {
            pCpuCore = K2_GET_CONTAINER(K2OSKERN_CPUCORE, pScan, Sched.CpuCoreListLink);
            if (pCpuCore->Sched.mpRunThread == apThread)
                break;
            pScan = pScan->mpNext;
        } while (pScan != NULL);
        K2_ASSERT(pScan != NULL);

        pCpuCore->Sched.mpRunThread = NULL;
        // next state state set below unconditionally for all

        if (pCpuCore == gData.Sched.mpSchedulingCore)
        {
            //
            // same core as the one executing this code.
            // core will have set active thread to NULL
            // when it called into the scheduler
            //
            K2_ASSERT(pCpuCore->mpActiveThread == NULL);
        }
        else
        {
            //
            // thread is running on some other core
            //
            KernCpuCore_SendIciToOneCore(gData.Sched.mpSchedulingCore, pCpuCore->mCoreIx, KernCpuCoreEvent_Ici_Stop);
            K2_CpuReadBarrier();
            do {
                pCheck = pCpuCore->mpActiveThread;
                K2_CpuReadBarrier();
                if (pCheck != apThread)
                    break;
                K2OSKERN_MicroStall(10);
            } while (1);

            //
            // we force-stopped the thread.  we need to update its quantum drain and
            // its total run time
            //
            if (gData.Sched.mCurrentAbsTime < apThread->Sched.mAbsTimeAtStop)
            {
                apThread->Sched.mQuantumLeft = 0;
                apThread->Sched.mTotalRunTimeMs += apThread->Sched.mAbsTimeAtStop - gData.Sched.mCurrentAbsTime;
            }
        }

        //
        // move core on core priority list to idle
        //
        pCpuCore->Sched.mActivePrio = K2OS_THREADPRIO_IDLE;
        if (pCpuCore->Sched.CpuCoreListLink.mpNext != NULL)
        {
            K2LIST_Remove(&gData.Sched.CpuCorePrioList, &pCpuCore->Sched.CpuCoreListLink);
            K2LIST_AddAtTail(&gData.Sched.CpuCorePrioList, &pCpuCore->Sched.CpuCoreListLink);
        }
    }
    else if (apThread->Sched.State.mRunState == KernThreadRunState_Ready)
    {
        K2LIST_Remove(&gData.Sched.ReadyThreadsByPrioList[apThread->Sched.mActivePrio], &apThread->Sched.ReadyListLink);
        gData.Sched.mReadyThreadCount--;
    }
    else
    {
        K2_ASSERT(apThread->Sched.State.mRunState == KernThreadRunState_Transition);
    }

    apThread->Sched.State.mRunState = aNewRunState;
}

