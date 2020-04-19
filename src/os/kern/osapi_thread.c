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

UINT32 K2_CALLCONV_CALLERCLEANS K2OS_ThreadGetOwnId(void)
{
    K2OSKERN_OBJ_THREAD *pThisThread;

    K2_ASSERT(gData.mKernInitStage >= KernInitStage_Threaded);

    pThisThread = K2OSKERN_CURRENT_THREAD;

    return pThisThread->Env.mId;
}

K2STAT K2_CALLCONV_CALLERCLEANS K2OS_ThreadGetStatus(void)
{
    K2OSKERN_OBJ_THREAD *pThisThread;

    K2_ASSERT(gData.mKernInitStage >= KernInitStage_Threaded);

    pThisThread = K2OSKERN_CURRENT_THREAD;

    return pThisThread->Env.mLastStatus;
}

void K2_CALLCONV_CALLERCLEANS K2OS_ThreadSetStatus(K2STAT aStatus)
{
    K2OSKERN_OBJ_THREAD *pThisThread;

    if (gData.mKernInitStage < KernInitStage_Threaded)
        return;

    pThisThread = K2OSKERN_CURRENT_THREAD;

    pThisThread->Env.mLastStatus = aStatus;
}

BOOL K2_CALLCONV_CALLERCLEANS K2OS_ThreadAllocLocal(UINT32 *apRetSlotId)
{
    K2STAT                  stat;
    BOOL                    result;
    BOOL                    ok;
    UINT32                  slotBit;
    K2OSKERN_OBJ_THREAD *   pThisThread;
    K2OSKERN_OBJ_PROCESS *  pThisProc;

    K2_ASSERT(gData.mKernInitStage >= KernInitStage_Threaded);

    do {
        if (NULL == apRetSlotId)
        {
            stat = K2STAT_ERROR_BAD_ARGUMENT;
            break;
        }

        pThisThread = K2OSKERN_CURRENT_THREAD;
        pThisProc = pThisThread->mpProc;

        ok = K2OS_CritSecEnter(&pThisProc->TlsMaskSec);
        K2_ASSERT(ok);

        if (pThisProc->mTlsMask == (UINT32)-1)
        {
            stat = K2STAT_ERROR_OUT_OF_RESOURCES;
        }
        else
        {
            slotBit = K2BIT_GetLowestOnly32(~pThisProc->mTlsMask);
            pThisProc->mTlsMask |= slotBit;
            stat = K2STAT_NO_ERROR;
        }

        ok = K2OS_CritSecLeave(&pThisProc->TlsMaskSec);
        K2_ASSERT(ok);

        if (K2STAT_IS_ERROR(stat))
            break;

        K2BIT_GetLowestPos32(slotBit, apRetSlotId);

    } while (0);

    result = (!K2STAT_IS_ERROR(stat));
    if (!result)
        K2OS_ThreadSetStatus(stat);

    return result;
}

BOOL K2_CALLCONV_CALLERCLEANS K2OS_ThreadFreeLocal(UINT32 aSlotId)
{
    K2STAT                  stat;
    BOOL                    result;
    UINT32                  slotBit;
    BOOL                    ok;
    K2OSKERN_OBJ_THREAD *   pThisThread;
    K2OSKERN_OBJ_PROCESS *  pThisProc;
    K2LIST_LINK *           pLink;
    K2OSKERN_OBJ_THREAD *   pOtherThread;

    K2_ASSERT(gData.mKernInitStage >= KernInitStage_Threaded);

    do {
        if (aSlotId >= 32)
        {
            stat = K2STAT_ERROR_BAD_ARGUMENT;
            break;
        }

        slotBit = (1 << aSlotId);

        pThisThread = K2OSKERN_CURRENT_THREAD;
        pThisProc = pThisThread->mpProc;

        if (0 == (pThisProc->mTlsMask & slotBit))
        {
            stat = K2STAT_ERROR_NOT_IN_USE;
            break;
        }

        ok = K2OS_CritSecEnter(&pThisProc->TlsMaskSec);
        K2_ASSERT(ok);

        if (0 == (pThisProc->mTlsMask & slotBit))
        {
            stat = K2STAT_ERROR_NOT_IN_USE;
        }
        else
        {
            pThisProc->mTlsMask &= ~slotBit;
            stat = K2STAT_NO_ERROR;

            //
            // clear value in slots in all threads for the process
            //
            ok = K2OSKERN_SeqIntrLock(&pThisProc->ThreadListSeqLock);
            pLink = pThisProc->ThreadList.mpHead;
            K2_ASSERT(pLink != NULL);
            do {
                pOtherThread = K2_GET_CONTAINER(K2OSKERN_OBJ_THREAD, pLink, ProcThreadListLink);
                pOtherThread->Env.mTlsValue[aSlotId] = 0;
                pLink = pLink->mpNext;
            } while (pLink != NULL);
            K2OSKERN_SeqIntrUnlock(&pThisProc->ThreadListSeqLock, ok);
        }

        ok = K2OS_CritSecLeave(&pThisProc->TlsMaskSec);
        K2_ASSERT(ok);

    } while (0);

    result = (!K2STAT_IS_ERROR(stat));
    if (!result)
        K2OS_ThreadSetStatus(stat);

    return result;
}

