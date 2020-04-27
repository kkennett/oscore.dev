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

BOOL KernSched_Exec_ThreadExit(void)
{
    BOOL                        changedSomething;
    BOOL                        msgRes;
    K2OSKERN_OBJ_THREAD *       pExitingThread;
    K2OS_MSGIO                  msgIo;
    K2STAT                      stat;
    K2OSKERN_SCHED_WAITENTRY *  pEntry;
    K2OSKERN_SCHED_MACROWAIT *  pWait;
    K2LIST_ANCHOR *             pAnchor;
    K2LIST_LINK *               pListLink;

    K2_ASSERT(gData.Sched.mpActiveItem->mSchedItemType == KernSchedItem_ThreadExit);

    pExitingThread = gData.Sched.mpActiveItemThread;

//    K2OSKERN_Debug("SCHED:Thread(%d) EXIT\n", pExitingThread->Env.mId);

    changedSomething = KernSched_MakeThreadInactive(pExitingThread, KernThreadRunState_Transition);

    pExitingThread->Sched.State.mLifeStage = KernThreadLifeStage_Exited;
    pExitingThread->Sched.State.mRunState = KernThreadRunState_None;

    //
    // thread is now signalled
    //
    if (pExitingThread->Hdr.WaitEntryPrioList.mNodeCount > 0)
    {
        pAnchor = &pExitingThread->Hdr.WaitEntryPrioList;
        K2_ASSERT(pAnchor->mNodeCount > 0);
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
    // tell the exec thread have exited (passes it the reference)
    //
    msgIo.mOpCode = SYSMSG_OPCODE_THREAD_EXIT;
    msgIo.mPayload[0] = (UINT32)pExitingThread;
    msgIo.mPayload[1] = 0;

    K2_ASSERT(NULL != gData.mpMsgSlot_K2OSEXEC);

    msgRes = KernSchedEx_MsgSend(
        gData.mpMsgSlot_K2OSEXEC, 
        &pExitingThread->MsgExit,
        &msgIo, 
        FALSE, 
        (K2OSKERN_OBJ_MSG **)&msgIo.mPayload[1], 
        &stat);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));

    if (msgRes)
        changedSomething = TRUE;

    return changedSomething;
}
