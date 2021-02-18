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
#include "K2OsLoader.h"

LOADER_DATA gData;

#if K2_TARGET_ARCH_IS_ARM
static CHAR16 const * const sgpKernSubDirName = L"k2os\\a32\\kern";
#else
static CHAR16 const * const sgpKernSubDirName = L"k2os\\x32\\kern";
#endif

static EFI_GUID const sgEfiFileInfoId = EFI_FILE_INFO_ID;

static EFI_STATUS sLoadRofs(EFI_FILE_PROTOCOL * apKernDir)
{
    EFI_STATUS              efiStatus;
    EFI_FILE_PROTOCOL *     pProt;
    UINTN                   infoSize;
    EFI_FILE_INFO *         pFileInfo;
    UINT32                  filePages;
    EFI_PHYSICAL_ADDRESS    pagesPhys;
    UINTN                   bytesToRead;

    //
    // load the builtin ROFS now from sgpKernDir.  shove physical address into gData.LoadInfo.mBuiltinRofsPhys
    //
    efiStatus = apKernDir->Open(
        apKernDir,
        &pProt,
        L"builtin.img",
        EFI_FILE_MODE_READ,
        0);
    if (EFI_ERROR(efiStatus))
        return efiStatus;

    do {
        infoSize = 0;
        efiStatus = pProt->GetInfo(
            pProt,
            (EFI_GUID *)&sgEfiFileInfoId,
            &infoSize, NULL);
        if (efiStatus != EFI_BUFFER_TOO_SMALL)
        {
            K2Printf(L"*** could not retrieve size of file info, status %r\n", efiStatus);
            break;
        }

        efiStatus = gBS->AllocatePool(EfiLoaderData, infoSize, (void **)&pFileInfo);
        if (efiStatus != EFI_SUCCESS)
        {
            K2Printf(L"*** could not allocate memory for file info, status %r\n", efiStatus);
            break;
        }

        do
        {
            efiStatus = pProt->GetInfo(
                pProt,
                (EFI_GUID *)&sgEfiFileInfoId,
                &infoSize, pFileInfo);
            if (efiStatus != EFI_SUCCESS)
            {
                K2Printf(L"*** could not get file info, status %r\n", efiStatus);
                break;
            }

            gData.mRofsBytes = bytesToRead = (UINTN)pFileInfo->FileSize;

            filePages = (UINT32)(K2_ROUNDUP(bytesToRead, K2_VA32_MEMPAGE_BYTES) / K2_VA32_MEMPAGE_BYTES);

            efiStatus = gBS->AllocatePages(AllocateAnyPages, K2OS_EFIMEMTYPE_BUILTIN, filePages, &pagesPhys);
            if (efiStatus != EFI_SUCCESS)
            {
                K2Printf(L"*** AllocatePages for builtin failed with status %r\n", efiStatus);
                break;
            }

            do {
                gData.LoadInfo.mBuiltinRofsPhys = ((UINT32)pagesPhys);

                K2MEM_Zero((void *)gData.LoadInfo.mBuiltinRofsPhys, filePages * K2_VA32_MEMPAGE_BYTES);

                efiStatus = pProt->Read(pProt, &bytesToRead, (void *)((UINT32)pagesPhys));
                if (EFI_ERROR(efiStatus))
                {
                    K2Printf(L"*** ReadSectors failed with efi Status %r\n", efiStatus);
                }

            } while (0);

            if (EFI_ERROR(efiStatus))
            {
                gBS->FreePages(pagesPhys, filePages);
            }

        } while (0);

        gBS->FreePool(pFileInfo);

    } while (0);

    pProt->Close(pProt);

    return efiStatus;
}

