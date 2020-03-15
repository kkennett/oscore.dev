#include "k2osacpi.h"

ACPI_STATUS
AcpiOsCreateLock(
    ACPI_SPINLOCK           *OutHandle)
{
    K2_ASSERT(0);
    return ACPI_FAILURE(1);
}

void
AcpiOsDeleteLock(
    ACPI_SPINLOCK           Handle)
{
    K2_ASSERT(0);
}

ACPI_CPU_FLAGS
AcpiOsAcquireLock(
    ACPI_SPINLOCK           Handle)
{
    K2_ASSERT(0);
    return 0;
}

void
AcpiOsReleaseLock(
    ACPI_SPINLOCK           Handle,
    ACPI_CPU_FLAGS          Flags)
{
    K2_ASSERT(0);
}