BOOL K2_CALLCONV_CALLERCLEANS K2OS_ThreadSetLocal(UINT32 aSlotId, UINT32 aValue)
{
    K2STAT                  stat;
    BOOL                    result;
    UINT32                  slotBit;
    BOOL                    ok;
    K2OSKERN_OBJ_THREAD *   pThisThread;
    K2OSKERN_OBJ_PROCESS *  pThisProc;

    K2_ASSERT(gData.mKernInitStage >= KernInitStage_Threaded);

    do {
        if (aSlotId >= 32)
        {
            stat = K2STAT_ERROR_BAD_ARGUMENT;
            break;
        }

        slotBit = (1 << aSlotId);

        pThisThread = K2OSKERN_CURRENT_THREAD;
        pThisProc = pThisThread->mpProc;

        if (0 == (pThisProc->mTlsMask & slotBit))
        {
            stat = K2STAT_ERROR_NOT_IN_USE;
            break;
        }

        ok = K2OS_CritSecEnter(&pThisProc->TlsMaskSec);
        K2_ASSERT(ok);

        if (0 == (pThisProc->mTlsMask & slotBit))
        {
            stat = K2STAT_ERROR_NOT_IN_USE;
        }
        else
        {
            pThisThread->Env.mTlsValue[aSlotId] = aValue;
            stat = K2STAT_NO_ERROR;
        }

        ok = K2OS_CritSecLeave(&pThisProc->TlsMaskSec);
        K2_ASSERT(ok);

    } while (0);

    result = (!K2STAT_IS_ERROR(stat));
    if (!result)
        K2OS_ThreadSetStatus(stat);

    return result;
}

BOOL K2_CALLCONV_CALLERCLEANS K2OS_ThreadGetLocal(UINT32 aSlotId, UINT32 *apRetValue)
{
    K2STAT                  stat;
    BOOL                    result;
    UINT32                  slotBit;
    BOOL                    ok;
    K2OSKERN_OBJ_THREAD *   pThisThread;
    K2OSKERN_OBJ_PROCESS *  pThisProc;

    K2_ASSERT(gData.mKernInitStage >= KernInitStage_Threaded);

    do {
        if (aSlotId >= 32)
        {
            stat = K2STAT_ERROR_BAD_ARGUMENT;
            break;
        }

        if (apRetValue == NULL)
        {
            stat = K2STAT_ERROR_BAD_ARGUMENT;
            break;
        }

        slotBit = (1 << aSlotId);

        pThisThread = K2OSKERN_CURRENT_THREAD;
        pThisProc = pThisThread->mpProc;

        ok = K2OS_CritSecEnter(&pThisProc->TlsMaskSec);
        K2_ASSERT(ok);

        if (0 == (pThisProc->mTlsMask & slotBit))
        {
            stat = K2STAT_ERROR_NOT_IN_USE;
        }
        else
        {
            *apRetValue = pThisThread->Env.mTlsValue[aSlotId];
            stat = K2STAT_NO_ERROR;
        }

        ok = K2OS_CritSecLeave(&pThisProc->TlsMaskSec);
        K2_ASSERT(ok);

    } while (0);

    result = (!K2STAT_IS_ERROR(stat));
    if (!result)
        K2OS_ThreadSetStatus(stat);

    return result;
}

