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

void SetupPciConfig(void)
{
    ACPI_STATUS acpiStatus;

    //
    // find MCFG if it exists
    //
    ACPI_TABLE_HEADER * pMCFG;
    acpiStatus = AcpiGetTable(ACPI_SIG_MCFG, 0, &pMCFG);
    if (!ACPI_FAILURE(acpiStatus))
    {
        //
        // find memory mapped io segments for PCI bridges
        //
        K2_ASSERT(pMCFG->Length > sizeof(ACPI_TABLE_MCFG));
        UINT32 sizeEnt = pMCFG->Length - sizeof(ACPI_TABLE_MCFG);
        UINT32 entCount = sizeEnt / sizeof(ACPI_MCFG_ALLOCATION);
        K2OSKERN_Debug("Content %d, each %d\n", sizeEnt, sizeof(ACPI_MCFG_ALLOCATION));
        ACPI_MCFG_ALLOCATION *pAlloc = (ACPI_MCFG_ALLOCATION *)(((UINT8 *)pMCFG) + sizeof(ACPI_TABLE_MCFG));
        do {
            K2OSKERN_Debug("\nAddress:    %08X\n", (UINT32)(pAlloc->Address & 0xFFFFFFFF));
            K2OSKERN_Debug("PciSegment: %d\n", pAlloc->PciSegment);
            K2OSKERN_Debug("StartBus:   %d\n", pAlloc->StartBusNumber);
            K2OSKERN_Debug("EndBus:     %d\n", pAlloc->EndBusNumber);
            pAlloc = (ACPI_MCFG_ALLOCATION *)(((UINT8 *)pAlloc) + sizeof(ACPI_MCFG_ALLOCATION));
        } while (--entCount);
    }
}

