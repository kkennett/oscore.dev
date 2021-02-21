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

//
// global
//
KERN_DATA               gData;
K2_pf_ASSERT            K2_Assert = KernEx_Assert;
K2_pf_EXTRAP_MOUNT      K2_ExTrap_Mount = KernEx_TrapMount;
K2_pf_EXTRAP_DISMOUNT   K2_ExTrap_Dismount = KernEx_TrapDismount;
K2_pf_RAISE_EXCEPTION   K2_RaiseException = KernEx_RaiseException;

UINT8 image_padding[4095] __attribute__((section(".bss_padding")));

//
// C++ constructors/destructors
//
typedef void (*__vfpt)(void);
typedef void (*__vfpv)(void *);
extern __attribute__((weak)) __vfpt __ctors[];
extern __attribute__((weak)) UINT32 __ctors_count;
extern UINT32                       __bss_begin;
extern UINT32                       __data_end;

static void 
__call_ctors(
    void
)
{
    UINT32 i;
    UINT32 c;

    c = (UINT32)&__ctors_count;
    if (c==0)
        return;
    for(i=0;i<c;i++)
        (*__ctors[i])();
}

void K2_CALLCONV_REGS __attribute__((noreturn)) 
Kern_Main(
    K2OS_UEFI_LOADINFO const *apLoadInfo
    )
{
    //
    // zero globals
    //
    K2MEM_Zero(&gData, sizeof(KERN_DATA));

    //
    // save the load info because we are going to destroy the transition page that holds it
    //
    K2MEM_Copy(&gData.LoadInfo, apLoadInfo, sizeof(K2OS_UEFI_LOADINFO));

    //
    // early hal init
    //
    K2OSHAL_EarlyInit();

    //
    // init for debug messages
    //
    K2OSKERN_SeqInit(&gData.DebugSeqLock);

    //
    // send out first debug message
    //
    K2OSKERN_Debug("\n\n===========\nK2OS Kernel\n===========\n\n");

    //
    // architectural inits
    //
    KernArch_InitAtEntry();

    //
    // physical memory inits
    //
    KernPhys_Init();

    //
    // call C++ constructors now if there are any
    //
    if ((((UINT32)&__ctors_count)!=0) &&
        (((UINT32)&__ctors)!=0))
        __call_ctors();

    //
    // set up process fundamentals
    //
    KernProc_Init();

    //
    // set up cpu fundamentals
    //
    KernCpu_Init();

    //
    // launch the CPU now
    //
    KernArch_LaunchCpuCores();

    //
    // this will never actually happen.  this is here so that the bss padding comes in
    //
    if (0x12345678 == (UINT32)apLoadInfo)
        K2MEM_Copy(&gData, image_padding, 4095);

    while (1);
}

//
// libgcc functions
//
int  
__cxa_atexit(
    __vfpv f, 
    void * a, 
    void *apModule
)
{
    //
    // should never be called
    //
    K2_ASSERT(0);
    return 0;
}

void 
__call_dtors(
    void *apModule
)
{
    //
    // should never be called
    //
    K2_ASSERT(0);
}

#if K2_TARGET_ARCH_IS_ARM

void 
__aeabi_idiv0(
    void
) 
K2_CALLCONV_NAKED;

void 
__aeabi_idiv0(
    void
)
{
    asm("mov r0, %[code]\n" : : [code]"r"(K2STAT_EX_ZERODIVIDE));
    asm("mov r12, %[targ]\n" : : [targ]"r"((UINT32)K2_RaiseException));
    asm("bx r12\n");
}

void
__aeabi_ldiv0(
    void
)
K2_CALLCONV_NAKED;

void
__aeabi_ldiv0(
    void
)
{
    asm("mov r0, %[code]\n" : : [code]"r"(K2STAT_EX_ZERODIVIDE));
    asm("mov r12, %[targ]\n" : : [targ]"r"((UINT32)K2_RaiseException));
    asm("bx r12\n");
}

int
__aeabi_atexit(
    void *  object, 
    __vfpv  destroyer, 
    void *  dso_handle
)
{
    return __cxa_atexit(destroyer, object, dso_handle);
}

#endif