BOOL K2_CALLCONV_CALLERCLEANS K2OS_ThreadGetInfo(K2OS_TOKEN aThreadToken, K2OS_THREADINFO *apRetInfo)
{
    K2STAT                  stat;
    K2STAT                  stat2;
    K2OSKERN_OBJ_THREAD *   pThreadObj;

    if (aThreadToken == NULL)
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_BAD_TOKEN);
        return FALSE;
    }

    if (apRetInfo == NULL)
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_BAD_ARGUMENT);
        return FALSE;
    }

    if (apRetInfo->mStructBytes < sizeof(K2OS_THREADINFO))
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_TOO_SMALL);
        return FALSE;
    }

    stat = KernTok_TranslateToAddRefObjs(1, &aThreadToken, (K2OSKERN_OBJ_HEADER **)&pThreadObj);
    if (!K2STAT_IS_ERROR(stat))
    {
        if (pThreadObj->Hdr.mObjType == K2OS_Obj_Thread)
        {
            K2MEM_Copy(apRetInfo, &pThreadObj->Info, sizeof(K2OS_THREADINFO));

            if (pThreadObj->Sched.State.mStopFlags != 0)
                apRetInfo->mState = K2OS_ThreadState_Stopped;
            else if (pThreadObj->Sched.State.mLifeStage >= KernThreadLifeStage_Exited)
                apRetInfo->mState = K2OS_ThreadState_Dead;
            else
            {
                if (pThreadObj->Sched.State.mRunState == KernThreadRunState_Running)
                    apRetInfo->mState = K2OS_ThreadState_Running;
                else if (pThreadObj->Sched.State.mRunState == KernThreadRunState_Ready)
                    apRetInfo->mState = K2OS_ThreadState_Ready;
                else
                    apRetInfo->mState = K2OS_ThreadState_Waiting;
            }
        }
        else
            stat = K2STAT_ERROR_BAD_TOKEN;

        stat2 = KernObj_Release(&pThreadObj->Hdr);
        K2_ASSERT(!K2STAT_IS_ERROR(stat2));
    }

    if (K2STAT_IS_ERROR(stat))
    {
        K2OS_ThreadSetStatus(stat);
        return FALSE;
    }

    return TRUE;
}

BOOL K2_CALLCONV_CALLERCLEANS K2OS_ThreadGetOwnInfo(K2OS_THREADINFO *apRetInfo)
{
    K2OSKERN_OBJ_THREAD * pThisThread;

    if (apRetInfo == NULL)
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_BAD_ARGUMENT);
        return FALSE;
    }

    if (apRetInfo->mStructBytes < sizeof(K2OS_THREADINFO))
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_TOO_SMALL);
        return FALSE;
    }

    pThisThread = K2OSKERN_CURRENT_THREAD;

    K2MEM_Copy(apRetInfo, &pThisThread->Info, sizeof(K2OS_THREADINFO));

    if (pThisThread->Sched.State.mStopFlags != 0)
        apRetInfo->mState = K2OS_ThreadState_Stopped;
    else if (pThisThread->Sched.State.mLifeStage >= KernThreadLifeStage_Exited)
        apRetInfo->mState = K2OS_ThreadState_Dead;
    else
    {
        if (pThisThread->Sched.State.mRunState == KernThreadRunState_Running)
            apRetInfo->mState = K2OS_ThreadState_Running;
        else if (pThisThread->Sched.State.mRunState == KernThreadRunState_Ready)
            apRetInfo->mState = K2OS_ThreadState_Ready;
        else
            apRetInfo->mState = K2OS_ThreadState_Waiting;
    }

    return TRUE;
}

void   K2_CALLCONV_CALLERCLEANS K2OS_ThreadExit(UINT32 aExitCode)
{
    K2OSKERN_OBJ_THREAD * pThisThread;

    pThisThread = K2OSKERN_CURRENT_THREAD;

    pThisThread->Info.mExitCode = aExitCode;

    K2OSKERN_Debug("Thread(%d) calling scheduler for exit with Code %08X\n", pThisThread->Env.mId, pThisThread->Info.mExitCode);

    pThisThread->Sched.Item.mSchedItemType = KernSchedItem_ThreadExit;
    pThisThread->Sched.Item.Args.ThreadExit.mNormal = FALSE;

    KernArch_ThreadCallSched();

    //
    // should never return from exitthread call
    //

    K2OSKERN_Panic("Thread %d resumed after exit trap!\n", pThisThread->Env.mId);
}

BOOL K2_CALLCONV_CALLERCLEANS K2OS_ThreadKill(K2OS_TOKEN aThreadToken, UINT32 aForcedExitCode)
{
    K2STAT                  stat;
    K2STAT                  stat2;
    K2OSKERN_OBJ_THREAD *   pThreadObj;

    if (aThreadToken == NULL)
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_BAD_TOKEN);
        return FALSE;
    }

    stat = KernTok_TranslateToAddRefObjs(1, &aThreadToken, (K2OSKERN_OBJ_HEADER **)&pThreadObj);
    if (!K2STAT_IS_ERROR(stat))
    {
        if (pThreadObj->Hdr.mObjType == K2OS_Obj_Thread)
        {
            stat = KernThread_Kill(pThreadObj, aForcedExitCode);
        }
        else
        {
            stat = K2STAT_ERROR_BAD_TOKEN;
        }

        stat2 = KernObj_Release(&pThreadObj->Hdr);
        K2_ASSERT(!K2STAT_IS_ERROR(stat2));
    }

    if (K2STAT_IS_ERROR(stat))
    {
        K2OS_ThreadSetStatus(stat);
        return FALSE;
    }

    return TRUE;
}

