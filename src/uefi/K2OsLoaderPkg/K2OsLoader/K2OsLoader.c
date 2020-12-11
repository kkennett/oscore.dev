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

static
EFI_STATUS
sFindSmbiosAndAcpi(
    void
)
{
    EFI_STATUS status;

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

//    K2Printf(L"ACPI OEM ID:  %c%c%c%c%c%c\n",
//        gData.mpAcpi->OemId[0], gData.mpAcpi->OemId[1], gData.mpAcpi->OemId[2],
//        gData.mpAcpi->OemId[3], gData.mpAcpi->OemId[4], gData.mpAcpi->OemId[5]);

    return EFI_SUCCESS;
}

static
BOOLEAN
sVerifyHAL(
    DLX * apHalDlx
    )
{
    K2STAT  stat;
    UINT32  funcAddr;

    stat = DLX_FindExport(
        apHalDlx,
        DlxSeg_Text,
        "K2OSHAL_DebugOut",
        &funcAddr);
    if (K2STAT_IS_ERROR(stat))
    {
        K2Printf(L"*** Required HAL export \"K2OSHAL_DebugOut\" missing\n");
        return FALSE;
    }

    stat = DLX_FindExport(
        apHalDlx,
        DlxSeg_Text,
        "K2OSHAL_DebugIn",
        &funcAddr);
    if (K2STAT_IS_ERROR(stat))
    {
        K2Printf(L"*** Required HAL export \"K2OSHAL_DebugIn\" missing\n");
        return FALSE;
    }

    return TRUE;
}

static K2DLXSUPP_HOST sgDlxHost =
{
    sizeof(K2DLXSUPP_HOST),

    sysDLX_CritSec,
    NULL,
    sysDLX_Open,
    NULL,
    sysDLX_ReadSectors,
    sysDLX_Prepare,
    sysDLX_PreCallback,
    sysDLX_PostCallback,
    sysDLX_Finalize,
    NULL,
    sysDLX_Purge,
    NULL
};

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

void
sSetupGraphics(void)
{
    EFI_STATUS                          status;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *      pGop;
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL_UNION pixel;

    status = gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, NULL, (VOID **)&pGop);
    if (!EFI_ERROR(status))
    {
        if ((NULL != pGop->Mode) && (NULL != pGop->Mode->Info))
        {
            gData.LoadInfo.BootGraf.mFrameBufferPhys = (UINT32)pGop->Mode->FrameBufferBase;
            gData.LoadInfo.BootGraf.mFrameBufferBytes = (UINT32)pGop->Mode->FrameBufferSize;
            K2MEM_Copy(&gData.LoadInfo.BootGraf.ModeInfo, &pGop->Mode->Info, sizeof(K2EFI_GRAPHICS_OUTPUT_MODE_INFORMATION));

            //
            // clear to dim blue
            //
            pixel.Raw = 0;
            pixel.Pixel.Blue = 32;
            pGop->Blt(
                pGop,
                &pixel.Pixel,
                EfiBltVideoFill,
                0,
                0,
                0,
                0,
                pGop->Mode->Info->HorizontalResolution,
                pGop->Mode->Info->VerticalResolution,
                0);
        }
    }
}

