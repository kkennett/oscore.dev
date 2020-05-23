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

static char sgSymDump[SYM_NAME_MAX_LEN * K2OS_MAX_CPU_COUNT];

static BOOL sgInIntr[K2OS_MAX_CPU_COUNT] = { 0, };

static K2OSKERN_OBJ_INTR *  sgpIntrObjByIrqIx[X32_NUM_IDT_ENTRIES] = { 0, };

void
sPopTrap(
    K2OSKERN_CPUCORE volatile * apThisCore,
    K2OSKERN_OBJ_THREAD *       apCurThread,
    K2_EXCEPTION_TRAP *         apExTrap,
    X32_EXCEPTION_CONTEXT *     apContext,
    K2STAT                      aExCode
)
{
    UINT32  targetESP;

    /* The exception statuscode goes into the trap result, and the trap is dismounted */
    apExTrap->mTrapResult = aExCode;

    /* we overwrite the exception context with the values from the trap record.  */
    K2MEM_Copy(&apContext->REGS, &apExTrap->SavedContext.EDI, sizeof(X32_PUSHA));

    /* must tweak returned EAX to nonzero so the return value from the trap mount is not FALSE */
    apContext->REGS.EAX = 0xFFFFFFFF;

    /* if we are in user mode we handle the trap differently */
    if (!apCurThread->mIsInKernelMode)
    {
        K2_ASSERT(0);
    }

    /* if we get here this is a kernel mode trap and we need to do some histrionics
       with the stack to restore at the proper point following the exception */

    /* dismount the kernel-mode trap */
    apCurThread->mpKernExTrapStack = apExTrap->mpNextTrap;

    /* target ESP before pseudo-push of exception context is the ESP that
       will be effective when we return from this exception */
    targetESP = (apExTrap->SavedContext.ESP + 4) - X32KERN_SIZEOF_KERNELMODE_EXCEPTION_CONTEXT;

    /* overwrite stuff in the source context with the proper return values */
    apContext->DS = X32_SEGMENT_SELECTOR_KERNEL_DATA | X32_SELECTOR_RPL_KERNEL;
    apContext->KernelMode.EIP = apExTrap->SavedContext.EIP;
    apContext->KernelMode.EFLAGS = apExTrap->SavedContext.EFLAGS;
    apContext->KernelMode.CS = X32_SEGMENT_SELECTOR_KERNEL_CODE | X32_SELECTOR_RPL_KERNEL;

    /* now do the assembly that copies the context down in the stack (up in memory)
       to the new stack pointer to properly return and restore the original ESP.
       The ESP in the context record will be ignored during the popa */
    sgInIntr[apThisCore->mCoreIx] = FALSE;

    X32Kern_KernelExTrapReturn(apContext, targetESP);

    K2OSKERN_Panic("ExTrap returned\n");
}

static 
void 
sAbort(
    K2OSKERN_CPUCORE volatile * apThisCore,
    X32_EXCEPTION_CONTEXT *     apContext
)
{
    K2OSKERN_Debug("Core %d, Exception Context @ %08X\n", apThisCore->mCoreIx, apContext);
    K2OSKERN_Debug("Exception %d\n", apContext->Exception_Vector);
    K2OSKERN_Debug("CR2 = %08X\n", X32_ReadCR2());
    X32Kern_DumpKernelModeExceptionContext(apContext);
    X32Kern_DumpStackTrace(
        gpProc0,
        apContext->KernelMode.EIP,
        apContext->REGS.EBP,
        ((UINT32)apContext) + X32KERN_SIZEOF_KERNELMODE_EXCEPTION_CONTEXT,
        &sgSymDump[apThisCore->mCoreIx * SYM_NAME_MAX_LEN]
    );
    K2OSKERN_Panic(NULL);
}

