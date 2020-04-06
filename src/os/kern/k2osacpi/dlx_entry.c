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

#include "k2osacpi.h"

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

#define CHK_WATER 0

K2STAT
K2_CALLCONV_REGS
dlx_entry(
    DLX *   apDlx,
    UINT32  aReason
)
{
    ACPI_STATUS         Status;
#if CHK_WATER
    K2LIST_LINK *       pListLink;
    CACHE_HDR *         pCache;
#endif
    ACPI_EVENT_STATUS   evtStatus;

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
    if (ACPI_FAILURE(Status))
    {
        K2OSKERN_Debug("K2OSACPI: Failure with installing Power Button handler\n");
        return (Status);
    }
    Status = AcpiEnableEvent(ACPI_EVENT_POWER_BUTTON, 0);
    if (ACPI_FAILURE(Status))
    {
        K2OSKERN_Debug("K2OSACPI: Failure enabling Power Button handler\n");
        return (Status);
}

#if CHK_WATER
    K2OSKERN_Debug("\n=========================\nAT ACPI INIT DONE:\n");
    pListLink = gK2OSACPI_CacheList.mpHead;
    while (pListLink != NULL)
    {
        pCache = K2_GET_CONTAINER(CACHE_HDR, pListLink, CacheListLink);
        K2OSKERN_Debug("%4d/%4d (high %4d) -- %s\n", pCache->mMaxDepth - pCache->FreeList.mNodeCount, pCache->mMaxDepth, pCache->mHighwater, pCache->mpCacheName);
        pListLink = pListLink->mpNext;
    }
    K2OSKERN_Debug("=========================\n");
#endif

#if 0
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
    return K2STAT_NO_ERROR;
}

