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
    BOOL changedSomething;

    changedSomething = FALSE;

    if (apAlarm->mIsPeriodic)
    {
        if (KernSched_AddTimerItem(&apAlarm->SchedTimerItem, gData.Sched.mCurrentAbsTime, (UINT64)apAlarm->mIntervalMs))
            changedSomething = TRUE;
    }

    //
    // now release threads that were waiting on this alarm (if any)
    // for waitalls, the status of the alarm goes to the sticky status
    //
    K2_ASSERT(0);



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

    if (isMount)
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

        return changedSomething;
    }

    if (pAlarm->mIsMounted)
    {
        if (!pAlarm->mIsSignalled)
        {
            KernSched_DelTimerItem(&pAlarm->SchedTimerItem);
        }
        pAlarm->mIsMounted = FALSE;
    }

    return FALSE;
}
