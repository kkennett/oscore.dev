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
    KernSched_Exec_ThreadWait,          // KernSchedItem_ThreadWait
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
        K2LIST_AddAtHead(&gData.Sched.CpuCorePrioList, &apCore->Sched.PrioListLink);

        return;
    }

    if (aInsertAtTailOfSamePrio)
    {
        do {
            pOther = K2_GET_CONTAINER(K2OSKERN_CPUCORE, pFind, Sched.PrioListLink);
            if (apCore->Sched.mActivePrio <= pOther->Sched.mActivePrio)
            {
                K2LIST_AddBefore(&gData.Sched.CpuCorePrioList, &apCore->Sched.PrioListLink, pFind);
                return;
            }
            pFind = pFind->mpNext;
        } while (pFind != NULL);

        K2LIST_AddAtTail(&gData.Sched.CpuCorePrioList, &apCore->Sched.PrioListLink);

        return;
    }

    do {
        pOther = K2_GET_CONTAINER(K2OSKERN_CPUCORE, pFind, Sched.PrioListLink);
        if (apCore->Sched.mActivePrio < pOther->Sched.mActivePrio)
        {
            K2LIST_AddBefore(&gData.Sched.CpuCorePrioList, &apCore->Sched.PrioListLink, pFind);
            return;
        }
        pFind = pFind->mpNext;
    } while (pFind != NULL);

    K2LIST_AddAtTail(&gData.Sched.CpuCorePrioList, &apCore->Sched.PrioListLink);
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

    pThread->Sched.State.mLifeStage = KernThreadLifeStage_Started;
    pThread->Sched.State.mRunState = KernThreadRunState_Running;

    gData.Sched.mSysWideThreadCount = 1;

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

    gData.Sched.SchedTimerSchedTimerItem.mIsSchedTimerItem = TRUE;
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

static void sScheduleThreadsToCores(void)
{
    K2_ASSERT(0);
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

            if (gData.Sched.mpActiveItem->mSchedItemType == KernSchedItem_SchedTimer)
            {
                //
                // setting this to invalid allows it to be used again. there
                // should only ever be one of these in flight in the whole
                // system
                //
                gData.Sched.SchedTimerSchedItem.mSchedItemType = KernSchedItem_Invalid;
                //
                // nothing else happens here. the whole point of the timer
                // is to get us into the scheduler.  if the scheduler needs to
                // set another timer it will do so
                //
            }
            else
            {
                //
                // all other items come from a thread
                //
                gData.Sched.mpActiveItemThread = K2_GET_CONTAINER(K2OSKERN_OBJ_THREAD, gData.Sched.mpActiveItem, Sched.Item);

                K2_ASSERT(sgSchedHandlers[gData.Sched.mpActiveItem->mSchedItemType] != NULL);

                subChangedSomething= sgSchedHandlers[gData.Sched.mpActiveItem->mSchedItemType]();
                if (!changedSomething)
                    changedSomething = subChangedSomething;

                gData.Sched.mpActiveItemThread = NULL;
            }

            gData.Sched.mpActiveItem = NULL;

        } while (sgpItemList != NULL);

    } while (1);

    subChangedSomething = KernSched_TimePassed();
    if (!changedSomething)
        changedSomething = subChangedSomething;

    return changedSomething;
}

void KernSched_Exec(void)
{
    K2LIST_LINK *           pLink;
    K2OSKERN_CPUCORE *      pCpuCore;
    K2OSKERN_OBJ_THREAD *   pThread;
    BOOL                    somethingChanged;

    /* execute any scheduler items queued and give debugger a chance to run */
    sgpItemList = sgpItemListEnd = NULL;
    do {
        somethingChanged = sExecItems();

        if (!gData.mDebuggerActive)
            break;

        somethingChanged = KernDebug_Service(somethingChanged);

    } while (somethingChanged);

    /* put threads onto cores */
    if (somethingChanged)
        sScheduleThreadsToCores();

    /* release cores that are due to run something they are not running */
    pLink = gData.Sched.CpuCorePrioList.mpHead;
    do
    {
        pCpuCore = K2_GET_CONTAINER(K2OSKERN_CPUCORE, pLink, Sched.PrioListLink);

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
    // timer fire time is in the gData.Sched.SchedTimerSchedItem.Event.mAbsTime
    //
    K2_ASSERT(gData.Sched.SchedTimerSchedItem.mSchedItemType == KernSchedItem_Invalid);
    gData.Sched.SchedTimerSchedItem.mSchedItemType = KernSchedItem_SchedTimer;
    sQueueSchedItem(&gData.Sched.SchedTimerSchedItem);
}

void KernSched_InsertThreadToReadyList(K2OSKERN_OBJ_THREAD *apThread, BOOL aEndOfListAtPrio)
{
    UINT32          activePrio;
    K2LIST_ANCHOR * pAnchor;
    K2LIST_LINK *   pListLink;

    activePrio = apThread->Sched.mActivePrio;
    K2_ASSERT(activePrio < K2OS_THREADPRIO_LEVELS);

    K2_ASSERT(apThread->Sched.State.mLifeStage < KernThreadLifeStage_Exited);
    K2_ASSERT(apThread->Sched.State.mRunState == KernThreadRunState_Ready);
    K2_ASSERT(apThread->Sched.State.mStopFlags == 0);

    pAnchor = &gData.Sched.ReadyThreadsByPrioList[activePrio];
    pListLink = &apThread->Sched.PrioListLink;

    if (aEndOfListAtPrio)
        K2LIST_AddAtTail(pAnchor, pListLink);
    else
        K2LIST_AddAtHead(pAnchor, pListLink);
}
