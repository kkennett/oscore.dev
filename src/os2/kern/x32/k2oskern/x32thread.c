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

void
KernArch_ResumeThread(
    K2OSKERN_CPUCORE volatile * apThisCore,
    BOOL                        aUseQuantumTimer
)
{
    K2OSKERN_OBJ_THREAD *   pRunThread;
    UINT32                  stackPtr;

    //
    // interrupts should be off when we get here
    //
    K2_ASSERT(FALSE == K2OSKERN_GetIntr());

    K2_ASSERT(NULL != apThisCore->RunList.mpHead);
    
    pRunThread = K2_GET_CONTAINER(K2OSKERN_OBJ_THREAD, apThisCore->RunList.mpHead, CpuRunListLink);

    // idle thread should never actually run
    K2_ASSERT(pRunThread != &apThisCore->IdleThread);

    gpX32Kern_PerCoreFS[apThisCore->mCoreIx] = pRunThread->mIx;

    stackPtr = (UINT32)&pRunThread->Context;

    pRunThread->mpLastRunCore = apThisCore;

    if (pRunThread->mInSysCall)
    {
        pRunThread->Context.REGS.EAX = pRunThread->mSysCall_Result;
        pRunThread->mInSysCall = FALSE;
    }

    //
    // interrupts must be enabled in the user mode context we are returning to, or something is wrong.
    //
    K2_ASSERT(((X32_EXCEPTION_CONTEXT *)stackPtr)->UserMode.CS == (X32_SEGMENT_SELECTOR_USER_CODE | X32_SELECTOR_RPL_USER));
    K2_ASSERT(((X32_EXCEPTION_CONTEXT *)stackPtr)->DS == (X32_SEGMENT_SELECTOR_USER_DATA | X32_SELECTOR_RPL_USER));
    K2_ASSERT(((X32_EXCEPTION_CONTEXT *)stackPtr)->UserMode.EFLAGS & X32_EFLAGS_INTENABLE);

    //
    // note next interrupt does not come from inside kernel, and return to the user mode thread
    //
    apThisCore->mIntrWhileInKernel = FALSE;
    X32Kern_InterruptReturn((UINT32)&pRunThread->Context);

    K2OSKERN_Panic("Switch to Thread returned!\n");
}