BOOL   K2_CALLCONV_CALLERCLEANS K2OS_ThreadSetAttr(K2OS_TOKEN aThreadToken, K2OS_THREADATTR const *apAttr)
{
    K2STAT                  stat;
    K2STAT                  stat2;
    K2OSKERN_OBJ_THREAD *   pThreadObj;
    K2OS_THREADATTR         newAttr;

    if (aThreadToken == NULL)
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_BAD_TOKEN);
        return FALSE;
    }

    if (apAttr == NULL)
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_BAD_ARGUMENT);
        return FALSE;
    }

    K2MEM_Copy(&newAttr, apAttr, sizeof(K2OS_THREADATTR));

    stat = KernTok_TranslateToAddRefObjs(1, &aThreadToken, (K2OSKERN_OBJ_HEADER **)&pThreadObj);
    if (!K2STAT_IS_ERROR(stat))
    {
        if (pThreadObj->Hdr.mObjType == K2OS_Obj_Thread)
        {
            if (newAttr.mFieldMask & K2OS_THREADATTR_PRIORITY)
            {
                if (newAttr.mPriority == pThreadObj->Sched.mBasePrio)
                    newAttr.mFieldMask &= ~K2OS_THREADATTR_PRIORITY;
            }

            if (newAttr.mFieldMask & K2OS_THREADATTR_AFFINITYMASK)
            {
                if (newAttr.mPriority == pThreadObj->Sched.Attr.mAffinityMask)
                    newAttr.mFieldMask &= ~K2OS_THREADATTR_AFFINITYMASK;
            }

            if (newAttr.mFieldMask & K2OS_THREADATTR_QUANTUM)
            {
                if (newAttr.mPriority == pThreadObj->Sched.Attr.mQuantum)
                    newAttr.mFieldMask &= ~K2OS_THREADATTR_QUANTUM;
            }

            if ((newAttr.mFieldMask & K2OS_THREADATTR_VALID_MASK) == 0)
                stat = K2STAT_NO_ERROR;
            else
            {
                stat = KernThread_SetAttr(pThreadObj, &newAttr);
            }
        }
        else
        {
            stat = K2STAT_ERROR_BAD_TOKEN;
        }

        stat2 = KernObj_Release(&pThreadObj->Hdr);
        K2_ASSERT(!K2STAT_IS_ERROR(stat2));
    }

    if (K2STAT_IS_ERROR(stat))
    {
        K2OS_ThreadSetStatus(stat);
        return FALSE;
    }

    return TRUE;
}

BOOL   K2_CALLCONV_CALLERCLEANS K2OS_ThreadGetAttr(K2OS_TOKEN aThreadToken, K2OS_THREADATTR *apRetAttr)
{
    K2STAT                  stat;
    K2STAT                  stat2;
    K2OSKERN_OBJ_THREAD *   pThreadObj;
    K2OS_THREADATTR         curAttr;

    if (aThreadToken == NULL)
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_BAD_TOKEN);
        return FALSE;
    }

    if (apRetAttr == NULL)
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_BAD_ARGUMENT);
        return FALSE;
    }

    K2MEM_Zero(apRetAttr, sizeof(K2OS_THREADATTR));

    stat = KernTok_TranslateToAddRefObjs(1, &aThreadToken, (K2OSKERN_OBJ_HEADER **)&pThreadObj);
    if (!K2STAT_IS_ERROR(stat))
    {
        if (pThreadObj->Hdr.mObjType == K2OS_Obj_Thread)
        {
            K2MEM_Copy(&curAttr, &pThreadObj->Sched.Attr, sizeof(K2OS_THREADATTR));
            curAttr.mPriority = pThreadObj->Sched.mBasePrio;
            curAttr.mFieldMask = K2OS_THREADATTR_VALID_MASK;
        }
        else
        {
            stat = K2STAT_ERROR_BAD_TOKEN;
        }

        stat2 = KernObj_Release(&pThreadObj->Hdr);
        K2_ASSERT(!K2STAT_IS_ERROR(stat2));
    }

    if (K2STAT_IS_ERROR(stat))
    {
        K2OS_ThreadSetStatus(stat);
        return FALSE;
    }

    K2MEM_Copy(apRetAttr, &curAttr, sizeof(curAttr));

    return TRUE;
}

