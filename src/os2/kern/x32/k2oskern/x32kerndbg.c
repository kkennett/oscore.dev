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

static void
sEmitSymbolName(
    K2OSKERN_OBJ_PROCESS *  apProc,
    UINT32                  aAddr,
    char *                  pBuffer
)
{
    KernDbg_FindClosestSymbol(apProc, aAddr, pBuffer, X32_SYM_NAME_MAX_LEN);
    if (*pBuffer == 0)
    {
        KernDbg_Output("?(%08X)", aAddr);
        return;
    }
    KernDbg_Output("%s", pBuffer);
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

    KernDbg_Output("StackTrace:\n");
    KernDbg_Output("------------------\n");
    KernDbg_Output("ESP       %08X\n", aESP);
    KernDbg_Output("EIP       %08X ", aEIP);
    sEmitSymbolName(apProc, aEIP, apBuffer);
    KernDbg_Output("\n");

    KernDbg_Output("%08X ", aEBP);
    if (aEBP == 0)
    {
        KernDbg_Output("\n");
        return;
    }
    pBackPtr = (UINT32 *)aEBP;
    KernDbg_Output("%08X ", pBackPtr[1]);
    sEmitSymbolName(apProc, pBackPtr[1], apBuffer);
    KernDbg_Output("\n");

    do {
        pBackPtr = (UINT32 *)pBackPtr[0];
        KernDbg_Output("%08X ", pBackPtr);
        if (pBackPtr == NULL)
        {
            KernDbg_Output("\n");
            return;
        }
        KernDbg_Output("%08X ", pBackPtr[1]);
        if (pBackPtr[1] == 0)
        {
            KernDbg_Output("\n");
            break;
        }
        sEmitSymbolName(apProc, pBackPtr[1], apBuffer);
        KernDbg_Output("\n");
    } while (1);
}

static
void
sDumpCommonExContext(
    X32_EXCEPTION_CONTEXT *apContext
)
{
    KernDbg_Output("Vector  0x%08X\n", apContext->Exception_Vector);
    KernDbg_Output("ErrCode 0x%08X", apContext->Exception_ErrorCode);
    if (apContext->Exception_Vector == X32_EX_PAGE_FAULT)
    {
        KernDbg_Output(" PAGE FAULT %sMode %sPresent %s\n",
            (apContext->Exception_ErrorCode & X32_EX_PAGE_FAULT_FROM_USER) ? "User" : "Kernel",
            (apContext->Exception_ErrorCode & X32_EX_PAGE_FAULT_PRESENT) ? "" : "Not",
            (apContext->Exception_ErrorCode & X32_EX_PAGE_FAULT_ON_WRITE) ? "Write" : "Read");
    }
    else
    {
        KernDbg_Output("\n");
    }
    KernDbg_Output("EAX     0x%08X\n", apContext->REGS.EAX);
    KernDbg_Output("ECX     0x%08X\n", apContext->REGS.ECX);
    KernDbg_Output("EDX     0x%08X\n", apContext->REGS.EDX);
    KernDbg_Output("EBX     0x%08X\n", apContext->REGS.EBX);
    KernDbg_Output("ESP     0x%08X\n", apContext->REGS.ESP_Before_PushA);
    KernDbg_Output("EBP     0x%08X\n", apContext->REGS.EBP);
    KernDbg_Output("ESI     0x%08X\n", apContext->REGS.ESI);
    KernDbg_Output("EDI     0x%08X\n", apContext->REGS.EDI);
    KernDbg_Output("DS      0x%08X\n", apContext->DS);
}

void
X32Kern_DumpUserModeExceptionContext(
    X32_EXCEPTION_CONTEXT *apContext
)
{
    KernDbg_Output("------------------\n");
    KernDbg_Output("UserMode Exception Context @ 0x%08X\n", apContext);
    sDumpCommonExContext(apContext);
    KernDbg_Output("EIP     0x%08X\n", apContext->UserMode.EIP);
    KernDbg_Output("CS      0x%08X\n", apContext->UserMode.CS);
    KernDbg_Output("EFLAGS  0x%08X\n", apContext->UserMode.EFLAGS);
    KernDbg_Output("U-ESP   0x%08X\n", apContext->UserMode.ESP);
    KernDbg_Output("SS      0x%08X\n", apContext->UserMode.SS);
}

void
X32Kern_DumpKernelModeExceptionContext(
    X32_EXCEPTION_CONTEXT * apContext
)
{
    KernDbg_Output("------------------\n");
    sDumpCommonExContext(apContext);
    KernDbg_Output("EIP     0x%08X\n", apContext->KernelMode.EIP);
    KernDbg_Output("CS      0x%08X\n", apContext->KernelMode.CS);
    KernDbg_Output("EFLAGS  0x%08X\n", apContext->KernelMode.EFLAGS);
    KernDbg_Output("------------------\n");
}
