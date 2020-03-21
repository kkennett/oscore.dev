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

#include <lib/k2ramheap.h>

static
BOOL
sCheckHdrSent(
    K2OS_RAMHEAP_NODE * apNode,
    BOOL                aCheckFree
)
{
    UINT32 sent;

    sent = apNode->mSentinel;

    if (sent == K2OS_RAMHEAP_NODE_HDRSENT_FREE)
    {
        return aCheckFree;
    }

    if (sent == K2OS_RAMHEAP_NODE_HDRSENT_USED)
    {
        return !aCheckFree;
    }

    return FALSE;
}

static
BOOL
sCheckEndSent(
    K2OS_RAMHEAP_NODE *apNode
)
{
    UINT32 * pEnd;
    pEnd = (UINT32 *)(((UINT32)apNode) + sizeof(K2OS_RAMHEAP_NODE) + apNode->TreeNode.mUserVal);
    return (*pEnd == K2OS_RAMHEAP_NODE_ENDSENT);
}

static
K2OS_RAMHEAP_CHUNK *
sScanForAddrChunk(
    K2OS_RAMHEAP *  apHeap,
    UINT32          aAddr
)
{
    K2LIST_LINK *       pLink;
    K2OS_RAMHEAP_CHUNK *pChunk;

    pLink = apHeap->ChunkList.mpHead;
    do {
        pChunk = K2_GET_CONTAINER(K2OS_RAMHEAP_CHUNK, pLink, ChunkListLink);
        if ((aAddr >= pChunk->mStart) &&
            (aAddr < pChunk->mBreak))
        {
            return pChunk;
        }
        pLink = pChunk->ChunkListLink.mpNext;
    } while (pLink != NULL);

    return NULL;
}

static
void
sLockHeap(
    K2OS_RAMHEAP * apHeap
)
{
    if (apHeap->fLock == NULL)
        return;
    apHeap->mLockDisp = apHeap->fLock(apHeap);
}

static
void
sUnlockHeap(
    K2OS_RAMHEAP * apHeap
)
{
    if (apHeap->fUnlock == NULL)
        return;
    apHeap->fUnlock(apHeap, apHeap->mLockDisp);
}

#if 0

static
void
sDump(
    K2OS_RAMHEAP *apHeap
)
{
    K2LIST_LINK *           pLink;
    K2LIST_LINK *           pChunkLink;
    K2OS_RAMHEAP_CHUNK *    pChunk;
    K2OS_RAMHEAP_NODE *     pNode;
    char                    sentCode[8];
    
    sentCode[4] = 0;
    printf("\n--------ADDRLIST-------------\n");
    pChunkLink = apHeap->ChunkList.mpHead;
    if (pChunkLink != NULL)
    {
        printf("--BYCHUNK\n");
        do {
            pChunk = K2_GET_CONTAINER(K2OS_RAMHEAP_CHUNK, pChunkLink, ChunkListLink);
            printf("%08X CHUNK\n", (UINT32)pChunk);
            pNode = pChunk->mpFirstNodeInChunk;
            if (pNode != NULL)
            {
                do {
                    K2MEM_Copy(sentCode, &pNode->mSentinel, 4);
                    printf("   %08X %s %08X %d\n", (UINT32)pNode, sentCode, pNode->TreeNode.mUserVal, pNode->TreeNode.mUserVal);
                    if (pNode == pChunk->mpLastNodeInChunk)
                        break;
                    pNode = K2_GET_CONTAINER(K2OS_RAMHEAP_NODE, pNode->AddrListLink.mpNext, AddrListLink);
                } while (TRUE);
            }
            pChunkLink = pChunkLink->mpNext;
        } while (pChunkLink != NULL);
    }

    pLink = apHeap->AddrList.mpHead;
    if (pLink != NULL)
    {
        printf("--LINEAR\n");
        do {
            pNode = K2_GET_CONTAINER(K2OS_RAMHEAP_NODE, pLink, AddrListLink);
            K2MEM_Copy(sentCode, &pNode->mSentinel, 4);
            printf("%08X %s %08X %d\n", (UINT32)pNode, sentCode, pNode->TreeNode.mUserVal, pNode->TreeNode.mUserVal);

            pLink = pLink->mpNext;
        } while (pLink != NULL);
    }

    printf("------------------------------\n");
}

#endif

