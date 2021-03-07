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
KernNotify_Init(
    K2OSKERN_OBJ_NOTIFY *apNotify
)
{
    K2OSKERN_SeqInit(&apNotify->Lock);
    apNotify->Locked.mState = KernNotifyState_Idle;
    K2LIST_Init(&apNotify->Locked.WaitingThreadList);
    apNotify->Locked.mDataWord = 0;
}

UINT32
KernNotify_Signal(
    K2OSKERN_OBJ_NOTIFY *   apNotify,
    UINT32                  aSignalBits
)
{
    K2OSKERN_OBJ_THREAD *   pReleasedThread;
    UINT32                  result;

    pReleasedThread = NULL;

    K2OSKERN_SeqLock(&apNotify->Lock);

    if (apNotify->Locked.mState != KernNotifyState_Waiting)
    {
        K2_ASSERT(0 == apNotify->Locked.WaitingThreadList.mNodeCount);

        if (apNotify->Locked.mState == KernNotifyState_Idle)
        {
            apNotify->Locked.mDataWord = aSignalBits;
            apNotify->Locked.mState = KernNotifyState_Active;
        }
        else
        {
            // active. received more notify info
            apNotify->Locked.mDataWord |= aSignalBits;
        }

        result = apNotify->Locked.mDataWord;
    }
    else
    {
        K2_ASSERT(0 != apNotify->Locked.WaitingThreadList.mNodeCount);

        pReleasedThread = K2_GET_CONTAINER(K2OSKERN_OBJ_THREAD, apNotify->Locked.WaitingThreadList.mpHead, NotifyWaitListLink);

        K2LIST_Remove(&apNotify->Locked.WaitingThreadList, &pReleasedThread->NotifyWaitListLink);

        K2_ASSERT(KernThreadState_WaitingOnNotify == pReleasedThread->mState);

        result = aSignalBits;

        if (0 == apNotify->Locked.WaitingThreadList.mNodeCount)
        {
            apNotify->Locked.mState = KernNotifyState_Idle;
        }
    }

    K2OSKERN_SeqUnlock(&apNotify->Lock);

    if (NULL != pReleasedThread)
    {
        pReleasedThread->mSysCall_Result = result;
        pReleasedThread->mSysCall_Status = K2STAT_THREAD_WAITED;
        pReleasedThread->mState = KernThreadState_Migrating;
        KernCpu_MigrateThread((UINT32)-1, pReleasedThread);
    }

    return result;
}

