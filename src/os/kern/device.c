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

#define DEVMAP_CHUNK 128

K2STAT
K2OSKERN_MapDevice(
    UINT32      aPhysDeviceAddr,
    UINT32      aPageCount,
    UINT32 *    apRetVirtAddr
)
{
    K2STAT                  stat;
    BOOL                    disp;
    UINT32                  virtAddr;
    K2OSKERN_OBJ_SEGMENT *  pSeg;
    K2OSKERN_OBJ_THREAD *   pCurThread;
    UINT32                  chunkLeft;

    if (gData.mKernInitStage < KernInitStage_MemReady)
        return K2STAT_ERROR_API_ORDER;

    pCurThread = K2OSKERN_CURRENT_THREAD;

    pSeg = NULL;
    stat = KernMem_SegAllocToThread(pCurThread);
    if (K2STAT_IS_ERROR(stat))
    {
        K2OS_ThreadSetStatus(stat);
        return stat;
    }

    pSeg = pCurThread->mpWorkSeg;

    K2_ASSERT(pSeg != NULL);

    do {

        stat = KernMem_VirtAllocToThread(pCurThread, 0, aPageCount, FALSE);
        if (K2STAT_IS_ERROR(stat))
            break;

        K2MEM_Zero(pSeg, sizeof(K2OSKERN_OBJ_SEGMENT));
        pSeg->Hdr.mObjType = K2OS_Obj_Segment;
        pSeg->Hdr.mRefCount = 1;
        K2LIST_Init(&pSeg->Hdr.WaitEntryPrioList);
        pSeg->mSegAndMemPageAttr = K2OSKERN_SEG_ATTR_TYPE_DEVMAP | K2OS_MAPTYPE_KERN_DEVICEIO;
        pSeg->Info.DeviceMap.mPhysDeviceAddr = aPhysDeviceAddr;

        K2_ASSERT(pCurThread->WorkPages_Dirty.mNodeCount == 0);
        K2_ASSERT(pCurThread->WorkPages_Clean.mNodeCount == 0);

        stat = KernMem_CreateSegmentFromThread(pCurThread, pSeg, NULL);
        if (!K2STAT_IS_ERROR(stat))
        {
            K2_ASSERT(pCurThread->mpWorkSeg == NULL);

            *apRetVirtAddr = pSeg->ProcSegTreeNode.mUserVal;
            K2_ASSERT(pCurThread->mWorkVirt_Range == 0);
            K2_ASSERT(pCurThread->mWorkVirt_PageCount == 0);

            //
            // map the device pages to the virtual range just created
            //
            chunkLeft = DEVMAP_CHUNK;
            virtAddr = pSeg->ProcSegTreeNode.mUserVal;

            disp = K2OSKERN_SeqIntrLock(&gData.KernVirtMapLock);

            do {
                KernMap_MakeOnePresentPage(
                    K2OS_KVA_KERNVAMAP_BASE,
                    virtAddr,
                    aPhysDeviceAddr,
                    K2OS_MAPTYPE_KERN_DEVICEIO);
                virtAddr += K2_VA32_MEMPAGE_BYTES;
                aPhysDeviceAddr += K2_VA32_MEMPAGE_BYTES;

                if (--chunkLeft == 0)
                {
                    if (aPageCount > 1)
                    {
                        K2OSKERN_SeqIntrUnlock(&gData.KernVirtMapLock, disp);
                        disp = K2OSKERN_SeqIntrLock(&gData.KernVirtMapLock);
                        chunkLeft = DEVMAP_CHUNK;
                    }
                }

            } while (--aPageCount);

            K2OSKERN_SeqIntrUnlock(&gData.KernVirtMapLock, disp);
        }
        else
        {
            KernMem_VirtFreeFromThread(pCurThread);
        }

    } while (0);

    if (K2STAT_IS_ERROR(stat))
    {
        KernMem_SegFreeFromThread(pCurThread);
        K2OS_ThreadSetStatus(stat);
    }

    return stat;
}

K2STAT
K2OSKERN_UnmapDevice(
    UINT32  aVirtDeviceAddr
)
{
    K2OSKERN_OBJ_SEGMENT *  pSeg;
    K2TREE_NODE *           pTreeNode;
    BOOL                    disp;

    if (gData.mKernInitStage < KernInitStage_MemReady)
        return K2STAT_ERROR_API_ORDER;

    //
    // find the segment containing the address
    //

    disp = K2OSKERN_SeqIntrLock(&gpProc0->SegTreeSeqLock);

    pTreeNode = K2TREE_Find(&gpProc0->SegTree, aVirtDeviceAddr);
    if (pTreeNode != NULL)
    {
        pSeg = K2_GET_CONTAINER(K2OSKERN_OBJ_SEGMENT, pTreeNode, ProcSegTreeNode);
        KernObj_AddRef(&pSeg->Hdr);
    }
    else
        pSeg = NULL;

    K2OSKERN_SeqIntrUnlock(&gpProc0->SegTreeSeqLock, disp);

    if (pSeg == NULL)
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_FOUND);
        return K2STAT_ERROR_NOT_FOUND;
    }

    if ((pSeg->mSegAndMemPageAttr & K2OSKERN_SEG_ATTR_TYPE_MASK) != K2OSKERN_SEG_ATTR_TYPE_DEVMAP)
    {
        KernObj_Release(&pSeg->Hdr);
        K2OS_ThreadSetStatus(K2STAT_ERROR_BAD_ARGUMENT);
        return K2STAT_ERROR_BAD_ARGUMENT;
    }

    //
    // double-release the segment
    //
    KernObj_Release(&pSeg->Hdr);    // caller reference
    KernObj_Release(&pSeg->Hdr);    // local reference

    return K2STAT_NO_ERROR;
}

