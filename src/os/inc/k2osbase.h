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

#ifndef __K2OSBASE_H
#define __K2OSBASE_H

//
// Systems that interact with the OS can include this without
// including the OS specific primitives.  For example, UEFI
// source can include this as long as it has the shared/inc
// path on its list of include paths
//

#include <k2systype.h>

#include "k2osdefs.inc"

#include <spec/acpi.h>
#include <spec/smbios.h>
#include <spec/k2uefi.h>

#include <lib/k2mem.h>
#include <lib/k2asc.h>
#include <lib/k2list.h>
#include <lib/k2atomic.h>
#include <lib/k2tree.h>
#include <lib/k2crc.h>

#if K2_TARGET_ARCH_IS_INTEL
#include <lib/k2archx32.h>
#endif
#if K2_TARGET_ARCH_IS_ARM
#include <lib/k2archa32.h>
#endif

//
//------------------------------------------------------------------------
//

#define K2OS_TIMEOUT_INFINITE   ((UINT32)-1)

//
//------------------------------------------------------------------------
//

typedef K2_RAW_TOKEN K2OS_TOKEN;

typedef UINT32 (K2_CALLCONV_REGS *K2OS_pf_THREAD_ENTRY)(void *apArgument);

//
//------------------------------------------------------------------------
//

typedef struct _K2OS_CPUINFO K2OS_CPUINFO;
struct _K2OS_CPUINFO
{
    UINT32  mCpuId;
    UINT32  mAddrSet;
    UINT32  mAddrGet;
    UINT32  mAddrClear;
    UINT32  mClearValue;
};

//
// this data typically lives halfway into the
// transition page between UEFI and the kernel
//
typedef struct _K2OS_UEFI_LOADINFO K2OS_UEFI_LOADINFO;
struct _K2OS_UEFI_LOADINFO
{
    UINT32                  mMarker;
    K2EFI_SYSTEM_TABLE *    mpEFIST;
    DLX_pf_ENTRYPOINT       mSystemVirtualEntrypoint;
    UINTN                   mKernArenaLow;
    UINTN                   mKernArenaHigh;
    DLX *                   mpDlxCrt;
    UINT32                  mTransBasePhys;
    UINT32                  mZeroPagePhys;
    UINT32                  mTransitionPageAddr;
    UINT32                  mDebugPageVirt;
    UINT32                  mEfiMapSize;
    UINT32                  mEfiMemDescSize;
    UINT32                  mEfiMemDescVer;
    UINT32                  mCpuCoreCount;
    UINT32                  mFwTabPagesPhys;
    UINT32                  mFwTabPagesVirt;
    UINT32                  mFwTabPageCount;
    K2OS_CPUINFO            CpuInfo[K2OS_MAX_CPU_COUNT];
};
K2_STATIC_ASSERT(sizeof(K2OS_UEFI_LOADINFO) < (K2_VA32_MEMPAGE_BYTES / 2));
K2_STATIC_ASSERT(sizeof(K2OS_UEFI_LOADINFO) == (K2OS_UEFI_LOADINFO_OFFSET_CPU_INFO + (sizeof(K2OS_CPUINFO) * K2OS_MAX_CPU_COUNT)));

//
// There is one of these entries per page of physical address space (4GB)
// passed in from EFI to the kernel.  Depending on how much
// physical memory there actually is, not all the virtual pages holding this
// information will be mapped to distinct physical pages. There is a 'zero page'
// that any empty space regions of 4MB aligned to 4MB boundaries will be tracked by.
// This means that if you only have 64MB of memory in your system, you are only 
// paying for the physical memory to track that 64MB, not for the physical memory
// needed to track 4GB.  If you *do* have 4GB of memory in your system, you will
// pay a maximum of 16MB memory charge to track it at the page level.
// 16 bytes * 1 Million pages = 16MB.  THis is the size of the virtual area
// carved out in the kernel va space for this purpose.  VA space is cheap
//
typedef struct _K2OS_PHYSTRACK_UEFI K2OS_PHYSTRACK_UEFI;
struct _K2OS_PHYSTRACK_UEFI
{
    UINT32  mProp;
    UINT32  mType;
    UINT32  mUnused0;
    UINT32  mUnused1;
};
K2_STATIC_ASSERT(sizeof(K2OS_PHYSTRACK_UEFI) == K2OS_PHYSTRACK_BYTES);
K2_STATIC_ASSERT(sizeof(K2OS_PHYSTRACK_UEFI) == sizeof(K2TREE_NODE));

//
// this comes from the base CRT for dlx
// for everything that links with it, which 
// is everything except k2oscrt (kern and user)
//
DLX *
K2OS_GetDlxModule(
    void
);

//
//------------------------------------------------------------------------
//

typedef struct _K2OS_SYSINFO K2OS_SYSINFO;
struct _K2OS_SYSINFO
{
    UINT32  mStructBytes;
    UINT32  mCrtVersion;
    UINT32  mKernVersion;
    UINT32  mHalVersion;
    UINT32  mHalVendorMarker;
    UINT32  mCpuCount;
    UINT32  mMaxCacheLineBytes;
};

//
//------------------------------------------------------------------------
//

enum _K2OS_CACHEOP
{
    K2OS_CACHEOP_Init,
    K2OS_CACHEOP_FlushData,
    K2OS_CACHEOP_InvalidateData,
    K2OS_CACHEOP_FlushInvalidateData,
    K2OS_CACHEOP_InvalidateInstructions
};
typedef enum _K2OS_CACHEOP K2OS_CACHEOP;

void
K2OS_CacheOperation(
    K2OS_CACHEOP    aCacheOp,
    void *          apAddress,
    UINT32          aBytes
);

//
//------------------------------------------------------------------------
//

#endif // __K2OSBASE_H