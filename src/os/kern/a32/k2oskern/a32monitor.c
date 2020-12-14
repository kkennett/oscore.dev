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

void KernArch_MonitorSwitchToProcZero(K2OSKERN_CPUCORE volatile *apThisCore)
{
    K2OSKERN_OBJ_THREAD *   pThread;
    K2OSKERN_OBJ_PROCESS *  pOldProc;

    pThread = apThisCore->mpActiveThread;
    if (pThread != NULL)
    {
        K2_ASSERT(pThread->mpProc == gpProc0);
    }

    pOldProc = apThisCore->mpActiveProc;
    if (gpProc0 != pOldProc)
    {
        if (pOldProc != NULL)
            A32_TLBInvalidateASID_UP(pOldProc->mId);

        A32_WriteTTBR0(gpProc0->mTransTableRegVal);
        A32_WriteCONTEXT(gpProc0->mId);
		
        apThisCore->mpActiveProc = gpProc0;
        K2_CpuWriteBarrier();
    }
}

void KernArch_SwitchFromMonitorToThread(K2OSKERN_CPUCORE volatile *apThisCore)
{
    K2OSKERN_COREPAGE *     pCorePage;
    K2OSKERN_OBJ_THREAD *   pThread;
    K2OSKERN_OBJ_PROCESS *  pProc;
    K2OSKERN_OBJ_PROCESS *  pOldProc;
    
    //
    // interrupts are off 
    //
    K2_ASSERT(apThisCore->mIsIdle == FALSE);

    K2_ASSERT((A32_ReadCPSR() & A32_PSR_MODE_MASK) == A32_PSR_MODE_SYS);

    pThread = apThisCore->mpActiveThread;
    K2_ASSERT(pThread != NULL);
    K2_ASSERT(pThread->Sched.State.mRunState == KernThreadRunState_Running);

    pProc = pThread->mpProc;
    pOldProc = apThisCore->mpActiveProc;
    if (pProc != pOldProc)
    {
        if (pOldProc != NULL)
            A32_TLBInvalidateASID_UP(pOldProc->mId);

        A32_WriteTTBR0(pProc->mTransTableRegVal);
        A32_WriteCONTEXT(pProc->mId);
        apThisCore->mpActiveProc = pProc;
        K2_CpuWriteBarrier();
    }

    A32_WriteTPIDRPRW((UINT32)pThread);

    apThisCore->mIsInMonitor = FALSE;

    pCorePage = K2_GET_CONTAINER(K2OSKERN_COREPAGE, apThisCore, CpuCore);

    A32Kern_ResumeThread(
        pThread->mStackPtr_Kernel,
        (UINT32)&pCorePage->mStacks[K2OSKERN_COREPAGE_STACKS_BYTES - 4]);

    K2OSKERN_Panic("Switch to Thread returned!\n");
}

