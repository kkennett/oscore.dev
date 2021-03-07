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

static char sgSymDump[X32_SYM_NAME_MAX_LEN * K2OS_MAX_CPU_COUNT];

static BOOL sgInIntr[K2OS_MAX_CPU_COUNT] = { 0, };

static K2OSKERN_OBJ_INTR * sgpIntrObjByIrqIx[X32_NUM_IDT_ENTRIES] = { 0, };

void
X32Kern_Intr_OnException(
    K2OSKERN_CPUCORE volatile * apThisCore,
    X32_EXCEPTION_CONTEXT *     apContext
)
{

    //
    // return true to run exec.  otherwise will return to interrupted activity
    //
    if ((X32_SEGMENT_SELECTOR_USER_CODE | X32_SELECTOR_RPL_USER) == apContext->KernelMode.CS)
        return;

    // 
    // kernel mode exception = panic
    //

    K2OSKERN_Debug("Core %d, Exception in kernel mode. Context @ %08X\n", apThisCore->mCoreIx, apContext);
    K2OSKERN_Debug("Exception %d\n", apContext->Exception_Vector);
    K2OSKERN_Debug("CR2 = %08X\n", X32_ReadCR2());

    X32Kern_DumpKernelModeExceptionContext(apContext);
    X32Kern_DumpStackTrace(
        gpProc1,
        apContext->KernelMode.EIP,
        apContext->REGS.EBP,
        ((UINT32)apContext) + X32KERN_SIZEOF_KERNELMODE_EXCEPTION_CONTEXT,
        &sgSymDump[apThisCore->mCoreIx * X32_SYM_NAME_MAX_LEN]
    );
    K2OSKERN_Panic(NULL);
}

BOOL 
X32Kern_Intr_OnSystemCall(
    K2OSKERN_CPUCORE volatile * apThisCore,
    X32_EXCEPTION_CONTEXT *     apContext,
    K2OSKERN_OBJ_THREAD *       apCallingThread
)
{
    apCallingThread->mSysCall_Arg1 = apContext->REGS.ECX;
    apCallingThread->mSysCall_Arg2 = apContext->REGS.EDX;
    return KernIntr_OnSystemCall(apThisCore, apCallingThread, &apContext->REGS.EAX);
}

BOOL 
X32Kern_Intr_OnIrq(
    K2OSKERN_CPUCORE volatile * apThisCore,
    UINT32                      aDevIrqNum
)
{
    if (sgpIntrObjByIrqIx[aDevIrqNum] != NULL)
    {
        return KernIntr_OnIrq(apThisCore, sgpIntrObjByIrqIx[aDevIrqNum]);
    }

    K2OSKERN_Debug("****Unhandled Irq VECTOR %d. Treated as spurious and masked\n", aDevIrqNum);
    X32Kern_MaskDevIrq(aDevIrqNum);

    //
    // return true to run exec.  otherwise will return to interrupted activity
    //
    return FALSE;
}

