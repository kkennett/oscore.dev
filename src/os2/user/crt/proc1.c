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
#include "crt.h"

typedef union _CrtHeapTrack CrtHeapTrack;
union _CrtHeapTrack
{
    K2LIST_LINK     ListLink;
    K2HEAP_NODE     HeapNode;
};

#define INIT_TRACK_ITEMS    32

static CRT_INIT_INFO    sgInitInfo;
static K2HEAP_ANCHOR    sgKernHeap;
static K2LIST_ANCHOR    sgTrackList;
static CrtHeapTrack     sgInitTrack[INIT_TRACK_ITEMS];

static K2HEAP_NODE *   
sgInitAcqNode(
    K2HEAP_ANCHOR * apHeap
)
{
    CrtHeapTrack *pTrack;

    K2_ASSERT(apHeap == &sgKernHeap);
    if (0 == sgTrackList.mNodeCount)
        return NULL;

    pTrack = (CrtHeapTrack *)sgTrackList.mpHead;
    K2LIST_Remove(&sgTrackList, &pTrack->ListLink);
    
    return &pTrack->HeapNode;
}

static void 
sgInitRelNode(
    K2HEAP_ANCHOR * apHeap,
    K2HEAP_NODE *   apNode
)
{
    K2_ASSERT(apHeap == &sgKernHeap);
    K2LIST_AddAtTail(&sgTrackList, &((CrtHeapTrack *)apNode)->ListLink);
}

void   
CrtKern_InitProc1(
    void
)
{
    BOOL            ok;
    UINT32          ix;
    K2STAT          stat;
    K2TREE_NODE *   pTreeNode;
    K2HEAP_NODE *   pHeapNode;

    ok = CrtKern_SysCall1(K2OS_SYSCALL_ID_CRT_GET_INFO, (UINT32)&sgInitInfo);
    K2_ASSERT(ok);

    K2_ASSERT(sgInitInfo.mKernVirtBot <= sgInitInfo.mKernVirtBotPt);
    K2_ASSERT(sgInitInfo.mKernVirtTop >= sgInitInfo.mKernVirtTopPt);

    //
    // init initial heap tracking
    //
    K2LIST_Init(&sgTrackList);
    for (ix = 0; ix < INIT_TRACK_ITEMS; ix++)
    {
        K2LIST_AddAtTail(&sgTrackList, &sgInitTrack[ix].ListLink);
    }

    //
    // start proc1 heap
    //
    K2HEAP_Init(&sgKernHeap, sgInitAcqNode, sgInitRelNode);
    if (sgInitInfo.mKernVirtBot != sgInitInfo.mKernVirtBotPt)
    {
        stat = K2HEAP_AddFreeSpaceNode(&sgKernHeap, sgInitInfo.mKernVirtBot, sgInitInfo.mKernVirtBotPt - sgInitInfo.mKernVirtBot, NULL);
        K2_ASSERT(!K2STAT_IS_ERROR(stat));
    }

    if (sgInitInfo.mKernVirtTop != sgInitInfo.mKernVirtTopPt)
    {
        stat = K2HEAP_AddFreeSpaceNode(&sgKernHeap, sgInitInfo.mKernVirtTopPt, sgInitInfo.mKernVirtTop - sgInitInfo.mKernVirtTopPt, NULL);
        K2_ASSERT(!K2STAT_IS_ERROR(stat));
    }

    pTreeNode = K2TREE_FirstNode(&sgKernHeap.SizeTree);
    if (NULL == pTreeNode)
    {
        CrtDbg_Printf("Kernel virt Heap empty at startup\n");
    }
    else
    {
        CrtDbg_Printf("Kernel Virt heap at startup:\n----------\n");
        do
        {
            pHeapNode = K2_GET_CONTAINER(K2HEAP_NODE, pTreeNode, SizeTreeNode);
            CrtDbg_Printf("Size %08X Addr %08X\n", pHeapNode->SizeTreeNode.mUserVal, pHeapNode->AddrTreeNode.mUserVal);
            pTreeNode = K2TREE_NextNode(&sgKernHeap.SizeTree, pTreeNode);
        } while (pTreeNode != NULL);
        CrtDbg_Printf("----------\n");
    }
}