EFI_STATUS
EFIAPI
K2OsLoaderEntryPoint (
    IN EFI_HANDLE           ImageHandle,
    IN EFI_SYSTEM_TABLE *   SystemTable
    )
{
    EFI_STATUS              efiStatus;
    K2STAT                  k2Stat;
    DLX *                   pDlxHal;
    DLX *                   pDlxKern;
    DLX *                   pDlxAcpi;
    DLX *                   pDlxExec;
    EFI_EVENT               efiEvent;
    EFI_PHYSICAL_ADDRESS    physAddr;

//    K2Printf(L"\n\n\nK2Loader\n----------------\n");

    efiStatus = Loader_InitArch();
    if (EFI_ERROR(efiStatus))
        return efiStatus;

    K2MEM_Zero(&gData, sizeof(gData));
    gData.mKernArenaLow = K2OS_KVA_FREE_BOTTOM;
    gData.mKernArenaHigh = K2OS_KVA_FREE_TOP;
    gData.LoadInfo.mpEFIST = (K2EFI_SYSTEM_TABLE *)gST;

    sSetupGraphics();

//    K2Printf(L"Scanning for SMBIOS and ACPI...\n");

    efiStatus = sFindSmbiosAndAcpi();
    if (EFI_ERROR(efiStatus))
    {
        K2Printf(L"*** Failed to validate SMBIOS/ACPI = %r\n\n\n", efiStatus);
        Loader_DoneArch();
        return efiStatus;
    }

    efiStatus = Loader_FillCpuInfo();
    if (EFI_ERROR(efiStatus))
    {
        K2Printf(L"*** Failed to gather CPU info with result = %r\n\n\n", efiStatus);
        Loader_DoneArch();
        return efiStatus;
    }

    ASSERT(gData.LoadInfo.mCpuCoreCount > 0);

//    K2Printf(L"CpuInfo says there are %d CPUs\n", gData.LoadInfo.mCpuCoreCount);

    efiStatus = gBS->CreateEventEx (
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
        Loader_DoneArch();
        return efiStatus;
    }

    do {
        efiStatus = gBS->AllocatePages(AllocateAnyPages, K2OS_EFIMEMTYPE_EFI_MAP, K2OS_EFIMAP_PAGECOUNT, (EFI_PHYSICAL_ADDRESS *)&physAddr);
        if (efiStatus != EFI_SUCCESS)
        {
            K2Printf(L"*** Failed to allocate pages for EFI memory map, status %r\n", efiStatus);
            break;
        }

        gData.mpMemoryMap = (EFI_MEMORY_DESCRIPTOR *)(UINTN)physAddr;

        do {
            efiStatus = gBS->AllocatePages(AllocateAnyPages, K2OS_EFIMEMTYPE_LOADER, 1, &gData.mLoaderPagePhys);
            if (efiStatus != EFI_SUCCESS)
            {
                K2Printf(L"*** Failed to allocate page for K2 loader with %r\n", efiStatus);
                break;
            }

            do
            {
                efiStatus = sysDLX_Init(ImageHandle);
                if (efiStatus != EFI_SUCCESS)
                {
                    K2Printf(L"*** sysDLX_Init failed with %r\n", efiStatus);
                    break;
                }

                do {

                    K2_ASSERT(gData.mLoaderImageHandle == ImageHandle);

                    efiStatus = EFI_NOT_FOUND;

                    k2Stat = K2DLXSUPP_Init((void *)((UINT32)gData.mLoaderPagePhys), &sgDlxHost, TRUE, FALSE);
                    if (K2STAT_IS_ERROR(k2Stat))
                    {
                        K2Printf(L"*** InitDLX returned status 0x%08X\n", k2Stat);
                        break;
                    }

//                    K2Printf(L"Loading k2oshal.dlx...\n");
                    k2Stat = DLX_Acquire("k2oshal.dlx", NULL, &pDlxHal);
                    if (K2STAT_IS_ERROR(k2Stat))
                        break;

                    do
                    {
//                        K2Printf(L"Verifying k2oshal.dlx...\n");
                        if (!sVerifyHAL(pDlxHal))
                            break;

                        k2Stat = DLX_Acquire("k2osacpi.dlx", NULL, &pDlxAcpi);
                        if (K2STAT_IS_ERROR(k2Stat))
                            break;

                        do {
                            k2Stat = DLX_Acquire("k2osexec.dlx", NULL, &pDlxExec);
                            if (K2STAT_IS_ERROR(k2Stat))
                                break;

                            do {
                                k2Stat = DLX_Acquire("k2oskern.dlx", NULL, &pDlxKern);
                                if (K2STAT_IS_ERROR(k2Stat))
                                    break;

                                do
                                {
                                    k2Stat = DLX_Acquire("k2oscrt.dlx", NULL, &gData.LoadInfo.mpDlxCrt);
                                    if (gData.LoadInfo.mpDlxCrt == NULL)
                                        break;

                                    do
                                    {
                                        k2Stat = Loader_CreateVirtualMap();
                                        if (K2STAT_IS_ERROR(k2Stat))
                                            break;

                                        k2Stat = Loader_MapAllDLX();
                                        if (K2STAT_IS_ERROR(k2Stat))
                                            break;

                                        k2Stat = Loader_AssembleAcpi();
                                        if (K2STAT_IS_ERROR(k2Stat))
                                            break;

#if 1
                                        gData.LoadInfo.mDebugPageVirt = gData.mKernArenaLow;
                                        gData.mKernArenaLow += K2_VA32_MEMPAGE_BYTES;
#if K2_TARGET_ARCH_IS_ARM
                                        k2Stat = K2VMAP32_MapPage(&gData.Map, gData.LoadInfo.mDebugPageVirt, 0x021E8000, K2OS_MAPTYPE_KERN_DEVICEIO);
#else
                                        k2Stat = K2VMAP32_MapPage(&gData.Map, gData.LoadInfo.mDebugPageVirt, 0x000B8000, K2OS_MAPTYPE_KERN_DEVICEIO);
#endif
                                        //                        K2Printf(L"DebugPageVirt = 0x%08X\n", gData.LoadInfo.mDebugPageVirt);
                                        if (K2STAT_IS_ERROR(k2Stat))
                                            break;
#else
                                        gData.LoadInfo.mDebugPageVirt = 0;
#endif

//                                        K2Printf(L"\n----------------\nTransition...\n");

                                        k2Stat = Loader_TrackEfiMap();
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

                                        if (!K2STAT_IS_ERROR(k2Stat))
                                            Loader_TransitionToKernel();

                                    } while (0);

                                    gRT->ResetSystem(EfiResetWarm, EFI_DEVICE_ERROR, 0, NULL);
                                    while (1);

                                } while (0);

                                DLX_Release(pDlxKern);

                            } while (0);

                            DLX_Release(pDlxExec);

                        } while (0);

                        DLX_Release(pDlxAcpi);

                    } while (0);

                    DLX_Release(pDlxHal);

                } while (0);

                sysDLX_Done();

            } while (0);

            gBS->FreePages(gData.mLoaderPagePhys, 1);

        } while (0);

        gBS->FreePages((EFI_PHYSICAL_ADDRESS)((UINTN)gData.mpMemoryMap), K2OS_EFIMAP_PAGECOUNT);

    } while (0);

    gBS->CloseEvent(efiEvent);

    Loader_DoneArch();

    return efiStatus;
}
