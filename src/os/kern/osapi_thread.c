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

        ok = K2OS_CritSecEnter(&pThisProc->ThreadListSec);
        K2_ASSERT(ok);
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
            pLink = pThisProc->ThreadList.mpHead;
            K2_ASSERT(pLink != NULL);
            do {
                pOtherThread = K2_GET_CONTAINER(K2OSKERN_OBJ_THREAD, pLink, ProcThreadListLink);
                pOtherThread->Env.mTlsValue[aSlotId] = 0;
                pLink = pLink->mpNext;
            } while (pLink != NULL);
        }

        ok = K2OS_CritSecLeave(&pThisProc->TlsMaskSec);
        K2_ASSERT(ok);
        ok = K2OS_CritSecLeave(&pThisProc->ThreadListSec);
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

BOOL   K2_CALLCONV_CALLERCLEANS K2OS_ThreadCreate(K2OS_THREADCREATE const *apCreate, K2OS_TOKEN *apRetThreadToken)
{
    K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_IMPL);
    return FALSE;
}

BOOL   K2_CALLCONV_CALLERCLEANS K2OS_ThreadGetInfo(K2OS_TOKEN aThreadToken, K2OS_THREADINFO *apRetInfo)
{
    K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_IMPL);
    return FALSE;
}

BOOL   K2_CALLCONV_CALLERCLEANS K2OS_ThreadGetOwnInfo(K2OS_THREADINFO *apRetInfo)
{
    K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_IMPL);
    return FALSE;
}

BOOL   K2_CALLCONV_CALLERCLEANS K2OS_ThreadAcquire(UINT32 aThreadId, K2OS_TOKEN *apRetThreadToken)
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

void   K2_CALLCONV_CALLERCLEANS K2OS_ThreadExit(UINT32 aExitCode)
{
    K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_IMPL);
}

BOOL   K2_CALLCONV_CALLERCLEANS K2OS_ThreadKill(K2OS_TOKEN aThreadToken, UINT32 aForcedExitCode)
{
    K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_IMPL);
    return FALSE;
}

BOOL   K2_CALLCONV_CALLERCLEANS K2OS_ThreadSetAttr(K2OS_TOKEN aThreadToken, K2OS_THREADATTR const *apAttr)
{
    K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_IMPL);
    return FALSE;
}

BOOL   K2_CALLCONV_CALLERCLEANS K2OS_ThreadGetAttr(K2OS_TOKEN aThreadToken, K2OS_THREADATTR *apRetAttr)
{
    K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_IMPL);
    return FALSE;
}

BOOL   K2_CALLCONV_CALLERCLEANS K2OS_ThreadSetOwnAttr(K2OS_THREADATTR const *apInfo)
{
    K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_IMPL);
    return FALSE;
}

BOOL   K2_CALLCONV_CALLERCLEANS K2OS_ThreadGetOwnAttr(K2OS_THREADATTR *apRetInfo)
{
    K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_IMPL);
    return FALSE;
}

BOOL   K2_CALLCONV_CALLERCLEANS K2OS_ThreadSetPauseLock(BOOL aSetLock)
{
    K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_IMPL);
    return FALSE;
}

void   K2_CALLCONV_CALLERCLEANS K2OS_ThreadSleep(UINT32 aMilliseconds)
{
    K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_IMPL);
}

BOOL   K2_CALLCONV_CALLERCLEANS K2OS_ThreadWaitOne(K2OS_TOKEN aToken, UINT32 aTimeoutMs)
{
    K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_IMPL);
    return FALSE;
}

UINT32 K2_CALLCONV_CALLERCLEANS K2OS_ThreadWaitAny(UINT32 aTokenCount, K2OS_TOKEN *apTokenArray, UINT32 aTimeoutMs)
{
    K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_IMPL);
    return 0;
}

BOOL   K2_CALLCONV_CALLERCLEANS K2OS_ThreadWaitAll(UINT32 aTokenCount, K2OS_TOKEN *apTokenArray, UINT32 aTimeoutMs)
{
    K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_IMPL);
    return FALSE;
}

