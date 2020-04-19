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
    // STATE: Set up public thread state
    // pThread->Info.mThreadState = K2OS_Thread_Init;
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

    K2OSKERN_Debug("Thread(%d) calling scheduler for exit with Code %08X\n", apThisThread->Env.mId, apThisThread->Info.mExitCode);

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

void KernThread_Instantiate(K2OSKERN_OBJ_THREAD *apThisThread, K2OSKERN_OBJ_PROCESS *apProc, K2OS_THREADCREATE const *apCreate)
{
    K2OSKERN_THREAD_PAGE *  pNewThreadPage;
    K2OSKERN_OBJ_THREAD *   pNewThread;
    K2OSKERN_OBJ_SEGMENT *  pSeg;
    BOOL                    disp;
    
    pSeg = apThisThread->mpThreadCreateSeg;
    K2_ASSERT(pSeg != NULL);

    pNewThreadPage = (K2OSKERN_THREAD_PAGE *)
        (pSeg->ProcSegTreeNode.mUserVal + (pSeg->mPagesBytes - K2_VA32_MEMPAGE_BYTES));

    pNewThread = (K2OSKERN_OBJ_THREAD *)&pNewThreadPage->Thread;
    
    K2MEM_Zero(pNewThread, sizeof(K2OSKERN_OBJ_THREAD));

    K2_ASSERT(pSeg->Info.Thread.mpThread == pNewThread);

    pNewThread->Hdr.mObjType = K2OS_Obj_Thread;
    pNewThread->Hdr.mRefCount = 1;
    K2LIST_Init(&pNewThread->Hdr.WaitingThreadsPrioList);
    pNewThread->mpProc = apProc;
    pNewThread->mpStackSeg = pSeg;
    pNewThread->mIsKernelThread = (((UINT32)apCreate->mEntrypoint) >= K2OS_KVA_KERN_BASE) ? TRUE : FALSE;
    pNewThread->mIsInKernelMode = TRUE;

    K2LIST_Init(&pNewThread->WorkPages_Dirty);
    K2LIST_Init(&pNewThread->WorkPages_Clean);
    K2LIST_Init(&pNewThread->WorkPtPages_Dirty);
    K2LIST_Init(&pNewThread->WorkPtPages_Clean);

    pNewThread->Info.mStructBytes = sizeof(K2OS_THREADINFO);
    pNewThread->Info.mProcessId = apProc->mId;
    // STATE: set up public thread state
//    pNewThread->Info.mThreadState = K2OS_Thread_Init;
    K2MEM_Copy(&pNewThread->Info.CreateInfo, apCreate, sizeof(K2OS_THREADCREATE));

    disp = K2OSKERN_SeqIntrLock(&gData.ProcListSeqLock);
    K2OSKERN_SeqIntrLock(&apProc->ThreadListSeqLock);

    K2LIST_AddAtTail(&apProc->ThreadList, &pNewThread->ProcThreadListLink);

    pNewThread->Sched.State.mLifeStage = KernThreadLifeStage_Instantiated;
    pNewThread->Sched.State.mRunState = KernThreadRunState_None;
    pNewThread->Sched.State.mStopFlags = KERNTHREAD_STOP_FLAG_NONE;

    apThisThread->mpThreadCreateSeg = NULL;

    K2OSKERN_SeqIntrUnlock(&apProc->ThreadListSeqLock, FALSE);
    K2OSKERN_SeqIntrUnlock(&gData.ProcListSeqLock, disp);
}

K2STAT KernThread_Start(K2OSKERN_OBJ_THREAD *apThisThread, K2OSKERN_OBJ_THREAD *apThread)
{
    K2_ASSERT(apThread->Sched.State.mLifeStage == KernThreadLifeStage_Instantiated);
    K2_ASSERT(apThread->Sched.State.mRunState == KernThreadRunState_None);
    K2_ASSERT(apThread->Sched.State.mStopFlags == KERNTHREAD_STOP_FLAG_NONE);

    apThisThread->Sched.Item.mSchedItemType = KernSchedItem_ThreadCreate;
    apThisThread->Sched.Item.Args.ThreadCreate.mpThread = apThread;
    
    KernArch_ThreadCallSched();

    return apThisThread->Sched.Item.mResult;
}

K2STAT KernThread_Dispose(K2OSKERN_OBJ_THREAD *apThread)
{
    //
    // thread cannot be in created state 
    //
    if ((apThread->Sched.State.mLifeStage == KernThreadLifeStage_Init) &&
        (apThread->Sched.State.mLifeStage == KernThreadLifeStage_Started))
    {
        K2OSKERN_Panic("*** Invalid thread disposal\n");
    }

    K2_ASSERT(0);

    return K2STAT_ERROR_NOT_IMPL;
}
