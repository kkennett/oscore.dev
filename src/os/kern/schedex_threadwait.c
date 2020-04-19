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

#define K2OS_TIMEOUT_INFINITE   ((UINT32)-1)

#define K2OS_WAIT_MAX_TOKENS    64
#define K2OS_WAIT_SIGNALLED_0   0
#define K2OS_WAIT_ABANDONED_0   K2OS_WAIT_MAX_TOKENS
#define K2OS_WAIT_SUCCEEDED(x)  ((K2OS_WAIT_SIGNALLED_0 <= (x)) && ((x) < K2OS_WAIT_ABANDONED_0))

//    struct _K2OSKERN_SCHED_ITEM_ARGS_THREAD_WAIT
//        UINT32 mTimeoutMs;
//        UINT32 mMacroResult;

BOOL KernSched_Exec_ThreadWait(void)
{
    K2OSKERN_SCHED_MACROWAIT *  pWait;
    K2OSKERN_SCHED_WAITENTRY *  pEntry;
    K2OSKERN_OBJ_WAITABLE       objWait;
    UINT32                      ix;
    BOOL                        isSatisfied;

    K2_ASSERT(gData.Sched.mpActiveItem->mSchedItemType == KernSchedItem_ThreadWait);

    pWait = gData.Sched.mpActiveItem->Args.ThreadWait.mpMacroWait;

    K2_ASSERT(pWait->mNumEntries <= K2OS_WAIT_MAX_TOKENS);

    K2OSKERN_Debug("SCHED:Wait(%d, %d)\n", pWait->mNumEntries, gData.Sched.mpActiveItem->Args.ThreadWait.mTimeoutMs);

    isSatisfied = FALSE;

    for (ix = 0; ix < pWait->mNumEntries; ix++)
    {
        pEntry = &pWait->SchedWaitEntry[ix];
        objWait = pEntry->mWaitObj;

        //
        // check for wait already being satisfied
        //
        switch (objWait.mpHdr->mObjType)
        {
        case K2OS_Obj_Event:
            if (objWait.mpEvent->mIsSignaled)
            {
                if (objWait.mpEvent->mIsAutoReset)
                {
                    //
                    // nobody is waiting, or it would be signalled
                    //
                    objWait.mpEvent->mIsSignaled = FALSE;
                }
                isSatisfied = TRUE;
            }
            break;

        case K2OS_Obj_Name:
            if (objWait.mpName->Event_IsOwned.mIsSignaled)
            {
                isSatisfied = TRUE;
            }
            break;

        case K2OS_Obj_Process:
            if (objWait.mpProc->mState >= KernProcState_Done)
            {
                isSatisfied = TRUE;
            }
            break;

        case K2OS_Obj_Thread:
            if (objWait.mpThread->Info.mThreadState >= K2OS_Thread_Exited)
            {
                isSatisfied = TRUE;
            }
            break;

        case K2OS_Obj_Semaphore:
            if (objWait.mpSem->mCurCount > 0)
            {
                objWait.mpSem->mCurCount--;
                isSatisfied = TRUE;
            }
            break;

        default:
            K2_ASSERT(0);
            break;
        }
        
        if (isSatisfied)
            break;

        //
        // continue setup for wait on this item
        //
        pEntry->mMacroIndex = (UINT8)ix;
        pEntry->mStickyPulseStatus = 0;
    }

    if (isSatisfied)
    {
        gData.Sched.mpActiveItem->mResult = K2OS_WAIT_SIGNALLED_0 + ix;
        return FALSE;
    }

    //
    // if we get here, we are doing the wait unless timeout is zero
    //
    if (gData.Sched.mpActiveItem->Args.ThreadWait.mTimeoutMs == 0)
    {
        gData.Sched.mpActiveItem->mResult = K2STAT_ERROR_TIMEOUT;
        return FALSE;
    }

    //
    // doing the wait, with a timeout
    //
    K2_ASSERT(0);

    pWait->mpWaitingThread = gData.Sched.mpActiveItemThread;

    return TRUE;  // if something changes scheduling-wise, return true
}
