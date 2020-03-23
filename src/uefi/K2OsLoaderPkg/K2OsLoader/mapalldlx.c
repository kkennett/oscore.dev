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
#include "K2OsLoader.h"

static UINTN const sgSegToMapType[DlxSeg_Reloc] =
{
    0,                        // DlxSeg_Info
    K2OS_MAPTYPE_KERN_TEXT,   // DlxSeg_Text
    K2OS_MAPTYPE_KERN_READ,   // DlxSeg_Read
    K2OS_MAPTYPE_KERN_DATA,   // DlxSeg_Data
    K2OS_MAPTYPE_KERN_READ    // DlxSeg_Sym
};

static
K2STAT
sMapOneFile(
    EFIFILE *apFile
    )
{
    K2STAT      status;
    UINTN       segIx;
    UINTN       segBytes;
    UINT32      virtAddr;
    UINT32      virtEnd;
    UINT32      physAddr;
    EFIDLX *    pDlx;

    // node page contains DLXINFO
    status = K2VMAP32_MapPage(&gData.Map, apFile->mPageAddr_Virt, (UINT32)apFile->mPageAddr_Phys, K2OS_MAPTYPE_KERN_DATA);
    if (K2STAT_IS_ERROR(status))
    {
        K2Printf(L"*** MapPage for node page addr 0x%08X failed\n", apFile->mPageAddr_Virt);
        return status;
    }

    pDlx = apFile->mpDlx;
    for (segIx = DlxSeg_Text; segIx < DlxSeg_Reloc; segIx++)
    {
        segBytes = K2_ROUNDUP(pDlx->Info.SegInfo[segIx].mMemActualBytes, K2_VA32_MEMPAGE_BYTES);
        if (segBytes > 0)
        {
            virtAddr = pDlx->SegAlloc.Segment[segIx].mLinkAddr;
            if (virtAddr != 0)
            {
                physAddr = pDlx->SegAlloc.Segment[segIx].mDataAddr;
                K2_ASSERT(physAddr != 0);
                virtEnd = virtAddr + segBytes;
                do
                {
                    status = K2VMAP32_MapPage(&gData.Map, virtAddr, physAddr, sgSegToMapType[segIx]);
                    if (K2STAT_IS_ERROR(status))
                    {
                        K2Printf(L"*** MapPage for dlx %08X seg %d vaddr %08X failed\n", pDlx, segIx, virtAddr);
                        return status;
                    }
                    virtAddr += K2_VA32_MEMPAGE_BYTES;
                    physAddr += K2_VA32_MEMPAGE_BYTES;
                } while (virtAddr != virtEnd);
            }
        }
    }

    return 0;
}

K2STAT 
Loader_MapAllDLX(
    void
    )
{
    K2LIST_LINK * pLink;
    K2STAT status;

    pLink = gData.EfiFileList.mpHead;
    while (pLink != NULL)
    {
        status = sMapOneFile(K2_GET_CONTAINER(EFIFILE, pLink, ListLink));
        if (K2STAT_IS_ERROR(status))
            return status;
        pLink = pLink->mpNext;
    }
    return 0;
}
