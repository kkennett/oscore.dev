#include "k2osacpi.h"

void *
AcpiOsAllocate(
    ACPI_SIZE               Size)
{
    K2OSKERN_Debug("ACPI:AcpiOsAllocate(%d)\n", Size);
    return K2OS_HeapAlloc(Size);
}

void
AcpiOsFree(
    void *                  Memory)
{
    K2OS_HeapFree(Memory);
}

