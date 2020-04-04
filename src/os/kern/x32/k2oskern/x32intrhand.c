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

#define SYM_NAME_MAX_LEN    80

static char sgSymDump[SYM_NAME_MAX_LEN * K2OS_MAX_CPU_COUNT];

static
void 
sEmitSymbolName(
    K2OSKERN_CPUCORE volatile * apThisCore,
    UINT32                      aAddr
)
{
    KernDlx_FindClosestSymbol(apThisCore->mpActiveProc, aAddr, &sgSymDump[apThisCore->mCoreIx * SYM_NAME_MAX_LEN], SYM_NAME_MAX_LEN);
    if (sgSymDump[apThisCore->mCoreIx * SYM_NAME_MAX_LEN] == 0)
    {
        K2OSKERN_Debug("?(%08X)", aAddr);
        return;
    }
    K2OSKERN_Debug("%s", &sgSymDump[apThisCore->mCoreIx * SYM_NAME_MAX_LEN]);
}

static
void 
sX32Kern_DumpStackTrace(
    K2OSKERN_CPUCORE volatile * apThisCore,
    UINT32 aEIP, 
    UINT32 aEBP, 
    UINT32 aESP
)
{
    UINT32 *pBackPtr;

    K2OSKERN_Debug("StackTrace:\n");
    K2OSKERN_Debug("------------------\n");
    K2OSKERN_Debug("ESP       %08X\n", aESP);
    K2OSKERN_Debug("EIP       %08X ", aEIP);
    sEmitSymbolName(apThisCore, aEIP);
    K2OSKERN_Debug("\n");

    K2OSKERN_Debug("%08X ", aEBP);
    if (aEBP == 0)
    {
        K2OSKERN_Debug("\n");
        return;
    }
    pBackPtr = (UINT32 *)aEBP;
    K2OSKERN_Debug("%08X ", pBackPtr[1]);
    sEmitSymbolName(apThisCore, pBackPtr[1]);
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
        sEmitSymbolName(apThisCore, pBackPtr[1]);
        K2OSKERN_Debug("\n");
    } while (1);
}

