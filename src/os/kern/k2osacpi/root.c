#include "k2osacpi.h"

ACPI_PHYSICAL_ADDRESS
AcpiOsGetRootPointer(
    void)
{
    BOOL    ok;
    UINT32  addr;

    ok = K2OS_SysGetProperty(K2OS_SYSPROP_ID_KERN_FWTABLES_PHYS_ADDR, &addr, sizeof(UINT32));
    K2_ASSERT(ok);

    return (ACPI_PHYSICAL_ADDRESS)addr;
}