static
void
sCheckLockedHeap(
    K2OS_RAMHEAP *apHeap
)
{
    K2LIST_LINK *           pLink;
    K2OS_RAMHEAP_CHUNK *    pChunk;
    K2OS_RAMHEAP_NODE *     pNode;
    K2LIST_LINK *           pNextLink;
    K2OS_RAMHEAP_CHUNK *    pNextChunk;
    K2OS_RAMHEAP_NODE *     pNextNode;
    UINT32                  checkOverhead;
    UINT32                  checkAllocBytes;
    UINT32                  checkFreeBytes;
    UINT32                  checkAllocCount;
    UINT32                  checkFreeCount;
    K2TREE_NODE *           pNextTreeNode;

    pLink = apHeap->AddrList.mpHead;
    if (pLink == NULL)
    {
        K2_ASSERT(apHeap->AddrList.mpTail == NULL);
        K2_ASSERT(K2TREE_IsEmpty(&apHeap->UsedTree));
        K2_ASSERT(K2TREE_IsEmpty(&apHeap->FreeTree));
        K2_ASSERT(apHeap->HeapState.mAllocCount == 0);
        K2_ASSERT(apHeap->HeapState.mTotalAlloc == 0);
        K2_ASSERT(apHeap->HeapState.mTotalFree == 0);
        K2_ASSERT(apHeap->HeapState.mTotalOverhead == 0);
        return;
    }

    K2_ASSERT(apHeap->HeapState.mAllocCount > 0);
    K2_ASSERT(apHeap->HeapState.mTotalAlloc > 0);
    K2_ASSERT(!K2TREE_IsEmpty(&apHeap->UsedTree));
    if (apHeap->HeapState.mTotalFree > 0)
    {
        K2_ASSERT(!K2TREE_IsEmpty(&apHeap->FreeTree));
    }

    pChunk = sScanForAddrChunk(apHeap, (UINT32)pLink);
    K2_ASSERT(pChunk != NULL);
    K2_ASSERT(pChunk->mStart == (UINT32)pChunk);
    K2_ASSERT(pChunk->ChunkListLink.mpPrev == NULL);

    pNode = K2_GET_CONTAINER(K2OS_RAMHEAP_NODE, pLink, AddrListLink);
    K2_ASSERT(pChunk->mpFirstNodeInChunk == pNode);
    K2_ASSERT(sCheckHdrSent(pNode, (pNode->mSentinel == K2OS_RAMHEAP_NODE_HDRSENT_FREE)));
    K2_ASSERT(sCheckEndSent(pNode));

    if (pNode->mSentinel == K2OS_RAMHEAP_NODE_HDRSENT_USED)
    {
        K2_ASSERT(K2TREE_FirstNode(&apHeap->UsedTree) == &pNode->TreeNode);
        pNextTreeNode = K2TREE_NextNode(&apHeap->UsedTree, &pNode->TreeNode);
    }
    else
    {
        pNextTreeNode = K2TREE_FirstNode(&apHeap->UsedTree);
    }

    checkOverhead = sizeof(K2OS_RAMHEAP_CHUNK);
    checkAllocCount = 0;
    checkFreeCount = 0;
    checkAllocBytes = 0;
    checkFreeBytes = 0;

    //
    // pLink = first link in all blocks
    // pChunk = first chunk in all chunks
    // pNode = first node in all nodes
    //
    do {
        checkOverhead += K2OS_RAMHEAP_NODE_OVERHEAD;
        if (pNode->mSentinel == K2OS_RAMHEAP_NODE_HDRSENT_USED)
        {
            checkAllocCount++;
            checkAllocBytes += pNode->TreeNode.mUserVal;
        }
        else
        {
            checkFreeCount++;
            checkFreeBytes += pNode->TreeNode.mUserVal;
        }

        pNextLink = pLink->mpNext;
        if (pNextLink == NULL)
        {
            //
            // should be last node in heap
            //

            //
            // last node in chunk
            //
            K2_ASSERT(pNode == pChunk->mpLastNodeInChunk);

            //
            // last chunk in heap
            //
            K2_ASSERT(pChunk->ChunkListLink.mpNext == NULL);

            //
            // last node in tree
            //
            K2_ASSERT(pNextTreeNode == NULL);
            if (pNode->mSentinel == K2OS_RAMHEAP_NODE_HDRSENT_USED)
            {
                K2_ASSERT(K2TREE_LastNode(&apHeap->UsedTree) == &pNode->TreeNode);
            }

            break;
        }

        //
        // next node should not be last node in heap
        //
        if (pChunk->mpLastNodeInChunk == pNode)
        {
            K2_ASSERT(pChunk->ChunkListLink.mpNext != NULL);

            pNextChunk = K2_GET_CONTAINER(K2OS_RAMHEAP_CHUNK, pChunk->ChunkListLink.mpNext, ChunkListLink);
            K2_ASSERT(pNextChunk->ChunkListLink.mpPrev = &pChunk->ChunkListLink);
            checkOverhead += sizeof(K2OS_RAMHEAP_CHUNK);

            K2_ASSERT(pNextLink == &pNextChunk->mpFirstNodeInChunk->AddrListLink);
            K2_ASSERT(pNextChunk->mpFirstNodeInChunk->AddrListLink.mpPrev == pLink);
        }
        else
            pNextChunk = pChunk;

        K2_ASSERT(sScanForAddrChunk(apHeap, (UINT32)pNextLink) == pNextChunk);

        pNextNode = K2_GET_CONTAINER(K2OS_RAMHEAP_NODE, pNextLink, AddrListLink);

        K2_ASSERT(sCheckHdrSent(pNextNode, pNextNode->mSentinel == K2OS_RAMHEAP_NODE_HDRSENT_FREE));
        K2_ASSERT(sCheckEndSent(pNextNode));

        if (pChunk == pNextChunk)
        {
            K2_ASSERT((((UINT32)pNode) + pNode->TreeNode.mUserVal + K2OS_RAMHEAP_NODE_OVERHEAD) == (UINT32)pNextNode);
        }

        if (pNextNode->mSentinel == K2OS_RAMHEAP_NODE_HDRSENT_USED)
        {
            K2_ASSERT(pNextTreeNode == &pNextNode->TreeNode);
            pNextTreeNode = K2TREE_NextNode(&apHeap->UsedTree, &pNextNode->TreeNode);
        }
        else
        {

        }

        pLink = pNextLink;
        pNode = pNextNode;
        pChunk = pNextChunk;

    } while (1);

    K2_ASSERT(checkAllocBytes == apHeap->HeapState.mTotalAlloc);
    K2_ASSERT(checkFreeBytes == apHeap->HeapState.mTotalFree);
    K2_ASSERT(checkAllocCount == apHeap->UsedTree.mNodeCount);
    K2_ASSERT(checkAllocCount == apHeap->HeapState.mAllocCount);
    K2_ASSERT(checkFreeCount == apHeap->FreeTree.mNodeCount);
    K2_ASSERT(checkOverhead == apHeap->HeapState.mTotalOverhead);
}

