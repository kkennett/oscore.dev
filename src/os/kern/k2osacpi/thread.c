#include "k2osacpi.h"

ACPI_THREAD_ID
AcpiOsGetThreadId(
    void)
{
    return K2OS_ThreadGetOwnId();
}

ACPI_STATUS
AcpiOsExecute(
    ACPI_EXECUTE_TYPE       Type,
    ACPI_OSD_EXEC_CALLBACK  Function,
    void                    *Context)
{
    K2_ASSERT(0);
    return ACPI_FAILURE(1);
}

void
AcpiOsWaitEventsComplete(
    void)
{
    K2_ASSERT(0);
}

void
AcpiOsSleep(
    UINT64                  Milliseconds)
{
    K2OS_ThreadSleep(Milliseconds);
}

void
AcpiOsStall(
    UINT32                  Microseconds)
{
    K2OSKERN_MicroStall(Microseconds);
}

