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

void KernMonitor_OneTimeInit(void)
{
    // 
    // any other initialization before core 0 
    // enters the monitor loop for the first
    // time
}

void KernMonitor_Run(void)
{
    K2OSKERN_CPUCORE *      pThisCore;
    K2OSKERN_OBJ_THREAD *   pThread;

    pThisCore = K2OSKERN_GET_CURRENT_CPUCORE;
    K2_ASSERT(pThisCore->mIsExecuting);
    K2_ASSERT(pThisCore->mIsInMonitor);

    if (pThisCore->mIsIdle)
    {
        pThisCore->mIsIdle = FALSE;
//        K2OSKERN_Debug("!Core %d: Leave idle\n", pThisCore->mCoreIx);
    }
//    else
//        K2OSKERN_Debug("!Core %d: Enter monitor\n", pThisCore->mCoreIx);

    /* interrupts MUST BE ON running here */
#ifdef K2_DEBUG
    if (FALSE == K2OSKERN_GetIntr())
        K2OSKERN_Panic("Interrupts disabled in monitor!\n");
#endif

    do
    {
        KernCpuCore_DrainEvents(pThisCore);

        KernSched_Check(pThisCore);

        if (pThisCore->mpPendingEventListHead == NULL)
        {
            K2OSKERN_SetIntr(FALSE);

            if (pThisCore->mpPendingEventListHead == NULL)
            {
                pThread = pThisCore->mpActiveThread;
                if (pThread == NULL)
                {
                    pThread = pThisCore->mpAssignThread;
                    if (pThread != NULL)
                    {
                        pThisCore->mpActiveThread = pThread;
                        pThisCore->mpAssignThread = NULL;
                    }
                }
                else
                {
                    K2_ASSERT(pThisCore->mpAssignThread == NULL);
                }

                if (pThread == NULL)
                {
                    pThisCore->mIsIdle = TRUE;
//                    K2OSKERN_Debug("!Core %d: Enter idle\n", pThisCore->mCoreIx);
                    //
                    // interrupts are off. exiting from this function
                    // returns to the caller, which knows this and will
                    // go into the idle loop waiting for an interrupt at
                    // which point it will re-enter this function again
                    // 
                    return;
                }

                //
                // we have a thread to run, so return to it
                //
//                K2OSKERN_Debug("!Core %d run Thread %d\n", pThisCore->mCoreIx, pThread->Env.mId);
                KernArch_SwitchFromMonitorToThread(pThisCore);

                //
                // will never return here
                //
                K2OSKERN_Panic("Switching from monitor to thread returned!");
            }

            K2OSKERN_SetIntr(TRUE);
        }
    } while (1);

    //
    // should never get here
    //
    K2OSKERN_Panic("KernMonitor_Run ended!\n");
}