static
int
sAddrCompare(
    UINT32          aKey,
    K2TREE_NODE *   apTreeNode
)
{
    //
    // key is address of first byte in a block
    //
    K2_ASSERT(aKey >= sizeof(K2OS_RAMHEAP_NODE));
    return (int)((aKey - sizeof(K2OS_RAMHEAP_NODE)) - ((UINT32)K2_GET_CONTAINER(K2OS_RAMHEAP_NODE, apTreeNode, TreeNode)));
}

static
int
sSizeCompare(
    UINT32          aKey,
    K2TREE_NODE *   apTreeNode
)
{
    return (int)(aKey - apTreeNode->mUserVal);
}

void
K2OS_RAMHEAP_Init(
    K2OS_RAMHEAP *          apHeap,
    pf_K2OS_RAMHEAP_Lock    LockFunc,
    pf_K2OS_RAMHEAP_Unlock  UnlockFunc
)
{
    K2MEM_Zero(apHeap, sizeof(K2OS_RAMHEAP));
    K2LIST_Init(&apHeap->ChunkList);
    K2LIST_Init(&apHeap->AddrList);
    K2TREE_Init(&apHeap->UsedTree, sAddrCompare);
    K2TREE_Init(&apHeap->FreeTree, sSizeCompare);
    apHeap->fLock = LockFunc;
    apHeap->fUnlock = UnlockFunc;
    apHeap->mLockDisp = 0;
}

static 
void
sAllocFromNode(
    K2OS_RAMHEAP *      apHeap,
    K2OS_RAMHEAP_NODE * apAllocFromNode,
    UINT32              aByteCount4,
    UINT32 *            apRetAllocAddr
)
{
    UINT32              nodeFreeSpace;
    UINT32 *            pEnd;
    K2OS_RAMHEAP_NODE * pNewNode;
    K2OS_RAMHEAP_CHUNK *pChunk;
    UINT32              key;

    K2_ASSERT(sCheckHdrSent(apAllocFromNode, TRUE));
    K2_ASSERT(sCheckEndSent(apAllocFromNode));

    K2_ASSERT(aByteCount4 > 0);
    K2_ASSERT((aByteCount4 & 3) == 0);
    nodeFreeSpace = apAllocFromNode->TreeNode.mUserVal;
    K2_ASSERT(nodeFreeSpace >= aByteCount4);

    pChunk = sScanForAddrChunk(apHeap, (UINT32)apAllocFromNode);
    K2_ASSERT(pChunk != NULL);

    K2TREE_Remove(&apHeap->FreeTree, &apAllocFromNode->TreeNode);

    if ((nodeFreeSpace - aByteCount4) >= (K2OS_RAMHEAP_NODE_OVERHEAD + 4))
    {
        //
        // cut the node into two and create a new free node
        //
        pEnd = (UINT32 *)(((UINT8 *)apAllocFromNode) + sizeof(K2OS_RAMHEAP_NODE) + aByteCount4);
        *pEnd = K2OS_RAMHEAP_NODE_ENDSENT;
        pNewNode = (K2OS_RAMHEAP_NODE *)(pEnd + 1);
        if (apAllocFromNode == pChunk->mpLastNodeInChunk)
            pChunk->mpLastNodeInChunk = pNewNode;
        pNewNode->mSentinel = K2OS_RAMHEAP_NODE_HDRSENT_FREE;
        pNewNode->TreeNode.mUserVal = nodeFreeSpace - (K2OS_RAMHEAP_NODE_OVERHEAD + aByteCount4);
        apAllocFromNode->TreeNode.mUserVal = aByteCount4;
        K2LIST_AddAfter(&apHeap->AddrList, &pNewNode->AddrListLink, &apAllocFromNode->AddrListLink);
        K2TREE_Insert(&apHeap->FreeTree, pNewNode->TreeNode.mUserVal, &pNewNode->TreeNode);
        pEnd = (UINT32 *)(((UINT8 *)pNewNode) + sizeof(K2OS_RAMHEAP_NODE) + pNewNode->TreeNode.mUserVal);
        K2_ASSERT(sCheckEndSent(pNewNode));
        apHeap->HeapState.mTotalOverhead += K2OS_RAMHEAP_NODE_OVERHEAD;
        apHeap->HeapState.mTotalFree -= aByteCount4 + K2OS_RAMHEAP_NODE_OVERHEAD;
    }
    else
    {
        //
        // else we take the whole node - whatever is left is not big enough for another node
        //
        apHeap->HeapState.mTotalFree -= apAllocFromNode->TreeNode.mUserVal;
    }

    apAllocFromNode->mSentinel = K2OS_RAMHEAP_NODE_HDRSENT_USED;

    key = ((UINT32)apAllocFromNode) + sizeof(K2OS_RAMHEAP_NODE);

    K2TREE_Insert(&apHeap->UsedTree, key, &apAllocFromNode->TreeNode);

    *apRetAllocAddr = key;

    apHeap->HeapState.mAllocCount++;
    apHeap->HeapState.mTotalAlloc += apAllocFromNode->TreeNode.mUserVal;
}

