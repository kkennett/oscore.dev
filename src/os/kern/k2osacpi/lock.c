#include "k2osacpi.h"

ACPI_STATUS
AcpiOsCreateLock(
    ACPI_SPINLOCK           *OutHandle)
{
    K2OSKERN_SEQLOCK *pLock;

    pLock = (K2OSKERN_SEQLOCK *)K2OS_HeapAlloc(sizeof(K2OSKERN_SEQLOCK));
    if (pLock == NULL)
    {
        K2_ASSERT(0);
        return AE_ERROR;
    }

    K2OSKERN_SeqIntrInit(pLock);

    *OutHandle = pLock;

    return AE_OK;
}

void
AcpiOsDeleteLock(
    ACPI_SPINLOCK           Handle)
{
    BOOL ok;
    K2_ASSERT(Handle != NULL);
    ok = K2OS_HeapFree(Handle);
    K2_ASSERT(ok);
}

ACPI_CPU_FLAGS
AcpiOsAcquireLock(
    ACPI_SPINLOCK   Handle)
{
    return (ACPI_CPU_FLAGS)K2OSKERN_SeqIntrLock((K2OSKERN_SEQLOCK *)Handle);
}

void
AcpiOsReleaseLock(
    ACPI_SPINLOCK           Handle,
    ACPI_CPU_FLAGS          Flags)
{
    K2OSKERN_SeqIntrUnlock((K2OSKERN_SEQLOCK *)Handle, (BOOL)Flags);
}