static
void sX32Kern_sDumpCommonExceptionContext(
    X32_EXCEPTION_CONTEXT *apContext
)
{
    K2OSKERN_Debug("ErrCode 0x%08X\n", apContext->Exception_ErrorCode);
    K2OSKERN_Debug("IntNum  0x%08X\n", apContext->Exception_IntNum);
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

static
void sX32Kern_DumpUserModeExceptionContext(
    X32_EXCEPTION_CONTEXT *apContext
)
{
    K2OSKERN_Debug("------------------\n");
    K2OSKERN_Debug("UserMode Exception Context @ 0x%08X\n", apContext);
    sX32Kern_sDumpCommonExceptionContext(apContext);
    K2OSKERN_Debug("EIP     0x%08X\n", apContext->UserMode.EIP);
    K2OSKERN_Debug("CS      0x%08X\n", apContext->UserMode.CS);
    K2OSKERN_Debug("EFLAGS  0x%08X\n", apContext->UserMode.EFLAGS);
    K2OSKERN_Debug("U-ESP   0x%08X\n", apContext->UserMode.ESP);
    K2OSKERN_Debug("SS      0x%08X\n", apContext->UserMode.SS);
}

static
void sX32Kern_DumpKernelModeExceptionContext(
    UINT32                  aCoreIx, 
    X32_EXCEPTION_CONTEXT * apContext
)
{
    K2OSKERN_Debug("------------------\n");
    K2OSKERN_Debug("Core    %d\n", aCoreIx);
    sX32Kern_sDumpCommonExceptionContext(apContext);
    K2OSKERN_Debug("EIP     0x%08X\n", apContext->KernelMode.EIP);
    K2OSKERN_Debug("CS      0x%08X\n", apContext->KernelMode.CS);
    K2OSKERN_Debug("EFLAGS  0x%08X\n", apContext->KernelMode.EFLAGS);
    K2OSKERN_Debug("------------------\n");
}

static 
BOOL 
sX32Kern_Exception(
    K2OSKERN_CPUCORE volatile * apThisCore,
    K2OSKERN_OBJ_THREAD *       apCurThread,
    X32_EXCEPTION_CONTEXT *     apContext
)
{
    K2_EXCEPTION_TRAP *pExTrap;

    if (apCurThread == NULL)
    {
        sX32Kern_DumpKernelModeExceptionContext(apThisCore->mCoreIx, apContext);
        sX32Kern_DumpStackTrace(
            apThisCore,
            apContext->KernelMode.EIP,
            apContext->REGS.EBP,
            ((UINT32)apContext) + X32KERN_SIZEOF_KERNELMODE_EXCEPTION_CONTEXT);
        K2OSKERN_Panic("Exception %d in Monitor\n", apContext->Exception_IntNum);
    }

    K2OSKERN_Debug("Exception %d in %s Mode while running Thread %d, Context @ 0x%08X\n",
        apContext->Exception_IntNum,
        apCurThread->mIsInKernelMode ? "Kernel" : "User",
        apCurThread->Info.mThreadId,
        apContext);

    if (apCurThread->mIsInKernelMode)
    {
        pExTrap = apCurThread->mpKernExTrapStack;
        if (pExTrap == NULL)
        {
            sX32Kern_DumpKernelModeExceptionContext(apThisCore->mCoreIx, apContext);
            sX32Kern_DumpStackTrace(
                apThisCore,
                apContext->KernelMode.EIP,
                apContext->REGS.EBP,
                ((UINT32)apContext) + X32KERN_SIZEOF_KERNELMODE_EXCEPTION_CONTEXT);
            K2OSKERN_Panic("Untrapped Exception\n");
        }
    }
    else
    {
        K2_ASSERT(0);
    }

    //
    // there is a mounted trap record for this thread in this mode
    //
    K2_ASSERT(0);

    return TRUE;
}

static BOOL sgInIntr[K2OS_MAX_CPU_COUNT] = { 0, };

void
X32Kern_InterruptHandler(
    X32_EXCEPTION_CONTEXT aContext
    )
{
    K2OSKERN_CPUCORE *                  pThisCore;
    K2OSKERN_CPUCORE_EVENT volatile *   pCoreEvent;
    KernCpuCoreEventType                eventType;
    K2OSKERN_OBJ_THREAD *               pActiveThread;
    BOOL                                enterMonitor;
    BOOL                                threadFaulted;

    pThisCore = K2OSKERN_GET_CURRENT_CPUCORE;
    enterMonitor = FALSE;

    if (sgInIntr[pThisCore->mCoreIx])
    {
        K2OSKERN_Panic("Recursive exception or interrupt\n");
        while (1);
    }
    sgInIntr[pThisCore->mCoreIx] = TRUE;

    pActiveThread = pThisCore->mpActiveThread;

    if (aContext.Exception_IntNum >= X32KERN_INTR_ICI_BASE)
    {
        if (aContext.Exception_IntNum >= X32KERN_INTR_DEV_BASE)
        {
            //
            // this is an IRQ from a device
            //
            if (aContext.Exception_IntNum == X32KERN_INTR_LVT_TIMER)
            {
                //
                // 1ms tick
                //
                if (X32Kern_IntrTimerTick())
                {
                    pCoreEvent = &gData.Sched.SchedTimerSchedItem.CpuCoreEvent;
                    pCoreEvent->mEventType = KernCpuCoreEvent_SchedTimerFired;
                    pCoreEvent->mEventAbsTimeMs = K2OS_SysUpTimeMs();
                    pCoreEvent->mSrcCoreIx = pThisCore->mCoreIx;

                    KernIntr_QueueCpuCoreEvent(pThisCore, pCoreEvent);

                    enterMonitor = TRUE;
                }
            }
            else
            {
                K2OSKERN_Debug("Non-Timer IRQ %d on core %d\n", aContext.Exception_IntNum, pThisCore->mCoreIx);
                KernIntr_RecvIrq(pThisCore, aContext.Exception_IntNum);
            }
        }
        else
        {
            //
            // this is an ICI from another core
            //
            K2_ASSERT(aContext.Exception_IntNum - X32KERN_INTR_ICI_BASE < gData.mCpuCount);

            pCoreEvent = &pThisCore->IciFromOtherCore[aContext.Exception_IntNum - X32KERN_INTR_ICI_BASE];
            eventType = pCoreEvent->mEventType;

            K2_ASSERT((eventType >= KernCpuCoreEvent_Ici_Wakeup) && (eventType <= KernCpuCoreEvent_Ici_Debug));

            KernIntr_QueueCpuCoreEvent(pThisCore, pCoreEvent);
            enterMonitor = TRUE;
        }
    }
    else
    {
        threadFaulted = sX32Kern_Exception(pThisCore, pActiveThread, &aContext);
        if (threadFaulted)
            enterMonitor = TRUE;
    }

    if (enterMonitor)
    {
        if (pActiveThread != NULL)
        {
            pActiveThread->mStackPtr_Kernel = (UINT32)&aContext;
        }

        pThisCore->mIsInMonitor = TRUE;

        sgInIntr[pThisCore->mCoreIx] = FALSE;

        X32Kern_EnterMonitor(pThisCore->TSS.mESP0);
    }

    sgInIntr[pThisCore->mCoreIx] = FALSE;
}

