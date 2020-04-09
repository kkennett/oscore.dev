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

static void
NotifyHandler(
    ACPI_HANDLE                 Device,
    UINT32                      Value,
    void                        *Context)
{
    K2OSKERN_Debug("K2OSACPI:NotifyHandler = Received a Notify %08X\n", Value);
}

static ACPI_STATUS
RegionInit(
    ACPI_HANDLE                 RegionHandle,
    UINT32                      Function,
    void                        *HandlerContext,
    void                        **RegionContext)
{
    K2OSKERN_Debug("K2OSACPI:RegionInit = Received a region init for %08X\n", RegionHandle);
    if (Function == ACPI_REGION_DEACTIVATE)
    {
        *RegionContext = NULL;
    }
    else
    {
        *RegionContext = RegionHandle;
    }
    return (AE_OK);
}

static ACPI_STATUS
RegionHandler(
    UINT32                      Function,
    ACPI_PHYSICAL_ADDRESS       Address,
    UINT32                      BitWidth,
    UINT64                      *Value,
    void                        *HandlerContext,
    void                        *RegionContext)
{
    K2OSKERN_Debug("K2OSACPI:RegionHandler = Received a Region Access\n");
    return (AE_OK);
}

static UINT32
PowerButtonHandler(
    void *Context
)
{
    K2OSKERN_Debug("K2OSACPI:PowerButtonHandler\n");
    AcpiClearEvent(ACPI_EVENT_POWER_BUTTON);
    return (AE_OK);
}

static ACPI_STATUS
sInstallHandlers(void)
{
    ACPI_STATUS             Status;

    Status = AcpiInstallNotifyHandler(ACPI_ROOT_OBJECT,
        ACPI_SYSTEM_NOTIFY, NotifyHandler, NULL);
    if (ACPI_FAILURE(Status))
    {
        K2OSKERN_Debug("K2OSACPI: Failure with installing notify handler\n");
        return (Status);
    }

    Status = AcpiInstallAddressSpaceHandler(ACPI_ROOT_OBJECT,
        ACPI_ADR_SPACE_SYSTEM_MEMORY, RegionHandler, RegionInit, NULL);
    if (ACPI_FAILURE(Status))
    {
        K2OSKERN_Debug("K2OSACPI: Failure with installing OpRegion handler\n");
        return (Status);
    }

    return (AE_OK);
}

ACPI_STATUS sDeviceWalkCallback(
    ACPI_HANDLE Object,
    UINT32      NestingLevel,
    void *      Context,
    void **     ReturnValue)
{
    ACPI_BUFFER bufDesc;
    char        charBuf[8];
    ACPI_STATUS status;

    bufDesc.Length = 8;
    bufDesc.Pointer = charBuf;

    charBuf[0] = 0;
    status = AcpiGetName(Object, ACPI_SINGLE_NAME, &bufDesc);
    if (!ACPI_FAILURE(status))
    {
        charBuf[4] = 0;
        K2OSKERN_Debug("%3d %s\n", NestingLevel, charBuf);
    }

    return AE_OK;
}

void K2OSEXEC_Run(void)
{
    ACPI_STATUS     Status;

    Status = AcpiInitializeSubsystem();
    K2_ASSERT(!ACPI_FAILURE(Status));

    Status = AcpiInitializeTables(NULL, 16, FALSE);
    K2_ASSERT(!ACPI_FAILURE(Status));

    Status = sInstallHandlers();
    K2_ASSERT(!ACPI_FAILURE(Status));

    Status = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);
    K2_ASSERT(!ACPI_FAILURE(Status));

    Status = AcpiLoadTables();
    K2_ASSERT(!ACPI_FAILURE(Status));

    Status = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION);
    K2_ASSERT(!ACPI_FAILURE(Status));

    Status = AcpiInstallFixedEventHandler(ACPI_EVENT_POWER_BUTTON, PowerButtonHandler, NULL);
    K2_ASSERT(!ACPI_FAILURE(Status));

    Status = AcpiEnableEvent(ACPI_EVENT_POWER_BUTTON, 0);
    K2_ASSERT(!ACPI_FAILURE(Status));

