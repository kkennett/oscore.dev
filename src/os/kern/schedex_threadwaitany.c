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

    pAnchor = &apEntry->mWaitObj.mpHdr->WaitingThreadsPrioList;
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
        if (pOtherThread->Sched.mActivePrio > apWaitThread->Sched.mActivePrio)
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

BOOL KernSched_Exec_ThreadWaitAny(void)
{
    K2OSKERN_SCHED_MACROWAIT *  pWait;
    K2OSKERN_SCHED_WAITENTRY *  pEntry;
    K2OSKERN_OBJ_WAITABLE       objWait;
    UINT32                      ix;
    BOOL                        isSatisfiedWithoutChange;

    K2_ASSERT(gData.Sched.mpActiveItem->mSchedItemType == KernSchedItem_ThreadWaitAny);

    pWait = gData.Sched.mpActiveItem->Args.ThreadWait.mpMacroWait;

    K2_ASSERT(pWait->mNumEntries <= K2OS_WAIT_MAX_TOKENS);

    K2_ASSERT(FALSE == pWait->mWaitAll);

    K2OSKERN_Debug("SCHED:WaitAny(%d, %d)\n", pWait->mNumEntries, gData.Sched.mpActiveItem->Args.ThreadWait.mTimeoutMs);

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
            if (objWait.mpEvent->mIsSignaled)
            {
                if (objWait.mpEvent->mIsAutoReset)
                {
                    //
                    // nobody is waiting, or it would not be signalled
                    //
                    objWait.mpEvent->mIsSignaled = FALSE;
                }
                isSatisfiedWithoutChange = TRUE;
            }
            break;

        case K2OS_Obj_Name:
            if (objWait.mpName->Event_IsOwned.mIsSignaled)
            {
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

    if (isSatisfiedWithoutChange)
    {
        gData.Sched.mpActiveItem->mResult = K2OS_WAIT_SIGNALLED_0 + ix;
        return FALSE;
    }

    if (pWait->mNumEntries != 0)
    {
        if (gData.Sched.mpActiveItem->Args.ThreadWait.mTimeoutMs == 0)
        {
            //
            // tested stuff, but nothing is signalled
            //
            gData.Sched.mpActiveItem->mResult = K2STAT_ERROR_TIMEOUT;
            return FALSE;
        }
    }

    //
    // if we get here, no matter what thread loses the rest of its quantum
    //

    gData.Sched.mpActiveItemThread->Sched.mQuantumLeft = 0;
    
    apCore->Sched.mExecFlags |= K2OSKERN_SCHED_CPUCORE_EXECFLAG_QUANTUM_ZERO;

    if (gData.Sched.mpActiveItemThread->Sched.State.mThreadRunState == KernThreadRunState_Running)
    {
        KernSched_StopThread(gData.Sched.mpActiveItemThread, NULL, KernThreadState_Ready, TRUE);
    }

    //
    // thread is guaranteed to not be running
    //



    K2_ASSERT(gData.Sched.mpActiveItemThread->Sched.State.mThreadRunState == KernThreadRunState_Ready);




    if (gData.Sched.mpActiveItem->Args.ThreadWait.mTimeoutMs == 0)
    {
        K2_ASSERT(pWait->mNumEntries != 0);




        //
        // nothing thread was testing for wait was signalled, and no timeout. 
        // thread just gives up remainder of its timeslice
        //
        if (gData.Sched.mpActiveItemThread->Sched.State.mThreadRunState == KernThreadRunState_Running)
        {
            gData.Sched.mpActiveItemThread->Sched.mQuantumLeft = 0;
            KernSched_StopThread(gData.Sched.mpActiveItemThread, NULL, KernThreadState_Ready, TRUE);
            return TRUE;
        }

        gData.Sched.mpActiveItem->mResult = K2STAT_ERROR_TIMEOUT;
        return FALSE;
    }

    //
    // doing the wait, possibly with a timeout.  thread loses its quantum
    // here regardless
    //
    gData.Sched.mpActiveItemThread->Sched.mQuantumLeft = 0;

    gData.Sched.mpActiveItem->mResult = K2STAT_THREAD_WAITED;

    //
    // thread transitions to waiting state
    //
    KernSched_MakeThreadInactive(gData.Sched.mpActiveItemThread, KernThreadRunState_Waiting);

    pWait->mpWaitingThread = gData.Sched.mpActiveItemThread;

    //
    // mount the wait to the waitlists of the objects it is waiting on
    //
    for (ix = 0; ix < pWait->mNumEntries; ix++)
    {
        pEntry = &pWait->SchedWaitEntry[ix];
        sWaitList_Insert(pWait->mpWaitingThread, pEntry);
    }

    if (gData.Sched.mpActiveItem->Args.ThreadWait.mTimeoutMs != K2OS_TIMEOUT_INFINITE)
    {
        //
        // mount the timer item for the timeout.  This is done last in case the timeout
        // is already expired and the wait is completed
        //
        pWait->SchedTimerItem.mType = KernSchedTimerItemType_Wait;
        KernSched_AddTimerItem(
            &pWait->SchedTimerItem,
            gData.Sched.mpActiveItemThread->Sched.Item.CpuCoreEvent.mEventAbsTimeMs,
            gData.Sched.mpActiveItem->Args.ThreadWait.mTimeoutMs
        );
    }
    else
    {
        pWait->SchedTimerItem.mType = KernSchedTimerItemType_Error;
    }

    return TRUE;  // if something changes scheduling-wise, return true
}