BOOL   K2_CALLCONV_CALLERCLEANS K2OS_ThreadSetOwnAttr(K2OS_THREADATTR const *apAttr)
{
    K2STAT                  stat;
    K2OSKERN_OBJ_THREAD *   pThisThread;
    K2OS_THREADATTR         newAttr;
    
    if (apAttr == NULL)
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_BAD_ARGUMENT);
        return FALSE;
    }

    pThisThread = K2OSKERN_CURRENT_THREAD;

    K2MEM_Copy(&newAttr, apAttr, sizeof(K2OS_THREADATTR));

    if (newAttr.mFieldMask & K2OS_THREADATTR_PRIORITY)
    {
        if (newAttr.mPriority == pThisThread->Sched.mBasePrio)
            newAttr.mFieldMask &= ~K2OS_THREADATTR_PRIORITY;
    }

    if (newAttr.mFieldMask & K2OS_THREADATTR_AFFINITYMASK)
    {
        if (newAttr.mPriority == pThisThread->Sched.Attr.mAffinityMask)
            newAttr.mFieldMask &= ~K2OS_THREADATTR_AFFINITYMASK;
    }

    if (newAttr.mFieldMask & K2OS_THREADATTR_QUANTUM)
    {
        if (newAttr.mPriority == pThisThread->Sched.Attr.mQuantum)
            newAttr.mFieldMask &= ~K2OS_THREADATTR_QUANTUM;
    }

    if ((newAttr.mFieldMask & K2OS_THREADATTR_VALID_MASK) == 0)
    {
        stat = K2STAT_NO_ERROR;
    }
    else
    {
        stat = KernThread_SetAttr(pThisThread, &newAttr);
        if (K2STAT_IS_ERROR(stat))
        {
            K2OS_ThreadSetStatus(stat);
            return FALSE;
        }
    }

    return TRUE;
}

BOOL   K2_CALLCONV_CALLERCLEANS K2OS_ThreadGetOwnAttr(K2OS_THREADATTR *apRetAttr)
{
    K2OSKERN_OBJ_THREAD *   pThisThread;

    if (apRetAttr == NULL)
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_BAD_ARGUMENT);
        return FALSE;
    }

    pThisThread = K2OSKERN_CURRENT_THREAD;

    K2MEM_Copy(apRetAttr, &pThisThread->Sched.Attr, sizeof(K2OS_THREADATTR));

    apRetAttr->mPriority = pThisThread->Sched.mBasePrio;
    apRetAttr->mFieldMask = K2OS_THREADATTR_VALID_MASK;

    return TRUE;
}

K2OS_TOKEN  K2_CALLCONV_CALLERCLEANS K2OS_ThreadAcquireByName(K2OS_TOKEN aNameToken)
{
    return KernTok_CreateNoAddRef_FromNamedObject(aNameToken, K2OS_Obj_Thread);
}

K2OS_TOKEN  K2_CALLCONV_CALLERCLEANS K2OS_ThreadAcquireById(UINT32 aThreadId)
{
    BOOL                    disp;
    K2LIST_LINK *           pProcScan;
    K2OSKERN_OBJ_PROCESS *  pProcess;
    K2LIST_LINK *           pThreadScan;
    K2OSKERN_OBJ_THREAD *   pThread;
    K2STAT                  stat;
    K2OSKERN_OBJ_HEADER *   pRefObj;
    K2OS_TOKEN              tokThread;

    if (aThreadId == 0)
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_BAD_ARGUMENT);
        return NULL;
    }

    disp = K2OSKERN_SeqIntrLock(&gData.ProcListSeqLock);

    pProcScan = gData.ProcList.mpHead;
    K2_ASSERT(pProcScan != NULL);

    do {
        pProcess = K2_GET_CONTAINER(K2OSKERN_OBJ_PROCESS, pProcScan, ProcListLink);

        K2OSKERN_SeqIntrLock(&pProcess->ThreadListSeqLock);
        
        pThreadScan = pProcess->ThreadList.mpHead;

        while (pThreadScan != NULL)
        {
            pThread = K2_GET_CONTAINER(K2OSKERN_OBJ_THREAD, pThreadScan, ProcThreadListLink);
            if (pThread->Env.mId == aThreadId)
            {
                stat = KernObj_AddRef(&pThread->Hdr);
                K2_ASSERT(!K2STAT_IS_ERROR(stat));
                break;
            }

            pThreadScan = pThreadScan->mpNext;
        }

        K2OSKERN_SeqIntrUnlock(&pProcess->ThreadListSeqLock, FALSE);

        if (pThreadScan != NULL)
            break;

        pProcScan = pProcScan->mpNext;
    } while (pProcScan != NULL);

    K2OSKERN_SeqIntrUnlock(&gData.ProcListSeqLock, disp);

    if (pThreadScan == NULL)
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_FOUND);
        return NULL;
    }

    //
    // reference to thread was added above. this is for the new token
    // if it is able to be created
    //
    pRefObj = &pThread->Hdr;
    stat = KernTok_CreateNoAddRef(1, &pRefObj, &tokThread);
    if (K2STAT_IS_ERROR(stat))
    {
        stat = KernObj_Release(pRefObj);
        K2_ASSERT(!K2STAT_IS_ERROR(stat));

        K2OS_ThreadSetStatus(stat);
        return NULL;
    }

    return tokThread;
}