#if 0
    ACPI_EVENT_STATUS   evtStatus;
    AcpiGetEventStatus(ACPI_EVENT_PMTIMER, &evtStatus);
    K2OSKERN_Debug("ACPI_EVENT_PMTIMER          status = %08X\n", evtStatus);
    K2OSKERN_Debug("  %s %s %s %s %s %s\n\n",
        (evtStatus & ACPI_EVENT_FLAG_ENABLED) ? "ENB" : "DIS",
        (evtStatus & ACPI_EVENT_FLAG_WAKE_ENABLED) ? "WKE" : "---",
        (evtStatus & ACPI_EVENT_FLAG_STATUS_SET) ? "SET" : "---",
        (evtStatus & ACPI_EVENT_FLAG_ENABLE_SET) ? "SETENB" : "NOTENB",
        (evtStatus & ACPI_EVENT_FLAG_HAS_HANDLER) ? "HAND" : "----",
        (evtStatus & ACPI_EVENT_FLAG_MASKED) ? "MASK" : "LIVE"
    );

    AcpiGetEventStatus(ACPI_EVENT_GLOBAL, &evtStatus);
    K2OSKERN_Debug("ACPI_EVENT_GLOBAL           status = %08X\n", evtStatus);
    K2OSKERN_Debug("  %s %s %s %s %s %s\n\n",
        (evtStatus & ACPI_EVENT_FLAG_ENABLED) ? "ENB" : "DIS",
        (evtStatus & ACPI_EVENT_FLAG_WAKE_ENABLED) ? "WKE" : "---",
        (evtStatus & ACPI_EVENT_FLAG_STATUS_SET) ? "SET" : "---",
        (evtStatus & ACPI_EVENT_FLAG_ENABLE_SET) ? "SETENB" : "NOTENB",
        (evtStatus & ACPI_EVENT_FLAG_HAS_HANDLER) ? "HAND" : "----",
        (evtStatus & ACPI_EVENT_FLAG_MASKED) ? "MASK" : "LIVE"
    );

    AcpiGetEventStatus(ACPI_EVENT_POWER_BUTTON, &evtStatus);
    K2OSKERN_Debug("ACPI_EVENT_POWER_BUTTON     status = %08X\n", evtStatus);
    K2OSKERN_Debug("  %s %s %s %s %s %s\n\n",
        (evtStatus & ACPI_EVENT_FLAG_ENABLED) ? "ENB" : "DIS",
        (evtStatus & ACPI_EVENT_FLAG_WAKE_ENABLED) ? "WKE" : "---",
        (evtStatus & ACPI_EVENT_FLAG_STATUS_SET) ? "SET" : "---",
        (evtStatus & ACPI_EVENT_FLAG_ENABLE_SET) ? "SETENB" : "NOTENB",
        (evtStatus & ACPI_EVENT_FLAG_HAS_HANDLER) ? "HAND" : "----",
        (evtStatus & ACPI_EVENT_FLAG_MASKED) ? "MASK" : "LIVE"
    );

    AcpiGetEventStatus(ACPI_EVENT_SLEEP_BUTTON, &evtStatus);
    K2OSKERN_Debug("ACPI_EVENT_SLEEP_BUTTON     status = %08X\n", evtStatus);
    K2OSKERN_Debug("  %s %s %s %s %s %s\n\n",
        (evtStatus & ACPI_EVENT_FLAG_ENABLED) ? "ENB" : "DIS",
        (evtStatus & ACPI_EVENT_FLAG_WAKE_ENABLED) ? "WKE" : "---",
        (evtStatus & ACPI_EVENT_FLAG_STATUS_SET) ? "SET" : "---",
        (evtStatus & ACPI_EVENT_FLAG_ENABLE_SET) ? "SETENB" : "NOTENB",
        (evtStatus & ACPI_EVENT_FLAG_HAS_HANDLER) ? "HAND" : "----",
        (evtStatus & ACPI_EVENT_FLAG_MASKED) ? "MASK" : "LIVE"
    );

    AcpiGetEventStatus(ACPI_EVENT_RTC, &evtStatus);
    K2OSKERN_Debug("ACPI_EVENT_RTC              status = %08X\n", evtStatus);
    K2OSKERN_Debug("  %s %s %s %s %s %s\n\n",
        (evtStatus & ACPI_EVENT_FLAG_ENABLED) ? "ENB" : "DIS",
        (evtStatus & ACPI_EVENT_FLAG_WAKE_ENABLED) ? "WKE" : "---",
        (evtStatus & ACPI_EVENT_FLAG_STATUS_SET) ? "SET" : "---",
        (evtStatus & ACPI_EVENT_FLAG_ENABLE_SET) ? "SETENB" : "NOTENB",
        (evtStatus & ACPI_EVENT_FLAG_HAS_HANDLER) ? "HAND" : "----",
        (evtStatus & ACPI_EVENT_FLAG_MASKED) ? "MASK" : "LIVE"
    );
#endif

    //
    // find MCFG if it exists
    //
    ACPI_TABLE_HEADER * pMCFG;
    Status = AcpiGetTable(ACPI_SIG_MCFG, 0, &pMCFG);
    if (!ACPI_FAILURE(Status))
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

#if 0
    void *pWalkRet;
    pWalkRet = NULL;
    Status = AcpiGetDevices(
        NULL,
        sDeviceWalkCallback,
        NULL,
        &pWalkRet
    );
#endif

#if 1
    UINT64          last, newTick;
    K2OSKERN_Debug("Hang ints on\n");
    last = K2OS_SysUpTimeMs();
    while (1)
    {
        do {
            newTick = K2OS_SysUpTimeMs();
        } while (newTick - last < 1000);
        last = newTick;
        K2OSKERN_Debug("Tick %d\n", (UINT32)(newTick & 0xFFFFFFFF));
    }
#endif
}
