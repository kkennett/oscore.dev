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
KernThread_CleanupOne(
    K2OSKERN_OBJ_HEADER *apObj
)
{
    K2_ASSERT(0);
}

void    
KernThread_InitOne(
    K2OSKERN_OBJ_THREAD *apThread
)
{
    K2MEM_Zero(apThread, sizeof(K2OSKERN_OBJ_THREAD));
    apThread->Hdr.mObjType = KernObj_Thread;
    apThread->mState = KernThreadState_Init;
    apThread->mAffinityMask = (1 << gData.LoadInfo.mCpuCoreCount) - 1;
    apThread->Hdr.mfCleanup = KernThread_CleanupOne;
    K2LIST_Init(&apThread->HelpPtPageList);
    K2LIST_Init(&apThread->HeldPageList);
}

void 
KernThread_Exception(
    K2OSKERN_CPUCORE volatile *apThisCore
)
{
    K2OSKERN_OBJ_THREAD *pCurThread;

    pCurThread = K2OSKERN_GetThisCoreCurrentThread();
    //
    // thread current context saved to its context var
    //

    K2OSKERN_Debug("Thread %d caused exception\n", pCurThread->mIx);
    K2_ASSERT(0);
}

void
KernThread_SystemCall(
    K2OSKERN_CPUCORE volatile *apThisCore
)
{
    K2OSKERN_OBJ_THREAD *   pCurThread;
    K2OS_USER_THREAD_PAGE * pThreadPage;

    pCurThread = K2OSKERN_GetThisCoreCurrentThread();
    K2_ASSERT(FALSE != pCurThread->mInSysCall);
    //
    // thread current context saved to its context var
    //

    switch (pCurThread->mSysCall_Id)
    {
    case K2OS_SYSCALL_ID_SIGNAL_NOTIFY:
        KernThread_SysCall_SignalNotify(apThisCore, pCurThread);
        break;

    case K2OS_SYSCALL_ID_WAIT_FOR_NOTIFY:
    case K2OS_SYSCALL_ID_TEST_NOTIFY:
        KernThread_SysCall_WaitForNotify(apThisCore, pCurThread,
            (K2OS_SYSCALL_ID_TEST_NOTIFY == pCurThread->mSysCall_Id) ? TRUE : FALSE
            );
        break;

    case K2OS_SYSCALL_ID_RENDER_PTMAP:
        KernPhys_RenderPtMap(pCurThread->mpProc, (UINT8 *)pCurThread->mSysCall_Arg0);
        break;

    case K2OS_SYSCALL_ID_RAISE_EXCEPTION:
        KernThread_SysCall_RaiseException(apThisCore, pCurThread);
        break;

    case K2OS_SYSCALL_ID_NOTIFY_CREATE:
        KernThread_SysCall_CreateNotify(apThisCore, pCurThread);
        break;

    case K2OS_SYSCALL_ID_TOKEN_DESTROY:
        K2_ASSERT(0);
        break;

    default:
        K2OSKERN_Debug("Unknown system call\n");
        pCurThread->mSysCall_Result = 0;
        pThreadPage = pCurThread->mpKernRwViewOfUserThreadPage;
        pThreadPage->mLastStatus = K2STAT_ERROR_NOT_IMPL;
        break;
    }
}

void 
KernThread_SysCall_SignalNotify(
    K2OSKERN_CPUCORE volatile * apThisCore,
    K2OSKERN_OBJ_THREAD *       apCurThread
)
{
    K2OS_USER_THREAD_PAGE * pThreadPage;
    K2OSKERN_OBJ_NOTIFY *   pNotify;

    pThreadPage = apCurThread->mpKernRwViewOfUserThreadPage;

    if (0 == pThreadPage->mSysCall_Arg1)
    {
        apCurThread->mSysCall_Result = 0;
        pThreadPage->mLastStatus = K2STAT_ERROR_BAD_ARGUMENT;
    }
    else
    {
        //
        // translate token from apCurThread->mSysCall_Arg0 to notify
        //
        pNotify = (K2OSKERN_OBJ_NOTIFY *)apCurThread->mSysCall_Arg0;

        //
        // this may release threads to run on this core or other cores
        //
        apCurThread->mSysCall_Result = KernNotify_Signal(apThisCore, pNotify, pThreadPage->mSysCall_Arg1);
        pThreadPage->mLastStatus = K2STAT_NO_ERROR;
    }
}

void 
KernThread_SysCall_WaitForNotify(
    K2OSKERN_CPUCORE volatile * apThisCore,
    K2OSKERN_OBJ_THREAD *       apCurThread,
    BOOL                        aNonBlocking
)

{
    K2OS_USER_THREAD_PAGE * pThreadPage;
    K2OSKERN_OBJ_NOTIFY *   pNotify;
    BOOL                    disp;

    pThreadPage = apCurThread->mpKernRwViewOfUserThreadPage;

    //
    // translate token from apCurThread->mSysCall_Arg1 to notify
    //
    pNotify = (K2OSKERN_OBJ_NOTIFY *)apCurThread->mSysCall_Arg0;

    disp = K2OSKERN_SeqLock(&pNotify->Lock);

    if (pNotify->Locked.mState == KernNotifyState_Active)
    {
        //
        // we are not going to block
        //
        apCurThread->mSysCall_Result = pNotify->Locked.mDataWord;
        pThreadPage->mLastStatus = K2STAT_NO_ERROR;
        pNotify->Locked.mDataWord = 0;
        pNotify->Locked.mState = KernNotifyState_Idle;
    }
    else
    {
        if (aNonBlocking)
        {
            apCurThread->mSysCall_Result = 0;
            pThreadPage->mLastStatus = K2STAT_ERROR_WAIT_FAILED;
        }
        else
        {
            //
            // this will get overridden when thread is released
            //
            apCurThread->mSysCall_Result = 0;
            pThreadPage->mLastStatus = K2STAT_ERROR_WAIT_FAILED;

            if (pNotify->Locked.mState == KernNotifyState_Idle)
            {
                pNotify->Locked.mState = KernNotifyState_Waiting;
            }
            else
            {
                K2_ASSERT(KernNotifyState_Waiting == pNotify->Locked.mState);
            }

            //
            // remove from the cpu run list and set flag that thread has changed
            //
            K2LIST_Remove((K2LIST_ANCHOR *)&apThisCore->RunList, &apCurThread->CpuRunListLink);
            apCurThread->mpCurrentCore = NULL;
            apCurThread->mState = KernThreadState_WaitingOnNotify;
            K2LIST_AddAtTail(&pNotify->Locked.WaitingThreadList, &apCurThread->NotifyWaitListLink);
            apThisCore->mThreadChanged = TRUE;
        }
    }

    K2OSKERN_SeqUnlock(&pNotify->Lock, disp);
}

void    
KernThread_SysCall_RaiseException(
    K2OSKERN_CPUCORE volatile * apThisCore,
    K2OSKERN_OBJ_THREAD *       apCurThread
)
{
    //
    // thread manually raised exception
    //
    KernArch_DumpThreadContext(apThisCore, apCurThread);
    K2OSKERN_Panic(NULL);
}

void    
KernThread_SysCall_CreateNotify(
    K2OSKERN_CPUCORE volatile * apThisCore,
    K2OSKERN_OBJ_THREAD *       apCurThread
)
{
    K2_ASSERT(0);
}
