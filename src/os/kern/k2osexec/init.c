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

#include "ik2osexec.h"

ACPI_TABLE_MADT * gpMADT;

void
K2OSEXEC_Init(
    K2OSEXEC_INIT_INFO * apInitInfo
)
{
    static BOOL sInitialized = FALSE;
    ACPI_TABLE_HEADER * pAcpiHdr;
    ACPI_STATUS         acpiStatus;

    K2OSKERN_Debug("%s(%d)\n", __FUNCTION__, __LINE__);

    //
    // exported function - guard against re-entry
    //
    if (FALSE != sInitialized)
        return;
    sInitialized = TRUE;

    gpMADT = NULL;

    Phys_Init(apInitInfo);

    K2OSKERN_Debug("%s(%d)\n", __FUNCTION__, __LINE__);

    acpiStatus = AcpiInitializeSubsystem();
    K2_ASSERT(!ACPI_FAILURE(acpiStatus));

    K2OSKERN_Debug("%s(%d)\n", __FUNCTION__, __LINE__);

    acpiStatus = AcpiInitializeTables(NULL, 16, FALSE);
    K2_ASSERT(!ACPI_FAILURE(acpiStatus));

    K2OSKERN_Debug("%s(%d)\n", __FUNCTION__, __LINE__);

    acpiStatus = AcpiGetTable(ACPI_SIG_MADT, 0, &pAcpiHdr);
    if (ACPI_FAILURE(acpiStatus))
        gpMADT = NULL;
    else
        gpMADT = (ACPI_TABLE_MADT *)pAcpiHdr;

    K2OSKERN_Debug("%s(%d)\n", __FUNCTION__, __LINE__);

    Pci_Init();

    K2OSKERN_Debug("%s(%d)\n", __FUNCTION__, __LINE__);

    Handlers_Init1();

    K2OSKERN_Debug("%s(%d)\n", __FUNCTION__, __LINE__);

    acpiStatus = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);
    K2_ASSERT(!ACPI_FAILURE(acpiStatus));

    acpiStatus = AcpiLoadTables();
    K2_ASSERT(!ACPI_FAILURE(acpiStatus));

    acpiStatus = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION);
    K2_ASSERT(!ACPI_FAILURE(acpiStatus));

    Dev_Init();

    Res_Init();

    Intr_Init();

    Dev_Dump();

    Handlers_Init2();

    FsProv_Init(apInitInfo);

    DrvStore_Init();

    Time_Start(apInitInfo);

    Builtin_Init(apInitInfo);
}

