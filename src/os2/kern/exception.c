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

#include "kern.h"

void K2_CALLCONV_REGS 
KernEx_Assert(
    char const *    apFile,
    int             aLineNum,
    char const *    apCondition
    )
{
    KernDbg_Panic("***KERNEL ASSERT:\nLocation: %s(%d)\nFALSE ==> %s\n", apFile, aLineNum, apCondition);
    while (1);
}

BOOL K2_CALLCONV_REGS 
KernEx_TrapMount(
    K2_EXCEPTION_TRAP *apTrap
)
{
    // always return FALSE as there is no trap handling inside the kernel
    return FALSE;
}

K2STAT K2_CALLCONV_REGS 
KernEx_TrapDismount(
    K2_EXCEPTION_TRAP *apTrap
    )
{
    // there is no trap handling inside the kernel, so if this is called there was no exception
    return K2STAT_NO_ERROR;
}

void K2_CALLCONV_REGS 
KernEx_RaiseException(
    K2STAT aExceptionCode
)
{
    KernDbg_Panic("***KERNEL EXCEPTION: %08X\n", aExceptionCode);
    while (1);
}