void K2_CALLCONV_CALLERCLEANS K2OS_ThreadSleep(UINT32 aMilliseconds)
{
    K2OSKERN_SCHED_MACROWAIT    wait;
    K2OSKERN_OBJ_THREAD *       pThisThread;
    K2STAT                      stat;

    K2_ASSERT(aMilliseconds != K2OS_TIMEOUT_INFINITE);

    pThisThread = K2OSKERN_CURRENT_THREAD;

    wait.mNumEntries = 0;
    wait.mWaitAll = FALSE;

    pThisThread->Sched.Item.mSchedItemType = KernSchedItem_ThreadWait;
    pThisThread->Sched.Item.Args.ThreadWait.mpMacroWait = &wait;
    pThisThread->Sched.Item.Args.ThreadWait.mTimeoutMs = aMilliseconds;
    
    KernArch_ThreadCallSched();

    stat = pThisThread->Sched.Item.mResult;
    if (K2STAT_IS_ERROR(stat))
    {
        K2OS_ThreadSetStatus(stat);
    }
}

static BOOL sCheckNoWait(K2OSKERN_OBJ_THREAD *apThisThread, K2OSKERN_OBJ_WAITABLE aObjWait, UINT32 *apResult)
{
    switch (aObjWait.mpHdr->mObjType)
    {
    case K2OS_Obj_Event:
        return aObjWait.mpEvent->mIsSignaled;

    case K2OS_Obj_Name:
        return aObjWait.mpName->Event_IsOwned.mIsSignaled;

    case K2OS_Obj_Process:
        return aObjWait.mpProc->mState >= KernProcState_Done;

    case K2OS_Obj_Thread:
        if (aObjWait.mpThread == apThisThread)
        {
            *apResult = K2STAT_ERROR_BAD_ARGUMENT;
            return TRUE;
        }
        return aObjWait.mpThread->Sched.State.mLifeStage >= KernThreadLifeStage_Exited;

    case K2OS_Obj_Semaphore:
        if (gData.mKernInitStage < KernInitStage_MultiThreaded)
        {
            //
            // very special case - changing the object and returning no wait
            //
            K2_ASSERT(aObjWait.mpSem->mCurCount > 0);
            aObjWait.mpSem->mCurCount--;
            return TRUE;
        }
        *apResult = K2STAT_THREAD_WAITED;
        return FALSE;

//    case K2OS_Obj_Alarm:
//    case K2OS_Obj_RwLock:
//    case K2OS_Obj_Mailbox:
//    case K2OS_Obj_Mailslot:
//    case K2OS_Obj_Msg:
//    case K2OS_Obj_RwLockReq:
    default:
        K2_ASSERT(0);
        break;
    }

    return FALSE;
}

UINT32 K2_CALLCONV_CALLERCLEANS K2OS_ThreadWaitOne(K2OS_TOKEN aToken, UINT32 aTimeoutMs)
{
    K2OSKERN_SCHED_MACROWAIT    wait;
    K2OSKERN_OBJ_THREAD *       pThisThread;
    K2OSKERN_OBJ_WAITABLE       objWait;
    K2STAT                      stat;
    UINT32                      result;

    pThisThread = K2OSKERN_CURRENT_THREAD;

    result = KernTok_TranslateToAddRefObjs(1, &aToken, (K2OSKERN_OBJ_HEADER **)&objWait);
    if (K2STAT_IS_ERROR(result))
    {
        K2OS_ThreadSetStatus(result);
        return result;
    }

    do {
        result = K2OS_WAIT_SIGNALLED_0;
        if (sCheckNoWait(pThisThread, objWait, &result))
            break;

        wait.mNumEntries = 1; 
        wait.mWaitAll = FALSE;
        wait.SchedWaitEntry[0].mWaitObj.mpHdr = objWait.mpHdr;
        pThisThread->Sched.Item.mSchedItemType = KernSchedItem_ThreadWait;
        pThisThread->Sched.Item.Args.ThreadWait.mpMacroWait = &wait;
        pThisThread->Sched.Item.Args.ThreadWait.mTimeoutMs = aTimeoutMs;
        KernArch_ThreadCallSched();

        result = pThisThread->Sched.Item.mResult;
        if (K2STAT_IS_ERROR(result))
        {
            K2OS_ThreadSetStatus(result);
        }

    } while (0);

    stat = KernObj_Release(objWait.mpHdr);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));

    return result;
}

