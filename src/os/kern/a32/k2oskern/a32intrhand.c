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

#include "a32kern.h"

static char sgSymDump[SYM_NAME_MAX_LEN * K2OS_MAX_CPU_COUNT];

void KernArch_InstallDevIntrHandler(K2OSKERN_OBJ_INTR *apIntr)
{
    K2_ASSERT(0);
}

void KernArch_SetDevIntrMask(K2OSKERN_OBJ_INTR *apIntr, BOOL aMask)
{
    K2_ASSERT(0);
}

void KernArch_RemoveDevIntrHandler(K2OSKERN_OBJ_INTR *apIntr)
{
    K2_ASSERT(0);
}

BOOL
A32Kern_CheckSvcInterrupt(
    UINT32 aStackPtr
)
{
    K2OSKERN_Debug("A32Kern_CheckSvcInterrupt()\n");
    while (1);

    /* interrupts off. In SVC mode.  r0-r3,r13(usr/sys),r15 in exception context. r12 saved in r14 spot in exception context. */
    K2_ASSERT(0);

    /* return nonzero for slow call (via A32Kern_InterruptHandler) */
    return 1;
}

BOOL
A32Kern_CheckIrqInterrupt(
    UINT32 aStackPtr
)
{
    K2OSKERN_Debug("A32Kern_CheckIrqInterrupt()\n");
    while (1);

    /* interrupts off. In IRQ mode.  r0-r3,r13(usr/sys),r15 in exception context. r12 saved in r14 spot in exception context. */
    K2_ASSERT(0);

    /* MUST return 0 if was in sys mode */
    return 0;
}

static void
sEmitSymbolName(
    K2OSKERN_OBJ_PROCESS *  apProc,
    UINT32                  aAddr,
    char *                  pBuffer
)
{
    KernDlx_FindClosestSymbol(apProc, aAddr, pBuffer, SYM_NAME_MAX_LEN);
    if (*pBuffer == 0)
    {
        K2OSKERN_Debug("?(%08X)", aAddr);
        return;
    }
    K2OSKERN_Debug("%s", pBuffer);
}

void A32Kern_DumpStackTrace(K2OSKERN_OBJ_PROCESS *apProc, UINT32 aPC, UINT32 aSP, UINT32* apBackPtr, UINT32 aLR, char *apBuffer)
{
    K2OSKERN_Debug("StackTrace:\n");
    K2OSKERN_Debug("------------------\n");
    K2OSKERN_Debug("SP       %08X\n", aSP);
    K2OSKERN_Debug("PC       ");
    sEmitSymbolName(apProc, aPC, apBuffer);
    K2OSKERN_Debug("\n");
    K2OSKERN_Debug("LR       %08X ", aLR);
    sEmitSymbolName(apProc, aLR, apBuffer);
    K2OSKERN_Debug("\n");

    K2OSKERN_Debug("%08X ", apBackPtr);
    if (apBackPtr == NULL)
    {
        K2OSKERN_Debug("\n");
        return;
    }
    K2OSKERN_Debug("%08X ", apBackPtr[0]);
    sEmitSymbolName(apProc, apBackPtr[0], apBuffer);
    K2OSKERN_Debug("\n");

    do {
        apBackPtr = (UINT32*)apBackPtr[-1];
        K2OSKERN_Debug("%08X ", apBackPtr);
        if (apBackPtr == NULL)
        {
            K2OSKERN_Debug("\n");
            return;
        }
        if (apBackPtr[0] == 0)
        {
            K2OSKERN_Debug("00000000\n");
            return;
        }
        K2OSKERN_Debug("%08X ", apBackPtr[0]);
        sEmitSymbolName(apProc, apBackPtr[0], apBuffer);
        K2OSKERN_Debug("\n");
    } while (1);
}

