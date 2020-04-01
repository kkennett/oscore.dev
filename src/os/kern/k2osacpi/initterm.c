#include "k2osacpi.h"

ACPI_STATUS
AcpiOsInitialize(
    void)
{
    K2OSKERN_Debug("AcpiOsInitialize()\n");
    return AE_OK;
}

ACPI_STATUS
AcpiOsTerminate(
    void)
{
    K2OSKERN_Debug("AcpiOsTerminate()\n");
    return AE_OK;
}