UINT32 K2_CALLCONV_CALLERCLEANS K2OS_ThreadWaitAny(UINT32 aTokenCount, K2OS_TOKEN *apTokenArray, UINT32 aTimeoutMs)
{
    if (aTokenCount == 0)
    {
        K2OS_ThreadSleep(aTimeoutMs);
        return K2STAT_THREAD_WAITED;
    }

    if (apTokenArray == NULL)
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_BAD_ARGUMENT);
        return K2STAT_ERROR_BAD_ARGUMENT;
    }

    if (aTokenCount == 1)
    {
        return K2OS_ThreadWaitOne(apTokenArray[0], aTimeoutMs);
    }

    //
    // trying to wait on multiple things
    //
    K2_ASSERT(0);
    K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_IMPL);
    return K2STAT_ERROR_TIMEOUT;
}

UINT32 K2_CALLCONV_CALLERCLEANS K2OS_ThreadWaitAll(UINT32 aTokenCount, K2OS_TOKEN *apTokenArray, UINT32 aTimeoutMs)
{
    if (aTokenCount == 0)
    {
        K2OS_ThreadSleep(aTimeoutMs);
        return K2STAT_THREAD_WAITED;
    }

    if (apTokenArray == NULL)
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_BAD_ARGUMENT);
        return K2STAT_ERROR_BAD_ARGUMENT;
    }

    if (aTokenCount == 1)
    {
        return K2OS_ThreadWaitOne(apTokenArray[0], aTimeoutMs);
    }

    //
    // trying to wait on multiple things
    //
    K2_ASSERT(0);
    K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_IMPL);
    return K2STAT_ERROR_TIMEOUT;
}

BOOL   K2_CALLCONV_CALLERCLEANS K2OS_ThreadSetPauseLock(BOOL aSetLock)
{
    K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_IMPL);
    return FALSE;
}

BOOL   K2_CALLCONV_CALLERCLEANS K2OS_ThreadPause(K2OS_TOKEN aThreadToken, UINT32 *apRetPauseCount, UINT32 aTimeoutMs)
{
    K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_IMPL);
    return FALSE;
}

BOOL   K2_CALLCONV_CALLERCLEANS K2OS_ThreadResume(K2OS_TOKEN aThreadToken, UINT32 *apRetPauseCount)
{
    K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_IMPL);
    return FALSE;
}

