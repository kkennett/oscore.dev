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

void KernThread_Dump(K2OSKERN_OBJ_THREAD *apThread)
{
    K2OSKERN_Debug("DUMP THREAD %08X\n", apThread);
}

static void sInit_BeforeVirt(void)
{
    K2OSKERN_OBJ_THREAD *   pThread;
    K2OSKERN_THREAD_PAGE *  pThreadPage;
    K2STAT                  stat;

    //
    // proc is initialized at dlx entry
    // we init thread right after to not conflict with whatever proc is doing
    // 
    pThread = gpThread0;
    K2MEM_Zero(pThread, sizeof(K2OSKERN_OBJ_THREAD));
    pThread->Hdr.mObjType = K2OS_Obj_Thread;
    pThread->Hdr.mObjFlags = K2OSKERN_OBJ_FLAG_PERMANENT;
    pThread->Hdr.mRefCount = 0x7FFFFFFF;
    pThread->mpProc = gpProc0;
    K2LIST_AddAtTail(&gpProc0->ThreadList, &pThread->ProcThreadListLink);
    pThread->mpStackSeg = &gpProc0->SegObjPrimaryThreadStack;
    pThread->Env.mId = 1;
    pThread->mIsKernelThread = TRUE;
    pThread->mIsInKernelMode = TRUE;
    pThread->Info.mStructBytes = sizeof(K2OS_THREADINFO);
    pThread->Info.CreateInfo.mStructBytes = sizeof(K2OS_THREADCREATE);
    pThread->Info.CreateInfo.mEntrypoint = K2OSKERN_Thread0;
    pThread->Info.CreateInfo.mStackPages = K2OS_KVA_THREAD0_PHYS_BYTES / K2_VA32_MEMPAGE_BYTES;
    pThread->Info.CreateInfo.Attr.mFieldMask = K2OS_THREADATTR_VALID_MASK;
    pThread->Info.CreateInfo.Attr.mPriority = K2OS_THREADPRIO_NORMAL;
    pThread->Info.CreateInfo.Attr.mAffinityMask = (1 << gData.mCpuCount) - 1;
    pThread->Info.CreateInfo.Attr.mQuantum = K2OS_THREAD_DEFAULT_QUANTUM;
    K2LIST_Init(&pThread->WorkPages_Dirty);
    K2LIST_Init(&pThread->WorkPages_Clean);
    K2LIST_Init(&pThread->WorkPtPages_Dirty);
    K2LIST_Init(&pThread->WorkPtPages_Clean);

    pThreadPage = K2OSKERN_THREAD_PAGE_FROM_THREAD(pThread);
    pThread->mStackPtr_Kernel = (UINT32)(&pThreadPage->mKernStack[K2OSKERN_THREAD_KERNSTACK_BYTECOUNT - 4]);

    stat = KernObj_Add(&pThread->Hdr, NULL);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));
}

void KernInit_Thread(void)
{
    switch (gData.mKernInitStage)
    {
    case KernInitStage_Before_Virt:
        sInit_BeforeVirt();
        break;

    default:
        break;
    }
}

void K2_CALLCONV_REGS KernThread_Entry(K2OSKERN_OBJ_THREAD *apThisThread)
{
    if (((UINT32)apThisThread->Info.CreateInfo.mEntrypoint) < K2OS_KVA_KERN_BASE)
    {
        K2_ASSERT(0);
    }

    apThisThread->Info.mExitCode = apThisThread->Info.CreateInfo.mEntrypoint(apThisThread->Info.CreateInfo.mpArg);

//    K2OSKERN_Debug("Thread(%d) calling scheduler for exit with Code %08X\n", apThisThread->Env.mId, apThisThread->Info.mExitCode);

    apThisThread->Sched.Item.mSchedItemType = KernSchedItem_ThreadExit;
    apThisThread->Sched.Item.Args.ThreadExit.mNormal = TRUE;

    KernArch_ThreadCallSched();

    //
    // should never return from exitthread call
    //

    K2OSKERN_Panic("Thread %d resumed after exit trap!\n", apThisThread->Env.mId);
}

K2STAT KernThread_Kill(K2OSKERN_OBJ_THREAD *apThread, UINT32 aForcedExitCode)
{
    K2_ASSERT(0);
    return K2STAT_ERROR_NOT_IMPL;
}

K2STAT KernThread_SetAttr(K2OSKERN_OBJ_THREAD *apThread, K2OS_THREADATTR const *apNewAttr)
{
    K2_ASSERT(0);
    return K2STAT_ERROR_NOT_IMPL;
}

