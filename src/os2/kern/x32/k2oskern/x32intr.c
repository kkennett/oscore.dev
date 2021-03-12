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

BOOL
K2_CALLCONV_REGS
K2OSKERN_SetIntr(
    BOOL aEnable
)
{
    return !X32_SetCoreInterruptMask(!aEnable);
}

BOOL
K2_CALLCONV_REGS
K2OSKERN_GetIntr(
    void
    )
{
    return X32_GetCoreInterruptMask() ? FALSE : TRUE;
}

void    
KernArch_SendIci(
    K2OSKERN_CPUCORE volatile * apThisCore,
    UINT32                      aTargetMask
)
{
    UINT32 reg;

    /* wait for pending send bit to clear */
    do {
        reg = MMREG_READ32(K2OS_KVA_X32_LOCAPIC, X32_LOCAPIC_OFFSET_ICR_LOW32);
    } while (reg & (1 << 12));

    /* set target cpus mask */
    reg = (aTargetMask & ((1 << gData.LoadInfo.mCpuCoreCount)-1)) << 24;
    K2_ASSERT(0 != reg);

    /* send to logical CPU interrupt X32KERN_VECTOR_ICI_BASE + my Index */
    MMREG_WRITE32(K2OS_KVA_X32_LOCAPIC, X32_LOCAPIC_OFFSET_ICR_HIGH32, reg);
    MMREG_WRITE32(K2OS_KVA_X32_LOCAPIC, X32_LOCAPIC_OFFSET_ICR_LOW32, X32_LOCAPIC_ICR_LOW_LEVEL_ASSERT | X32_LOCAPIC_ICR_LOW_LOGICAL | X32_LOCAPIC_ICR_LOW_MODE_FIXED | (X32KERN_VECTOR_ICI_BASE + apThisCore->mCoreIx));
}

