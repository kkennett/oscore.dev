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
#include "crt.h"

void *              __dso_handle;

extern DLX_INFO *   gpDlxInfo;
extern void *       __data_end;

int  __cxa_atexit(__vfpv f, void *a, DLX *apDlx);
void __call_dtors(DLX *apDlx);

#if K2_TARGET_ARCH_IS_ARM

int __aeabi_atexit(void *object, __vfpv destroyer, void *dso_handle)
{
    return __cxa_atexit(destroyer, object, (DLX *)dso_handle);
}

#endif

static 
void 
K2_CALLCONV_REGS 
sAssert(char const * apFile, int aLineNum, char const * apCondition)
{
    while (1);
}

K2_pf_ASSERT K2_Assert = sAssert;

DLX *
K2OS_GetDlxModule(
    void
)
{
    return (DLX *)__dso_handle;
}

void
K2_CALLCONV_REGS
__k2oscrt_user_entry(
    UINT32 aCoreIx,
    UINT32 aArg2
    )
{
    //
    // used to pull in gpDlxInfo for linker. that's it.  this comparison
    // will never be true
    //
    if (aCoreIx == 0x01020304)
        gpDlxInfo++;

    //
    // actual code goes here
    //


    while (1);
}