K2STAT KernThread_Instantiate(K2OSKERN_OBJ_THREAD *apThisThread, K2OSKERN_OBJ_PROCESS *apProc, K2OS_THREADCREATE const *apCreate)
{
    K2OSKERN_THREAD_PAGE *  pNewThreadPage;
    K2OSKERN_OBJ_THREAD *   pNewThread;
    K2OSKERN_OBJ_SEGMENT *  pSeg;
    BOOL                    disp;
    K2STAT                  stat;
    
    pSeg = apThisThread->mpThreadCreateSeg;
    K2_ASSERT(pSeg != NULL);

    pNewThreadPage = (K2OSKERN_THREAD_PAGE *)
        (pSeg->ProcSegTreeNode.mUserVal + (pSeg->mPagesBytes - K2_VA32_MEMPAGE_BYTES));

    pNewThread = (K2OSKERN_OBJ_THREAD *)&pNewThreadPage->Thread;
    
    K2MEM_Zero(pNewThread, sizeof(K2OSKERN_OBJ_THREAD));

    K2_ASSERT(pSeg->Info.Thread.mpThread == pNewThread);

    pNewThread->Hdr.mObjType = K2OS_Obj_Thread;
    pNewThread->Hdr.mRefCount = 1;
    K2LIST_Init(&pNewThread->Hdr.WaitEntryPrioList);
    pNewThread->mpProc = apProc;
    pNewThread->mIsKernelThread = (((UINT32)apCreate->mEntrypoint) >= K2OS_KVA_KERN_BASE) ? TRUE : FALSE;
    pNewThread->mIsInKernelMode = TRUE;

    K2LIST_Init(&pNewThread->WorkPages_Dirty);
    K2LIST_Init(&pNewThread->WorkPages_Clean);
    K2LIST_Init(&pNewThread->WorkPtPages_Dirty);
    K2LIST_Init(&pNewThread->WorkPtPages_Clean);

    pNewThread->Info.mStructBytes = sizeof(K2OS_THREADINFO);
    pNewThread->Info.mProcessId = apProc->mId;
    K2MEM_Copy(&pNewThread->Info.CreateInfo, apCreate, sizeof(K2OS_THREADCREATE));

    stat = KernMsg_Create(&pNewThread->MsgExit);
    if (K2STAT_IS_ERROR(stat))
        return stat;
    pNewThread->MsgExit.Hdr.mObjFlags |= K2OSKERN_OBJ_FLAG_EMBEDDED;

    disp = K2OSKERN_SeqIntrLock(&gData.ProcListSeqLock);
    K2OSKERN_SeqIntrLock(&apProc->ThreadListSeqLock);

    K2LIST_AddAtTail(&apProc->ThreadList, &pNewThread->ProcThreadListLink);

    pNewThread->Sched.State.mLifeStage = KernThreadLifeStage_Instantiated;
    pNewThread->Sched.State.mRunState = KernThreadRunState_None;
    pNewThread->Sched.State.mStopFlags = KERNTHREAD_STOP_FLAG_NONE;

    pNewThread->Sched.Attr = apCreate->Attr;

    pNewThread->mpStackSeg = pSeg;
    apThisThread->mpThreadCreateSeg = NULL;

    //
    // threads can never have a name
    //
    stat = KernObj_Add(&pNewThread->Hdr, NULL);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));

    gData.Sched.mSysWideThreadCount++;

    K2OSKERN_SeqIntrUnlock(&apProc->ThreadListSeqLock, FALSE);
    K2OSKERN_SeqIntrUnlock(&gData.ProcListSeqLock, disp);

    return K2STAT_NO_ERROR;
}

K2STAT KernThread_Start(K2OSKERN_OBJ_THREAD *apThisThread, K2OSKERN_OBJ_THREAD *apThread)
{
    K2_ASSERT(apThread->Sched.State.mLifeStage == KernThreadLifeStage_Instantiated);
    K2_ASSERT(apThread->Sched.State.mRunState == KernThreadRunState_None);
    K2_ASSERT(apThread->Sched.State.mStopFlags == KERNTHREAD_STOP_FLAG_NONE);

    apThisThread->Sched.Item.mSchedItemType = KernSchedItem_ThreadCreate;
    apThisThread->Sched.Item.Args.ThreadCreate.mpThread = apThread;
    
    KernArch_ThreadCallSched();

    return apThisThread->Sched.Item.mSchedCallResult;
}

