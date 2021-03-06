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

BOOL KernSched_Exec_ThreadStop(void)
{
    K2OS_MSGIO                  msgIo;
    K2LIST_LINK *               pListLink;
    UINT32                      workPrio;
    K2OSKERN_OBJ_THREAD *       pThread;
    K2OSKERN_OBJ_THREAD *       pReadyThread;
    K2OSKERN_CPUCORE volatile * pCore;
    K2STAT                      stat;

    K2_ASSERT(gData.Sched.mpActiveItem->mSchedItemType == KernSchedItem_ThreadStop);

    pThread = gData.Sched.mpActiveItemThread;

    pThread->Sched.mQuantumLeft = 0;

    //
    // we are stopping this thread, and need to give the core something
    // else to do.  if the core goes idle we just stop the thread but other wise we
    // preempt it with something
    //
    K2_ASSERT(pThread->Sched.mLastRunCoreIx < gData.mCpuCount);
    pCore = K2OSKERN_COREIX_TO_CPUCORE(pThread->Sched.mLastRunCoreIx);

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
        KernSched_StopThread(pThread, pCore, KernThreadRunState_Stopped, TRUE);
    }
    else
    {
        KernSched_PreemptCore(pCore, pThread, KernThreadRunState_Stopped, pReadyThread);
    }

    //
    // tell the exec thread has stopped
    //
    msgIo.mOpCode = SYSMSG_OPCODE_THREAD_STOP;
    msgIo.mPayload[0] = (UINT32)pThread;
    K2_ASSERT(NULL != gData.mpMsgBox_K2OSEXEC);
    KernSchedEx_MsgSend(gData.mpMsgBox_K2OSEXEC, &pThread->MsgSvc, &msgIo, &stat);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));

    return TRUE;
}
