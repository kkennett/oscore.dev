#include "k2osacpi.h"

typedef struct _CACHE_HDR CACHE_HDR;
struct _CACHE_HDR
{
    char *          mpCacheName;
    UINT32          mObjectSize;
    UINT32          mMaxDepth;
    K2OS_CRITSEC    CritSec;
    K2LIST_ANCHOR   FreeList;
    UINT8           CacheData[4];
};

static void sInitCache(CACHE_HDR *apCache)
{
    UINT32          MaxDepth;
    K2LIST_LINK *   pListLink;

    MaxDepth = apCache->mMaxDepth;

    K2LIST_Init(&apCache->FreeList);
    pListLink = (K2LIST_LINK *)&apCache->CacheData;
    do {
        K2LIST_AddAtTail(&apCache->FreeList, pListLink);
        pListLink = (K2LIST_LINK *)(((UINT32)pListLink) + apCache->mObjectSize);
    } while (--MaxDepth);
}

ACPI_STATUS
AcpiOsCreateCache(
    char                    *CacheName,
    UINT16                  ObjectSize,
    UINT16                  MaxDepth,
    ACPI_CACHE_T            **ReturnCache)
{
    CACHE_HDR *     pRet;
    UINT32          cacheBytes;
    BOOL            ok;

    K2_ASSERT(ObjectSize > 0);
    K2_ASSERT(MaxDepth > 0);
    
    cacheBytes = (((UINT32)ObjectSize) * ((UINT32)MaxDepth)) + sizeof(CACHE_HDR) - 4;

    pRet = (CACHE_HDR *)K2OS_HeapAlloc(cacheBytes);
    if (pRet == NULL)
    {
        K2OSKERN_Debug("Cache alloc failed\n");
        return AE_ERROR;
    }

    K2MEM_Zero(pRet, cacheBytes);

    pRet->mpCacheName = CacheName;
    
    pRet->mObjectSize = ObjectSize;
    if (pRet->mObjectSize < sizeof(K2LIST_LINK))
        pRet->mObjectSize = sizeof(K2LIST_LINK);
    else
        pRet->mObjectSize = (pRet->mObjectSize + 3) & ~3;

    pRet->mMaxDepth = MaxDepth;

    ok = K2OS_CritSecInit(&pRet->CritSec);
    K2_ASSERT(ok);

    sInitCache(pRet);

    *ReturnCache = (ACPI_CACHE_T *)pRet;

    return AE_OK;
}

ACPI_STATUS
AcpiOsDeleteCache(
    ACPI_CACHE_T            *Cache)
{
    BOOL        ok;
    CACHE_HDR * pCache;

    pCache = (CACHE_HDR *)Cache;
    ok = K2OS_CritSecDone(&pCache->CritSec);
    K2_ASSERT(ok);

    K2MEM_Zero(pCache, sizeof(CACHE_HDR));

    ok = K2OS_HeapFree((void *)Cache);
    K2_ASSERT(ok);

    return AE_OK;
}

ACPI_STATUS
AcpiOsPurgeCache(
    ACPI_CACHE_T            *Cache)
{
    BOOL            ok;
    CACHE_HDR *     pCache;

    pCache = (CACHE_HDR *)Cache;

    ok = K2OS_CritSecEnter(&pCache->CritSec);
    K2_ASSERT(ok);

    sInitCache(pCache);

    ok = K2OS_CritSecLeave(&pCache->CritSec);
    K2_ASSERT(ok);

    return AE_OK;
}

void *
AcpiOsAcquireObject(
    ACPI_CACHE_T            *Cache)
{
    BOOL            ok;
    CACHE_HDR *     pCache;
    K2LIST_LINK *   pListLink;

    pCache = (CACHE_HDR *)Cache;

    ok = K2OS_CritSecEnter(&pCache->CritSec);
    K2_ASSERT(ok);

    if (pCache->FreeList.mNodeCount > 0)
    {
        pListLink = pCache->FreeList.mpHead;
        K2LIST_Remove(&pCache->FreeList, pListLink);
    }
    else
        pListLink = NULL;

    ok = K2OS_CritSecLeave(&pCache->CritSec);
    K2_ASSERT(ok);

    return (UINT8 *)pListLink;
}

ACPI_STATUS
AcpiOsReleaseObject(
    ACPI_CACHE_T            *Cache,
    void                    *Object)
{
    BOOL            ok;
    CACHE_HDR *     pCache;

    pCache = (CACHE_HDR *)Cache;

    // assert Object is inside range of cache memory and on an object boundary

    ok = K2OS_CritSecEnter(&pCache->CritSec);
    K2_ASSERT(ok);

    K2_ASSERT(pCache->FreeList.mNodeCount < pCache->mMaxDepth);

    K2LIST_AddAtTail(&pCache->FreeList, (K2LIST_LINK *)Object);

    ok = K2OS_CritSecLeave(&pCache->CritSec);
    K2_ASSERT(ok);

    return AE_OK;
}

