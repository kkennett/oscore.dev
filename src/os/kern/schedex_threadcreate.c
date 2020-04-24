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

BOOL KernSched_Exec_ThreadCreate(void)
{
    K2OSKERN_OBJ_THREAD *   pNewThread;

    K2_ASSERT(gData.Sched.mpActiveItem->mSchedItemType == KernSchedItem_ThreadCreate);

    pNewThread = gData.Sched.mpActiveItem->Args.ThreadCreate.mpThread;

    K2_ASSERT(pNewThread->Sched.State.mLifeStage == KernThreadLifeStage_Instantiated);

    K2_ASSERT(pNewThread->Sched.Attr.mPriority < K2OS_THREADPRIO_LEVELS);
    pNewThread->Sched.mBasePrio = pNewThread->Sched.Attr.mPriority;
    pNewThread->Sched.mThreadActivePrio = pNewThread->Sched.mBasePrio;
    K2_ASSERT(pNewThread->Sched.mThreadActivePrio < K2OS_THREADPRIO_LEVELS);

    K2_ASSERT(pNewThread->Sched.Attr.mQuantum > 0);
    K2_ASSERT(pNewThread->Sched.Attr.mAffinityMask != 0);

    //
    // a thread that is not in a purgeable state holds a reference to itself.
    //
    KernObj_AddRef(&pNewThread->Hdr);

    pNewThread->Env.mId = gData.Sched.mNextThreadId++;

    //
    // get ready to run
    //
    KernArch_PrepareThread(pNewThread);
    pNewThread->Sched.mLastRunCoreIx = gData.mCpuCount;

    //
    // thread life stage moves to started, it starts as ready, and is not stopped
    //
    K2_ASSERT(pNewThread->Sched.State.mLifeStage == KernThreadLifeStage_Run);
    K2_ASSERT(pNewThread->Sched.State.mRunState == KernThreadRunState_Transition);
    K2_ASSERT(pNewThread->Sched.State.mStopFlags == KERNTHREAD_STOP_FLAG_NONE);

    gData.Sched.mSysWideThreadCount++;

    //
    // make thread ready or running
    //
    KernSched_MakeThreadActive(pNewThread, TRUE);

    gData.Sched.mpActiveItem->mSchedCallResult = K2STAT_NO_ERROR;

    return TRUE;  // if something changes scheduling-wise, return true
}