static
K2STAT
sPushBreak(
    K2OS_RAMHEAP *          apHeap,
    K2OS_RAMHEAP_CHUNK *    apChunk,
    UINT32                  aPushAmount,
    K2OS_RAMHEAP_NODE **    apRetEndNode
)
{
    //
    // find the last node in this chunk 
    //
    K2OS_RAMHEAP_NODE * pNode;
    K2OS_RAMHEAP_NODE * pNewNode;
    UINT32 *            pEnd;
        
    //
    // push the break in the node
    //
    if (!K2OS_VirtPagesCommit(apChunk->mBreak, aPushAmount, K2OS_PAGEATTR_READWRITE))
    {
        //
        // should never happen in K2OS because of pool fill on initial alloc
        //
        return K2OS_ThreadGetStatus();
    }

    //
    // may not be used but we set it here
    //
    pNewNode = (K2OS_RAMHEAP_NODE *)apChunk->mBreak;

    //
    // break has moved
    //
    apChunk->mBreak += aPushAmount;

    //
    // now expand the chunk by adding the free space
    //
    pNode = apChunk->mpLastNodeInChunk;
    if (pNode->mSentinel == K2OS_RAMHEAP_NODE_HDRSENT_FREE)
    {
        //
        // last node in chunk is free and will be the end node returned (no new node)
        //
        K2TREE_Remove(&apHeap->FreeTree, &pNode->TreeNode);
        pEnd = (UINT32 *)(((UINT8 *)pNode) + sizeof(K2OS_RAMHEAP_NODE) + pNode->TreeNode.mUserVal);
        K2_ASSERT(*pEnd == K2OS_RAMHEAP_NODE_ENDSENT);
        *pEnd = 0;
        pNode->TreeNode.mUserVal += aPushAmount;
        apHeap->HeapState.mTotalFree += aPushAmount;
    }
    else
    {
        //
        // last node in chunk is used and a new node will be the end node returned
        //
        pNewNode->mSentinel = K2OS_RAMHEAP_NODE_HDRSENT_FREE;
        pNewNode->TreeNode.mUserVal = aPushAmount - K2OS_RAMHEAP_NODE_OVERHEAD;
        K2LIST_AddAfter(&apHeap->AddrList, &pNewNode->AddrListLink, &pNode->AddrListLink);
        apChunk->mpLastNodeInChunk = pNewNode;
        pNode = pNewNode;
        apHeap->HeapState.mTotalFree += pNewNode->TreeNode.mUserVal;
        apHeap->HeapState.mTotalOverhead += K2OS_RAMHEAP_NODE_OVERHEAD;
    }

    *apRetEndNode = pNode;

    pEnd = (UINT32 *)(((UINT8 *)pNode) + sizeof(K2OS_RAMHEAP_NODE) + pNode->TreeNode.mUserVal);
    *pEnd = K2OS_RAMHEAP_NODE_ENDSENT;
    pEnd++;

    K2_ASSERT(pEnd == ((UINT32*)apChunk->mBreak));

    K2TREE_Insert(&apHeap->FreeTree, pNode->TreeNode.mUserVal, &pNode->TreeNode);

    return K2STAT_OK;
}