void
X32Kern_InterruptHandler(
    X32_EXCEPTION_CONTEXT aContext
    )
{
    K2OSKERN_CPUCORE volatile * pThisCore;
    K2OSKERN_OBJ_THREAD *       pCurrentThread;
    KernCpuExecReason           reason;

    pThisCore = K2OSKERN_GET_CURRENT_CPUCORE;

    if (sgInIntr[pThisCore->mCoreIx])
    {
        K2OSKERN_Panic("Recursive entry to interrupt handler\n");
        while (1);
    }
    sgInIntr[pThisCore->mCoreIx] = TRUE;

    //
    // re-set the fs in case somebody in user land changed it
    //
    X32_SetFS((pThisCore->mCoreIx * X32_SIZEOF_GDTENTRY) | X32_SELECTOR_TI_LDT | X32_SELECTOR_RPL_USER);

    //
    // now we can retrieve the current thread (if there is one)
    //
    pCurrentThread = K2OSKERN_GetThisCoreCurrentThread();

    //
    // now we can figure out what to do
    //
    reason = KernCpuExecReason_Invalid;
    if (aContext.Exception_Vector < X32KERN_DEVVECTOR_BASE)
    {
        X32Kern_Intr_OnException(pThisCore, &aContext);
        reason = KernCpuExecReason_Exception;
    }
    else if (aContext.Exception_Vector == 255)
    {
        K2_ASSERT(NULL != pCurrentThread);
        // fix up EFLAGS since it was captured by SYSENTER after kernel was entered
        aContext.UserMode.EFLAGS |= X32_EFLAGS_INTENABLE;
        if (X32Kern_Intr_OnSystemCall(pThisCore, &aContext, pCurrentThread))
            reason = KernCpuExecReason_SystemCall;
    }
    else
    {
        if (aContext.Exception_Vector >= X32KERN_VECTOR_ICI_BASE)
        {
            K2_ASSERT(aContext.Exception_Vector <= X32KERN_VECTOR_ICI_LAST);
            if (KernIntr_OnIci(pThisCore, aContext.Exception_Vector - X32KERN_VECTOR_ICI_BASE))
                reason = KernCpuExecReason_Ici;
        }
        else
        {
            K2_ASSERT(KernArch_VectorToDevIrq(aContext.Exception_Vector) < X32_DEVIRQ_MAX_COUNT);
            if (X32Kern_Intr_OnIrq(pThisCore, KernArch_VectorToDevIrq(aContext.Exception_Vector)))
                reason = KernCpuExecReason_Irq;
        }
        X32Kern_EOI(aContext.Exception_Vector);
    }

    sgInIntr[pThisCore->mCoreIx] = FALSE;

    if (KernCpuExecReason_Invalid != reason)
    {
        // save context to thread if there is one
        if (NULL != pCurrentThread)
        {
            K2MEM_Copy(&pCurrentThread->Context, &aContext, X32KERN_SIZEOF_USERMODE_EXCEPTION_CONTEXT);
        }

        // this will never return
        KernCpu_Exec(pThisCore, reason);

        K2OSKERN_Panic("KernCpu_Exec returned\n");
    }
}

void 
KernArch_InstallDevIntrHandler(
    K2OSKERN_OBJ_INTR *apIntr
)
{
    UINT32 irqIx;

    irqIx = apIntr->IrqConfig.mSourceIrq;

    K2_ASSERT(irqIx < X32_DEVIRQ_MAX_COUNT);

    K2OSKERN_SeqLock(&gX32Kern_IntrSeqLock);

    K2_ASSERT(sgpIntrObjByIrqIx[irqIx] == NULL);
    sgpIntrObjByIrqIx[irqIx] = apIntr;
    X32Kern_ConfigDevIrq(&apIntr->IrqConfig);

    K2OSKERN_SeqUnlock(&gX32Kern_IntrSeqLock);
}

void 
KernArch_SetDevIntrMask(
    K2OSKERN_OBJ_INTR * apIntr, 
    BOOL                aMask
)
{
    UINT32 irqIx;

    irqIx = apIntr->IrqConfig.mSourceIrq;

    K2_ASSERT(irqIx < X32_DEVIRQ_MAX_COUNT);

    K2OSKERN_SeqLock(&gX32Kern_IntrSeqLock);

    K2_ASSERT(sgpIntrObjByIrqIx[irqIx] != NULL);
    if (aMask)
        X32Kern_MaskDevIrq(irqIx);
    else
        X32Kern_UnmaskDevIrq(irqIx);

    K2OSKERN_SeqUnlock(&gX32Kern_IntrSeqLock);
}

void 
KernArch_RemoveDevIntrHandler(
    K2OSKERN_OBJ_INTR *apIntr
)
{
    UINT32 irqIx;

    irqIx = apIntr->IrqConfig.mSourceIrq;

    K2_ASSERT(irqIx < X32_DEVIRQ_MAX_COUNT);

    K2OSKERN_SeqLock(&gX32Kern_IntrSeqLock);

    K2_ASSERT(sgpIntrObjByIrqIx[irqIx] == apIntr);
    X32Kern_MaskDevIrq(irqIx);
    sgpIntrObjByIrqIx[irqIx] = NULL;

    K2OSKERN_SeqUnlock(&gX32Kern_IntrSeqLock);
}

void
KernArch_Panic(
    K2OSKERN_CPUCORE volatile * apThisCore, 
    BOOL                        aDumpStack
)
{
    if (aDumpStack)
    {
        X32Kern_DumpStackTrace(
            gpProc1,
            (UINT32)K2_RETURN_ADDRESS,
            X32_ReadEBP(),
            X32_ReadESP(),
            &sgSymDump[apThisCore->mCoreIx * X32_SYM_NAME_MAX_LEN]
        );
    }
    while (1);
}

