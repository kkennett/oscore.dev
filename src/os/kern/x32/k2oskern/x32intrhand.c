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

#include "x32kern.h"

void
X32Kern_InterruptHandler(
    X32_EXCEPTION_CONTEXT aContext
    )
{
    K2OSKERN_CPUCORE *                  pThisCore;
    K2OSKERN_CPUCORE_EVENT volatile *   pCoreEvent;
    KernCpuCoreEventType                eventType;
    K2OSKERN_OBJ_THREAD *               pActiveThread;
    BOOL                                enterMonitor;

    pThisCore = K2OSKERN_GET_CURRENT_CPUCORE;
    enterMonitor = FALSE;

    if (aContext.Exception_IntNum >= X32KERN_INTR_ICI_BASE)
    {
        if (aContext.Exception_IntNum >= X32KERN_INTR_DEV_BASE)
        {
            //
            // this is an IRQ from a device
            //
            if (aContext.Exception_IntNum == X32KERN_INTR_LVT_TIMER)
            {
                //
                // 1ms tick
                //
                if (X32Kern_IntrTimerTick())
                {
                    pCoreEvent = &gData.Sched.SchedTimerSchedItem.CpuCoreEvent;
                    pCoreEvent->mEventType = KernCpuCoreEvent_SchedTimerFired;
                    pCoreEvent->mEventAbsTimeMs = K2OS_GetAbsTimeMs();
                    pCoreEvent->mSrcCoreIx = pThisCore->mCoreIx;

                    KernIntr_QueueCpuCoreEvent(pThisCore, pCoreEvent);

                    enterMonitor = TRUE;
                }
            }
            else
            {
                K2OSKERN_Debug("Non-Timer IRQ %d on core %d\n", aContext.Exception_IntNum, pThisCore->mCoreIx);
                KernIntr_RecvIrq(pThisCore, aContext.Exception_IntNum);
            }
        }
        else
        {
            //
            // this is an ICI from another core
            //
            K2_ASSERT(aContext.Exception_IntNum - X32KERN_INTR_ICI_BASE < gData.mCpuCount);

            pCoreEvent = &pThisCore->IciFromOtherCore[aContext.Exception_IntNum - X32KERN_INTR_ICI_BASE];
            eventType = pCoreEvent->mEventType;

            K2_ASSERT((eventType >= KernCpuCoreEvent_Ici_Wakeup) && (eventType <= KernCpuCoreEvent_Ici_Debug));

            KernIntr_QueueCpuCoreEvent(pThisCore, pCoreEvent);
            enterMonitor = TRUE;
        }
    }
    else
    {
        K2OSKERN_Debug("Exception %d on Core %d, errorcode %d\n", aContext.Exception_IntNum, pThisCore->mCoreIx, aContext.Exception_ErrorCode);
        KernIntr_Exception(aContext.Exception_IntNum);
    }

    if (enterMonitor)
    {
        pActiveThread = pThisCore->mpActiveThread;
        if (pActiveThread != NULL)
        {
            pActiveThread->mStackPtr_Kernel = (UINT32)&aContext;
        }

        pThisCore->mIsInMonitor = TRUE;

        X32Kern_EnterMonitor(pThisCore->TSS.mESP0);
    }
}

