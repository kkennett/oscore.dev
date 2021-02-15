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

//
// globally defined public specifications
//
#include <spec/acpi.h>
#include <spec/fat.h>
#include <spec/smbios.h>
#include <spec/k2uefi.h>

//
// all these libraries are in the shared global CRT
//
#include <lib/k2mem.h>
#include <lib/k2asc.h>
#include <lib/k2list.h>
#include <lib/k2atomic.h>
#include <lib/k2tree.h>
#include <lib/k2crc.h>

#if __cplusplus
extern "C" {
#endif

//
//------------------------------------------------------------------------
//


//
// this data comes in halfway into the
// transition page between UEFI and the kernel
//
typedef struct _K2OS_UEFI_LOADINFO K2OS_UEFI_LOADINFO;
struct _K2OS_UEFI_LOADINFO
{
    UINT32                  mMarker;

    K2EFI_SYSTEM_TABLE *    mpEFIST;

    DLX_pf_ENTRYPOINT       mSystemVirtualEntrypoint;
    DLX_pf_ENTRYPOINT       mKernDlxEntry;

    UINT32                  mBuiltinRofsPhys;

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
    UINT32                  mFwFacsPhys;
    UINT32                  mFwXFacsPhys;
};
K2_STATIC_ASSERT(sizeof(K2OS_UEFI_LOADINFO) < (K2_VA32_MEMPAGE_BYTES / 2));
K2_STATIC_ASSERT(sizeof(K2OS_UEFI_LOADINFO) == K2OS_UEFI_LOADINFO_SIZE_BYTES);

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
//------------------------------------------------------------------------
//

#if __cplusplus
}
#endif

#endif // __K2OSBASE_H