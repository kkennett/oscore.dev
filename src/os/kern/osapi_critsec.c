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

BOOL K2_CALLCONV_CALLERCLEANS K2OS_CritSecInit(K2OS_CRITSEC *apSec)
{
    K2STAT              stat;
    K2OSKERN_CRITSEC *  pSec;

    K2_ASSERT(gData.mKernInitStage > KernInitStage_dlx_entry);
    K2_ASSERT(apSec != NULL);
    K2_ASSERT(((UINT32)apSec) >= K2OS_KVA_KERN_BASE);
    if (((NULL == apSec) || (((UINT32)apSec)) < K2OS_KVA_KERN_BASE))
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_BAD_ARGUMENT);
        return FALSE;
    }

    pSec = (K2OSKERN_CRITSEC *)apSec;

    K2MEM_Zero(pSec, sizeof(K2OSKERN_CRITSEC));

    K2OSKERN_SeqIntrInit(&pSec->SeqLock);

    stat = KernEvent_Create(&pSec->Event, NULL, TRUE, TRUE);
    if (K2STAT_IS_ERROR(stat))
        return FALSE;

    pSec->Event.Hdr.mObjFlags |= K2OSKERN_OBJ_FLAG_EMBEDDED;

    return TRUE;
}

BOOL K2_CALLCONV_CALLERCLEANS K2OS_CritSecTryEnter(K2OS_CRITSEC *apSec)
{
    K2OSKERN_OBJ_THREAD *   pThisThread;
    K2OSKERN_CRITSEC *      pSec;
    BOOL                    disp;
    BOOL                    gotIntoSec;

    if (FALSE == K2OSKERN_GetIntr())
        K2OSKERN_Panic("Interrupts disabled at CritSecTryEnter!\n");

    K2_ASSERT(gData.mKernInitStage >= KernInitStage_Threaded);
    K2_ASSERT(apSec != NULL);
    K2_ASSERT(((UINT32)apSec) >= K2OS_KVA_KERN_BASE);
    if (((NULL == apSec) || (((UINT32)apSec)) < K2OS_KVA_KERN_BASE))
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_BAD_ARGUMENT);
        return FALSE;
    }

    pThisThread = K2OSKERN_CURRENT_THREAD;
    pSec = (K2OSKERN_CRITSEC *)apSec;

    if (pSec->mpOwner == pThisThread)
    {
        pSec->mRecursionCount++;
        return TRUE;
    }

    disp = K2OSKERN_SeqIntrLock(&pSec->SeqLock);
    if (pSec->Event.mIsSignalled)
    {
        //
        // auto-reset event is signalled. better not be anything waiting.
        //
        K2_ASSERT(pSec->Event.Hdr.WaitEntryPrioList.mNodeCount == 0);
        pSec->Event.mIsSignalled = FALSE;
        gotIntoSec = TRUE;
    }
    else
        gotIntoSec = FALSE;
    K2OSKERN_SeqIntrUnlock(&pSec->SeqLock, disp);

    if (gotIntoSec)
    {
        pSec->mpOwner = pThisThread;
        K2LIST_AddAtHead(&pThisThread->Sched.OwnedCritSecList, &pSec->ThreadOwnedCritSecLink);
        pSec->mRecursionCount = 1;
        return TRUE;
    }

    K2OS_ThreadSetStatus(K2STAT_ERROR_OWNED);
    return FALSE;
}

BOOL K2_CALLCONV_CALLERCLEANS K2OS_CritSecEnter(K2OS_CRITSEC *apSec)
{
    K2OSKERN_OBJ_THREAD *   pThisThread;
    K2OSKERN_CRITSEC *      pSec;
    BOOL                    disp;
    BOOL                    gotIntoSec;
    UINT32                  result;
    K2OSKERN_OBJ_HEADER *   pHdr;

    if (FALSE == K2OSKERN_GetIntr())
        K2OSKERN_Panic("Interrupts disabled at CritSecEnter!\n");

    K2_ASSERT(gData.mKernInitStage >= KernInitStage_Threaded);
    K2_ASSERT(apSec != NULL);
    K2_ASSERT(((UINT32)apSec) >= K2OS_KVA_KERN_BASE);
    if (((NULL == apSec) || (((UINT32)apSec)) < K2OS_KVA_KERN_BASE))
    {
        K2_ASSERT(0);
        K2OS_ThreadSetStatus(K2STAT_ERROR_BAD_ARGUMENT);
        return FALSE;
    }

    pThisThread = K2OSKERN_CURRENT_THREAD;
    pSec = (K2OSKERN_CRITSEC *)apSec;

    if (pSec->mpOwner == pThisThread)
    {
        pSec->mRecursionCount++;
        return TRUE;
    }

    disp = K2OSKERN_SeqIntrLock(&pSec->SeqLock);
    if (pSec->Event.mIsSignalled)
    {
        //
        // auto-reset event is signalled. better not be anything waiting.
        //
        K2_ASSERT(pSec->Event.Hdr.WaitEntryPrioList.mNodeCount == 0);
        pSec->Event.mIsSignalled = FALSE;
        gotIntoSec = TRUE;
    }
    else
    {
        K2Trace(K2TRACE_THREAD_SEC_WAIT, 1, pThisThread->Env.mId);
        pSec->mWaitingThreadsCount++;
        gotIntoSec = FALSE;
    }
    K2OSKERN_SeqIntrUnlock(&pSec->SeqLock, disp);

    if (!gotIntoSec)
    {
        pHdr = &pSec->Event.Hdr;
        result = KernThread_Wait(1, &pHdr, FALSE, K2OS_TIMEOUT_INFINITE);
        if (result == K2OS_WAIT_SIGNALLED_0)
        {
            gotIntoSec = TRUE;
        }

        //
        // wait will be error if section was abandoned (owner called 
        // critsec_done while holding the sec and people waiting on it)
        // if that's the case then the sec is GONE
        //
        if (result != K2OS_WAIT_ERROR)
        {
            disp = K2OSKERN_SeqIntrLock(&pSec->SeqLock);
            pSec->mWaitingThreadsCount--;
            K2OSKERN_SeqIntrUnlock(&pSec->SeqLock, disp);
        }
        else
            pSec = NULL;
    }

    if (gotIntoSec)
    {
        K2Trace(K2TRACE_THREAD_ENTERED_SEC, 2, pThisThread->Env.mId, pSec);
        pSec->mpOwner = pThisThread;
        K2LIST_AddAtHead(&pThisThread->Sched.OwnedCritSecList, &pSec->ThreadOwnedCritSecLink);
        pSec->mRecursionCount = 1;
        return TRUE;
    }

    return FALSE;
}