static
K2STAT
sGrowHeapChunk(
    K2OS_RAMHEAP *      apHeap,
    K2OS_RAMHEAP_CHUNK *apChunk,
    UINT32              aByteCount4,
    UINT32 *            apRetAllocAddr
)
{
    K2STAT              stat;
    UINT32              spaceLeft;
    UINT32              breakPushAmount;
    K2OS_RAMHEAP_NODE * pNewEndNode;

    spaceLeft = apChunk->mTop - apChunk->mBreak;

    breakPushAmount = K2_ROUNDUP(aByteCount4 + K2OS_RAMHEAP_NODE_OVERHEAD, K2_VA32_MEMPAGE_BYTES);

    K2_ASSERT(spaceLeft >= breakPushAmount); // caller will have validated this before call

    stat = sPushBreak(apHeap, apChunk, breakPushAmount, &pNewEndNode);

    if (K2STAT_IS_ERROR(stat))
        return stat;

    K2_ASSERT(sCheckHdrSent(pNewEndNode, TRUE));
    K2_ASSERT(sCheckEndSent(pNewEndNode));
    K2_ASSERT(pNewEndNode->TreeNode.mUserVal >= aByteCount4);

    sAllocFromNode(apHeap, pNewEndNode, aByteCount4, apRetAllocAddr);

    return K2STAT_NO_ERROR;
}

static
void
iAddAndAllocFromChunk(
    K2OS_RAMHEAP *  apHeap,
    UINT32          aChunkBase,
    UINT32          aChunkInit,
    UINT32          aChunkFull,
    UINT32          aByteCount4,
    UINT32 *        apRetAllocAddr
)
{
    K2OS_RAMHEAP_CHUNK *    pChunk;
    K2OS_RAMHEAP_CHUNK *    pChunkPrev;
    K2OS_RAMHEAP_CHUNK *    pChunkNext;
    K2OS_RAMHEAP_NODE *     pNode;
    K2LIST_LINK *           pScan;
    K2OS_RAMHEAP_CHUNK *    pScanChunk;
    UINT32 *                pEnd;

    //
    // find prev and next node to new chunk
    //
    pScan = apHeap->ChunkList.mpHead;
    if (pScan != NULL)
    {
        pChunkPrev = NULL;
        do {
            pScanChunk = K2_GET_CONTAINER(K2OS_RAMHEAP_CHUNK, pScan, ChunkListLink);
            if (pScanChunk->mStart > aChunkBase)
                break;
            pChunkPrev = pScanChunk;
            pScan = pScan->mpNext;
        } while (pScan != NULL);
        if (pScan != NULL)
            pChunkNext = pScanChunk;
        else
            pChunkNext = NULL;
    }
    else
    {
        pChunkPrev = NULL;
        pChunkNext = NULL;
    }
    if ((pChunkPrev != NULL) && (pChunkNext != NULL))
    {
        K2_ASSERT(pChunkPrev->mpLastNodeInChunk->AddrListLink.mpNext == &pChunkNext->mpFirstNodeInChunk->AddrListLink);
        K2_ASSERT(pChunkNext->mpFirstNodeInChunk->AddrListLink.mpPrev == &pChunkPrev->mpLastNodeInChunk->AddrListLink);
    }

    pChunk = (K2OS_RAMHEAP_CHUNK *)aChunkBase;
    pChunk->mStart = aChunkBase;
    pChunk->mBreak = aChunkBase + aChunkInit;
    pChunk->mTop = aChunkBase + aChunkFull;
    pChunk->mpFirstNodeInChunk = (K2OS_RAMHEAP_NODE *)(((UINT8 *)pChunk) + sizeof(K2OS_RAMHEAP_CHUNK));
    pChunk->mpLastNodeInChunk = pChunk->mpFirstNodeInChunk;
    apHeap->HeapState.mTotalOverhead += sizeof(K2OS_RAMHEAP_CHUNK) + K2OS_RAMHEAP_NODE_OVERHEAD;
    pNode = pChunk->mpFirstNodeInChunk;
    pNode->mSentinel = K2OS_RAMHEAP_NODE_HDRSENT_FREE;
    pNode->TreeNode.mUserVal = aChunkInit - (sizeof(K2OS_RAMHEAP_CHUNK) + K2OS_RAMHEAP_NODE_OVERHEAD);
    apHeap->HeapState.mTotalFree += pNode->TreeNode.mUserVal;
    pEnd = (UINT32 *)(((UINT8 *)pNode) + sizeof(K2OS_RAMHEAP_NODE) + pNode->TreeNode.mUserVal);
    *pEnd = K2OS_RAMHEAP_NODE_ENDSENT;
    K2_ASSERT(((UINT32)(pEnd + 1)) == pChunk->mBreak);

    K2TREE_Insert(&apHeap->FreeTree, pNode->TreeNode.mUserVal, &pNode->TreeNode);

    if (pChunkNext == NULL)
    {
        K2LIST_AddAtTail(&apHeap->ChunkList, &pChunk->ChunkListLink);
        K2LIST_AddAtTail(&apHeap->AddrList, &pNode->AddrListLink);
    }
    else
    {
        K2LIST_AddBefore(&apHeap->ChunkList, &pChunk->ChunkListLink, &pChunkNext->ChunkListLink);
        K2LIST_AddBefore(&apHeap->AddrList, &pNode->AddrListLink, &pChunkNext->mpFirstNodeInChunk->AddrListLink);
    }

    sAllocFromNode(apHeap, pNode, aByteCount4, apRetAllocAddr);

    sCheckLockedHeap(apHeap);
}

