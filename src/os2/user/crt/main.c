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

void *                          __dso_handle;

extern DLX_INFO const * const   gpDlxInfo;
extern void *                   __data_end;

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
    *((UINT32 *)0) = 0;
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

static UINT8 sgDlxInitMem[K2_VA32_MEMPAGE_BYTES * 3];

#if 0
typedef K2STAT (*pfK2DLXSUPP_CritSec)(BOOL aEnter);
typedef void   (*pfK2DLXSUPP_AtReInit)(DLX *apDlx, UINT32 aModulePageLinkAddr, K2DLXSUPP_HOST_FILE *apInOutHostFile);
typedef K2STAT (*pfK2DLXSUPP_AcqAlreadyLoaded)(void *apAcqContext, K2DLXSUPP_HOST_FILE aHostFile);
typedef K2STAT (*pfK2DLXSUPP_Open)(void *apAcqContext, char const * apFileSpec, char const *apNamePart, UINT32 aNamePartLen, K2DLXSUPP_OPENRESULT *apRetResult);
typedef K2STAT (*pfK2DLXSUPP_ReadSectors)(void *apAcqContext, K2DLXSUPP_HOST_FILE aHostFile, void *apBuffer, UINT32 aSectorCount);
typedef K2STAT (*pfK2DLXSUPP_Prepare)(void *apAcqContext, K2DLXSUPP_HOST_FILE aHostFile, DLX_INFO *apInfo, UINT32 aInfoSize, BOOL aKeepSymbols, K2DLXSUPP_SEGALLOC *apRetAlloc);
typedef BOOL   (*pfK2DLXSUPP_PreCallback)(void *apAcqContext, K2DLXSUPP_HOST_FILE aHostFile, BOOL aIsLoad, DLX *apDlx);
typedef K2STAT (*pfK2DLXSUPP_PostCallback)(void *apAcqContext, K2DLXSUPP_HOST_FILE aHostFile, K2STAT aUserStatus, DLX *apDlx);
typedef K2STAT (*pfK2DLXSUPP_Finalize)(void *apAcqContext, K2DLXSUPP_HOST_FILE aHostFile, K2DLXSUPP_SEGALLOC *apUpdateAlloc);
typedef K2STAT (*pfK2DLXSUPP_RefChange)(K2DLXSUPP_HOST_FILE aHostFile, DLX *apDlx, INT32 aRefChange);
typedef K2STAT (*pfK2DLXSUPP_Purge)(K2DLXSUPP_HOST_FILE aHostFile);
typedef K2STAT (*pfK2DLXSUPP_ErrorPoint)(char const *apFile, int aLine, K2STAT aStatus);

typedef struct _K2DLXSUPP_HOST ;
struct _K2DLXSUPP_HOST
{
    UINT32                        mHostSizeBytes;

    pfK2DLXSUPP_CritSec           CritSec;
    pfK2DLXSUPP_AcqAlreadyLoaded  AcqAlreadyLoaded;
    pfK2DLXSUPP_Open              Open;
    pfK2DLXSUPP_AtReInit          AtReInit;
    pfK2DLXSUPP_ReadSectors       ReadSectors;
    pfK2DLXSUPP_Prepare           Prepare;
    pfK2DLXSUPP_PreCallback       PreCallback;
    pfK2DLXSUPP_PostCallback      PostCallback;
    pfK2DLXSUPP_Finalize          Finalize;
    pfK2DLXSUPP_RefChange         RefChange;
    pfK2DLXSUPP_Purge             Purge;
    pfK2DLXSUPP_ErrorPoint        ErrorPoint;
};
#endif
static K2DLXSUPP_HOST sgDlxHost;

static K2STAT 
sReInitDlx(
    void
)
{
    K2STAT stat;
    UINT8 *pLoaderPage;

    pLoaderPage = (UINT8 *)((((UINT32)sgDlxInitMem) + (K2_VA32_MEMPAGE_BYTES - 1)) & K2_VA32_PAGEFRAME_MASK);

    stat = K2DLXSUPP_Init(
        pLoaderPage,
        &sgDlxHost,
        TRUE,   // keep symbols
        TRUE,   // reinit
        FALSE); // link in place only
    K2_ASSERT(!K2STAT_IS_ERROR(stat));

    return stat;

}


void
K2_CALLCONV_REGS
__k2oscrt_user_entry(
    UINT32 aCoreIx,
    UINT32 aArg2
    )
{
    //
    // this will never execute as aCoreIx will never equal 0xFEEDF00D
    // it is here to pull in exports and must be 'reachable' code.
    // the *ADDRESS OF* gpDlxInfo is even invalid and trying to use
    // it in executing code will cause a fault.
    //
    if (aCoreIx == 0xFEEDF00D)
        __dso_handle = (void *)(*((UINT32 *)gpDlxInfo));

    //
    // must be the first thing that k2oscrt does
    //
    *((UINT32 *)sgDlxInitMem) = (UINT32)K2DLXSUPP_Init;
    *(((UINT32 *)sgDlxInitMem) + 1) = (UINT32)DLX_Acquire;
    K2OS_SYSCALL(K2OS_SYSCALL_ID_CRT_INIT, (UINT32)sgDlxInitMem);

    //
    // actual code goes here
    //
    K2OS_Thread_SetLastStatus(K2STAT_NO_ERROR);

    sReInitDlx();

    while (1);
}

