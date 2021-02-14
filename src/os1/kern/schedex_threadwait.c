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
sWaitList_Insert(
    K2OSKERN_OBJ_THREAD *       apWaitThread,
    K2OSKERN_SCHED_WAITENTRY *  apEntry
)
{
    K2OSKERN_OBJ_THREAD *       pOtherThread;
    K2LIST_ANCHOR *             pAnchor;
    K2LIST_LINK *               pListLink;
    K2OSKERN_SCHED_MACROWAIT *  pOtherWait;
    K2OSKERN_SCHED_WAITENTRY *  pOtherEntry;

    pAnchor = &apEntry->mWaitObj.mpHdr->WaitEntryPrioList;
    if (pAnchor->mNodeCount == 0)
    {
        K2LIST_AddAtHead(pAnchor, &apEntry->WaitPrioListLink);
        return;
    }

    pListLink = pAnchor->mpHead;
    do {
        pOtherEntry = K2_GET_CONTAINER(K2OSKERN_SCHED_WAITENTRY, pListLink, WaitPrioListLink);
        pOtherWait = K2_GET_CONTAINER(K2OSKERN_SCHED_MACROWAIT, pOtherEntry, SchedWaitEntry[pOtherEntry->mMacroIndex]);
        pOtherThread = pOtherWait->mpWaitingThread;
        if (pOtherThread->Sched.mThreadActivePrio > apWaitThread->Sched.mThreadActivePrio)
            break;
        pListLink = pListLink->mpNext;
    } while (pListLink != NULL);

    //
    // pOtherThread is of lower priority than apWaitThread
    //
    if (pListLink != NULL)
        K2LIST_AddBefore(pAnchor, &apEntry->WaitPrioListLink, &pOtherEntry->WaitPrioListLink);
    else
        K2LIST_AddAtTail(pAnchor, &apEntry->WaitPrioListLink);
}

void sWaitList_Remove(
    K2OSKERN_OBJ_THREAD *       apWaitThread,
    K2OSKERN_SCHED_WAITENTRY *  apEntry
)
{
    K2LIST_ANCHOR *             pAnchor;

    pAnchor = &apEntry->mWaitObj.mpHdr->WaitEntryPrioList;
    K2_ASSERT(pAnchor->mNodeCount != 0);

    K2LIST_Remove(pAnchor, &apEntry->WaitPrioListLink);
}

void KernSched_EndThreadWait(K2OSKERN_SCHED_MACROWAIT * apWait, UINT32 aWaitResult)
{
    UINT32                  ix;
    K2OSKERN_OBJ_THREAD *   pThread;

    pThread = apWait->mpWaitingThread;

    K2_ASSERT(pThread->Sched.State.mRunState == KernThreadRunState_Waiting);

    pThread->Sched.State.mRunState = KernThreadRunState_Transition;

    K2_ASSERT(pThread->Sched.Item.Args.ThreadWait.mpMacroWait == apWait);

    for (ix = 0; ix < apWait->mNumEntries; ix++)
    {
        sWaitList_Remove(pThread, &apWait->SchedWaitEntry[ix]);
    }

    if (aWaitResult != K2STAT_ERROR_TIMEOUT)
    {
        if (apWait->SchedTimerItem.mOnQueue)
        {
            KernSched_DelTimerItem(&apWait->SchedTimerItem);
        }
    }

    apWait->mWaitResult = aWaitResult;

    K2_ASSERT(pThread->Sched.Item.mSchedCallResult == K2STAT_THREAD_WAITED);

    KernSched_MakeThreadActive(pThread, TRUE);
}

