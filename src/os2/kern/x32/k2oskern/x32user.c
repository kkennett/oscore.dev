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

static UINT8 const sgPublicApiCode[] =
{
/* 00 */    0xCD, 0xFF,  // int 255
/* 02 */    0xC3,        // ret
/* 03 */    0x90,        // nop

/* 04 */    0x52,                           // push edx
/* 05 */    0x51,                           // push ecx
/* 06 */    0xe8, 0x00, 0x00, 0x00, 0x00,   // call next instruction (relative 0) (push eip)

/* 0B */    0x59,                           // 1+  pop ecx
/* 0C */    0x83, 0xC1, 0x08,               // 3+  add ecx, (((8))) - eff addr of instruction after sysenter
/* 0F */    0x89, 0xE2,                     // 2+  mov edx, esp 
/* 11 */    0x0F, 0x34,                     // 2=  sysenter

/* 13 */    0x59,                           // (8) pop ecx
/* 14 */    0x5A,                           //     pop edx
/* 15 */    0xC3                            //     ret
};

void
KernArch_UserInit(
    UINT32 aCrtEntryPoint
)
{
    //
    // set up idle thread contexts for each core.  shove their stacks at the end of their
    // tls pages since proc1 will never use that many TLS
    //
    UINT32                      coreIx;
    UINT32 *                    pThreadPtrs;
    K2OSKERN_CPUCORE volatile * pCore;
    K2OSKERN_OBJ_THREAD *       pThread;
    UINT32                      stackPtr;

    pThreadPtrs = ((UINT32 *)K2OS_KVA_THREADPTRS_BASE);

    for (coreIx = 0; coreIx < gData.LoadInfo.mCpuCoreCount; coreIx++)
    {
        pCore = K2OSKERN_COREIX_TO_CPUCORE(coreIx);
        pThread = (K2OSKERN_OBJ_THREAD *)&pCore->IdleThread;
        K2_ASSERT(pThreadPtrs[pThread->mIx] == (UINT32)pThread);

        pThread->Context.DS = (X32_SEGMENT_SELECTOR_USER_DATA | X32_SELECTOR_RPL_USER);
        pThread->Context.REGS.ECX = pCore->mCoreIx; // first arg to entry point
        pThread->Context.REGS.EDX = 0; // second arg to entry point
        pThread->Context.EIP = aCrtEntryPoint;
        pThread->Context.CS = (X32_SEGMENT_SELECTOR_USER_CODE | X32_SELECTOR_RPL_USER);
        pThread->Context.EFLAGS = X32_EFLAGS_INTENABLE | X32_EFLAGS_SBO | X32_EFLAGS_ZERO;
        stackPtr = (pThread->mIx * K2_VA32_MEMPAGE_BYTES) + 0xFFC;
        *((UINT32 *)stackPtr) = 0;
        stackPtr -= sizeof(UINT32);
        *((UINT32 *)stackPtr) = 0;
        K2OSKERN_Debug("thread stack init @ %08X\n", stackPtr);
        pThread->Context.ESP = stackPtr;
        pThread->Context.SS = (X32_SEGMENT_SELECTOR_USER_DATA | X32_SELECTOR_RPL_USER);
    }

    //
    // threads are ready to execute in user mode now once the cores are started
    //

    //
    // copy in the public api code to the public api page
    //
    K2MEM_Copy((void *)K2OS_KVA_PUBLICAPI_SYSCALL, sgPublicApiCode, sizeof(sgPublicApiCode));
    X32_CacheFlushAll(); // wbinvd

    //
    // threads should be able to make a system call now
    //


}
