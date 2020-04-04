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

#include "x32kern.h"

void KernArch_PrepareThread(K2OSKERN_OBJ_THREAD *apThread, UINT32 aUserModeStackPtr)
{
    K2OSKERN_THREAD_PAGE *  pThreadPage;
    X32_EXCEPTION_CONTEXT * pInitCtx;
    UINT32                  stackPtr;

    K2_ASSERT(apThread != NULL);
    K2_ASSERT(apThread->Info.CreateInfo.mEntrypoint != NULL);
    K2_ASSERT(apThread->Info.mState == K2OS_Thread_Init);

    pThreadPage = K2_GET_CONTAINER(K2OSKERN_THREAD_PAGE, apThread, Thread);

    stackPtr = (UINT32)(&pThreadPage->mKernStack[K2OSKERN_THREAD_KERNSTACK_BYTECOUNT - sizeof(UINT32)]);

    if (apThread->mIsKernelThread == FALSE)
    {
        K2_ASSERT(0);
    }

    stackPtr -= X32KERN_SIZEOF_KERNELMODE_EXCEPTION_CONTEXT;
    pInitCtx = (X32_EXCEPTION_CONTEXT *)stackPtr;
    K2MEM_Zero(pInitCtx, X32KERN_SIZEOF_KERNELMODE_EXCEPTION_CONTEXT);
    pInitCtx->DS = X32_SEGMENT_SELECTOR_KERNEL_DATA | X32_SELECTOR_RPL_KERNEL;
    pInitCtx->REGS.ECX = (UINT32)apThread;
    pInitCtx->REGS.EBP = 0;
    pInitCtx->REGS.ESP_Before_PushA = (UINT32)&pInitCtx->Exception_IntNum;
    pInitCtx->KernelMode.EFLAGS = X32_EFLAGS_INTENABLE | X32_EFLAGS_SBO;
    pInitCtx->KernelMode.CS = X32_SEGMENT_SELECTOR_KERNEL_CODE | X32_SELECTOR_RPL_KERNEL;
    pInitCtx->KernelMode.EIP = (UINT32)KernThread_Entry;

    apThread->mIsInKernelMode = TRUE;

    apThread->mStackPtr_Kernel = stackPtr;
}

void X32Kern_ThreadCallSched(X32_EXCEPTION_CONTEXT aContext)
{
    K2OSKERN_CPUCORE *      pThisCore;
    K2OSKERN_OBJ_THREAD *   pActiveThread;

    pThisCore = K2OSKERN_GET_CURRENT_CPUCORE;
    K2_ASSERT(pThisCore->mIsInMonitor == FALSE);

    pActiveThread = pThisCore->mpActiveThread;
    K2_ASSERT(pActiveThread->mIsInKernelMode);

    pActiveThread->mStackPtr_Kernel = (UINT32)&aContext;

    //
    // queue sched item to core as a core event 
    //
    pActiveThread->Sched.Item.CpuCoreEvent.mEventType = KernCpuCoreEvent_SchedulerCall;
    pActiveThread->Sched.Item.CpuCoreEvent.mEventAbsTimeMs = K2OS_SysUpTimeMs();
    pActiveThread->Sched.Item.CpuCoreEvent.mSrcCoreIx = pThisCore->mCoreIx;

    KernIntr_QueueCpuCoreEvent(pThisCore, &pActiveThread->Sched.Item.CpuCoreEvent);

    pThisCore->mIsInMonitor = TRUE;
    X32Kern_EnterMonitor(pThisCore->TSS.mESP0);

    /* should never return */
    K2_ASSERT(0);
}

