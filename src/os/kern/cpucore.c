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

static void sInit_AtDlxEntry(void)
{
    UINT32              coreIx;
    K2OSKERN_CPUCORE *  pCore;

    K2_ASSERT(gData.mCpuCount > 0);
    K2_ASSERT(gData.mCpuCount <= K2OS_MAX_CPU_COUNT);

    for (coreIx = 0; coreIx < gData.mCpuCount; coreIx++)
    {
        pCore = K2OSKERN_COREIX_TO_CPUCORE(coreIx);
        K2MEM_Zero(pCore, sizeof(K2OSKERN_CPUCORE));
        pCore->mCoreIx = coreIx;
        K2_CpuWriteBarrier();
    }
}

void KernInit_CpuCore(void)
{
    switch (gData.mKernInitStage)
    {
    case KernInitStage_dlx_entry:
        sInit_AtDlxEntry();
        break;

    default:
        break;
    }
}

static
void
sProcessOneCpuCoreEvent(
    K2OSKERN_CPUCORE *      apThisCore,
    KernCpuCoreEventType    aEventType,
    UINT64                  aEventTime
)
{
//    K2OSKERN_Debug("!Core %d: Event %d\n", apThisCore->mCoreIx,aEventType);

    switch (aEventType)
    {
    case KernCpuCoreEvent_SchedulerCall:
        KernSched_RespondToCallFromThread(apThisCore);
        break;
    case KernCpuCoreEvent_SchedTimerFired:
        KernSched_TimerFired(apThisCore);
        break;
    case KernCpuCoreEvent_Ici_Wakeup:
        //
        // no-op. we just brought the core out of idle into its monitor
        //
        break;
    case KernCpuCoreEvent_Ici_Stop:
        if (apThisCore->mpActiveThread != NULL)
        {
            apThisCore->Sched.mLastStopAbsTimeMs = aEventTime;
            apThisCore->mpActiveThread->Sched.mAbsTimeAtStop = aEventTime;
            apThisCore->mpActiveThread = NULL;
            K2_CpuWriteBarrier();
        }
        break;
    case KernCpuCoreEvent_Ici_TlbInv:
        KernSched_PerCpuTlbInvEvent(apThisCore);
        break;
    case KernCpuCoreEvent_Ici_PageDirUpdate:
        K2_ASSERT(0);
        break;
    case KernCpuCoreEvent_Ici_Panic:
        KernPanic_Ici(apThisCore);
        break;
    case KernCpuCoreEvent_Ici_Debug:
        K2_ASSERT(0);
        break;
    default:
        K2_ASSERT(0);
        break;
    }
}

void KernCpuCore_DrainEvents(K2OSKERN_CPUCORE *apThisCore)
{
    K2OSKERN_CPUCORE_EVENT volatile *   pEvent;
    K2OSKERN_CPUCORE_EVENT volatile *   pEventList;
    K2OSKERN_CPUCORE_EVENT volatile *   pEventListEnd;
    K2OSKERN_CPUCORE_EVENT volatile *   pPendNew;
    K2OSKERN_CPUCORE_EVENT volatile *   pIns;
    KernCpuCoreEventType                eventType;
    UINT64                              eventTime;

    pEventList = pEventListEnd = NULL;
    do
    {
        //
        // get new pending events and move them to the end of the working list
        //
        pPendNew = (K2OSKERN_CPUCORE_EVENT *)K2ATOMIC_Exchange(
            (volatile UINT32 *)&apThisCore->mpPendingEventListHead, 0);

        if (pPendNew != NULL)
        {
            //
            // invert the new events order and stick at the end
            // of the current event list
            //
            pIns = pEventListEnd;
            if (pIns != NULL)
            {
                do
                {
                    pEvent = pPendNew;
                    pPendNew = pPendNew->mpNext;
                    pEvent->mpNext = pIns->mpNext;
                    pIns->mpNext = pEvent;
                    if (pIns == pEventListEnd)
                        pEventListEnd = pEvent;
                } while (pPendNew != NULL);
            }
            else
            {
                pEventListEnd = pPendNew;
                do
                {
                    pEvent = pPendNew;
                    pPendNew = pPendNew->mpNext;
                    pEvent->mpNext = pEventList;
                    pEventList = pEvent;
                } while (pPendNew != NULL);
            }
        }

        //
        // pull next cpucore event from event list
        //
        pEvent = pEventList;
        if (pEvent == NULL)
            break;

        pEventList = pEvent->mpNext;
        if (pEventList == NULL)
            pEventListEnd = NULL;

        // 
        // get the event id and clear it.  For ICI events that have no data
        // associated with them this lets the sending core see the event
        // as 'received by target', where 'target' is this core
        //
        eventType = pEvent->mEventType;
        eventTime = pEvent->mEventAbsTimeMs;
        pEvent->mpNext = NULL;
        K2_ASSERT(eventType != KernCpuCoreEvent_None);
        K2_ASSERT(eventType < KernCpuCoreEventType_Count);
        pEvent->mEventType = KernCpuCoreEvent_None;
        K2_CpuWriteBarrier();

        sProcessOneCpuCoreEvent(apThisCore, eventType, eventTime);

    } while (1);
}