BOOL K2_CALLCONV_CALLERCLEANS K2OS_CritSecLeave(K2OS_CRITSEC *apSec)
{
    K2OSKERN_OBJ_THREAD *   pThisThread;
    K2OSKERN_CRITSEC *      pSec;
    BOOL                    disp;
    K2STAT                  stat;
    BOOL                    leftSec;

    if (FALSE == K2OSKERN_GetIntr())
        K2OSKERN_Panic("Interrupts disabled at CritSecLeave!\n");

    K2_ASSERT(gData.mKernInitStage >= KernInitStage_Threaded);
    K2_ASSERT(apSec != NULL);
    K2_ASSERT(((UINT32)apSec) >= K2OS_KVA_KERN_BASE);
    if (((NULL == apSec) || (((UINT32)apSec)) < K2OS_KVA_KERN_BASE))
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_BAD_ARGUMENT);
        return FALSE;
    }

    pThisThread = K2OSKERN_CURRENT_THREAD;
    pSec = (K2OSKERN_CRITSEC *)apSec;

    if (pSec->mpOwner != pThisThread)
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_OWNED);
        return FALSE;
    }

    K2_ASSERT(pSec->Event.mIsSignalled == FALSE);

    if (0 != --pSec->mRecursionCount)
    {
        return TRUE;
    }

    pSec->mpOwner = NULL;
    K2LIST_Remove(&pThisThread->Sched.OwnedCritSecList, &pSec->ThreadOwnedCritSecLink);

    disp = K2OSKERN_SeqIntrLock(&pSec->SeqLock);
    if (pSec->mWaitingThreadsCount == 0)
    {
        pSec->Event.mIsSignalled = TRUE;
        leftSec = TRUE;
    }
    else
    {
        leftSec = FALSE;
    }
    K2OSKERN_SeqIntrUnlock(&pSec->SeqLock, disp);

    if (!leftSec)
    {
        stat = KernEvent_Change(&pSec->Event, TRUE);
        K2_ASSERT(!K2STAT_IS_ERROR(stat));
    }

    K2Trace(K2TRACE_THREAD_LEFT_SEC, 2, pThisThread->Env.mId, pSec);

    return TRUE;
}

BOOL K2_CALLCONV_CALLERCLEANS K2OS_CritSecDone(K2OS_CRITSEC *apSec)
{
    K2OSKERN_CRITSEC *  pSec;
    BOOL                ok;
    K2STAT              stat;

    ok = K2OS_CritSecEnter(apSec);
    if (!ok)
        return FALSE;

    //
    // critsec done in the kernel must be done carefully
    // by the critsec users. all users except the one calling
    // done must have been shut down and not use the sec
    // any more.  There are two asserts here trying to catch
    // cases where other users are waiting either before or
    // after the sec event is destroyed.  if the second one
    // fires then somebody is using the sec after their wait
    // has failed
    //

    pSec = (K2OSKERN_CRITSEC *)apSec;

    ok = K2OSKERN_SeqIntrLock(&pSec->SeqLock);
    
    K2_ASSERT(pSec->mWaitingThreadsCount == 0);
    
    K2OSKERN_SeqIntrUnlock(&pSec->SeqLock, ok);

    //
    // this should make anything waiting on the sec fail their wait
    // with K2OS_WAIT_ERROR
    //
    stat = KernObj_Release(&pSec->Event.Hdr);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));

    ok = K2OSKERN_SeqIntrLock(&pSec->SeqLock);

    K2_ASSERT(pSec->mWaitingThreadsCount == 0);

    K2OSKERN_SeqIntrUnlock(&pSec->SeqLock, ok);

    K2MEM_Zero(pSec, sizeof(K2OSKERN_CRITSEC));

    return TRUE;
}


