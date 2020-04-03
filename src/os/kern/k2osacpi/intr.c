#include "k2osacpi.h"

ACPI_STATUS
AcpiOsInstallInterruptHandler(
    UINT32                  InterruptNumber,
    ACPI_OSD_HANDLER        ServiceRoutine,
    void                    *Context)
{
    K2OSKERN_Debug("K2OSACPI: AcpiOsInstallInterruptHandler(%d) - %08X\n", InterruptNumber, ServiceRoutine);
    return AE_OK;
}

ACPI_STATUS
AcpiOsRemoveInterruptHandler(
    UINT32                  InterruptNumber,
    ACPI_OSD_HANDLER        ServiceRoutine)
{
    K2OSKERN_Debug("K2OSACPI: AcpiOsRemoveInterruptHandler(%d) - %08X\n", InterruptNumber, ServiceRoutine);
    return AE_OK;
}

