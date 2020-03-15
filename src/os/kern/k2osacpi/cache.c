#include "k2osacpi.h"

ACPI_STATUS
AcpiOsCreateCache(
    char                    *CacheName,
    UINT16                  ObjectSize,
    UINT16                  MaxDepth,
    ACPI_CACHE_T            **ReturnCache)
{
    K2_ASSERT(0);
    return ACPI_FAILURE(1);
}

ACPI_STATUS
AcpiOsDeleteCache(
    ACPI_CACHE_T            *Cache)
{
    K2_ASSERT(0);
    return ACPI_FAILURE(1);
}

ACPI_STATUS
AcpiOsPurgeCache(
    ACPI_CACHE_T            *Cache)
{
    K2_ASSERT(0);
    return ACPI_FAILURE(1);
}

void *
AcpiOsAcquireObject(
    ACPI_CACHE_T            *Cache)
{
    K2_ASSERT(0);
    return NULL;
}

ACPI_STATUS
AcpiOsReleaseObject(
    ACPI_CACHE_T            *Cache,
    void                    *Object)
{
    K2_ASSERT(0);
    return ACPI_FAILURE(1);
}