static EFI_STATUS sLoadKernelElf(EFI_FILE_PROTOCOL * apKernDir)
{
    EFI_STATUS              efiStatus;
    EFI_FILE_PROTOCOL *     pProt;
    UINTN                   infoSize;
    EFI_FILE_INFO *         pFileInfo;
    UINT32                  filePages;
    EFI_PHYSICAL_ADDRESS    pagesPhys;
    UINTN                   bytesToRead;

    //
    // load the kernel ELF now from sgpKernDir.  shove physical address into gData.mKernElfPhys
    //
    efiStatus = apKernDir->Open(
        apKernDir,
        &pProt,
        L"k2oskern.elf",
        EFI_FILE_MODE_READ,
        0);
    if (EFI_ERROR(efiStatus))
        return efiStatus;

    do {
        infoSize = 0;
        efiStatus = pProt->GetInfo(
            pProt,
            (EFI_GUID *)&sgEfiFileInfoId,
            &infoSize, NULL);
        if (efiStatus != EFI_BUFFER_TOO_SMALL)
        {
            K2Printf(L"*** could not retrieve size of file info, status %r\n", efiStatus);
            break;
        }

        efiStatus = gBS->AllocatePool(EfiLoaderData, infoSize, (void **)&pFileInfo);
        if (efiStatus != EFI_SUCCESS)
        {
            K2Printf(L"*** could not allocate memory for file info, status %r\n", efiStatus);
            break;
        }

        do
        {
            efiStatus = pProt->GetInfo(
                pProt,
                (EFI_GUID *)&sgEfiFileInfoId,
                &infoSize, pFileInfo);
            if (efiStatus != EFI_SUCCESS)
            {
                K2Printf(L"*** could not get file info, status %r\n", efiStatus);
                break;
            }

            gData.LoadInfo.mKernSizeBytes = bytesToRead = (UINTN)pFileInfo->FileSize;

            filePages = (UINT32)(K2_ROUNDUP(bytesToRead, K2_VA32_MEMPAGE_BYTES) / K2_VA32_MEMPAGE_BYTES);

            efiStatus = gBS->AllocatePages(AllocateAnyPages, K2OS_EFIMEMTYPE_KERNEL_ELF, filePages, &pagesPhys);
            if (efiStatus != EFI_SUCCESS)
            {
                K2Printf(L"*** AllocatePages for builtin failed with status %r\n", efiStatus);
                break;
            }

            do {
                gData.mKernElfPhys = ((UINT32)pagesPhys);

                K2MEM_Zero((void *)gData.mKernElfPhys, filePages * K2_VA32_MEMPAGE_BYTES);

                efiStatus = pProt->Read(pProt, &bytesToRead, (void *)((UINT32)pagesPhys));
                if (EFI_ERROR(efiStatus))
                {
                    K2Printf(L"*** ReadSectors failed with efi Status %r\n", efiStatus);
                }

            } while (0);

            if (EFI_ERROR(efiStatus))
            {
                gBS->FreePages(pagesPhys, filePages);
            }

        } while (0);

        gBS->FreePool(pFileInfo);

    } while (0);

    pProt->Close(pProt);

    return efiStatus;
}

EFI_STATUS LoadKernelArena(void)
{
    EFI_STATUS                       status;
    EFI_LOADED_IMAGE_PROTOCOL *      pLoadedImage = NULL;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *pFileProt = NULL;
    EFI_FILE_PROTOCOL *              pRootDir = NULL;
    EFI_FILE_PROTOCOL *              pKernDir = NULL;

    status = gBS->OpenProtocol(gData.mLoaderImageHandle,
        &gEfiLoadedImageProtocolGuid,
        (void **)&pLoadedImage,
        gData.mLoaderImageHandle,
        NULL,
        EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);

    if (status != EFI_SUCCESS)
    {
        K2Printf(L"*** Open LoadedImageProt failed with %r\n", status);
        return status;
    }

    do
    {
        status = gBS->OpenProtocol(pLoadedImage->DeviceHandle,
            &gEfiSimpleFileSystemProtocolGuid,
            (void **)&pFileProt,
            gData.mLoaderImageHandle,
            NULL, 
            EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);

        if (status != EFI_SUCCESS)
        {
            K2Printf(L"*** Open FileSystemProt failed with %r\n", status);
            break;
        }
        
        do
        {
            status = pFileProt->OpenVolume(pFileProt, &pRootDir);
            if (status != EFI_SUCCESS)
            {
                K2Printf(L"*** OpenVolume failed with %r\n", status);
                break;
            }

            do
            {
                status = pRootDir->Open(pRootDir, &pKernDir, (CHAR16 *)sgpKernSubDirName, EFI_FILE_MODE_READ, 0);
                if (status != EFI_SUCCESS)
                {
                    K2Printf(L"*** Open %s failed with %r\n", sgpKernSubDirName, status);
                    break;
                }

                do
                {
                    status = sLoadRofs(pKernDir);
                    if (status != EFI_SUCCESS)
                    {
                        K2Printf(L"*** Could not load builtin image 'builtin.img' from %s with error %r\n", sgpKernSubDirName);
                        break;
                    }

                    status = sLoadKernelElf(pKernDir);
                    if (status != EFI_SUCCESS)
                    {
                        K2Printf(L"*** Could not load kernel ELF from %s with error %r\n", sgpKernSubDirName);
                        break;
                    }

                } while (0);

                pKernDir->Close(pKernDir);
                pKernDir = NULL;

            } while (0);

            pRootDir->Close(pRootDir);
            pRootDir = NULL;

        } while (0);

        gBS->CloseProtocol(pLoadedImage->DeviceHandle,
            &gEfiSimpleFileSystemProtocolGuid,
            gData.mLoaderImageHandle,
            NULL);
        pFileProt = NULL;

    } while (0);

    gBS->CloseProtocol(gData.mLoaderImageHandle,
        &gEfiLoadedImageProtocolGuid,
        gData.mLoaderImageHandle, 
        NULL);
    pLoadedImage = NULL;

    return status;
}

