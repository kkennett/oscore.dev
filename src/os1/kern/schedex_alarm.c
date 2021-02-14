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

BOOL KernSchedEx_AlarmFired(K2OSKERN_OBJ_ALARM *apAlarm)
{
    K2OSKERN_SCHED_WAITENTRY *  pEntry;
    K2OSKERN_SCHED_MACROWAIT *  pWait;
    K2LIST_ANCHOR *             pAnchor;
    K2LIST_LINK *               pListLink;
    BOOL                        changedSomething;

    changedSomething = FALSE;

    //
    // alarm timer item has been removed from the timer queue
    //

    if (!apAlarm->mIsPeriodic)
        apAlarm->mIsSignalled = TRUE;

    pAnchor = &apAlarm->Hdr.WaitEntryPrioList;
    if (pAnchor->mNodeCount > 0)
    {
        pListLink = pAnchor->mpHead;

        do {
            pEntry = K2_GET_CONTAINER(K2OSKERN_SCHED_WAITENTRY, pListLink, WaitPrioListLink);
            pWait = K2_GET_CONTAINER(K2OSKERN_SCHED_MACROWAIT, pEntry, SchedWaitEntry[pEntry->mMacroIndex]);

            pListLink = pListLink->mpNext;

            if (pWait->mWaitAll)
            {
                //
                // this *MAY* remove pEntry from the list.  we have already moved 
                // the link to the next link
                //
                if (KernSched_CheckSignalOne_SatisfyAll(pWait, pEntry))
                    changedSomething = TRUE;
            }
            else
            {
                //
                // this will remove pEntry from the list.  we have already moved 
                // the link to the next link
                //
                KernSched_EndThreadWait(pWait, K2OS_WAIT_SIGNALLED_0 + pEntry->mMacroIndex);
                changedSomething = TRUE;
            }
        } while (pListLink != NULL);
    }

    //
    // last thing we do is re-add the timer item to the timer queue if it is periodic
    //
    if (apAlarm->mIsPeriodic)
    {
        //
        // this can't fire immediately because we are using the current time and
        // the interval will be > 0
        //
        KernSched_AddTimerItem(&apAlarm->SchedTimerItem, gData.Sched.mCurrentAbsTime, (UINT64)apAlarm->mIntervalMs);
    }

    return changedSomething;
}

BOOL KernSched_Exec_AlarmChange(void)
{
    K2OSKERN_OBJ_ALARM *    pAlarm;
    BOOL                    isMount;
    BOOL                    changedSomething;

    K2_ASSERT(gData.Sched.mpActiveItem->mSchedItemType == KernSchedItem_AlarmChange);

    pAlarm = gData.Sched.mpActiveItem->Args.AlarmChange.mpAlarm;
    isMount = gData.Sched.mpActiveItem->Args.AlarmChange.mIsMount;

    K2OSKERN_Debug("SCHED:AlarmChange(%08X,%d)\n", pAlarm, isMount);

    changedSomething = FALSE;

    if (isMount)
    {
        if (pAlarm->mIsMounted)
        {
            gData.Sched.mpActiveItem->mSchedCallResult = K2STAT_ERROR_IN_USE;
        }
        else
        {
            K2_ASSERT(pAlarm->mIntervalMs != 0);
            K2_ASSERT(pAlarm->mIntervalMs != K2OS_TIMEOUT_INFINITE);

            pAlarm->SchedTimerItem.mType = KernSchedTimerItemType_Alarm;

            changedSomething = KernSched_AddTimerItem(
                &pAlarm->SchedTimerItem,
                gData.Sched.mpActiveItem->CpuCoreEvent.mEventAbsTimeMs,
                (UINT64)pAlarm->mIntervalMs
            );

            pAlarm->mIsMounted = TRUE;

            gData.Sched.mpActiveItem->mSchedCallResult = K2STAT_NO_ERROR;
        }

        return changedSomething;
    }

    if (pAlarm->mIsMounted)
    {
        if (!pAlarm->mIsMounted)
        {
            gData.Sched.mpActiveItem->mSchedCallResult = K2STAT_ERROR_NOT_IN_USE;
        }
        else
        {
            if (!pAlarm->mIsSignalled)
            {
                KernSched_DelTimerItem(&pAlarm->SchedTimerItem);
            }

            pAlarm->mIsMounted = FALSE;

            gData.Sched.mpActiveItem->mSchedCallResult = K2STAT_NO_ERROR;
        }
    }

    return FALSE;
}