BOOL 
sOnException(
    K2OSKERN_CPUCORE volatile * apThisCore,
    X32_EXCEPTION_CONTEXT *     apContext
)
{
    K2_EXCEPTION_TRAP *     pExTrap;
    K2OSKERN_OBJ_THREAD *   pCurThread;
    K2STAT                  exCode;
    BOOL                    isPageFault;

    isPageFault = FALSE;

    switch (apContext->Exception_Vector)
    {
    case X32_EX_DIVIDE_BY_ZERO:
        exCode = K2STAT_EX_ZERODIVIDE;
        break;

    case X32_EX_INVALID_OPCODE:
        exCode = K2STAT_EX_INSTRUCTION;
        break;

    case X32_EX_SEGMENT_NOT_PRESENT:
        exCode = K2STAT_EX_ACCESS;
        break;

    case X32_EX_STACK_FAULT:
        exCode = K2STAT_EX_STACK;
        break;

    case X32_EX_ALIGNMENT:
        exCode = K2STAT_EX_ALIGNMENT;
        break;

    case X32_EX_GENERAL:
        exCode = K2STAT_EX_ACCESS;
        break;

    case X32_EX_PAGE_FAULT:
        isPageFault = TRUE;
        exCode = K2STAT_EX_ACCESS;
        break;

    default:
        exCode = K2STAT_EX_UNKNOWN;
        break;
    }

    if (apThisCore->mIsInMonitor)
    {
        sAbort(apThisCore, apContext);
    }

    //
    // exception while thread running
    //
    pCurThread = apThisCore->mpActiveThread;
    K2_ASSERT(pCurThread != NULL);
    pCurThread->mStackPtr_Kernel = (UINT32)apContext;

    // 
    // check for exception trap
    //
    if (pCurThread->mIsInKernelMode)
    {
        pExTrap = pCurThread->mpKernExTrapStack;
        if (pExTrap != NULL)
        {
            sPopTrap(apThisCore, pCurThread, pExTrap, apContext, exCode);
            // this should never return
            K2OSKERN_Panic("sPopTrap returned\n");
        }
    }
    else
    {
        K2_ASSERT(0);
    }

    if (NULL == gData.mpMsgBox_K2OSEXEC)
    {
        sAbort(apThisCore, apContext);
    }

    pCurThread->mEx_FaultAddr = X32_ReadCR2();
    pCurThread->mEx_Code = exCode;
    pCurThread->mEx_IsPageFault = isPageFault;

    pCurThread->Sched.Item.CpuCoreEvent.mEventType = KernCpuCoreEvent_ThreadStop;
    pCurThread->Sched.Item.CpuCoreEvent.mEventAbsTimeMs = K2OS_SysUpTimeMs();
    pCurThread->Sched.Item.CpuCoreEvent.mSrcCoreIx = apThisCore->mCoreIx;

    pCurThread->Sched.Item.mSchedItemType = KernSchedItem_ThreadStop;

    KernIntr_QueueCpuCoreEvent(apThisCore, &pCurThread->Sched.Item.CpuCoreEvent);

    return TRUE;
}

void
X32Kern_InterruptHandler(
    X32_EXCEPTION_CONTEXT aContext
    )
{
    K2OSKERN_CPUCORE volatile *         pThisCore;
    K2OSKERN_CPUCORE_EVENT volatile *   pCoreEvent;
    UINT32                              devIrq;
    BOOL                                forceEnterMonitor;

    pThisCore = K2OSKERN_GET_CURRENT_CPUCORE;
    forceEnterMonitor = FALSE;

    if (sgInIntr[pThisCore->mCoreIx])
    {
        K2OSKERN_Panic("Recursive exception or interrupt\n");
        while (1);
    }
    sgInIntr[pThisCore->mCoreIx] = TRUE;

    if (aContext.Exception_Vector < X32KERN_DEVVECTOR_BASE)
    {
        forceEnterMonitor = sOnException(pThisCore, &aContext);
    }
    else
    {
//        K2OSKERN_Debug("Core %d Context %08X Vector %d\n", pThisCore->mCoreIx, &aContext, aContext.Exception_Vector);
        if (aContext.Exception_Vector >= X32KERN_VECTOR_ICI_BASE)
        {
            //
            // ICI from another core
            //
            K2_ASSERT(aContext.Exception_Vector <= X32KERN_VECTOR_ICI_LAST);

            pCoreEvent = &pThisCore->IciFromOtherCore[aContext.Exception_Vector - X32KERN_VECTOR_ICI_BASE];

            K2_ASSERT((pCoreEvent->mEventType >= KernCpuCoreEvent_Ici_Wakeup) && (pCoreEvent->mEventType <= KernCpuCoreEvent_Ici_Debug));

            KernIntr_QueueCpuCoreEvent(pThisCore, pCoreEvent);
        }
        else
        {
            //
            // LVT or external interrupt
            //
            devIrq = KernArch_VectorToDevIrq(aContext.Exception_Vector);
            K2_ASSERT(devIrq < X32_DEVIRQ_MAX_COUNT);
            if (sgpIntrObjByIrqIx[devIrq] != NULL)
            {
//                K2OSKERN_Debug("VECTOR %d -> IRQ %d on core %d\n", aContext.Exception_Vector, devIrq, pThisCore->mCoreIx);
                sgpIntrObjByIrqIx[devIrq]->mfHandler(sgpIntrObjByIrqIx[devIrq]->mpHandlerContext);
            }
            else
            {
                K2OSKERN_Debug("****Unhandled Interrupt VECTOR %d (IRQ %d). Masked\n", aContext.Exception_Vector, devIrq);
                X32Kern_MaskDevIrq(devIrq);
            }
        }
        //
        // end of interrupt
        //
        X32Kern_EOI(aContext.Exception_Vector);
    }

    if ((!pThisCore->mIsInMonitor) || (pThisCore->mIsIdle))
    {
        if ((forceEnterMonitor) ||
            (pThisCore->mpPendingEventListHead != NULL))
        {
            K2Trace(K2TRACE_X32INTR_MONITOR_ENTER, 1, aContext.Exception_Vector);

            if (pThisCore->mpActiveThread != NULL)
                pThisCore->mpActiveThread->mStackPtr_Kernel = (UINT32)&aContext;

            pThisCore->mIsInMonitor = TRUE;

            sgInIntr[pThisCore->mCoreIx] = FALSE;

            X32Kern_EnterMonitor(pThisCore->TSS.mESP0);

            // should never return here
            K2_ASSERT(0);
        }
    }

    sgInIntr[pThisCore->mCoreIx] = FALSE;
}

