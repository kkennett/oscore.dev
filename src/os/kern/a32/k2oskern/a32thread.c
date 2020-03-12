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

#include "a32kern.h"

void KernArch_PrepareThread(K2OSKERN_OBJ_THREAD *apThread, UINT32 aUserModeStackPtr)
{
    K2OSKERN_THREAD_PAGE *  pThreadPage;
    A32_EXCEPTION_CONTEXT * pInitCtx;
    UINT32                  stackPtr;
    UINT32                  initStackPtr;

    K2_ASSERT(apThread != NULL);
    K2_ASSERT(apThread->Info.CreateInfo.mEntrypoint != NULL);
    K2_ASSERT(apThread->Sched.mState == K2OS_THREAD_STATE_INIT);

    pThreadPage = K2_GET_CONTAINER(K2OSKERN_THREAD_PAGE, apThread, Thread);

    stackPtr = (UINT32)(&pThreadPage->mKernStack[K2OSKERN_THREAD_KERNSTACK_BYTECOUNT - sizeof(UINT32)]);

    if (apThread->mIsKernelThread == FALSE)
    {
        K2_ASSERT(0);
    }

    initStackPtr = stackPtr;
    stackPtr -= sizeof(A32_EXCEPTION_CONTEXT);
    pInitCtx = (A32_EXCEPTION_CONTEXT *)stackPtr;
    K2MEM_Zero(pInitCtx, sizeof(A32_EXCEPTION_CONTEXT));
    pInitCtx->SPSR = A32_PSR_MODE_SYS;
    pInitCtx->R[0] = (UINT32)apThread;
    pInitCtx->R[13] = initStackPtr;
    pInitCtx->R[15] = (UINT32)KernThread_Entry;

    apThread->mIsInKernelMode = TRUE;

    apThread->mStackPtr_Kernel = stackPtr;
}

void A32Kern_ThreadCallSched(UINT32 aStackPtr)
{
    K2OSKERN_CPUCORE *      pThisCore;
    K2OSKERN_OBJ_THREAD *   pActiveThread;

    K2OSKERN_COREPAGE *     pCorePage;
    A32_EXCEPTION_CONTEXT * pEx;

    pEx = (A32_EXCEPTION_CONTEXT *)aStackPtr;
    pEx->R[14] = pEx->R[15];
    pEx->R[13] = aStackPtr + sizeof(A32_EXCEPTION_CONTEXT);

    pThisCore = K2OSKERN_GET_CURRENT_CPUCORE;
    K2_ASSERT(pThisCore->mIsInMonitor == FALSE);

    pActiveThread = pThisCore->mpActiveThread;
    K2_ASSERT(pActiveThread->mIsInKernelMode);

    pActiveThread->mStackPtr_Kernel = aStackPtr;

    //
    // queue sched item to core as a core event 
    //
    pActiveThread->Sched.Item.CpuCoreEvent.mEventType = KernCpuCoreEvent_SchedulerCall;
    pActiveThread->Sched.Item.CpuCoreEvent.mEventAbsTimeMs = K2OS_GetAbsTimeMs();
    pActiveThread->Sched.Item.CpuCoreEvent.mSrcCoreIx = pThisCore->mCoreIx;

    KernIntr_QueueCpuCoreEvent(pThisCore, &pActiveThread->Sched.Item.CpuCoreEvent);

    pThisCore->mIsInMonitor = TRUE;
    pCorePage = K2_GET_CONTAINER(K2OSKERN_COREPAGE, pThisCore, CpuCore);
    A32Kern_ResumeInMonitor((UINT32)&pCorePage->mStacks[K2OSKERN_COREPAGE_STACKS_BYTES - 4]);

    /* should never return */
    K2_ASSERT(0);
}