void KernCpuCore_SendIciToOneCore(K2OSKERN_CPUCORE *apThisCore, UINT32 aTargetCoreIx, KernCpuCoreEventType aEventType)
{
    K2OSKERN_CPUCORE volatile *         pTargetCore;
    K2OSKERN_CPUCORE_EVENT volatile *   pEvent;

    K2_ASSERT(aEventType >= KernCpuCoreEvent_Ici_Wakeup);
    K2_ASSERT(aEventType <= KernCpuCoreEvent_Ici_Debug);

    K2_ASSERT(aTargetCoreIx != apThisCore->mCoreIx);
    K2_ASSERT(aTargetCoreIx < gData.mCpuCount);

    pTargetCore = K2OSKERN_COREIX_TO_CPUCORE(aTargetCoreIx);

    pEvent = &pTargetCore->IciFromOtherCore[apThisCore->mCoreIx];

    //
    // must wait for target core to clear any backed up Ici from this core
    //
    do {
        K2_CpuReadBarrier();
    } while (pEvent->mEventType != KernCpuCoreEvent_None);

    pEvent->mEventType = aEventType;
    pEvent->mSrcCoreIx = apThisCore->mCoreIx;
    pEvent->mEventAbsTimeMs = K2OS_SysUpTimeMs();
    K2_CpuWriteBarrier();
    KernArch_SendIci(apThisCore->mCoreIx, TRUE, aTargetCoreIx);
}

void KernCpuCore_SendIciToAllOtherCores(K2OSKERN_CPUCORE *apThisCore, KernCpuCoreEventType aEventType)
{
    K2OSKERN_CPUCORE volatile *         pOtherCore;
    K2OSKERN_CPUCORE_EVENT volatile *   pEvent;
    UINT32                              coreIx;
    UINT64                              absTime;

    K2_ASSERT(aEventType >= KernCpuCoreEvent_Ici_Wakeup);
    K2_ASSERT(aEventType <= KernCpuCoreEvent_Ici_Debug);

    if (gData.mCpuCount == 1)
        return;

    for (coreIx = 0; coreIx < gData.mCpuCount; coreIx++)
    {
        if (coreIx != apThisCore->mCoreIx)
        {
            pOtherCore = K2OSKERN_COREIX_TO_CPUCORE(coreIx);
            pEvent = &pOtherCore->IciFromOtherCore[apThisCore->mCoreIx];

            // stall waiting for other core to clear backed up ICI
            do {
                K2_CpuReadBarrier();
            } while (pEvent->mEventType != KernCpuCoreEvent_None);
        }
    }

    absTime = K2OS_SysUpTimeMs();

    for (coreIx = 0; coreIx < gData.mCpuCount; coreIx++)
    {
        if (coreIx != apThisCore->mCoreIx)
        {
            pOtherCore = K2OSKERN_COREIX_TO_CPUCORE(coreIx);
            pEvent = &pOtherCore->IciFromOtherCore[apThisCore->mCoreIx];

            pEvent->mEventType = aEventType;
            pEvent->mSrcCoreIx = apThisCore->mCoreIx;
            pEvent->mEventAbsTimeMs = absTime;
        }
    }

    K2_CpuWriteBarrier();
    KernArch_SendIci(apThisCore->mCoreIx, FALSE, 0);
}
