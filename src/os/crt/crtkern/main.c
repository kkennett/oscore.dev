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

#include "crtkern.h"

void *              __dso_handle;

K2OSKERN_SHARED     gShared;

extern DLX_INFO *   gpDlxInfo;
extern void *       __data_end;

int  __cxa_atexit(__vfpv f, void *a, DLX * apDlx);
void __call_dtors(DLX *apDlx);

#if K2_TARGET_ARCH_IS_ARM

int __aeabi_atexit(void *object, __vfpv destroyer, void *dso_handle)
{
    return __cxa_atexit(destroyer, object, (DLX *)dso_handle);
}

#endif

DLX *
K2OS_GetDlxModule(
    void
)
{
    return (DLX *)__dso_handle;
}

static 
void 
K2_CALLCONV_REGS 
sPreKernelAssert(
    char const *    apFile, 
    int             aLineNum, 
    char const *    apCondition
)
{
    while (1);
}
K2_pf_ASSERT            K2_Assert = sPreKernelAssert;
K2_pf_EXTRAP_MOUNT      K2_ExTrap_Mount = NULL;
K2_pf_EXTRAP_DISMOUNT   K2_ExTrap_Dismount = NULL;
K2_pf_RAISE_EXCEPTION   K2_RaiseException = NULL;

static
void
sInitialize(
    void
)
{
    K2STAT              status;
    DLX_pf_ENTRYPOINT   kernDlxEntry;

    K2_ASSERT(K2OS_UEFI_LOADINFO_OFFSET_MARKER           == K2_FIELDOFFSET(K2OS_UEFI_LOADINFO, mMarker));
    K2_ASSERT(K2OS_UEFI_LOADINFO_OFFSET_EFIST            == K2_FIELDOFFSET(K2OS_UEFI_LOADINFO, mpEFIST));
    K2_ASSERT(K2OS_UEFI_LOADINFO_OFFSET_SYSVIRTENTRY     == K2_FIELDOFFSET(K2OS_UEFI_LOADINFO, mSystemVirtualEntrypoint));
    K2_ASSERT(K2OS_UEFI_LOADINFO_OFFSET_KERNDLX_ENTRY    == K2_FIELDOFFSET(K2OS_UEFI_LOADINFO, mKernDlxEntry));
    K2_ASSERT(K2OS_UEFI_LOADINFO_OFFSET_UNUSED_10        == K2_FIELDOFFSET(K2OS_UEFI_LOADINFO, mUnused_10));
    K2_ASSERT(K2OS_UEFI_LOADINFO_OFFSET_DLXCRT           == K2_FIELDOFFSET(K2OS_UEFI_LOADINFO, mpDlxCrt));
    K2_ASSERT(K2OS_UEFI_LOADINFO_OFFSET_TRANSBASE_PHYS   == K2_FIELDOFFSET(K2OS_UEFI_LOADINFO, mTransBasePhys));
    K2_ASSERT(K2OS_UEFI_LOADINFO_OFFSET_ZEROPAGE_PHYS    == K2_FIELDOFFSET(K2OS_UEFI_LOADINFO, mZeroPagePhys));
    K2_ASSERT(K2OS_UEFI_LOADINFO_OFFSET_TRANS_PAGE_ADDR  == K2_FIELDOFFSET(K2OS_UEFI_LOADINFO, mTransitionPageAddr));
    K2_ASSERT(K2OS_UEFI_LOADINFO_OFFSET_DEBUG_PAGE_VIRT  == K2_FIELDOFFSET(K2OS_UEFI_LOADINFO, mDebugPageVirt));
    K2_ASSERT(K2OS_UEFI_LOADINFO_OFFSET_EFIMAP_SIZE      == K2_FIELDOFFSET(K2OS_UEFI_LOADINFO, mEfiMapSize));
    K2_ASSERT(K2OS_UEFI_LOADINFO_OFFSET_EFIMAP_DESC_SIZE == K2_FIELDOFFSET(K2OS_UEFI_LOADINFO, mEfiMemDescSize));
    K2_ASSERT(K2OS_UEFI_LOADINFO_OFFSET_EFIMAP_DESC_VER  == K2_FIELDOFFSET(K2OS_UEFI_LOADINFO, mEfiMemDescVer));
    K2_ASSERT(K2OS_UEFI_LOADINFO_OFFSET_CPU_CORE_COUNT   == K2_FIELDOFFSET(K2OS_UEFI_LOADINFO, mCpuCoreCount));
    K2_ASSERT(K2OS_UEFI_LOADINFO_OFFSET_FWTAB_PAGES_PHYS == K2_FIELDOFFSET(K2OS_UEFI_LOADINFO, mFwTabPagesPhys));
    K2_ASSERT(K2OS_UEFI_LOADINFO_OFFSET_FWTAB_PAGES_VIRT == K2_FIELDOFFSET(K2OS_UEFI_LOADINFO, mFwTabPagesVirt));
    K2_ASSERT(K2OS_UEFI_LOADINFO_OFFSET_FWTAB_PAGE_COUNT == K2_FIELDOFFSET(K2OS_UEFI_LOADINFO, mFwTabPageCount));
    K2_ASSERT(K2OS_UEFI_LOADINFO_OFFSET_FWFACS_PHYS      == K2_FIELDOFFSET(K2OS_UEFI_LOADINFO, mFwFacsPhys));
    K2_ASSERT(K2OS_UEFI_LOADINFO_OFFSET_XFWFACS_PHYS     == K2_FIELDOFFSET(K2OS_UEFI_LOADINFO, mFwXFacsPhys));
    K2_ASSERT(K2OS_UEFI_LOADINFO_OFFSET_CPU_INFO         == K2_FIELDOFFSET(K2OS_UEFI_LOADINFO, CpuInfo));

    kernDlxEntry = (DLX_pf_ENTRYPOINT)gShared.LoadInfo.mKernDlxEntry;

    status = kernDlxEntry(NULL, (UINT32)&gShared);
    K2_ASSERT(!K2STAT_IS_ERROR(status));

    K2_Assert = gShared.FuncTab.Assert;
    K2_ExTrap_Mount = gShared.FuncTab.ExTrap_Mount;
    K2_ExTrap_Dismount = gShared.FuncTab.ExTrap_Dismount;
    K2_RaiseException = gShared.FuncTab.RaiseException;
}

void
K2_CALLCONV_REGS
k2oscrt_kern_common_entry(
    K2OS_UEFI_LOADINFO const *  apUEFI,
    K2_CACHEINFO const *        apCacheInfo
)
{
    //
    // save builtin module handle 
    //
    __dso_handle = (void *)apUEFI->mpDlxCrt;

    //
    // copy in our copy of load info from transition page 
    //
    K2MEM_Zero(&gShared, sizeof(gShared));
    K2MEM_Copy(&gShared.LoadInfo, apUEFI, sizeof(K2OS_UEFI_LOADINFO));
    gShared.FuncTab.CoreThreadedPostInit = CrtKern_Threaded_InitAtExit;
    gShared.mpCacheInfo = apCacheInfo;

    //
    // Do all general k2oscrt initialization
    //
    sInitialize();

    //
    // Jump into kernel. will never return
    //
    gShared.FuncTab.Exec();

    //
    // this will never execute as apUEFI will never equal 0xFEEDFOOD
    // it is here to pull in exports and must be 'reachable' code
    //
    if (apUEFI == (K2OS_UEFI_LOADINFO const *)0xFEEDF00D)
        gpDlxInfo->mImportCount++;

    //
    // Exec will never exit
    //

    while (1);
}

