#include "k2osacpi.h"

UINT32 K2OSKERN_Debug(char const *pFormat, ...);

ACPI_STATUS
AcpiOsCreateSemaphore(
    UINT32                  MaxUnits,
    UINT32                  InitialUnits,
    ACPI_SEMAPHORE          *OutHandle)
{
    K2OS_TOKEN tokSem;

    K2OSKERN_Debug("OsCreateSemaphore()\n");
    if (!K2OS_SemaphoreCreate(NULL, MaxUnits, InitialUnits, &tokSem))
    {
        K2_ASSERT(0);
        return AE_ERROR;
    }

    *OutHandle = (ACPI_SEMAPHORE)tokSem;

    return AE_OK;
}

ACPI_STATUS
AcpiOsDeleteSemaphore(
    ACPI_SEMAPHORE          Handle)
{
    K2OSKERN_Debug("OsDeleteSemaphore()\n");
    if (!K2OS_TokenDestroy((K2OS_TOKEN)Handle))
    {
        K2_ASSERT(0);
        return AE_ERROR;
    }

    return AE_OK;
}

ACPI_STATUS
AcpiOsWaitSemaphore(
    ACPI_SEMAPHORE          Handle,
    UINT32                  Units,
    UINT16                  Timeout)
{
    BOOL    ok;
    UINT32  took;
    UINT32  waitResult;
    UINT32  k2Timeout;
    UINT32  elapsed;
    UINT64  snapTime;
    UINT64  newTime;

    K2_ASSERT(Units > 0);

    K2OSKERN_Debug("OsWaitSemaphore()\n");

    if (Timeout == ACPI_WAIT_FOREVER)
        k2Timeout = K2OS_TIMEOUT_INFINITE;
    else
        k2Timeout = (UINT32)Timeout;

    took = 0;
    snapTime = K2OS_SysUpTimeMs();
    do {
        waitResult = K2OS_ThreadWaitOne((K2OS_TOKEN)Handle, k2Timeout);
        if (waitResult != K2OS_WAIT_SIGNALLED_0)
        {
            break;
        }
        took++;
        if (took == Units)
            break;
        if (k2Timeout != K2OS_TIMEOUT_INFINITE)
        {
            newTime = K2OS_SysUpTimeMs();
            snapTime = newTime - snapTime;
            if (snapTime & 0xFFFFFFFF00000000ull)
                break;
            elapsed = (UINT32)snapTime;
            if (elapsed >= k2Timeout)
                break;
            k2Timeout -= elapsed;
            snapTime = newTime;
        }
    } while (1);

    if (took != Units)
    {
        if (took > 0)
        {
            ok = K2OS_SemaphoreRelease((K2OS_TOKEN)Handle, took, NULL);
            K2_ASSERT(ok);
        }
        return AE_ERROR;
    }

    return AE_OK;
}

ACPI_STATUS
AcpiOsSignalSemaphore(
    ACPI_SEMAPHORE          Handle,
    UINT32                  Units)
{
    BOOL ok;

    K2_ASSERT(Units > 0);

    K2OSKERN_Debug("OsSignalSemaphore()\n");

    ok = K2OS_SemaphoreRelease((K2OS_TOKEN)Handle, Units, NULL);
    K2_ASSERT(ok);

    return AE_OK;
}



