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
/* 00 */    0x89, 0xE0, // mov eax, esp
/* 02 */    0x0F, 0x34, // sysenter : eax=return esp, ecx, edx = call arguments
/* 04 */    0xC3        // ret instruction must be at this address (SYSCALL_API + 4)
};

void
KernArch_UserInit(
    void
)
{
    //
    // set up idle thread contexts for each core.  shove their stacks at the end of their
    // tls pages since proc1 will never use that many TLS
    //
    K2OSKERN_OBJ_THREAD *   pFirstThread;
    UINT32                  stackPtr;

    pFirstThread = &gpProc1->InitialThread;
    K2_ASSERT(*(((UINT32 *)K2OS_KVA_THREADPTRS_BASE) + pFirstThread->mIx) == (UINT32)pFirstThread);

    pFirstThread->Context.DS = (X32_SEGMENT_SELECTOR_USER_DATA | X32_SELECTOR_RPL_USER);
    pFirstThread->Context.REGS.ECX = 0; // first arg to entry point
    pFirstThread->Context.REGS.EDX = 0; // second arg to entry point
    pFirstThread->Context.EIP = gData.UserCrtInfo.mEntrypoint;
    pFirstThread->Context.CS = (X32_SEGMENT_SELECTOR_USER_CODE | X32_SELECTOR_RPL_USER);
    pFirstThread->Context.EFLAGS = X32_EFLAGS_INTENABLE | X32_EFLAGS_SBO | X32_EFLAGS_ZERO;

    //
    // put first thread initial stack at end of TLS page for that thread
    //
    stackPtr = (pFirstThread->mIx * K2_VA32_MEMPAGE_BYTES) + 0xFFC;
    *((UINT32 *)stackPtr) = 0;
    stackPtr -= sizeof(UINT32);
    *((UINT32 *)stackPtr) = 0;
    pFirstThread->Context.ESP = stackPtr;
    pFirstThread->Context.SS = (X32_SEGMENT_SELECTOR_USER_DATA | X32_SELECTOR_RPL_USER);

    //
    // first thread is ready to execute in user mode
    //

    //
    // copy in the public api code to the public api page
    //
    K2MEM_Copy((void *)K2OS_KVA_PUBLICAPI_SYSCALL, sgPublicApiCode, sizeof(sgPublicApiCode));
    X32_CacheFlushAll(); // wbinvd
}
