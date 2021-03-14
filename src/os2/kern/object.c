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

#include "kern.h"

void    
KernObj_Init(
    void
)
{
    K2OSKERN_SeqInit(&gData.ObjTreeSeqLock);

    K2TREE_Init(&gData.ObjTree, NULL);
}

void 
KernObj_Add(
    K2OSKERN_OBJ_HEADER *apObjHdr
)
{
    K2TREE_NODE *   pTreeNode;
    BOOL            disp;

    disp = K2OSKERN_SeqLock(&gData.ObjTreeSeqLock);

    pTreeNode = K2TREE_Find(&gData.ObjTree, (UINT32)apObjHdr);
    K2_ASSERT(NULL == pTreeNode);

    K2TREE_Insert(&gData.ObjTree, (UINT32)apObjHdr, &apObjHdr->ObjTreeNode);

    K2OSKERN_SeqUnlock(&gData.ObjTreeSeqLock, disp);
}

UINT32  
KernObj_AddRef(
    K2OSKERN_OBJ_HEADER *apObjHdr
)
{
    K2TREE_NODE *   pTreeNode;
    UINT32          result;
    BOOL            disp;

    disp = K2OSKERN_SeqLock(&gData.ObjTreeSeqLock);

    pTreeNode = K2TREE_Find(&gData.ObjTree, (UINT32)apObjHdr);
    K2_ASSERT(NULL != pTreeNode);

    result = ++apObjHdr->mRefCount;

    K2OSKERN_SeqUnlock(&gData.ObjTreeSeqLock, disp);

    return result;
}

UINT32
KernObj_Release(
    K2OSKERN_OBJ_HEADER *apObjHdr
)
{
    K2TREE_NODE *   pTreeNode;
    UINT32          result;
    BOOL            disp;

    disp = K2OSKERN_SeqLock(&gData.ObjTreeSeqLock);

    pTreeNode = K2TREE_Find(&gData.ObjTree, (UINT32)apObjHdr);
    K2_ASSERT(NULL != pTreeNode);

    result = --apObjHdr->mRefCount;
    if (0 == result)
    {
        K2TREE_Remove(&gData.ObjTree, &apObjHdr->ObjTreeNode);
    }

    K2OSKERN_SeqUnlock(&gData.ObjTreeSeqLock, disp);

    if (0 == result)
        return 0;

    K2_ASSERT(0 == (apObjHdr->mObjFlags & K2OSKERN_OBJ_FLAG_PERMANENT));

    //
    // clean up and latch object for release to proc1
    //
    if (NULL != apObjHdr->mfCleanup)
        apObjHdr->mfCleanup(apObjHdr);

    if (0 == (K2OSKERN_OBJ_FLAG_EMBEDDED & apObjHdr->mObjFlags))
    {
        KernHeap_Free(apObjHdr);
    }

    return result;
}