BOOL KernSched_Exec_ThreadWait(void)
{
    K2OSKERN_SCHED_MACROWAIT *  pWait;
    K2OSKERN_SCHED_WAITENTRY *  pEntry;
    K2OSKERN_OBJ_THREAD *       pThread;
    K2OSKERN_OBJ_WAITABLE       objWait;
    UINT32                      ix;
    BOOL                        isSatisfiedWithoutChange;
    KernThreadRunState          nextRunState;
    K2OSKERN_CPUCORE volatile * pCore;
    UINT32                      workPrio;
    K2LIST_LINK *               pListLink;
    K2OSKERN_OBJ_THREAD *       pReadyThread;

    K2_ASSERT(gData.Sched.mpActiveItem->mSchedItemType == KernSchedItem_ThreadWait);

    pWait = gData.Sched.mpActiveItem->Args.ThreadWait.mpMacroWait;

    pThread = gData.Sched.mpActiveItemThread;

    pWait->mWaitResult = K2OS_WAIT_ERROR;

    K2_ASSERT(pWait->mNumEntries <= K2OS_WAIT_MAX_TOKENS);

    if (pWait->mWaitAll)
    {
        //
        // we need to see if everything is already signalled without changing
        // anything. if it is, then we can go ahead and change things and
        // satisfy the wait immeidately.  if it is not, then we have to do
        // the wait without changing anything
        //
        isSatisfiedWithoutChange = TRUE;

        for (ix = 0; ix < pWait->mNumEntries; ix++)
        {
            pEntry = &pWait->SchedWaitEntry[ix];
            objWait = pEntry->mWaitObj;

            pEntry->mMacroIndex = (UINT8)ix;
            pEntry->mStickyPulseStatus = 0;

            switch (objWait.mpHdr->mObjType)
            {
            case K2OS_Obj_Event:
                if (!objWait.mpEvent->mIsSignalled)
                {
                    isSatisfiedWithoutChange = FALSE;
                }
                break;

            case K2OS_Obj_Process:
                if (objWait.mpProc->mState < KernProcState_Done)
                {
                    isSatisfiedWithoutChange = FALSE;
                }
                break;

            case K2OS_Obj_Thread:
                if (objWait.mpThread->Sched.State.mLifeStage < KernThreadLifeStage_Exited)
                {
                    isSatisfiedWithoutChange = FALSE;
                }
                break;

            case K2OS_Obj_Semaphore:
                if (objWait.mpSem->mCurCount > 0)
                {
                    isSatisfiedWithoutChange = FALSE;
                }
                break;

            case K2OS_Obj_Alarm:
                if (!objWait.mpAlarm->mIsSignalled)
                {
                    isSatisfiedWithoutChange = FALSE;
                }
                break;

            default:
                K2_ASSERT(0);
                break;
            }

            if (!isSatisfiedWithoutChange)
                break;
        }

        if (isSatisfiedWithoutChange)
        {
            //
            // can satisfy waitall immediately.
            //
            for (ix = 0; ix < pWait->mNumEntries; ix++)
            {
                pEntry = &pWait->SchedWaitEntry[ix];
                objWait = pEntry->mWaitObj;

                switch (objWait.mpHdr->mObjType)
                {
                case K2OS_Obj_Event:
                    K2_ASSERT(objWait.mpEvent->mIsSignalled);
                    if (objWait.mpEvent->mIsAutoReset)
                        objWait.mpEvent->mIsSignalled = FALSE;
                    break;

                case K2OS_Obj_Semaphore:
                    K2_ASSERT(objWait.mpSem->mCurCount > 0);
                    objWait.mpSem->mCurCount--;
                    break;

                case K2OS_Obj_Alarm:
                    if (objWait.mpAlarm->mIsSignalled)
                        pEntry->mStickyPulseStatus = TRUE;
                    break;

                default:
                    break;
                }
            }

            //
            // successful waitall returns SIGNALLED_0
            //
            ix = 0;
        }
    }
    else
    {
        
    //    K2OSKERN_Debug("%6d.Core %d: SCHED:Exec WaitAny(%d, %d)\n", 
        //        gData.Sched.mpSchedulingCore->mCoreIx, 
        //        (UINT32)K2OS_SysUpTimeMs(), 
        //        pWait->mNumEntries, 
        //        gData.Sched.mpActiveItem->Args.ThreadWait.mTimeoutMs);
        
        isSatisfiedWithoutChange = FALSE;

        for (ix = 0; ix < pWait->mNumEntries; ix++)
        {
            pEntry = &pWait->SchedWaitEntry[ix];
            objWait = pEntry->mWaitObj;

            //
            // check for wait already being satisfied
            //
            switch (objWait.mpHdr->mObjType)
            {
            case K2OS_Obj_Event:
                if (objWait.mpEvent->mIsSignalled)
                {
                    if (objWait.mpEvent->mIsAutoReset)
                    {
                        //
                        // nobody is waiting, or it would not be signalled
                        //
                        objWait.mpEvent->mIsSignalled = FALSE;
                    }
                    isSatisfiedWithoutChange = TRUE;
                }
                break;

            case K2OS_Obj_Process:
                if (objWait.mpProc->mState >= KernProcState_Done)
                {
                    isSatisfiedWithoutChange = TRUE;
                }
                break;

            case K2OS_Obj_Thread:
                if (objWait.mpThread->Sched.State.mLifeStage >= KernThreadLifeStage_Exited)
                {
                    isSatisfiedWithoutChange = TRUE;
                }
                break;

            case K2OS_Obj_Semaphore:
                if (objWait.mpSem->mCurCount > 0)
                {
                    objWait.mpSem->mCurCount--;
                    isSatisfiedWithoutChange = TRUE;
                }
                break;

            case K2OS_Obj_Alarm:
                if (objWait.mpAlarm->mIsSignalled)
                {
                    isSatisfiedWithoutChange = TRUE;
                }
                break;

            default:
                K2_ASSERT(0);
                break;
            }

            if (isSatisfiedWithoutChange)
                break;

            //
            // continue setup for wait on this item
            //
            pEntry->mMacroIndex = (UINT8)ix;
            pEntry->mStickyPulseStatus = 0;
        }
    }

    if (isSatisfiedWithoutChange)
    {
        pWait->mWaitResult = K2OS_WAIT_SIGNALLED_0 + ix;
        gData.Sched.mpActiveItem->mSchedCallResult = K2STAT_NO_ERROR;
        return FALSE;
    }

    //
    // if we get here, thread will stop unless it is a zero timeout
    //

    nextRunState = KernThreadRunState_Waiting;

    if (pWait->mNumEntries != 0)
    {
        if (gData.Sched.mpActiveItem->Args.ThreadWait.mTimeoutMs == 0)
        {
            //
            // tested stuff, but nothing is signalled
            //
            pWait->mWaitResult = K2STAT_ERROR_TIMEOUT;
            gData.Sched.mpActiveItem->mSchedCallResult = K2STAT_NO_ERROR;
            return FALSE;
        }
    }
    else
    {
        if (gData.Sched.mpActiveItem->Args.ThreadWait.mTimeoutMs == 0)
            nextRunState = KernThreadRunState_Ready;
    }

    //
    // if we get here, no matter what thread loses the rest of its quantum
    //
    gData.Sched.mpActiveItem->mSchedCallResult = K2STAT_THREAD_WAITED;
    
    K2_ASSERT(pThread->Sched.mLastRunCoreIx < gData.mCpuCount);
    pCore = K2OSKERN_COREIX_TO_CPUCORE(pThread->Sched.mLastRunCoreIx);

    if (pThread->Sched.State.mRunState == KernThreadRunState_Running)
    {
        pThread->Sched.mQuantumLeft = 0;

        if (nextRunState == KernThreadRunState_Ready)
        {
            //
            // thread is sleeping for 0 milliseconds. so all we did was stop it
            //
            K2_ASSERT(pWait->mNumEntries == 0);
            K2_ASSERT(gData.Sched.mpActiveItem->Args.ThreadWait.mTimeoutMs == 0);

            pCore->Sched.mExecFlags |= K2OSKERN_SCHED_CPUCORE_EXECFLAG_QUANTUM_ZERO;

            return KernSched_RunningThreadQuantumExpired(pCore, pThread);
        }

        //
        // we are stopping this thread so it can wait, and need to give the core something
        // else to do.  if the core goes idle we just stop the thread but other wise we
        // preempt it with something
        //
        K2_ASSERT(nextRunState == KernThreadRunState_Waiting);
        pListLink = NULL;
        if (gData.Sched.mReadyThreadCount > 0)
        {
            workPrio = 0;
            do {
                if (gData.Sched.ReadyThreadsByPrioList[workPrio].mNodeCount > 0)
                {
                    pListLink = gData.Sched.ReadyThreadsByPrioList[workPrio].mpHead;
                    do {
                        pReadyThread = K2_GET_CONTAINER(K2OSKERN_OBJ_THREAD, pListLink, Sched.ReadyListLink);
                        if (pReadyThread->Sched.Attr.mAffinityMask & (1 << pCore->mCoreIx))
                            break;
                        pListLink = pListLink->mpNext;
                    } while (pListLink != NULL);
                    if (pListLink != NULL)
                        break;
                }
                ++workPrio;
            } while (workPrio < K2OS_THREADPRIO_LEVELS);
        }

        if (pListLink == NULL)
        {
            KernSched_StopThread(pThread, pCore, KernThreadRunState_Waiting, TRUE);
        }
        else
        {
            KernSched_PreemptCore(pCore, pThread, KernThreadRunState_Waiting, pReadyThread);
        }
    }
    else
    {
        if (pThread->Sched.State.mRunState == KernThreadRunState_Ready)
        {
            if (nextRunState == KernThreadRunState_Ready)
            {
                //
                // thread is sleeping for 0 milliseconds. it was already made ready so this is a no-op
                //
                K2_ASSERT(pWait->mNumEntries == 0);
                K2_ASSERT(gData.Sched.mpActiveItem->Args.ThreadWait.mTimeoutMs == 0);
                return TRUE;
            }

            K2LIST_Remove(
                &gData.Sched.ReadyThreadsByPrioList[pThread->Sched.mThreadActivePrio],
                &pThread->Sched.ReadyListLink
            );
            gData.Sched.mReadyThreadCount--;
        }
        else
        {
            if (pThread->Sched.State.mRunState != KernThreadRunState_Transition)
            {
                K2OSKERN_Debug("**State = %d\n", pThread->Sched.State.mRunState);
            }
            K2_ASSERT(pThread->Sched.State.mRunState == KernThreadRunState_Transition);
        }

        pThread->Sched.State.mRunState = nextRunState;
    }

    K2_ASSERT(pThread->Sched.State.mRunState == KernThreadRunState_Waiting);
    K2_ASSERT((pWait->mNumEntries > 0) || (gData.Sched.mpActiveItem->Args.ThreadWait.mTimeoutMs > 0));

    gData.Sched.mpActiveItem->mSchedCallResult = K2STAT_THREAD_WAITED;

    pWait->mpWaitingThread = pThread;

    //
    // mount the wait to the waitlists of the objects it is waiting on
    //
    for (ix = 0; ix < pWait->mNumEntries; ix++)
    {
        pEntry = &pWait->SchedWaitEntry[ix];
        sWaitList_Insert(pWait->mpWaitingThread, pEntry);
    }

    pWait->SchedTimerItem.mOnQueue = FALSE;

    if (gData.Sched.mpActiveItem->Args.ThreadWait.mTimeoutMs != K2OS_TIMEOUT_INFINITE)
    {
        //
        // mount the timer item for the timeout.  This is done last in case the timeout
        // is already expired and the wait is completed
        //
        pWait->SchedTimerItem.mType = KernSchedTimerItemType_Wait;
        KernSched_AddTimerItem(
            &pWait->SchedTimerItem,
            pThread->Sched.Item.CpuCoreEvent.mEventAbsTimeMs,
            gData.Sched.mpActiveItem->Args.ThreadWait.mTimeoutMs
        );
    }
    else
    {
        pWait->SchedTimerItem.mType = KernSchedTimerItemType_Error;
    }

    return TRUE;  // if something changes scheduling-wise, return true
}
