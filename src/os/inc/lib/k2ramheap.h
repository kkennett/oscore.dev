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

#ifndef __K2RAMHEAP_H
#define __K2RAMHEAP_H

/* --------------------------------------------------------------------------------- */

#include <k2os.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _K2OS_RAMHEAP_STATE K2OS_RAMHEAP_STATE;
struct _K2OS_RAMHEAP_STATE
{
    UINT32  mAllocCount;
    UINT32  mTotalAlloc;
    UINT32  mTotalFree;
    UINT32  mTotalOverhead;
};

#define K2OS_RAMHEAP_CHUNK_MIN (64*1024)
K2_STATIC_ASSERT((K2OS_RAMHEAP_CHUNK_MIN % K2_VA32_MEMPAGE_BYTES) == 0);

#define K2OS_RAMHEAP_NODE_HDRSENT_FREE  K2_MAKEID4('F','R','E','E')
#define K2OS_RAMHEAP_NODE_HDRSENT_USED  K2_MAKEID4('U','S','E','D')
#define K2OS_RAMHEAP_NODE_ENDSENT       K2_MAKEID4('S','E','N','T')

typedef struct _K2OS_RAMHEAP_NODE K2OS_RAMHEAP_NODE;
struct _K2OS_RAMHEAP_NODE
{
    UINT32      mSentinel;      // K2OS_RAMHEAP_NODE_HDRSENT_xxx
    K2LIST_LINK AddrListLink;   // Always valid. Block on list sorted by address
    K2TREE_NODE TreeNode;       // Always valid. Free = Size Tree, Used = Address Tree
};

#define K2OS_RAMHEAP_NODE_OVERHEAD  (sizeof(K2OS_RAMHEAP_NODE) + sizeof(UINT32))

K2_STATIC_ASSERT(32 == K2OS_RAMHEAP_NODE_OVERHEAD);

typedef struct _K2OS_RAMHEAP_CHUNK K2OS_RAMHEAP_CHUNK;
struct _K2OS_RAMHEAP_CHUNK
{
    UINT32              mStart;
    UINT32              mBreak;
    UINT32              mTop;
    K2OS_RAMHEAP_NODE * mpFirstNodeInChunk;
    K2OS_RAMHEAP_NODE * mpLastNodeInChunk;
    K2LIST_LINK         ChunkListLink;
};

typedef struct _K2OS_RAMHEAP K2OS_RAMHEAP;

typedef UINT32  (*pf_K2OS_RAMHEAP_Lock)(K2OS_RAMHEAP *apRamHeap);
typedef void    (*pf_K2OS_RAMHEAP_Unlock)(K2OS_RAMHEAP *apRamHeap, UINT32 aDisp);

struct _K2OS_RAMHEAP
{
    K2LIST_ANCHOR           ChunkList;          // chunk list
    K2LIST_ANCHOR           AddrList;           // list of K2OS_RAMHEAP_NODE.AddrListLink
    K2TREE_ANCHOR           UsedTree;           // key is effective address (&K2OS_RAMHEAP_NODE + sizeof(K2OS_RAMHEAP_NODE))
    K2TREE_ANCHOR           FreeTree;           // key is size (K2OS_RAMHEAP_NODE.TreeNode.mUserVal);
    K2OS_RAMHEAP_STATE      HeapState;
    pf_K2OS_RAMHEAP_Lock    fLock;
    pf_K2OS_RAMHEAP_Unlock  fUnlock;
    UINT32                  mLockDisp;
};

void
K2OS_RAMHEAP_Init(
    K2OS_RAMHEAP *          apHeap,
    pf_K2OS_RAMHEAP_Lock    aLockFunc,
    pf_K2OS_RAMHEAP_Unlock  aUnlockFunc
);

K2STAT
K2OS_RAMHEAP_Alloc(
    K2OS_RAMHEAP *  apHeap,
    UINT32          aByteCount,
    BOOL            aAllowExpansion,
    void **         appRetPtr
);

K2STAT
K2OS_RAMHEAP_Free(
    K2OS_RAMHEAP *  apHeap,
    void *          aPtr
);

K2STAT
K2OS_RAMHEAP_GetState(
    K2OS_RAMHEAP *          apHeap,
    K2OS_RAMHEAP_STATE *    apRetState,
    UINT32 *                apRetLargestFree
);

#ifdef __cplusplus
};  // extern "C"
#endif

/* --------------------------------------------------------------------------------- */

#endif // __K2RAMHEAP_H