K2STAT
K2OS_RAMHEAP_AddAndAllocFromChunk(
    K2OS_RAMHEAP *  apHeap,
    UINT32          aChunkBase,
    UINT32          aChunkInit,
    UINT32          aChunkFull,
    UINT32          aByteCount4,
    UINT32 *        apRetAllocAddr
)
{
    UINT32  disp;

    if ((apHeap == NULL) || 
        (aByteCount4 == 0) || 
        (aByteCount4 & 3) || 
        (apRetAllocAddr == NULL) ||
        (aChunkBase & K2_VA32_MEMPAGE_OFFSET_MASK) ||
        (aChunkInit & K2_VA32_MEMPAGE_OFFSET_MASK) ||
        (aChunkFull & K2_VA32_MEMPAGE_OFFSET_MASK) ||
        (aChunkFull < K2OS_RAMHEAP_CHUNK_MIN) ||
        (aChunkInit > aChunkFull) || 
        (aChunkBase + aChunkFull < aChunkBase))
    {
        return K2STAT_ERROR_BAD_ARGUMENT;
    }

    disp = apHeap->fLock(apHeap);

    iAddAndAllocFromChunk(apHeap, aChunkBase, aChunkInit, aChunkFull, aByteCount4, apRetAllocAddr);

    apHeap->fUnlock(apHeap, disp);

    return K2STAT_NO_ERROR;
}

static
K2STAT
sNewHeapChunk(
    K2OS_RAMHEAP *  apHeap,
    UINT32          aByteCount4,
    UINT32 *        apRetAllocAddr
)
{
    UINT32                  heapChunkInit;
    UINT32                  heapChunkBytes;
    UINT32                  chunkBase;
    K2STAT                  stat;

    heapChunkInit = aByteCount4 + K2OS_RAMHEAP_NODE_OVERHEAD + sizeof(K2OS_RAMHEAP_CHUNK);
    heapChunkInit = K2_ROUNDUP(heapChunkInit, K2_VA32_MEMPAGE_BYTES);

    heapChunkBytes = heapChunkInit;
    if (heapChunkBytes < K2OS_RAMHEAP_CHUNK_MIN)
        heapChunkBytes = K2OS_RAMHEAP_CHUNK_MIN;

    chunkBase = 0;
    if (!K2OS_VirtPagesAlloc(&chunkBase, heapChunkBytes / K2_VA32_MEMPAGE_BYTES, 0, K2OS_PAGEATTR_NOACCESS))
    {
        return K2OS_ThreadGetStatus();
    }

    if (!K2OS_VirtPagesCommit(chunkBase, heapChunkInit / K2_VA32_MEMPAGE_BYTES, K2OS_PAGEATTR_READWRITE))
    {
        stat = K2OS_ThreadGetStatus();
        K2OS_VirtPagesFree(chunkBase);
        return stat;
    }

    sCheckLockedHeap(apHeap);

    iAddAndAllocFromChunk(apHeap, chunkBase, heapChunkInit, heapChunkBytes, aByteCount4, apRetAllocAddr);

    sCheckLockedHeap(apHeap);

    return K2STAT_NO_ERROR;
}

static
K2STAT
sGrowHeap(
    K2OS_RAMHEAP *  apHeap,
    UINT32          aByteCount4,
    UINT32 *        apRetAllocAddr
)
{
    K2LIST_LINK *       pLink;
    K2OS_RAMHEAP_CHUNK *pChunk;
    UINT32              space;

    pLink = apHeap->ChunkList.mpHead;
    
    if (pLink != NULL)
    {
        do {
            pChunk = K2_GET_CONTAINER(K2OS_RAMHEAP_CHUNK, pLink, ChunkListLink);
            space = pChunk->mTop - pChunk->mBreak;
            if (space >= (aByteCount4 + K2OS_RAMHEAP_NODE_OVERHEAD))
                break;
            pLink = pChunk->ChunkListLink.mpNext;
        } while (pLink != NULL);

        if (pLink != NULL)
        {
            return sGrowHeapChunk(apHeap, pChunk, aByteCount4, apRetAllocAddr);
        }
    }

    return sNewHeapChunk(apHeap, aByteCount4, apRetAllocAddr);
}

