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

static ACPI_STATUS
sInstallHandlers(void)
{
    ACPI_STATUS             Status;

    Status = AcpiInstallNotifyHandler(ACPI_ROOT_OBJECT,
        ACPI_SYSTEM_NOTIFY, NotifyHandler, NULL);
    if (ACPI_FAILURE(Status))
    {
        K2OSKERN_Debug("K2OSACPI: Failure will installing notify handler\n");
        return (Status);
    }

    Status = AcpiInstallAddressSpaceHandler(ACPI_ROOT_OBJECT,
        ACPI_ADR_SPACE_SYSTEM_MEMORY, RegionHandler, RegionInit, NULL);
    if (ACPI_FAILURE(Status))
    {
        K2OSKERN_Debug("K2OSACPI: Failure will installing OpRegion handler\n");
        return (Status);
    }

    return (AE_OK);
}

K2STAT 
K2_CALLCONV_REGS 
dlx_entry(
    DLX *   apDlx,
    UINT32  aReason
    )
{
    ACPI_STATUS Status;

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

    return K2STAT_NO_ERROR;
}

