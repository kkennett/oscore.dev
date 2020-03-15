#include "k2osacpi.h"

ACPI_STATUS
AcpiOsInstallInterruptHandler(
    UINT32                  InterruptNumber,
    ACPI_OSD_HANDLER        ServiceRoutine,
    void                    *Context)
{
    K2_ASSERT(0);
    return ACPI_FAILURE(1);
}

ACPI_STATUS
AcpiOsRemoveInterruptHandler(
    UINT32                  InterruptNumber,
    ACPI_OSD_HANDLER        ServiceRoutine)
{
    K2_ASSERT(0);
    return ACPI_FAILURE(1);
}

