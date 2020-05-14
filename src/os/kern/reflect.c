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
sThreadExited(
    K2OSKERN_OBJ_THREAD * apExitedThread
)
{
    K2OSKERN_Debug("Thread %d exited with code %d\n", apExitedThread->Env.mId, apExitedThread->Info.mExitCode);
    K2_ASSERT(apExitedThread->MsgSvc.Hdr.mRefCount == 1);
    apExitedThread->Sched.State.mLifeStage = KernThreadLifeStage_Cleanup;
    KernObj_Release(&apExitedThread->Hdr);
}

void
sThreadStopped(
    K2OSKERN_OBJ_THREAD * apStoppedThread
)
{
    //
    // just dump thre thread and stop
    //
    K2OSKERN_Debug("\nThread %d Stopped:\n", apStoppedThread->Env.mId);
    K2OSKERN_Debug("Ex_Code = %08X\n", apStoppedThread->mEx_Code);
    K2OSKERN_Debug("Ex Addr = %08X\n", apStoppedThread->mEx_FaultAddr);
    K2OSKERN_Debug("Ex %s a page fault\n", apStoppedThread->mEx_IsPageFault ? "is" : "is not");
    KernThread_Dump(apStoppedThread);
    K2OSKERN_Panic(NULL);
}

void
K2OSKERN_ReflectSysMsg(
    UINT32          aOpCode,
    UINT32 const *  apParam
)
{
    if ((aOpCode & SYSMSG_OPCODE_HIGH_MASK) != SYSMSG_OPCODE_HIGH)
        return;

    switch (aOpCode)
    {
    case SYSMSG_OPCODE_THREAD_EXIT:
        sThreadExited((K2OSKERN_OBJ_THREAD *)apParam[0]);
        break;

    case SYSMSG_OPCODE_THREAD_STOP:
        sThreadStopped((K2OSKERN_OBJ_THREAD *)apParam[0]);
        break;

    default:
        break;
    }
}
