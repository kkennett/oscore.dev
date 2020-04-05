#include "k2osacpi.h"

#define MAX_NUM_INTR 4

K2OSACPI_INTR sgIntr[MAX_NUM_INTR];

K2LIST_ANCHOR gK2OSACPI_IntrList;
K2LIST_ANCHOR gK2OSACPI_IntrFreeList;

ACPI_STATUS
AcpiOsInitialize(
    void)
{
    UINT32 ix;

    K2LIST_Init(&gK2OSACPI_IntrList);
    K2LIST_Init(&gK2OSACPI_IntrFreeList);

    for (ix = 0; ix < MAX_NUM_INTR; ix++)
    {
        K2LIST_AddAtTail(&gK2OSACPI_IntrFreeList, &sgIntr[ix].ListLink);
    }
    
    return AE_OK;
}

ACPI_STATUS
AcpiOsTerminate(
    void)
{
    return AE_OK;
}
