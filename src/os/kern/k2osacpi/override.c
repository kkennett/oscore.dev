#include "k2osacpi.h"

ACPI_STATUS
AcpiOsPredefinedOverride(
    const ACPI_PREDEFINED_NAMES *InitVal,
    ACPI_STRING                 *NewVal)
{
    K2_ASSERT(0);
    return ACPI_FAILURE(1);
}

ACPI_STATUS
AcpiOsTableOverride(
    ACPI_TABLE_HEADER       *ExistingTable,
    ACPI_TABLE_HEADER       **NewTable)
{
    K2_ASSERT(0);
    return ACPI_FAILURE(1);
}

ACPI_STATUS
AcpiOsPhysicalTableOverride(
    ACPI_TABLE_HEADER       *ExistingTable,
    ACPI_PHYSICAL_ADDRESS   *NewAddress,
    UINT32                  *NewTableLength)
{
    K2_ASSERT(0);
    return ACPI_FAILURE(1);
}