void A32Kern_DumpExceptionContext(K2OSKERN_CPUCORE volatile* apCore, UINT32 aReason, A32_EXCEPTION_CONTEXT* pEx)
{
    K2OSKERN_Debug("Exception %d (", aReason);
    switch (aReason)
    {
    case A32KERN_EXCEPTION_REASON_UNDEFINED_INSTRUCTION:
        K2OSKERN_Debug("Undefined Instruction");
        break;
    case A32KERN_EXCEPTION_REASON_SYSTEM_CALL:
        K2OSKERN_Debug("System Call");
        break;
    case A32KERN_EXCEPTION_REASON_PREFETCH_ABORT:
        K2OSKERN_Debug("Prefetch Abort");
        break;
    case A32KERN_EXCEPTION_REASON_DATA_ABORT:
        K2OSKERN_Debug("Data Abort");
        break;
    case A32KERN_EXCEPTION_REASON_IRQ_RESCHED:
        K2OSKERN_Debug("IRQ Resched");
        break;
    case A32KERN_EXCEPTION_REASON_RAISE_EXCEPTION:
        K2OSKERN_Debug("RaiseException");
        break;
    default:
        K2OSKERN_Debug("Unknown");
        break;
    }
    K2OSKERN_Debug(") in Arm32.");
    switch (pEx->SPSR & A32_PSR_MODE_MASK)
    {
    case A32_PSR_MODE_USR:
        K2OSKERN_Debug("USR");
        break;
    case A32_PSR_MODE_SYS:
        K2OSKERN_Debug("SYS");
        break;
    case A32_PSR_MODE_SVC:
        K2OSKERN_Debug("SVC");
        break;
    case A32_PSR_MODE_UNDEF:
        K2OSKERN_Debug("UND");
        break;
    case A32_PSR_MODE_ABT:
        K2OSKERN_Debug("ABT");
        break;
    case A32_PSR_MODE_IRQ:
        K2OSKERN_Debug("IRQ");
        break;
    case A32_PSR_MODE_FIQ:
        K2OSKERN_Debug("FIQ");
        break;
    default:
        K2OSKERN_Debug("???");
        break;
    }
    K2OSKERN_Debug(" mode. ExContext @ 0x%08X\n", pEx);
    K2OSKERN_Debug("------------------\n");
    K2OSKERN_Debug("Core      %d\n", apCore->mCoreIx);
    K2OSKERN_Debug("DFAR      %08X\n", A32_ReadDFAR());
    K2OSKERN_Debug("DFSR      %08X\n", A32_ReadDFSR());
    K2OSKERN_Debug("IFAR      %08X\n", A32_ReadIFAR());
    K2OSKERN_Debug("IFSR      %08X\n", A32_ReadIFSR());
    K2OSKERN_Debug("------------------\n");
    K2OSKERN_Debug("SPSR      %08X\n", pEx->SPSR);
    K2OSKERN_Debug("  r0=%08X   r8=%08X\n", pEx->R[0], pEx->R[8]);
    K2OSKERN_Debug("  r1=%08X   r9=%08X\n", pEx->R[1], pEx->R[9]);
    K2OSKERN_Debug("  r2=%08X  r10=%08X\n", pEx->R[2], pEx->R[10]);
    K2OSKERN_Debug("  r3=%08X  r11=%08X\n", pEx->R[3], pEx->R[11]);
    K2OSKERN_Debug("  r4=%08X  r12=%08X\n", pEx->R[4], pEx->R[12]);
    K2OSKERN_Debug("  r5=%08X   sp=%08X\n", pEx->R[5], pEx->R[13]);
    K2OSKERN_Debug("  r6=%08X   lr=%08X\n", pEx->R[6], pEx->R[14]);
    K2OSKERN_Debug("  r7=%08X   pc=%08X\n", pEx->R[7], pEx->R[15]);

//    if (aReason == A32KERN_EXCEPTION_REASON_DATA_ABORT)
//    {
//        KernArch_AuditVirt(A32_ReadDFAR(), 0, 0, (A32_ReadDFSR() & 0x800) ? K2OS_MEMPAGE_ATTR_WRITEABLE : K2OS_MEMPAGE_ATTR_READABLE);
//        KernMem_DumpVM();
//    }
}

UINT32
A32Kern_InterruptHandler(
    UINT32 aReason,
    UINT32 aStackPtr,
    UINT32 aCPSR
)
{
    //
    // We are in the mode detailed in the aCPSR. If we interrupted from user mode this will be set to SYS
    //

    K2OSKERN_CPUCORE volatile*  pThisCore;
    A32_EXCEPTION_CONTEXT*      pEx;

    pThisCore = K2OSKERN_GET_CURRENT_CPUCORE;
    pEx = (A32_EXCEPTION_CONTEXT*)aStackPtr;

    K2OSKERN_Debug("A32Kern_InterruptHandler(%d, %08X, %08X)\n", aReason, aStackPtr, aCPSR);

    A32Kern_DumpExceptionContext(pThisCore, aReason, pEx);
    A32Kern_DumpStackTrace(gpProc0, pEx->R[15], pEx->R[13], (UINT32*)pEx->R[11], pEx->R[14], &sgSymDump[pThisCore->mCoreIx * SYM_NAME_MAX_LEN]);
    K2OSKERN_Panic("mointor fault stop\n");

    while (1);
    //
    // TBD
    //

    K2_ASSERT(0);

    //
    // returns stack pointer to use when jumping into monitor
    //
    return 0;
}

void KernArch_Panic(K2OSKERN_CPUCORE volatile *apThisCore, BOOL aDumpStack)
{
    if (aDumpStack)
    {
        A32Kern_DumpStackTrace(
            apThisCore->mIsInMonitor ? gpProc0 : apThisCore->mpActiveProc,
            (UINT32)KernArch_Panic,
            A32_ReadStackPointer(),
            (UINT32 *)A32_ReadFramePointer(),
            (UINT32)K2_RETURN_ADDRESS,
            &sgSymDump[apThisCore->mCoreIx * SYM_NAME_MAX_LEN]
        );
    }
    while (1);
}

