//   
//   BSD 3-Clause License
//   
//   Copyright (c) 2020, Kurt Kennett
//   All rights reserved.
//   
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions are met:
//   
//   1. Redistributions of source code must retain the above copyright notice, this
//      list of conditions and the following disclaimer.
//   
//   2. Redistributions in binary form must reproduce the above copyright notice,
//      this list of conditions and the following disclaimer in the documentation
//      and/or other materials provided with the distribution.
//   
//   3. Neither the name of the copyright holder nor the names of its
//      contributors may be used to endorse or promote products derived from
//      this software without specific prior written permission.
//   
//   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
//   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
//   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
//   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
//   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
//   SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
//   CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
//   OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#include "k2osacpi.h"

static void sInitCache(CACHE_HDR *apCache)
{
    UINT32          MaxDepth;
    K2LIST_LINK *   pListLink;
    UINT8 *         pAct;

    MaxDepth = apCache->mMaxDepth;

    pListLink = (K2LIST_LINK *)&apCache->CacheData[0];
    do {
        pAct = ((UINT8 *)pListLink) + sizeof(K2LIST_LINK);
        K2MEM_Set(pAct, (UINT8)apCache->mCacheId, apCache->mObjectBytes - sizeof(K2LIST_LINK));
        K2LIST_AddAtTail(&apCache->FreeList, pListLink);
        pListLink = (K2LIST_LINK *)(((UINT32)pListLink) + apCache->mObjectBytes);
    } while (--MaxDepth);
}

static UINT32 sgSysCacheId = 0x65;
static UINT32 sgOperandCacheId = 0;
static K2LIST_ANCHOR sgOpCacheActive;

void sReleaseOperandObject(CACHE_HDR *apCache, K2LIST_LINK *apListLink, UINT32 aOffset)
{
    UINT32  effAddr;
    BOOL    ok;

    K2LIST_Remove(&sgOpCacheActive, apListLink);

    effAddr = apCache->mVirtBase + (aOffset * K2_VA32_MEMPAGE_BYTES);

    ok = K2OS_VirtPagesDecommit(effAddr, 1);
    K2_ASSERT(ok);
}

void sInitOperandCache(CACHE_HDR *apCache)
{
    UINT32          addr;
    BOOL            ok;
    UINT32          MaxDepth;
    UINT32          offset;
    K2LIST_LINK *   pListLink;

    if (apCache->mVirtBase != 0)
    {
        //
        // purge the list
        //
        while (sgOpCacheActive.mpHead != NULL)
        {
            pListLink = sgOpCacheActive.mpHead;

            offset = ((UINT32)pListLink) - ((UINT32)&apCache->CacheData[0]);
            K2_ASSERT((offset % sizeof(K2LIST_LINK)) == 0);
            offset /= sizeof(K2LIST_LINK);
            K2_ASSERT(offset < apCache->mMaxDepth);
            sReleaseOperandObject(apCache, pListLink, offset);

            K2_ASSERT(apCache->FreeList.mNodeCount < apCache->mMaxDepth);
            K2LIST_AddAtTail(&apCache->FreeList, pListLink);
        }
    }
    else
    {
        MaxDepth = apCache->mMaxDepth;

        pListLink = (K2LIST_LINK *)&apCache->CacheData[0];
        do {
            K2LIST_AddAtTail(&apCache->FreeList, pListLink);
            pListLink = (K2LIST_LINK *)(((UINT32)pListLink) + sizeof(K2LIST_LINK));
        } while (--MaxDepth);

        K2LIST_Init(&sgOpCacheActive);

        addr = 0;

        ok = K2OS_VirtPagesAlloc(&addr, apCache->mMaxDepth, 0, 0);
        K2_ASSERT(ok);

        apCache->mVirtBase = addr;
    }
}

UINT8 * sAcquireOperandObject(CACHE_HDR *apCache, K2LIST_LINK *apListLink, UINT32 aOffset)
{
    UINT32  effAddr;
    BOOL    ok;

    K2LIST_AddAtTail(&sgOpCacheActive, apListLink);

    effAddr = apCache->mVirtBase + (aOffset * K2_VA32_MEMPAGE_BYTES);

    ok = K2OS_VirtPagesCommit(effAddr, 1, K2OS_MAPTYPE_KERN_DATA);
    K2_ASSERT(ok);

    return (UINT8 *)effAddr;
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
    UINT32          newCacheId;

    K2_ASSERT(ObjectSize > 0);
    K2_ASSERT(MaxDepth > 0);

    newCacheId = sgSysCacheId++;

    if (0 == K2ASC_CompIns(CacheName, "Acpi-Operand"))
    {
        sgOperandCacheId = newCacheId;
        objectBytes = sizeof(K2LIST_LINK);
    }
    else
    {
        objectBytes = ObjectSize;
        objectBytes = (ObjectSize + 3) & ~3;
        objectBytes += sizeof(K2LIST_LINK);
    }

    cacheBytes = (objectBytes * ((UINT32)MaxDepth)) + sizeof(CACHE_HDR) - 4;

    pRet = (CACHE_HDR *)K2OS_HeapAlloc(cacheBytes);
    if (pRet == NULL)
    {
        return AE_ERROR;
    }

    K2MEM_Zero(pRet, cacheBytes);

    pRet->mpCacheName = CacheName;
    pRet->mMaxDepth = MaxDepth;
    pRet->mCacheId = newCacheId;
    K2LIST_Init(&pRet->FreeList);

    K2OSKERN_Debug("%02X = %s\n", pRet->mCacheId, pRet->mpCacheName);

    if (sgOperandCacheId != pRet->mCacheId)
    {
        pRet->mObjectBytes = objectBytes;
        sInitCache(pRet);
    }
    else
    {
        pRet->mObjectBytes = ObjectSize;
        sInitOperandCache(pRet);
    }

    K2LIST_AddAtTail(&gK2OSACPI_CacheList, &pRet->CacheListLink);

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

    K2LIST_Remove(&gK2OSACPI_CacheList, &pCache->CacheListLink);

    K2MEM_Zero(pCache, sizeof(CACHE_HDR));

    ok = K2OS_HeapFree((void *)Cache);
    K2_ASSERT(ok);

    return AE_OK;
}