K2STAT 
K2OS_RAMHEAP_Alloc(
    K2OS_RAMHEAP *  apHeap, 
    UINT32          aByteCount, 
    BOOL            aAllowExpansion,
    void **         appRetPtr
)
{
    K2TREE_NODE *   pTreeNode;
    K2STAT          stat;

    *appRetPtr = NULL;

    aByteCount = K2_ROUNDUP(aByteCount, 4);
    if (aByteCount == 0)
        return K2STAT_OK;

    stat = K2STAT_ERROR_UNKNOWN;

    sLockHeap(apHeap);

    pTreeNode = K2TREE_FindOrAfter(&apHeap->FreeTree, aByteCount);
    if (pTreeNode != NULL)
    {
        sAllocFromNode(apHeap, K2_GET_CONTAINER(K2OS_RAMHEAP_NODE, pTreeNode, TreeNode), aByteCount, (UINT32 *)appRetPtr);
        stat = K2STAT_NO_ERROR;
    }
    else if (aAllowExpansion)
    {
        stat = sGrowHeap(apHeap, aByteCount, (UINT32 *)appRetPtr);
    }
    else
    {
        stat = K2STAT_ERROR_OUT_OF_MEMORY;
    }

    sCheckLockedHeap(apHeap);

    sUnlockHeap(apHeap);

    return stat;
}

static
void
sFreeNode(
    K2OS_RAMHEAP *          apHeap,
    K2OS_RAMHEAP_CHUNK *    apChunk,
    K2OS_RAMHEAP_NODE *     apNode
)
{
    K2LIST_LINK *       pLink;
    K2OS_RAMHEAP_NODE * pPrev;
    K2OS_RAMHEAP_NODE * pNext;
    UINT32 *            pEnd;
    UINT32              chunkBytes;
    UINT32              chunkStart;

    K2_ASSERT(apNode->mSentinel == K2OS_RAMHEAP_NODE_HDRSENT_USED);

    //
    // remove apNode from tree and from tracking, but not from addr list
    //
    K2TREE_Remove(&apHeap->UsedTree, &apNode->TreeNode);
    apNode->mSentinel = 0;
    apHeap->HeapState.mAllocCount--;
    apHeap->HeapState.mTotalAlloc -= apNode->TreeNode.mUserVal;
    apHeap->HeapState.mTotalOverhead -= K2OS_RAMHEAP_NODE_OVERHEAD;

    //
    // check for merge with previous node in same chunk
    //
    if (apNode != apChunk->mpFirstNodeInChunk)
    {
        pLink = apNode->AddrListLink.mpPrev;
        pPrev = (pLink != NULL) ? K2_GET_CONTAINER(K2OS_RAMHEAP_NODE, pLink, AddrListLink) : NULL;
        if ((pPrev != NULL) && (pPrev->mSentinel == K2OS_RAMHEAP_NODE_HDRSENT_FREE))
         {
            //
            // clear pPrev end sentinel
            //
            pEnd = (UINT32 *)(((UINT8 *)pPrev) + sizeof(K2OS_RAMHEAP_NODE) + pPrev->TreeNode.mUserVal);
            K2_ASSERT(*pEnd == K2OS_RAMHEAP_NODE_ENDSENT);
            *pEnd = 0;
            pEnd++;
            K2_ASSERT((UINT32)pEnd == (UINT32)apNode);

            //
            // resize pPrev
            //
            K2TREE_Remove(&apHeap->FreeTree, &pPrev->TreeNode);
            pPrev->mSentinel = 0;
            K2LIST_Remove(&apHeap->AddrList, &apNode->AddrListLink);
            apHeap->HeapState.mTotalFree -= pPrev->TreeNode.mUserVal;
            apHeap->HeapState.mTotalOverhead -= K2OS_RAMHEAP_NODE_OVERHEAD;
            pPrev->TreeNode.mUserVal += K2OS_RAMHEAP_NODE_OVERHEAD + apNode->TreeNode.mUserVal;

            if (apNode == apChunk->mpLastNodeInChunk)
                apChunk->mpLastNodeInChunk = pPrev;

            //
            // switch so apNode is now pPrev
            //
            apNode = pPrev;
            sCheckEndSent(apNode);
        }
    }

    //
    // check for merge with next node in same chunk
    //
    if (apNode != apChunk->mpLastNodeInChunk)
    {
        pLink = apNode->AddrListLink.mpNext;
        pNext = (pLink != NULL) ? K2_GET_CONTAINER(K2OS_RAMHEAP_NODE, pLink, AddrListLink) : NULL;
        if ((pNext != NULL) && (pNext->mSentinel == K2OS_RAMHEAP_NODE_HDRSENT_FREE))
        {
            //
            // clear apNode end sentinel
            //
            pEnd = (UINT32 *)(((UINT8 *)apNode) + sizeof(K2OS_RAMHEAP_NODE) + apNode->TreeNode.mUserVal);
            K2_ASSERT(*pEnd == K2OS_RAMHEAP_NODE_ENDSENT);
            *pEnd = 0;
            pEnd++;
            K2_ASSERT((UINT32)pEnd == (UINT32)pNext);

            //
            // resize apNode and remove pNext
            //
            K2TREE_Remove(&apHeap->FreeTree, &pNext->TreeNode);
            pNext->mSentinel = 0;
            K2LIST_Remove(&apHeap->AddrList, &pNext->AddrListLink);
            apHeap->HeapState.mTotalFree -= pNext->TreeNode.mUserVal;
            apHeap->HeapState.mTotalOverhead -= K2OS_RAMHEAP_NODE_OVERHEAD;
            apNode->TreeNode.mUserVal += K2OS_RAMHEAP_NODE_OVERHEAD + pNext->TreeNode.mUserVal;

            if (pNext == apChunk->mpLastNodeInChunk)
                apChunk->mpLastNodeInChunk = apNode;

            sCheckEndSent(apNode);
        }
    }

    //
    // insert node into free tree
    //
    apNode->mSentinel = K2OS_RAMHEAP_NODE_HDRSENT_FREE;
    K2TREE_Insert(&apHeap->FreeTree, apNode->TreeNode.mUserVal, &apNode->TreeNode);
    apHeap->HeapState.mTotalFree += apNode->TreeNode.mUserVal;
    apHeap->HeapState.mTotalOverhead += K2OS_RAMHEAP_NODE_OVERHEAD;

    //
    // now see if entire chunk is free
    //
    if (apChunk->mpFirstNodeInChunk == apChunk->mpLastNodeInChunk)
    {
        K2_ASSERT(apChunk->mpFirstNodeInChunk->mSentinel == K2OS_RAMHEAP_NODE_HDRSENT_FREE);

        //
        // remove entire chunk from heap and release memory back to the system
        //
        chunkStart = apChunk->mStart;
        chunkBytes = apChunk->mBreak - apChunk->mStart;
        pNext = apChunk->mpFirstNodeInChunk;
        apHeap->HeapState.mTotalOverhead -= sizeof(K2OS_RAMHEAP_CHUNK) + K2OS_RAMHEAP_NODE_OVERHEAD;
        apHeap->HeapState.mTotalFree -= pNext->TreeNode.mUserVal;
        K2TREE_Remove(&apHeap->FreeTree, &pNext->TreeNode);
        K2LIST_Remove(&apHeap->AddrList, &pNext->AddrListLink);
        K2LIST_Remove(&apHeap->ChunkList, &apChunk->ChunkListLink);
        K2MEM_Zero(apChunk, sizeof(K2OS_RAMHEAP_CHUNK) + sizeof(K2OS_RAMHEAP_NODE));

        if (!K2OS_VirtPagesDecommit(chunkStart, chunkBytes / K2_VA32_MEMPAGE_BYTES))
        {
            //
            // cant do anything about this except stop if we are in debug config
            //
            K2_ASSERT(0);
        }

        if (!K2OS_VirtPagesFree(chunkStart))
        {
            //
            // cant do anything about this except stop if we are in debug config
            //
            K2_ASSERT(0);
        }
    }
}