K2STAT KernThread_Dispose(K2OSKERN_OBJ_THREAD *apThread)
{
    BOOL                    disp;
    K2OSKERN_OBJ_PROCESS *  pProc;
    K2OSKERN_OBJ_SEGMENT *  pSeg;

    K2_ASSERT(apThread != NULL);
    K2_ASSERT(apThread->Hdr.mObjType == K2OS_Obj_Thread);
    K2_ASSERT(apThread->Hdr.mRefCount == 0);
    K2_ASSERT(!(apThread->Hdr.mObjFlags & K2OSKERN_OBJ_FLAG_PERMANENT));
    K2_ASSERT(apThread->Hdr.WaitEntryPrioList.mNodeCount == 0);
    K2_ASSERT(apThread->WorkPages_Dirty.mNodeCount == 0);
    K2_ASSERT(apThread->WorkPages_Clean.mNodeCount == 0);
    K2_ASSERT(apThread->WorkPtPages_Dirty.mNodeCount == 0);
    K2_ASSERT(apThread->WorkPtPages_Clean.mNodeCount == 0);
    K2_ASSERT(apThread->mWorkVirt_Range == 0);
    K2_ASSERT(apThread->mWorkVirt_PageCount == 0);
    K2_ASSERT(apThread->mpWorkPage == NULL);
    K2_ASSERT(apThread->mpWorkPtPage == NULL);
    K2_ASSERT(apThread->mpWorkSeg == NULL);
    K2_ASSERT(apThread->mpThreadCreateSeg == NULL);

    K2_ASSERT(apThread->Sched.State.mLifeStage == KernThreadLifeStage_Cleanup);

    //
    // remove thread from process - it has already been purged from the object tree
    //
    pProc = apThread->mpProc;

    disp = K2OSKERN_SeqIntrLock(&gData.ProcListSeqLock);
    K2OSKERN_SeqIntrLock(&pProc->ThreadListSeqLock);

    K2LIST_Remove(&pProc->ThreadList, &apThread->ProcThreadListLink);

    pSeg = apThread->mpStackSeg;
    apThread->mpStackSeg = NULL;

    gData.Sched.mSysWideThreadCount--;

    K2OSKERN_SeqIntrUnlock(&pProc->ThreadListSeqLock, FALSE);
    K2OSKERN_SeqIntrUnlock(&gData.ProcListSeqLock, disp);

    KernObj_Release(&apThread->MsgExit.Hdr);

    //
    // thread is GONE after this call
    //
    KernObj_Release(&pSeg->Hdr);

    return K2STAT_NO_ERROR;
}

K2OSKERN_OBJ_HEADER * sTranslate_CheckNotAllNoWait(K2OSKERN_OBJ_THREAD *apThisThread, K2OSKERN_OBJ_WAITABLE aObjWait, K2STAT *apStat)
{
    //
    // returns what you are ACTUALLY going to wait on
    //

    switch (aObjWait.mpHdr->mObjType)
    {
    case K2OS_Obj_Event:
        if (!aObjWait.mpEvent->mIsSignalled)
            return aObjWait.mpHdr;

        if (!aObjWait.mpEvent->mIsAutoReset)
            return NULL;

        if (gData.mKernInitStage < KernInitStage_MultiThreaded)
        {
            //
            // very special case - changing the object and returning no wait
            //
            aObjWait.mpEvent->mIsSignalled = FALSE;
            return NULL;
        }

        //
        // signalled auto-reset event. we need to wait on it so the scheduler can release the right thread
        //
        return aObjWait.mpHdr;

    case K2OS_Obj_Name:
        K2_ASSERT(aObjWait.mpName->Event_IsOwned.mIsAutoReset == FALSE);
        return aObjWait.mpName->Event_IsOwned.mIsSignalled ? NULL : &aObjWait.mpName->Event_IsOwned.Hdr;

    case K2OS_Obj_Process:
        return (aObjWait.mpProc->mState >= KernProcState_Done) ? NULL : aObjWait.mpHdr;

    case K2OS_Obj_Thread:
        if (aObjWait.mpThread == apThisThread)
        {
            *apStat = K2STAT_ERROR_BAD_ARGUMENT;
            return NULL;
        }
        return (aObjWait.mpThread->Sched.State.mLifeStage >= KernThreadLifeStage_Exited) ? NULL : aObjWait.mpHdr;

    case K2OS_Obj_Semaphore:
        if (gData.mKernInitStage < KernInitStage_MultiThreaded)
        {
            //
            // very special case - changing the object and returning no wait
            //
            K2_ASSERT(aObjWait.mpSem->mCurCount > 0);
            aObjWait.mpSem->mCurCount--;
            return NULL;
        }
        return aObjWait.mpHdr;

    case K2OS_Obj_Mailbox:
        K2_ASSERT(aObjWait.mpMailbox->AvailEvent.mIsAutoReset == FALSE);
        return aObjWait.mpMailbox->AvailEvent.mIsSignalled ? NULL : &aObjWait.mpMailbox->AvailEvent.Hdr;

    case K2OS_Obj_Msg:
        K2_ASSERT(aObjWait.mpMsg->CompletionEvent.mIsAutoReset == FALSE);
        return aObjWait.mpMsg->CompletionEvent.mIsSignalled ? NULL : &aObjWait.mpMsg->CompletionEvent.Hdr;

    case K2OS_Obj_Alarm:
        if (aObjWait.mpAlarm->mIsSignalled)
            return NULL;

        //
        // it is eiterh not signalled yet, or is periodic
        //

        return aObjWait.mpHdr;

    case K2OS_Obj_Notify:
        K2_ASSERT(aObjWait.mpNotify->AvailEvent.mIsAutoReset == FALSE);
        return aObjWait.mpNotify->AvailEvent.mIsSignalled ? NULL : &aObjWait.mpNotify->AvailEvent.Hdr;

    default:
        K2_ASSERT(0);
        break;
    }

    return FALSE;
}

