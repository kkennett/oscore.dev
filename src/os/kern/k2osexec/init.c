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

#include "k2osexec.h"

ACPI_TABLE_MADT * gpMADT;
ACPI_TABLE_MCFG * gpMCFG;

static
ACPI_STATUS
sResourcesEnumCallback(
    ACPI_RESOURCE * Resource,
    void *          Context
)
{
    K2OSKERN_Debug("Res: Type %d Length %d\n", Resource->Type, Resource->Length);
    //    ACPI_RESOURCE_DATA              Data;
    return AE_OK;
}

ACPI_STATUS DeviceWalkCallback(
    ACPI_HANDLE Object,
    UINT32      NestingLevel,
    void *      Context,
    void **     ReturnValue)
{
    ACPI_BUFFER         bufDesc;
    char                charBuf[8];
    ACPI_STATUS         acpiStatus;
    ACPI_DEVICE_INFO *  pDevInfo;
    UINT32              ix;
    char *              pStr;
    ACPI_BUFFER         ResourceBuffer;

    bufDesc.Length = 8;
    bufDesc.Pointer = charBuf;

    charBuf[0] = 0;
    acpiStatus = AcpiGetName(Object, ACPI_SINGLE_NAME, &bufDesc);
    if (!ACPI_FAILURE(acpiStatus))
    {
        charBuf[4] = 0;
        acpiStatus = AcpiGetObjectInfo(Object, &pDevInfo);
        if (!ACPI_FAILURE(acpiStatus))
        {
            if (pDevInfo->Type == ACPI_TYPE_DEVICE)
            {
                for (ix = 0; ix < NestingLevel; ix++)
                    K2OSKERN_Debug(" ");

                K2OSKERN_Debug("%08X%08X %s\n", (UINT32)(pDevInfo->Address >> 32), (UINT32)(pDevInfo->Address & 0xFFFFFFFF), charBuf);

                pStr = pDevInfo->HardwareId.String;
                if (pStr != NULL)
                {
                    for (ix = 0; ix < NestingLevel + 2; ix++)
                        K2OSKERN_Debug(" ");
                    K2OSKERN_Debug("_HID(%s)\n", pStr);
                }

                pStr = pDevInfo->UniqueId.String;
                if (pStr != NULL)
                {
                    for (ix = 0; ix < NestingLevel + 2; ix++)
                        K2OSKERN_Debug(" ");
                    K2OSKERN_Debug("_UID(%s)\n", pStr);
                }

                pStr = pDevInfo->ClassCode.String;
                if (pStr != NULL)
                {
                    for (ix = 0; ix < NestingLevel + 2; ix++)
                        K2OSKERN_Debug(" ");
                    K2OSKERN_Debug("_CID(%s)\n", pStr);
                }

                K2MEM_Zero(&ResourceBuffer, sizeof(ResourceBuffer));
                ResourceBuffer.Length = ACPI_ALLOCATE_LOCAL_BUFFER;

                acpiStatus = AcpiGetCurrentResources(Object, &ResourceBuffer);
                if (!ACPI_FAILURE(acpiStatus))
                {
                    K2_ASSERT(ResourceBuffer.Pointer);
                    acpiStatus = AcpiWalkResourceBuffer(
                        &ResourceBuffer,
                        sResourcesEnumCallback,
                        NULL
                    );
                    K2OS_HeapFree(ResourceBuffer.Pointer);
                }
            }
            else
            {
                K2OSKERN_Debug("%s: Not DEVICE\n", charBuf);
            }
        }
        else
        {
            K2OSKERN_Debug("***%3d %s\n", NestingLevel, charBuf);
        }
    }

    return AE_OK;
}

void
K2OSEXEC_Init(
    K2OSEXEC_INIT_INFO * apInitInfo
)
{
    ACPI_TABLE_HEADER * pAcpiHdr;
    ACPI_STATUS         acpiStatus;
    void *              pWalkRet;

    gpMADT = NULL;
    gpMCFG = NULL;

    Phys_Init(apInitInfo);

    acpiStatus = AcpiInitializeSubsystem();
    K2_ASSERT(!ACPI_FAILURE(acpiStatus));

    acpiStatus = AcpiInitializeTables(NULL, 16, FALSE);
    K2_ASSERT(!ACPI_FAILURE(acpiStatus));

    acpiStatus = AcpiGetTable(ACPI_SIG_MADT, 0, &pAcpiHdr);
    if (ACPI_FAILURE(acpiStatus))
        gpMADT = NULL;
    else
        gpMADT = (ACPI_TABLE_MADT *)pAcpiHdr;

    acpiStatus = AcpiGetTable(ACPI_SIG_MCFG, 0, &pAcpiHdr);
    if (ACPI_FAILURE(acpiStatus))
        gpMCFG = NULL;
    else
        gpMCFG = (ACPI_TABLE_MCFG *)pAcpiHdr;

    Pci_Init();

    Handlers_Init1();

    acpiStatus = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);
    K2_ASSERT(!ACPI_FAILURE(acpiStatus));

    acpiStatus = AcpiLoadTables();
    K2_ASSERT(!ACPI_FAILURE(acpiStatus));

    acpiStatus = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION);
    K2_ASSERT(!ACPI_FAILURE(acpiStatus));

    Pci_Discover();

    Handlers_Init2();

    Time_Start(apInitInfo);

    pWalkRet = NULL;
    AcpiGetDevices(
        NULL,
        DeviceWalkCallback,
        NULL,
        &pWalkRet
    );
}

