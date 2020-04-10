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
static UINT64 sgLastSchedTime = 0;

BOOL KernSched_TimePassed(void)
{
    BOOL    executedSomething;
    UINT64  newTick;
    UINT32  elapsed;

    executedSomething = FALSE;

    newTick = sgTickCounter;
    K2_CpuReadBarrier();

    if (newTick > sgLastSchedTime)
    {
        elapsed = (UINT32)(newTick - sgLastSchedTime);

        //
        // 'elapsed' milliseconds have elapsed
        //
        K2OSKERN_Debug("SCHED: Elapsed(%d)\n", elapsed);

        sgLastSchedTime = newTick;
    }
    
    //    K2OSKERN_Debug("SCHED: Time %d\n", (UINT32)aTimeNow);

    return executedSomething;
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

void KernSched_StartSysTick(K2OSKERN_INTR_CONFIG const * apConfig)
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
