#include "k2osacpi.h"

ACPI_STATUS
AcpiOsReadPort(
    ACPI_IO_ADDRESS         Address,
    UINT32                  *Value,
    UINT32                  Width)
{
    K2_ASSERT(0);
    return ACPI_FAILURE(1);
}

ACPI_STATUS
AcpiOsWritePort(
    ACPI_IO_ADDRESS         Address,
    UINT32                  Value,
    UINT32                  Width)
{
    K2_ASSERT(0);
    return ACPI_FAILURE(1);
}