K2OS_TOKEN K2_CALLCONV_CALLERCLEANS K2OS_ThreadCreate(K2OS_THREADCREATE const *apCreate)
{
    K2OSKERN_OBJ_THREAD *   pThisThread;
    K2OSKERN_OBJ_SEGMENT *  pSeg;
    K2OSKERN_THREAD_PAGE *  pNewThreadPage;
    K2OSKERN_OBJ_THREAD *   pNewThread;
    K2OS_THREADCREATE       cret;
    K2STAT                  stat;
    K2OS_TOKEN              tokThread;
    K2OSKERN_OBJ_HEADER *   pObjHdr;

    K2_ASSERT(gData.mKernInitStage >= KernInitStage_MemReady);

    if (apCreate->mStructBytes < sizeof(K2OS_THREADCREATE))
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_BAD_ARGUMENT);
        return NULL;
    }

    pThisThread = K2OSKERN_CURRENT_THREAD;

    K2MEM_Copy(&cret, apCreate, sizeof(cret));

    if (cret.mStackPages == 0)
        cret.mStackPages = K2OS_THREAD_DEFAULT_STACK_PAGES;

    if (0 == (cret.Attr.mFieldMask & K2OS_THREADATTR_PRIORITY))
    {
        cret.Attr.mPriority = pThisThread->Sched.mBasePrio;
        cret.Attr.mFieldMask |= K2OS_THREADATTR_PRIORITY;
    }

    if (0 == (cret.Attr.mFieldMask & K2OS_THREADATTR_AFFINITYMASK))
    {
        cret.Attr.mAffinityMask = pThisThread->Sched.Attr.mAffinityMask;
        cret.Attr.mFieldMask |= K2OS_THREADATTR_AFFINITYMASK;
    }

    if (0 == (cret.Attr.mFieldMask & K2OS_THREADATTR_QUANTUM))
    {
        cret.Attr.mQuantum = K2OS_THREAD_DEFAULT_QUANTUM;
        cret.Attr.mFieldMask |= K2OS_THREADATTR_QUANTUM;
    }

    pNewThreadPage = NULL;
    pNewThread = NULL;

    stat = KernMem_SegAllocToThread(pThisThread);
    if (!K2STAT_IS_ERROR(stat))
    {
        do {
            stat = KernMem_VirtAllocToThread(pThisThread, 0, cret.mStackPages + 1, FALSE);
            if (K2STAT_IS_ERROR(stat))
                break;

            do {
                stat = KernMem_PhysAllocToThread(pThisThread, cret.mStackPages, KernPhys_Disp_Cached, FALSE);
                if (K2STAT_IS_ERROR(stat))
                    break;

                do {
                    pSeg = pThisThread->mpWorkSeg;

                    //
                    // these are not mapped yet
                    //
                    pNewThreadPage = (K2OSKERN_THREAD_PAGE *)
                        (pThisThread->mWorkVirt_Range + ((pThisThread->mWorkVirt_PageCount - 1) * K2_VA32_MEMPAGE_BYTES));
                    pNewThread = (K2OSKERN_OBJ_THREAD *)&pNewThreadPage->Thread;

                    K2MEM_Zero(pSeg, sizeof(K2OSKERN_OBJ_SEGMENT));
                    pSeg->Hdr.mObjType = K2OS_Obj_Segment;
                    pSeg->Hdr.mRefCount = 1;
                    K2LIST_Init(&pSeg->Hdr.WaitingThreadsPrioList);
                    pSeg->mSegAndMemPageAttr = K2OSKERN_SEG_ATTR_TYPE_THREAD | K2OS_MAPTYPE_KERN_DATA;
                    pSeg->Info.Thread.mpThread = pNewThread;

                    tokThread = NULL;

                    stat = KernMem_CreateSegmentFromThread(pThisThread, pSeg, NULL);
                    if (!K2STAT_IS_ERROR(stat))
                    {
                        K2_ASSERT(pThisThread->mpWorkSeg == NULL);
                        //
                        // creating a *thread* segment will add it to the owner
                        // process and move it to the thread's create segment pointer
                        //
                        K2_ASSERT(pThisThread->mpThreadCreateSeg != NULL);
                    }

                } while (0);

                if (K2STAT_IS_ERROR(stat))
                {
                    KernMem_PhysFreeFromThread(pThisThread);
                }

            } while (0);

            if (K2STAT_IS_ERROR(stat))
            {
                KernMem_VirtFreeFromThread(pThisThread);
            }

        } while (0);

        if (K2STAT_IS_ERROR(stat))
        {
            KernMem_SegFreeFromThread(pThisThread);
            K2_ASSERT(pThisThread->mpWorkSeg == NULL);
        }
    }

    if (K2STAT_IS_ERROR(stat))
    {
        K2OS_ThreadSetStatus(stat);
        return NULL;
    }

    //
    // if we get here, the thread segment is created
    //
    K2_ASSERT(pThisThread->mpThreadCreateSeg != NULL);
    K2_ASSERT(pNewThreadPage != NULL);
    K2_ASSERT(pNewThread == &pNewThreadPage->Thread);
    tokThread = NULL;

    KernThread_Instantiate(pThisThread, pThisThread->mpProc, &cret);

    //
    // instantiated thread holds the segment reference at this point
    //
    K2_ASSERT(pThisThread->mpThreadCreateSeg == NULL);

    pObjHdr = &pNewThread->Hdr;
    stat = KernTok_CreateNoAddRef(1, &pObjHdr, &tokThread);
    if (!K2STAT_IS_ERROR(stat))
    {
        K2_ASSERT(tokThread != NULL);
        stat = KernThread_Start(pThisThread, pNewThread);
        if (K2STAT_IS_ERROR(stat))
        {
            //
            // for a non-started thread, the only reference
            // is the one we have here in creating it
            //
            K2OS_TokenDestroy(tokThread);
            tokThread = NULL;
        }
        //
        // thread holds a reference to itself once it is started
        // it also holds a reference to its own segment
        //
    }
    else
    {
        K2_ASSERT(tokThread == NULL);
        KernThread_Dispose(pNewThread);
    }

    if (K2STAT_IS_ERROR(stat))
    {
        K2_ASSERT(tokThread == NULL);
        K2OS_ThreadSetStatus(stat);
        return NULL;
    }

    return tokThread;
}