static
EFI_STATUS
sFindSmbiosAndAcpi(
    void
)
{
    EFI_STATUS status;

    K2Printf(L"Scanning for SMBIOS and ACPI...\n");

    status = EfiGetSystemConfigurationTable(&gEfiSmbiosTableGuid, (VOID**)&gData.mpSmbios);
    if (EFI_ERROR(status))
    {
        K2Printf(L"***Could not locate SMBIOS in Configuration Tables (%r)\n", status);
        return status;
    }
    if (gData.mpSmbios->Type0 == NULL)
    {
        K2Printf(L"***SMBIOS Type0 is NULL\n");
        return EFI_NOT_FOUND;
    }
    if (gData.mpSmbios->Type1 == NULL)
    {
        K2Printf(L"***SMBIOS Type1 is NULL\n");
        return EFI_NOT_FOUND;
    }
    if (gData.mpSmbios->Type2 == NULL)
    {
        K2Printf(L"***SMBIOS Type2 is NULL\n");
        return EFI_NOT_FOUND;
    }

    status = EfiGetSystemConfigurationTable(&gEfiAcpiTableGuid, (VOID **)&gData.mpAcpi);
    if (EFI_ERROR(status)) 
    {
        K2Printf(L"***Could not locate ACPI in Configuration Tables (%r)\n", status);
        return status;
    }

    if ((gData.mpAcpi->XsdtAddress == 0) && (gData.mpAcpi->RsdtAddress == 0))
    {
        K2Printf(L"***ACPI Xsdt and Rsdp addresses are zero.\n");
        return EFI_NOT_FOUND;
    }

    K2Printf(L"ACPI OEM ID:  %c%c%c%c%c%c\n",
        gData.mpAcpi->OemId[0], gData.mpAcpi->OemId[1], gData.mpAcpi->OemId[2],
        gData.mpAcpi->OemId[3], gData.mpAcpi->OemId[4], gData.mpAcpi->OemId[5]);

    return EFI_SUCCESS;
}

static
VOID
EFIAPI
sAddressChangeEvent(
  IN EFI_EVENT  Event,
  IN VOID *     Context
  )
{
    EFI_STATUS Status;
    Status = gRT->ConvertPointer (0x0, (VOID **)&gData.LoadInfo.mpEFIST);
    ASSERT_EFI_ERROR(Status);
}

