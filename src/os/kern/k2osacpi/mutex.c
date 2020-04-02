#include "k2osacpi.h"

ACPI_STATUS
AcpiOsCreateMutex(
    ACPI_MUTEX              *OutHandle)
{
    return AE_ERROR;
}

void
AcpiOsDeleteMutex(
    ACPI_MUTEX              Handle)
{
}

ACPI_STATUS
AcpiOsAcquireMutex(
    ACPI_MUTEX              Handle,
    UINT16                  Timeout)
{
    return AE_ERROR;
}

void
AcpiOsReleaseMutex(
    ACPI_MUTEX              Handle)
{

}