UINT32 KernThread_Wait(UINT32 aObjCount, K2OSKERN_OBJ_HEADER **appObjHdr, BOOL aWaitAll, UINT32 aTimeoutMs)
{
    K2OSKERN_OBJ_THREAD *       pThisThread;
    K2STAT                      stat;
    K2OSKERN_SCHED_MACROWAIT    wait;
    K2OSKERN_SCHED_MACROWAIT *  pMacro;
    K2OSKERN_OBJ_HEADER *       pObjHdr;
    UINT32                      ix;
    UINT32                      result;

    K2_ASSERT(!aWaitAll);

    for (ix = 0; ix < aObjCount; ix++)
    {
        switch (appObjHdr[ix]->mObjType)
        {
        case K2OS_Obj_Name:
        case K2OS_Obj_Process:
        case K2OS_Obj_Thread:
        case K2OS_Obj_Event:
        case K2OS_Obj_Alarm:
        case K2OS_Obj_Semaphore:
        case K2OS_Obj_Mailbox:
        case K2OS_Obj_Msg:
        case K2OS_Obj_Interrupt:
        case K2OS_Obj_Notify:
            break;
        default:
            return K2STAT_ERROR_BAD_ARGUMENT;
        }
    }

    if (aObjCount < 2)
    {
        pMacro = &wait;
    }
    else
    {
        pMacro = (K2OSKERN_SCHED_MACROWAIT *)K2OS_HeapAlloc(sizeof(K2OSKERN_SCHED_MACROWAIT) + ((aObjCount - 1) * sizeof(K2OSKERN_SCHED_WAITENTRY)));
        if (pMacro == NULL)
        {
            return K2STAT_ERROR_OUT_OF_MEMORY;
        }
    }

    pThisThread = K2OSKERN_CURRENT_THREAD;

    stat = K2STAT_NO_ERROR;

    for (ix = 0; ix < aObjCount; ix++)
    {
        pObjHdr = sTranslate_CheckNotAllNoWait(pThisThread, (K2OSKERN_OBJ_WAITABLE)appObjHdr[ix], &stat);
        if (pObjHdr == NULL)
        {
            if (!K2STAT_IS_ERROR(stat))
            {
                result = K2OS_WAIT_SIGNALLED_0 + ix;
            }
            break;
        }
        pMacro->SchedWaitEntry[ix].mWaitObj.mpHdr = pObjHdr;
    }

    if (!K2STAT_IS_ERROR(stat))
    {
        if (ix == aObjCount)
        {
//            K2OSKERN_Debug("Thread %d +Wait\n", pThisThread->Env.mId);
            pMacro->mNumEntries = aObjCount;
            pMacro->mWaitAll = FALSE;

            pThisThread->Sched.Item.mSchedItemType = KernSchedItem_ThreadWait;
            pThisThread->Sched.Item.Args.ThreadWait.mpMacroWait = pMacro;
            pThisThread->Sched.Item.Args.ThreadWait.mTimeoutMs = aTimeoutMs;
            KernArch_ThreadCallSched();

            stat = pThisThread->Sched.Item.mSchedCallResult;
            result = pMacro->mWaitResult;

//            K2OSKERN_Debug("Thread %d -Wait stat %08X result %08X\n", pThisThread->Env.mId, stat, result);
        }
    }
    else
        result = stat;

    if (aObjCount >= 2)
        K2OS_HeapFree(pMacro);

    if (K2STAT_IS_ERROR(stat))
    {
        K2OS_ThreadSetStatus(stat);
        return K2OS_WAIT_ERROR;
    }

    return result;
}
