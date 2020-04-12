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
SystemNotifyHandler(
    ACPI_HANDLE                 Device,
    UINT32                      Value,
    void                        *Context)
{
    K2OSKERN_Debug("K2OSACPI:NotifyHandler = Received a Notify %08X\n", Value);
    K2_ASSERT(0);
}

static ACPI_STATUS
SysMemoryRegionInit(
    ACPI_HANDLE                 RegionHandle,
    UINT32                      Function,
    void                        *HandlerContext,
    void                        **RegionContext)
{
    K2OSKERN_Debug("K2OSACPI:SysMemoryRegionInit = Received a region init for %08X\n", RegionHandle);
    K2_ASSERT(0);
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
SysMemoryRegionHandler(
    UINT32                      Function,
    ACPI_PHYSICAL_ADDRESS       Address,
    UINT32                      BitWidth,
    UINT64                      *Value,
    void                        *HandlerContext,
    void                        *RegionContext)
{
    K2OSKERN_Debug("K2OSACPI:SysMemoryRegionHandler = Received a Region Access\n");
    K2_ASSERT(0);
    return (AE_OK);
}

void InstallHandlers1(void)
{
    ACPI_STATUS acpiStatus;

    acpiStatus = AcpiInstallNotifyHandler(ACPI_ROOT_OBJECT,
        ACPI_SYSTEM_NOTIFY, SystemNotifyHandler, NULL);
    K2_ASSERT(!ACPI_FAILURE(acpiStatus));

    acpiStatus = AcpiInstallAddressSpaceHandler(ACPI_ROOT_OBJECT,
        ACPI_ADR_SPACE_SYSTEM_MEMORY, SysMemoryRegionHandler, SysMemoryRegionInit, NULL);
    K2_ASSERT(!ACPI_FAILURE(acpiStatus));
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

void InstallHandlers2(void)
{
    ACPI_STATUS acpiStatus;

    acpiStatus = AcpiInstallFixedEventHandler(ACPI_EVENT_POWER_BUTTON, PowerButtonHandler, NULL);
    K2_ASSERT(!ACPI_FAILURE(acpiStatus));

    acpiStatus = AcpiEnableEvent(ACPI_EVENT_POWER_BUTTON, 0);
    K2_ASSERT(!ACPI_FAILURE(acpiStatus));
}