ACPI_STATUS
AcpiOsPurgeCache(
    ACPI_CACHE_T            *Cache)
{
    CACHE_HDR *     pCache;

    pCache = (CACHE_HDR *)Cache;

    if (sgOperandCacheId != pCache->mCacheId)
    {
        K2LIST_Init(&pCache->FreeList);
        sInitCache(pCache);
    }
    else
        sInitOperandCache(pCache);

    return AE_OK;
}

void *
AcpiOsAcquireObject(
    ACPI_CACHE_T            *Cache)
{
    CACHE_HDR *     pCache;
    K2LIST_LINK *   pListLink;
    UINT32          base;
    UINT32          offset;
    UINT8 *         pAct;

    pCache = (CACHE_HDR *)Cache;

    base = (UINT32)&pCache->CacheData[0];

    if (pCache->FreeList.mNodeCount == 0)
    {
        K2OSKERN_Panic("!!!!!!-------EMPTY CACHE(%s)-----------\n", pCache->mpCacheName);
        pAct = NULL;
    }
    else
    {
        pListLink = pCache->FreeList.mpHead;

        K2LIST_Remove(&pCache->FreeList, pListLink);

        offset = ((UINT32)pListLink) - base;
        if (sgOperandCacheId == pCache->mCacheId)
        {
            K2_ASSERT((offset % sizeof(K2LIST_LINK)) == 0);
            offset /= sizeof(K2LIST_LINK);
            K2_ASSERT(offset < pCache->mMaxDepth);
            pAct = sAcquireOperandObject(pCache, pListLink, offset);
        }
        else
        {
            K2_ASSERT((offset % pCache->mObjectBytes) == 0);
            offset /= pCache->mObjectBytes;
            K2_ASSERT(offset < pCache->mMaxDepth);
            pAct = ((UINT8 *)pListLink) + sizeof(K2LIST_LINK);
            //
            // if this fires an object was used after it was freed
            //
            K2_ASSERT(K2MEM_Verify(pAct, (UINT8)pCache->mCacheId, pCache->mObjectBytes - sizeof(K2LIST_LINK)));
            K2MEM_Zero(pAct, pCache->mObjectBytes - sizeof(K2LIST_LINK));
        }

        offset = (pCache->mMaxDepth - pCache->FreeList.mNodeCount);
        if (offset > pCache->mHighwater)
            pCache->mHighwater = offset;
    }

    return pAct;
}

ACPI_STATUS
AcpiOsReleaseObject(
    ACPI_CACHE_T            *Cache,
    void                    *Object)
{
    CACHE_HDR *     pCache;
    UINT32          base;
    UINT32          offset;
    K2LIST_LINK *   pListLink;

    pCache = (CACHE_HDR *)Cache;

    if (sgOperandCacheId == pCache->mCacheId)
    {
        pListLink = (K2LIST_LINK *)&pCache->CacheData[0];
        offset = (((UINT32)Object) - pCache->mVirtBase) / K2_VA32_MEMPAGE_BYTES;
        K2_ASSERT(offset < pCache->mMaxDepth);
        pListLink += offset;
        sReleaseOperandObject(pCache, pListLink, offset);
    }
    else
    {
        base = (UINT32)&pCache->CacheData[0];
        pListLink = (K2LIST_LINK *)(((UINT8 *)Object) - sizeof(K2LIST_LINK));
        offset = ((UINT32)pListLink) - base;
        K2_ASSERT((offset % pCache->mObjectBytes) == 0);
        offset /= pCache->mObjectBytes;
        K2_ASSERT(offset < pCache->mMaxDepth);
        K2MEM_Set(Object, (UINT8)pCache->mCacheId, pCache->mObjectBytes - sizeof(K2LIST_LINK));
    }

    K2_ASSERT(pCache->FreeList.mNodeCount < pCache->mMaxDepth);
    K2LIST_AddAtTail(&pCache->FreeList, pListLink);

    return AE_OK;
}