void X32Kern_RaiseKernelModeException(X32_EXCEPTION_CONTEXT aContext)
{
    K2OSKERN_CPUCORE volatile * pThisCore;
    K2OSKERN_OBJ_THREAD *       pCurThread;
    K2_EXCEPTION_TRAP *         pExTrap;

    //
    // interrupts are off
    //

    pThisCore = K2OSKERN_GET_CURRENT_CPUCORE;

    K2_ASSERT(!pThisCore->mIsInMonitor);

    K2OSKERN_Debug("--X32Kern_RaiseKernelModeException(%08X)\n", aContext.REGS.ECX);

    if (sgInIntr[pThisCore->mCoreIx])
    {
        K2OSKERN_Debug("Recursive exception\n");
        while (1);
    }
    sgInIntr[pThisCore->mCoreIx] = TRUE;

    K2_ASSERT(!pThisCore->mIsInMonitor);
    pCurThread = pThisCore->mpActiveThread;
    K2_ASSERT(pCurThread->mIsInKernelMode);
    pExTrap = pCurThread->mpKernExTrapStack;

    if (pExTrap == NULL)
    {
        // thread faulted in kernel with no trap to hold it
        pCurThread->mStackPtr_Kernel = (UINT32)&aContext;
        K2_CpuWriteBarrier();
        KernSched_UntrappedKernelRaiseException(pThisCore, pCurThread);
        pThisCore->mIsInMonitor = TRUE;
        sgInIntr[pThisCore->mCoreIx] = FALSE;
        X32Kern_EnterMonitor(pThisCore->TSS.mESP0);
    }

    sPopTrap(pThisCore, pCurThread, pExTrap, &aContext, aContext.REGS.ECX);
}

void KernArch_InstallDevIntrHandler(K2OSKERN_OBJ_INTR *apIntr)
{
    UINT32 irqIx;
    BOOL   disp;

    irqIx = apIntr->IrqConfig.mSourceIrq;

    K2_ASSERT(irqIx < X32_DEVIRQ_MAX_COUNT);

    disp = K2OSKERN_SeqIntrLock(&gX32Kern_IntrSeqLock);

    K2_ASSERT(sgpIntrObjByIrqIx[irqIx] == NULL);
    sgpIntrObjByIrqIx[irqIx] = apIntr;
    X32Kern_ConfigDevIrq(&apIntr->IrqConfig);

    K2OSKERN_SeqIntrUnlock(&gX32Kern_IntrSeqLock, disp);
}

void KernArch_SetDevIntrMask(K2OSKERN_OBJ_INTR *apIntr, BOOL aMask)
{
    UINT32 irqIx;
    BOOL   disp;

    irqIx = apIntr->IrqConfig.mSourceIrq;

    K2_ASSERT(irqIx < X32_DEVIRQ_MAX_COUNT);

    disp = K2OSKERN_SeqIntrLock(&gX32Kern_IntrSeqLock);

    K2_ASSERT(sgpIntrObjByIrqIx[irqIx] != NULL);
    if (aMask)
        X32Kern_MaskDevIrq(irqIx);
    else
        X32Kern_UnmaskDevIrq(irqIx);

    K2OSKERN_SeqIntrUnlock(&gX32Kern_IntrSeqLock, disp);
}

void KernArch_RemoveDevIntrHandler(K2OSKERN_OBJ_INTR *apIntr)
{
    UINT32 irqIx;
    BOOL   disp;

    irqIx = apIntr->IrqConfig.mSourceIrq;

    K2_ASSERT(irqIx < X32_DEVIRQ_MAX_COUNT);

    disp = K2OSKERN_SeqIntrLock(&gX32Kern_IntrSeqLock);

    K2_ASSERT(sgpIntrObjByIrqIx[irqIx] == apIntr);
    X32Kern_MaskDevIrq(irqIx);
    sgpIntrObjByIrqIx[irqIx] = NULL;

    K2OSKERN_SeqIntrUnlock(&gX32Kern_IntrSeqLock, disp);
}

void KernArch_Panic(K2OSKERN_CPUCORE volatile *apThisCore, BOOL aDumpStack)
{
    if (aDumpStack)
    {
        X32Kern_DumpStackTrace(
            apThisCore->mIsInMonitor ? gpProc0 : apThisCore->mpActiveProc,
            (UINT32)K2_RETURN_ADDRESS,
            X32_ReadEBP(),
            X32_ReadESP(),
            &sgSymDump[apThisCore->mCoreIx * SYM_NAME_MAX_LEN]
        );
    }
    while (1);
}

