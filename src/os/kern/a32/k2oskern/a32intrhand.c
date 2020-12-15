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
    //
    // TBD
    //

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
    //
    // TBD
    //

    /* interrupts off. In IRQ mode.  r0-r3,r13(usr/sys),r15 in exception context. r12 saved in r14 spot in exception context. */
    K2_ASSERT(0);

    /* MUST return 0 if was in sys mode */
    return 0;
}

UINT32
A32Kern_InterruptHandler(
    UINT32 aReason,
    UINT32 aStackPtr,
    UINT32 aCPSR
)
{
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
    while (1);
}
