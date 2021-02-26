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

static K2OSKERN_OBJ_INTR *  sgpIntrObjByIrqIx[X32_NUM_IDT_ENTRIES] = { 0, };

static void 
sAbort(
    K2OSKERN_CPUCORE volatile * apThisCore,
    X32_EXCEPTION_CONTEXT *     apContext
)
{
    BOOL wasKernelMode;

    K2OSKERN_Debug("Core %d, Exception Context @ %08X\n", apThisCore->mCoreIx, apContext);
    K2OSKERN_Debug("Exception %d\n", apContext->Exception_Vector);
    K2OSKERN_Debug("CR2 = %08X\n", X32_ReadCR2());

    wasKernelMode = (apContext->KernelMode.CS == (X32_SEGMENT_SELECTOR_USER_CODE | X32_SELECTOR_RPL_USER)) ? FALSE : TRUE;

    if (!wasKernelMode)
        X32Kern_DumpUserModeExceptionContext(apContext);
    else
        X32Kern_DumpKernelModeExceptionContext(apContext);
    X32Kern_DumpStackTrace(
        gpProc1,
        wasKernelMode ? 
            apContext->KernelMode.EIP 
          : apContext->UserMode.EIP,
        apContext->REGS.EBP,
        wasKernelMode ? 
            ((UINT32)apContext) + X32KERN_SIZEOF_KERNELMODE_EXCEPTION_CONTEXT 
          : apContext->UserMode.ESP,
        &sgSymDump[apThisCore->mCoreIx * X32_SYM_NAME_MAX_LEN]
    );
    K2OSKERN_Panic(NULL);
}

void
X32Kern_InterruptHandler(
    X32_EXCEPTION_CONTEXT aContext
    )
{
    K2OSKERN_CPUCORE volatile * pThisCore;
    UINT32                      devIrq;
    UINT32                      srcCore;

    pThisCore = K2OSKERN_GET_CURRENT_CPUCORE;

    if (sgInIntr[pThisCore->mCoreIx])
    {
        K2OSKERN_Panic("Recursive exception or interrupt\n");
        while (1);
    }
    sgInIntr[pThisCore->mCoreIx] = TRUE;

    if (aContext.Exception_Vector < X32KERN_DEVVECTOR_BASE)
    {
        // TBD - sOnException(pThisCore, &aContext);
        sAbort(pThisCore, &aContext);
    }
    else if (aContext.Exception_Vector == 255)
    {
        K2OSKERN_Debug("System Call\n");
//        sAbort(pThisCore, &aContext);
    }
    else
    {
        K2OSKERN_Debug("Core %d Context %08X Vector %d\n", pThisCore->mCoreIx, &aContext, aContext.Exception_Vector);
        if (aContext.Exception_Vector >= X32KERN_VECTOR_ICI_BASE)
        {
            //
            // ICI from another core
            //
            K2_ASSERT(aContext.Exception_Vector <= X32KERN_VECTOR_ICI_LAST);
            srcCore = aContext.Exception_Vector - X32KERN_VECTOR_ICI_BASE;
            // TBD - OnIci(srcCore);
            K2OSKERN_Debug("Ici from core %d\n", srcCore);
            sAbort(pThisCore, &aContext);
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
                K2OSKERN_Debug("VECTOR %d -> IRQ %d on core %d\n", aContext.Exception_Vector, devIrq, pThisCore->mCoreIx);
                sgpIntrObjByIrqIx[devIrq]->mfHandler(sgpIntrObjByIrqIx[devIrq]->mpHandlerContext);
                // TBD - signal interrupt object 
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

    sgInIntr[pThisCore->mCoreIx] = FALSE;
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

