#include "k2osacpi.h"

ACPI_STATUS
AcpiOsReadMemory(
    ACPI_PHYSICAL_ADDRESS   Address,
    UINT64                  *Value,
    UINT32                  Width)
{
    K2_ASSERT(0);
    return ACPI_FAILURE(1);
}

ACPI_STATUS
AcpiOsWriteMemory(
    ACPI_PHYSICAL_ADDRESS   Address,
    UINT64                  Value,
    UINT32                  Width)
{
    K2_ASSERT(0);
    return ACPI_FAILURE(1);
}