K2STAT 
K2OS_RAMHEAP_Free(
    K2OS_RAMHEAP * apHeap,
    void *      aPtr
)
{
    K2TREE_NODE *           pTreeNode;
    K2OS_RAMHEAP_CHUNK *    pChunk;
    K2STAT                  stat;

    sLockHeap(apHeap);

    sCheckLockedHeap(apHeap);

    pTreeNode = K2TREE_Find(&apHeap->UsedTree, (UINT32)aPtr);
    if (pTreeNode != NULL)
    {
        pChunk = sScanForAddrChunk(apHeap, (UINT32)aPtr);
        K2_ASSERT(pChunk != NULL);
        K2MEM_Set(aPtr, 0xFE, pTreeNode->mUserVal);
        sFreeNode(apHeap, pChunk, K2_GET_CONTAINER(K2OS_RAMHEAP_NODE, pTreeNode, TreeNode));
        sCheckLockedHeap(apHeap);
        stat = K2STAT_OK;
    }
    else
        stat = K2STAT_ERROR_NOT_FOUND;

    sUnlockHeap(apHeap);
    
    return stat;
}

K2STAT 
K2OS_RAMHEAP_GetState(
    K2OS_RAMHEAP *          apHeap,
    K2OS_RAMHEAP_STATE *    apRetState,
    UINT32 *                apRetLargestFree
)
{
    K2TREE_NODE *pTreeNode;

    sLockHeap(apHeap);

    if (apRetState != NULL)
    {
        K2MEM_Copy(apRetState, &apHeap->HeapState, sizeof(K2OS_RAMHEAP_STATE));
    }

    if (apRetLargestFree != NULL)
    {
        pTreeNode = K2TREE_LastNode(&apHeap->FreeTree);
        if (pTreeNode == NULL)
            *apRetLargestFree = 0;
        else
            *apRetLargestFree = pTreeNode->mUserVal;
    }

    sUnlockHeap(apHeap);

    return K2STAT_OK;
}

