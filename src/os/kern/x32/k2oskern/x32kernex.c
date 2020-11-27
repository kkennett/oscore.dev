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

void X32Kern_MountExceptionTrap(X32_CONTEXT aThreadContext)
{
    K2OSKERN_OBJ_THREAD *   pThisThread;
    K2_EXCEPTION_TRAP *     pTrap;

    //
    // interrupts are off
    //

    // get the trap and save goop to it
    pTrap = (K2_EXCEPTION_TRAP *)aThreadContext.ECX;
    pTrap->mTrapResult = K2STAT_ERROR_UNKNOWN;
    K2MEM_Copy(&pTrap->SavedContext, &aThreadContext, sizeof(X32_CONTEXT));

    // push this trap onto the trap stack
    pThisThread = K2OSKERN_CURRENT_THREAD;
    pTrap->mpNextTrap = pThisThread->mpKernExTrapStack;
    pThisThread->mpKernExTrapStack = pTrap;
}

static void
sEmitSymbolName(
    K2OSKERN_OBJ_PROCESS *  apProc,
    UINT32                  aAddr,
    char *                  pBuffer
)
{
    K2OSKERN_Debug("\nsEmitSymbolName(%08X, %08X, %08X\n", apProc, aAddr, pBuffer);
    KernDlx_FindClosestSymbol(apProc, aAddr, pBuffer, SYM_NAME_MAX_LEN);
    if (*pBuffer == 0)
    {
        K2OSKERN_Debug("?(%08X)", aAddr);
        return;
    }
    K2OSKERN_Debug("%s", pBuffer);
}

void
X32Kern_DumpStackTrace(
    K2OSKERN_OBJ_PROCESS *apProc,
    UINT32 aEIP,
    UINT32 aEBP,
    UINT32 aESP,
    char *  apBuffer
)
{
    UINT32 *pBackPtr;

    K2OSKERN_Debug("StackTrace:\n");
    K2OSKERN_Debug("------------------\n");
    K2OSKERN_Debug("ESP       %08X\n", aESP);
    K2OSKERN_Debug("EIP       %08X ", aEIP);
    sEmitSymbolName(apProc, aEIP, apBuffer);
    K2OSKERN_Debug("\n");

    K2OSKERN_Debug("%08X ", aEBP);
    if (aEBP == 0)
    {
        K2OSKERN_Debug("\n");
        return;
    }
    pBackPtr = (UINT32 *)aEBP;
    K2OSKERN_Debug("%08X ", pBackPtr[1]);
    sEmitSymbolName(apProc, pBackPtr[1], apBuffer);
    K2OSKERN_Debug("\n");

    do {
        pBackPtr = (UINT32 *)pBackPtr[0];
        K2OSKERN_Debug("%08X ", pBackPtr);
        if (pBackPtr == NULL)
        {
            K2OSKERN_Debug("\n");
            return;
        }
        K2OSKERN_Debug("%08X ", pBackPtr[1]);
        if (pBackPtr[1] == 0)
        {
            K2OSKERN_Debug("\n");
            break;
        }
        sEmitSymbolName(apProc, pBackPtr[1], apBuffer);
        K2OSKERN_Debug("\n");
    } while (1);
}

static
void
sDumpCommonExContext(
    X32_EXCEPTION_CONTEXT *apContext
)
{
    K2OSKERN_Debug("Vector  0x%08X\n", apContext->Exception_Vector);
    K2OSKERN_Debug("ErrCode 0x%08X", apContext->Exception_ErrorCode);
    if (apContext->Exception_Vector == X32_EX_PAGE_FAULT)
    {
        K2OSKERN_Debug(" PAGE FAULT %sMode %sPresent %s\n",
            (apContext->Exception_ErrorCode & X32_EX_PAGE_FAULT_FROM_USER) ? "User" : "Kernel",
            (apContext->Exception_ErrorCode & X32_EX_PAGE_FAULT_PRESENT) ? "" : "Not",
            (apContext->Exception_ErrorCode & X32_EX_PAGE_FAULT_ON_WRITE) ? "Write" : "Read");
    }
    else
    {
        K2OSKERN_Debug("\n");
    }
    K2OSKERN_Debug("EAX     0x%08X\n", apContext->REGS.EAX);
    K2OSKERN_Debug("ECX     0x%08X\n", apContext->REGS.ECX);
    K2OSKERN_Debug("EDX     0x%08X\n", apContext->REGS.EDX);
    K2OSKERN_Debug("EBX     0x%08X\n", apContext->REGS.EBX);
    K2OSKERN_Debug("ESP     0x%08X\n", apContext->REGS.ESP_Before_PushA);
    K2OSKERN_Debug("EBP     0x%08X\n", apContext->REGS.EBP);
    K2OSKERN_Debug("ESI     0x%08X\n", apContext->REGS.ESI);
    K2OSKERN_Debug("EDI     0x%08X\n", apContext->REGS.EDI);
    K2OSKERN_Debug("DS      0x%08X\n", apContext->DS);
}

void
X32Kern_DumpUserModeExceptionContext(
    X32_EXCEPTION_CONTEXT *apContext
)
{
    K2OSKERN_Debug("------------------\n");
    K2OSKERN_Debug("UserMode Exception Context @ 0x%08X\n", apContext);
    sDumpCommonExContext(apContext);
    K2OSKERN_Debug("EIP     0x%08X\n", apContext->UserMode.EIP);
    K2OSKERN_Debug("CS      0x%08X\n", apContext->UserMode.CS);
    K2OSKERN_Debug("EFLAGS  0x%08X\n", apContext->UserMode.EFLAGS);
    K2OSKERN_Debug("U-ESP   0x%08X\n", apContext->UserMode.ESP);
    K2OSKERN_Debug("SS      0x%08X\n", apContext->UserMode.SS);
}

void
X32Kern_DumpKernelModeExceptionContext(
    X32_EXCEPTION_CONTEXT * apContext
)
{
    K2OSKERN_Debug("------------------\n");
    sDumpCommonExContext(apContext);
    K2OSKERN_Debug("EIP     0x%08X\n", apContext->KernelMode.EIP);
    K2OSKERN_Debug("CS      0x%08X\n", apContext->KernelMode.CS);
    K2OSKERN_Debug("EFLAGS  0x%08X\n", apContext->KernelMode.EFLAGS);
    K2OSKERN_Debug("------------------\n");
}
