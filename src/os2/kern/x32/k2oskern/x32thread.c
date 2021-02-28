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
    K2OSKERN_CPUCORE volatile * apThisCore
)
{
    K2OSKERN_OBJ_THREAD *   pRunThread;
    UINT32                  stackPtr;

    K2_ASSERT(NULL != apThisCore->RunList.mpHead);
    
    pRunThread = K2_GET_CONTAINER(K2OSKERN_OBJ_THREAD, apThisCore->RunList.mpHead, CpuRunListLink);

    // idle thread should never actually run
    K2_ASSERT(pRunThread != &apThisCore->IdleThread);

    gX32Kern_PerCoreFS[apThisCore->mCoreIx] = pRunThread->mIx;

    stackPtr = (UINT32)&pRunThread->Context;

    //
    // interrupts must be enabled or something is wrong.
    //
    K2_ASSERT(((X32_EXCEPTION_CONTEXT *)stackPtr)->UserMode.CS == (X32_SEGMENT_SELECTOR_USER_CODE | X32_SELECTOR_RPL_USER));
    K2_ASSERT(((X32_EXCEPTION_CONTEXT *)stackPtr)->DS == (X32_SEGMENT_SELECTOR_USER_DATA | X32_SELECTOR_RPL_USER));
    K2_ASSERT(((X32_EXCEPTION_CONTEXT *)stackPtr)->UserMode.EFLAGS & X32_EFLAGS_INTENABLE);

    X32Kern_InterruptReturn((UINT32)&pRunThread->Context);

    K2OSKERN_Panic("Switch to Thread returned!\n");
}