#include "k2osacpi.h"

ACPI_STATUS
AcpiOsCreateSemaphore(
    UINT32                  MaxUnits,
    UINT32                  InitialUnits,
    ACPI_SEMAPHORE          *OutHandle)
{
    K2_ASSERT(0);
    return ACPI_FAILURE(1);
}

ACPI_STATUS
AcpiOsDeleteSemaphore(
    ACPI_SEMAPHORE          Handle)
{
    K2_ASSERT(0);
    return ACPI_FAILURE(1);
}

ACPI_STATUS
AcpiOsWaitSemaphore(
    ACPI_SEMAPHORE          Handle,
    UINT32                  Units,
    UINT16                  Timeout)
{
    K2_ASSERT(0);
    return ACPI_FAILURE(1);
}

ACPI_STATUS
AcpiOsSignalSemaphore(
    ACPI_SEMAPHORE          Handle,
    UINT32                  Units)
{
    K2_ASSERT(0);
    return ACPI_FAILURE(1);
}



