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

#include "ik2osacpi.h"

#define CACHE_BLOCK_ITEM_COUNT  32

typedef struct _CACHE_HDR       CACHE_HDR;
typedef struct _CACHE_BLOCK_HDR CACHE_BLOCK_HDR;
typedef struct _CACHE_ITEM_HDR  CACHE_ITEM_HDR;

struct _CACHE_ITEM_HDR
{
    CACHE_BLOCK_HDR *   mpBlock;
    K2LIST_LINK         ItemListLink;
    UINT8               Payload[4];
};

#define CACHE_ITEM_BYTES(alignedPayloadBytes) \
    (sizeof(CACHE_ITEM_HDR) - 4 + (alignedPayloadBytes))

struct _CACHE_BLOCK_HDR
{
    CACHE_HDR *     mpCache;
    K2LIST_LINK     BlockListLink;
    UINT32          mBlockFreeCount;
    CACHE_ITEM_HDR  Items[1];           // first of many(array)
};

#define CACHE_BLOCK_BYTES(blockItemCount, alignedPayloadBytes) \
    (sizeof(CACHE_BLOCK_HDR) - sizeof(CACHE_ITEM_HDR) + ((blockItemCount) * CACHE_ITEM_BYTES(alignedPayloadBytes)))

struct _CACHE_HDR
{
    K2LIST_LINK         CacheListLink;
    UINT32              mItemsPerBlock;
    UINT32              mTruePayloadBytes;
    UINT32              mTargetDepth;
    char *              mpCacheName;
    K2OSKERN_SEQLOCK    SeqLock;
    K2LIST_ANCHOR       BlockList;
    K2LIST_ANCHOR       ItemList;
    CACHE_BLOCK_HDR     Block0;         // only block 0 lives here
};

#define CACHE_BYTES(blockItemCount, alignedPayloadBytes) \
    ((sizeof(CACHE_HDR) - sizeof(CACHE_BLOCK_HDR)) + CACHE_BLOCK_BYTES(blockItemCount, alignedPayloadBytes))

ACPI_STATUS
AcpiOsCreateCache(
    char                    *CacheName,
    UINT16                  ObjectSize,
    UINT16                  MaxDepth,
    ACPI_CACHE_T            **ReturnCache)
{
    CACHE_HDR *         pRet;
    UINT32              alignedPayloadBytes;
    UINT32              cacheBytes;
    UINT32              ix;
    CACHE_ITEM_HDR *    pItemHdr;

    K2_ASSERT(ObjectSize > 0);
    K2_ASSERT(MaxDepth > 0);

    alignedPayloadBytes = (ObjectSize + 3) & ~3;
    
    cacheBytes = CACHE_BYTES(CACHE_BLOCK_ITEM_COUNT, alignedPayloadBytes);

    pRet = (CACHE_HDR *)K2OS_HeapAlloc(cacheBytes);
    if (pRet == NULL)
    {
        return AE_ERROR;
    }

    K2MEM_Zero(pRet, cacheBytes);
    pRet->mItemsPerBlock = CACHE_BLOCK_ITEM_COUNT;
    pRet->mTruePayloadBytes = ObjectSize;
    pRet->mTargetDepth = MaxDepth;
    pRet->mpCacheName = CacheName;
    K2OSKERN_SeqIntrInit(&pRet->SeqLock);
    K2LIST_Init(&pRet->BlockList);
    K2LIST_Init(&pRet->ItemList);

    pRet->Block0.mpCache = pRet;
    K2LIST_AddAtHead(&pRet->BlockList, &pRet->Block0.BlockListLink);

    pItemHdr = &pRet->Block0.Items[0];
    for (ix = 0; ix < CACHE_BLOCK_ITEM_COUNT; ix++)
    {
        pItemHdr->mpBlock = &pRet->Block0;
        K2LIST_AddAtTail(&pRet->ItemList, &pItemHdr->ItemListLink);
        K2MEM_Set(&pItemHdr->Payload[0], 0xFE, pRet->mTruePayloadBytes);
        pRet->Block0.mBlockFreeCount++;
    }

    K2LIST_AddAtTail(&gK2OSACPI_CacheList, &pRet->CacheListLink);

    while (pRet->ItemList.mNodeCount < pRet->mTargetDepth)
    {
        //
        // add a block
    }

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