EFI_STATUS
EFIAPI
K2OsLoaderEntryPoint (
    IN EFI_HANDLE           ImageHandle,
    IN EFI_SYSTEM_TABLE *   SystemTable
    )
{
    EFI_STATUS              efiStatus;
    EFI_EVENT               efiEvent;
    EFI_PHYSICAL_ADDRESS    physAddr;
    K2STAT                  stat;

    K2Printf(L"\n\n\nK2Loader\n----------------\n");

    K2MEM_Zero(&gData, sizeof(gData));

    gData.mLoaderImageHandle = ImageHandle;
    gData.mKernArenaLow = K2OS_KVA_FREE_BOTTOM;
    gData.mKernArenaHigh = K2OS_KVA_FREE_TOP;
    gData.LoadInfo.mpEFIST = (K2EFI_SYSTEM_TABLE *)gST;

    efiStatus = Loader_InitArch();
    if (EFI_ERROR(efiStatus))
        return efiStatus;

    do
    {
        efiStatus = sFindSmbiosAndAcpi();
        if (EFI_ERROR(efiStatus))
        {
            K2Printf(L"*** Failed to validate SMBIOS/ACPI = %r\n\n\n", efiStatus);
            break;
        }

        efiStatus = Loader_FillCpuInfo();
        if (EFI_ERROR(efiStatus))
        {
            K2Printf(L"*** Failed to gather CPU info with result = %r\n\n\n", efiStatus);
            break;
        }

        ASSERT(gData.LoadInfo.mCpuCoreCount > 0);
        K2Printf(L"CpuInfo says there are %d CPUs\n", gData.LoadInfo.mCpuCoreCount);

        efiStatus = gBS->AllocatePages(AllocateAnyPages, K2OS_EFIMEMTYPE_EFI_MAP, K2OS_EFIMAP_PAGECOUNT, (EFI_PHYSICAL_ADDRESS *)&physAddr);
        if (efiStatus != EFI_SUCCESS)
        {
            K2Printf(L"*** Failed to allocate pages for EFI memory map, status %r\n", efiStatus);
            break;
        }

        do
        {
            gData.mpMemoryMap = (EFI_MEMORY_DESCRIPTOR *)(UINTN)physAddr;

            efiStatus = gBS->CreateEventEx(
                EVT_NOTIFY_SIGNAL,
                TPL_NOTIFY,
                sAddressChangeEvent,
                NULL,
                &gEfiEventVirtualAddressChangeGuid,
                &efiEvent
            );
            if (efiStatus != EFI_SUCCESS)
            {
                K2Printf(L"*** Failed to allocate address change event with %r\n", efiStatus);
                break;
            }

            do
            {
                efiStatus = Loader_CreateVirtualMap();
                if (EFI_ERROR(efiStatus))
                {
                    K2Printf(L"*** Failed to create virtual map, result = %r\n\n\n", efiStatus);
                    break;
                }

                efiStatus = LoadKernelArena();
                if (EFI_ERROR(efiStatus))
                {
                    K2Printf(L"*** Failed to load ROFS and kernel ELF, result = %r\n\n\n", efiStatus);
                    break;
                }

                efiStatus = Loader_MapKernelArena();
                if (EFI_ERROR(efiStatus))
                {
                    K2Printf(L"*** Failed to map the kernel.elf into the virtual map, result = %r\n\n\n", efiStatus);
                    break;
                }

                stat = Loader_AssembleAcpi();
                if (K2STAT_IS_ERROR(stat))
                {
                    K2Printf(L"*** Failed to assemble ACPI info, result = %r\n\n\n", stat);
                    break;
                }

#if 1
                gData.LoadInfo.mDebugPageVirt = gData.mKernArenaLow;
                gData.mKernArenaLow += K2_VA32_MEMPAGE_BYTES;
#if K2_TARGET_ARCH_IS_ARM
                k2Stat = K2VMAP32_MapPage(&gData.Map, gData.LoadInfo.mDebugPageVirt, 0x02020000, K2OS_MAPTYPE_KERN_DEVICEIO);   // IMX6 UART 1
//                                        k2Stat = K2VMAP32_MapPage(&gData.Map, gData.LoadInfo.mDebugPageVirt, 0x021E8000, K2OS_MAPTYPE_KERN_DEVICEIO);     // IMX6 UART 2
#else
                stat = K2VMAP32_MapPage(&gData.Map, gData.LoadInfo.mDebugPageVirt, 0x000B8000, K2OS_MAPTYPE_KERN_DEVICEIO);
#endif
                //                        K2Printf(L"DebugPageVirt = 0x%08X\n", gData.LoadInfo.mDebugPageVirt);
                if (K2STAT_IS_ERROR(stat))
                    break;
#else
                gData.LoadInfo.mDebugPageVirt = 0;
#endif
                K2Printf(L"\n----------------\nTransition...\n");
                stat = Loader_TrackEfiMap();
                if (K2STAT_IS_ERROR(stat))
                    break;

                Loader_TransitionToKernel();

                K2Printf(L"ok.\n");

            } while (0);

            gBS->CloseEvent(efiEvent);

        } while (0);

        gBS->FreePages((EFI_PHYSICAL_ADDRESS)((UINTN)gData.mpMemoryMap), K2OS_EFIMAP_PAGECOUNT);

    } while (0);

    Loader_DoneArch();

while (1);

    return efiStatus;
}

#if 0

                                    k2Stat = Loader_AssembleAcpi();
                                    if (K2STAT_IS_ERROR(k2Stat))
                                        break;
                                    //
                                    // DO NOT ALLOCATE ANY MEMORY AFTER THIS
                                    //
                                    // DO NOT DO DEBUG PRINTS AFTER THIS
                                    //
                                    k2Stat = K2DLXSUPP_Handoff(
                                        &gData.LoadInfo.mpDlxCrt,
                                        sysDLX_ConvertLoadPtr,
                                        &gData.LoadInfo.mSystemVirtualEntrypoint);
                                    //
                                    // kernel dlx virtual entrypoint needs to go into loadinfo
                                    // this function works after handoff except for the segment
                                    // info, which we don't really care about here
                                    //
                                    k2Stat = K2DLXSUPP_GetInfo(
                                        pDlxKern, NULL,
                                        (DLX_pf_ENTRYPOINT *)&gData.LoadInfo.mKernDlxEntry,
                                        NULL, NULL, NULL);
                                    if (K2STAT_IS_ERROR(k2Stat))
                                        break;

                                } while (0);

                                gRT->ResetSystem(EfiResetWarm, EFI_DEVICE_ERROR, 0, NULL);



#endif