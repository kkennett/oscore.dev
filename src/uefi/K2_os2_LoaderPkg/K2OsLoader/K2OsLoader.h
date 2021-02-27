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
#ifndef __K2OSLOADER_H__
#define __K2OSLOADER_H__

#include <IndustryStandard/Smbios.h>
#include <IndustryStandard/Acpi50.h>

#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>

#include <Guid/FileInfo.h>

#include <Protocol/SimpleFileSystem.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/AcpiSystemDescriptionTable.h>
#include <Protocol/GraphicsOutput.h>

#include <lib/k2dlxsupp.h>

#include <k2osbase.h>
#include <lib/k2vmap32.h>
#include <lib/k2elf32.h>
#include <lib/k2sort.h>

#if K2_TARGET_ARCH_IS_ARM
#include <Library/ArmLib.h>
#else

#endif

typedef struct _LOADER_DATA LOADER_DATA;
struct _LOADER_DATA
{
    EFI_HANDLE										mLoaderImageHandle;
    K2VMAP32_CONTEXT								Map;
    EFI_MEMORY_DESCRIPTOR *							mpMemoryMap;
    UINTN											mMemMapKey;
    K2OS_UEFI_LOADINFO								LoadInfo;           // this is copied to transition page
    SMBIOS_STRUCTURE_POINTER *                      mpSmbios;
	EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER  * mpAcpi;
    UINT32                                          mKernElfPhys;
    UINT32                                          mRofsBytes;
    UINT32                                          mKernArenaLow;
    UINT32                                          mKernArenaHigh;
    UINT32                                          mPreRuntimeHigh;
};

extern LOADER_DATA gData;

UINTN       K2Printf(CHAR16 const * apFormat, ...);

EFI_STATUS  Loader_InitArch(void);
void        Loader_DoneArch(void);

EFI_STATUS  Loader_FillCpuInfo(void);

EFI_STATUS  Loader_CreateVirtualMap(void);

EFI_STATUS  Loader_MapKernelArena(void);

K2STAT      Loader_AssembleAcpi(void);

K2STAT      Loader_TrackEfiMap(void);

EFI_STATUS  Loader_UpdateMemoryMap(void);

K2STAT      Loader_AssignRuntimeVirtual(void);

void        Loader_TransitionToKernel(void);

#endif /* __K2OSLOADER_H__ */
