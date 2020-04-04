#include "k2osacpi.h"

typedef struct _CACHE_HDR CACHE_HDR;
struct _CACHE_HDR
{
    char *              mpCacheName;
    UINT32              mObjectBytes;
    UINT32              mMaxDepth;
    K2OSKERN_SEQLOCK    SeqLock;
    K2LIST_ANCHOR       FreeList;
    UINT8               CacheData[4];
};

static void sInitCache(CACHE_HDR *apCache)
{
    UINT32          MaxDepth;
    K2LIST_LINK *   pListLink;
    UINT8 *         pAct;

    MaxDepth = apCache->mMaxDepth;

    K2LIST_Init(&apCache->FreeList);
    pListLink = (K2LIST_LINK *)&apCache->CacheData[0];
    do {
        pAct = ((UINT8 *)pListLink) + sizeof(K2LIST_LINK);
        K2MEM_Set(pAct, 0x55, apCache->mObjectBytes - sizeof(K2LIST_LINK));
        K2LIST_AddAtTail(&apCache->FreeList, pListLink);
        pListLink = (K2LIST_LINK *)(((UINT32)pListLink) + apCache->mObjectBytes);
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
    UINT32          objectBytes;
    UINT32          cacheBytes;

    K2_ASSERT(ObjectSize > 0);
    K2_ASSERT(MaxDepth > 0);

//    K2OSKERN_Debug("AcpiOsCreateCache(\"%s\", %d, %d)\n", CacheName, ObjectSize, MaxDepth);

    objectBytes = ObjectSize;
    objectBytes = (ObjectSize + 3) & ~3;
    objectBytes += sizeof(K2LIST_LINK);
   
    cacheBytes = (objectBytes * ((UINT32)MaxDepth)) + sizeof(CACHE_HDR) - 4;

    pRet = (CACHE_HDR *)K2OS_HeapAlloc(cacheBytes);
    if (pRet == NULL)
    {
        return AE_ERROR;
    }

    K2MEM_Zero(pRet, cacheBytes);

    pRet->mpCacheName = CacheName;
    pRet->mObjectBytes = objectBytes;
    pRet->mMaxDepth = MaxDepth;

    K2OSKERN_SeqIntrInit(&pRet->SeqLock);

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

    K2MEM_Zero(pCache, sizeof(CACHE_HDR));

    ok = K2OS_HeapFree((void *)Cache);
    K2_ASSERT(ok);

    return AE_OK;
}

ACPI_STATUS
AcpiOsPurgeCache(
    ACPI_CACHE_T            *Cache)
{
    BOOL            disp;
    CACHE_HDR *     pCache;

    pCache = (CACHE_HDR *)Cache;

    disp = K2OSKERN_SeqIntrLock(&pCache->SeqLock);

    sInitCache(pCache);

    K2OSKERN_SeqIntrUnlock(&pCache->SeqLock, disp);

    return AE_OK;
}

void *
AcpiOsAcquireObject(
    ACPI_CACHE_T            *Cache)
{
    BOOL            disp;
    CACHE_HDR *     pCache;
    K2LIST_LINK *   pListLink;
    UINT32          base;
    UINT32          offset;
    UINT8 *         pAct;

    pCache = (CACHE_HDR *)Cache;

    base = (UINT32)&pCache->CacheData[0];

    disp = K2OSKERN_SeqIntrLock(&pCache->SeqLock);

    if (pCache->FreeList.mNodeCount > 0)
    {
        pListLink = pCache->FreeList.mpHead;

        offset = ((UINT32)pListLink) - base;
        K2_ASSERT((offset % pCache->mObjectBytes) == 0);
        K2_ASSERT((offset / pCache->mObjectBytes) < pCache->mMaxDepth);

        pAct = ((UINT8 *)pListLink) + sizeof(K2LIST_LINK);

        //
        // if this fires an object was used after it was freed
        //
        K2_ASSERT(K2MEM_Verify(pAct, 0x55, pCache->mObjectBytes - sizeof(K2LIST_LINK)));

        K2LIST_Remove(&pCache->FreeList, pListLink);
    }
    else
    {
        K2OSKERN_Debug("-------------EMPTY CACHE(%s)-----------\n", pCache->mpCacheName);
        pAct = NULL;
    }

    K2OSKERN_SeqIntrUnlock(&pCache->SeqLock, disp);

    if (pAct != NULL)
    {
        K2MEM_Zero(pAct, pCache->mObjectBytes - sizeof(K2LIST_LINK));
    }

    return pAct;
}

ACPI_STATUS
AcpiOsReleaseObject(
    ACPI_CACHE_T            *Cache,
    void                    *Object)
{
    BOOL            disp;
    CACHE_HDR *     pCache;
    UINT32          base;
    UINT32          offset;
    K2LIST_LINK *   pListLink;

    pCache = (CACHE_HDR *)Cache;

    base = (UINT32)&pCache->CacheData[0];

    pListLink = (K2LIST_LINK *)(((UINT8 *)Object) - sizeof(K2LIST_LINK));

    offset = ((UINT32)pListLink) - base;
    K2_ASSERT((offset % pCache->mObjectBytes) == 0);
    K2_ASSERT((offset / pCache->mObjectBytes) < pCache->mMaxDepth);

    K2MEM_Set(Object, 0x55, pCache->mObjectBytes - sizeof(K2LIST_LINK));

    disp = K2OSKERN_SeqIntrLock(&pCache->SeqLock);

    K2_ASSERT(pCache->FreeList.mNodeCount < pCache->mMaxDepth);

    K2LIST_AddAtTail(&pCache->FreeList, pListLink);

    K2OSKERN_SeqIntrUnlock(&pCache->SeqLock, disp);

    return AE_OK;
}

