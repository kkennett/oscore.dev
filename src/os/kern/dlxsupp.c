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

static UINT32 sSegToAttr(UINT32 aIndex)
{
    if (aIndex == DlxSeg_Text)
        return K2OS_MAPTYPE_KERN_TEXT;
    if ((aIndex == DlxSeg_Read) || (aIndex == DlxSeg_Sym))
        return K2OS_MAPTYPE_KERN_READ;
    return K2OS_MAPTYPE_KERN_DATA;
}

static 
void
sInitBuiltInDlx(
    K2OSKERN_OBJ_DLX *      apDlxObj,
    DLX *                   apDlx,
    UINT32                  aModulePageLinkAddr,
    K2DLXSUPP_HOST_FILE *   apInOutHostFile
)
{
    DLX_SEGMENT_INFO    segInfo[DlxSeg_Count];
    K2STAT              stat;
    UINT32              ix;
    UINT32              pageAddr;

    apDlxObj->mpDlx = apDlx;
    *apInOutHostFile = (K2DLXSUPP_HOST_FILE)apDlxObj;

    stat = gData.mpShared->FuncTab.GetDlxInfo(apDlx, NULL, NULL, segInfo, &pageAddr);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));
    K2_ASSERT(aModulePageLinkAddr == pageAddr);
    for (ix = 0; ix < DlxSeg_Reloc; ix++)
    {
        apDlxObj->SegObj[ix].Info.DlxPart.mSegmentIndex = ix;
        K2MEM_Copy(&apDlxObj->SegObj[ix].Info.DlxPart.DlxSegmentInfo, &segInfo[ix], sizeof(DLX_SEGMENT_INFO));
        if ((segInfo[ix].mMemActualBytes == 0) ||
            (segInfo[ix].mLinkAddr == 0))
        {
            apDlxObj->SegObj[ix].mSegAndMemPageAttr = 0;
        }
        else
        {
            apDlxObj->SegObj[ix].mSegAndMemPageAttr = K2OS_SEG_ATTR_TYPE_DLX_PART | sSegToAttr(ix);
            apDlxObj->SegObj[ix].Info.DlxPart.mpDlxObj = apDlxObj;
        }
    }

    apDlxObj->PageSeg.Hdr.mObjType = K2OS_Obj_Segment;
    apDlxObj->PageSeg.Hdr.mObjFlags = K2OSKERN_OBJ_FLAG_PERMANENT;
    apDlxObj->PageSeg.Hdr.mRefCount = 0x7FFFFFFF;
    apDlxObj->PageSeg.mpProc = gpProc0;
    apDlxObj->PageSeg.ProcSegTreeNode.mUserVal = pageAddr;
    apDlxObj->PageSeg.mPagesBytes = K2_VA32_MEMPAGE_BYTES;
    apDlxObj->PageSeg.mSegAndMemPageAttr = K2OS_MAPTYPE_KERN_DATA | K2OS_SEG_ATTR_TYPE_DLX_PAGE;
    apDlxObj->PageSeg.Info.DlxPage.mpDlxObj = apDlxObj;
}

void KernInit_DlxHost(void)
{
    //
    // actually happens AFTER "_AtReInit" callback has occurred
    //

    //
    // should put built-in DLX onto the list of loaded DLX here
    //
}

K2STAT KernDlx_CritSec(BOOL aEnter)
{
    return K2STAT_OK;
}

K2STAT KernDlx_Open(char const * apDlxName, UINT32 aDlxNameLen, K2DLXSUPP_OPENRESULT *apRetResult)
{
    return K2STAT_ERROR_NOT_IMPL;
}

void KernDlx_AtReInit(DLX *apDlx, UINT32 aModulePageLinkAddr, K2DLXSUPP_HOST_FILE *apInOutHostFile)
{
    //
    // this happens right after dlx_entry for kernel has completed
    //
    if (apDlx == gData.mpShared->LoadInfo.mpDlxCrt)
    {
        sInitBuiltInDlx(&gData.DlxCrt, apDlx, aModulePageLinkAddr, apInOutHostFile);
    }
    else if (apDlx == gData.mpShared->mpDlxHal)
    {
        sInitBuiltInDlx(&gData.DlxHal, apDlx, aModulePageLinkAddr, apInOutHostFile);
    }
    else if (apDlx == gData.mpShared->mpDlxKern)
    {
        sInitBuiltInDlx(&gpProc0->PrimaryModule, apDlx, aModulePageLinkAddr, apInOutHostFile);
    }
    else if (apDlx == gData.mpShared->mpDlxAcpi)
    {
        sInitBuiltInDlx(&gData.DlxAcpi, apDlx, aModulePageLinkAddr, apInOutHostFile);
    }
    else if (apDlx == gData.mpShared->mpDlxExec)
    {
        sInitBuiltInDlx(&gData.DlxExec, apDlx, aModulePageLinkAddr, apInOutHostFile);
    }
    else
    {
        K2_ASSERT(0);
    }
}

K2STAT KernDlx_ReadSectors(K2DLXSUPP_HOST_FILE aHostFile, void *apBuffer, UINT32 aSectorCount)
{
    return K2STAT_ERROR_NOT_IMPL;
}

K2STAT KernDlx_Prepare(K2DLXSUPP_HOST_FILE aHostFile, DLX_INFO *apInfo, UINT32 aInfoSize, BOOL aKeepSymbols, K2DLXSUPP_SEGALLOC *apRetAlloc)
{
    return K2STAT_ERROR_NOT_IMPL;
}

BOOL   KernDlx_PreCallback(K2DLXSUPP_HOST_FILE aHostFile, BOOL aIsLoad)
{
    return FALSE;
}

K2STAT KernDlx_PostCallback(K2DLXSUPP_HOST_FILE aHostFile, K2STAT aUserStatus)
{
    return K2STAT_ERROR_NOT_IMPL;
}

K2STAT KernDlx_Finalize(K2DLXSUPP_HOST_FILE aHostFile, K2DLXSUPP_SEGALLOC *apUpdateAlloc)
{
    return K2STAT_ERROR_NOT_IMPL;
}

K2STAT KernDlx_Purge(K2DLXSUPP_HOST_FILE aHostFile)
{
    return K2STAT_ERROR_NOT_IMPL;
}

#define MAX_DLX_NAME_LEN    32

UINT32 KernDlx_FindClosestSymbol(K2OSKERN_OBJ_PROCESS *apCurProc, UINT32 aAddr, char *apRetSymName, UINT32 aRetSymNameBufLen)
{
    BOOL                    disp;
    K2TREE_NODE *           pTreeNode;
    K2OSKERN_OBJ_SEGMENT *  pSeg;
    K2STAT                  stat;
    K2OSKERN_OBJ_DLX *      pDlxObj;
    char const *            pStockStr;
    UINT32                  symAddr;
    char                    dlxName[MAX_DLX_NAME_LEN];

    if (aAddr >= K2OS_KVA_KERN_BASE)
        apCurProc = gpProc0;

    disp = K2OSKERN_SeqIntrLock(&apCurProc->SegTreeSeqLock);

    pTreeNode = K2TREE_FindOrAfter(&apCurProc->SegTree, aAddr);
    if (pTreeNode == NULL)
    {
        pTreeNode = K2TREE_LastNode(&apCurProc->SegTree);
        pTreeNode = K2TREE_PrevNode(&apCurProc->SegTree, pTreeNode);
        if (pTreeNode != NULL)
        {
            pSeg = K2_GET_CONTAINER(K2OSKERN_OBJ_SEGMENT, pTreeNode, ProcSegTreeNode);
            K2_ASSERT(pSeg->ProcSegTreeNode.mUserVal < aAddr);
        }
        else
        {
            K2_ASSERT(apCurProc->SegTree.mNodeCount == 0);
            pSeg = NULL;
        }
    }
    else
    {
        if (pTreeNode->mUserVal != aAddr)
        {
            pTreeNode = K2TREE_PrevNode(&apCurProc->SegTree, pTreeNode);
            if (pTreeNode != NULL)
            {
                pSeg = K2_GET_CONTAINER(K2OSKERN_OBJ_SEGMENT, pTreeNode, ProcSegTreeNode);
                K2_ASSERT(pSeg->ProcSegTreeNode.mUserVal < aAddr);
            }
            else
            {
                pSeg = NULL;
            }
        }
        else
        {
            pSeg = K2_GET_CONTAINER(K2OSKERN_OBJ_SEGMENT, pTreeNode, ProcSegTreeNode);
        }
    }

    if (pSeg != NULL)
    {
        stat = KernObj_AddRef(&pSeg->Hdr);
        K2_ASSERT(!K2STAT_IS_ERROR(stat));
        symAddr = aAddr;
    }

    K2OSKERN_SeqIntrUnlock(&apCurProc->SegTreeSeqLock, disp);

    symAddr = pSeg->ProcSegTreeNode.mUserVal;

    if (pSeg != NULL)
    {
        pStockStr = NULL;

        //
        // what type of segment is this?
        //
        switch (pSeg->mSegAndMemPageAttr & K2OS_SEG_ATTR_TYPE_MASK)
        {
        case K2OS_SEG_ATTR_TYPE_DLX_PART:
            pDlxObj = pSeg->Info.DlxPart.mpDlxObj;
            K2_ASSERT(pDlxObj != NULL);
            stat = DLX_GetIdent(pDlxObj->mpDlx, dlxName, MAX_DLX_NAME_LEN, NULL);
            K2_ASSERT(!K2STAT_IS_ERROR(stat));
            K2_ASSERT(&pDlxObj->SegObj[pSeg->Info.DlxPart.mSegmentIndex] == pSeg);
            DLX_AddrToName(pDlxObj->mpDlx, aAddr, pSeg->Info.DlxPart.mSegmentIndex, apRetSymName, aRetSymNameBufLen);
            break;
        case K2OS_SEG_ATTR_TYPE_PROCESS:
            pStockStr = "PROCESS_AREA";
            break;
        case K2OS_SEG_ATTR_TYPE_THREAD:
            pStockStr = "THREAD_STACK";
            break;
        case K2OS_SEG_ATTR_TYPE_HEAP_TRACK:
            pStockStr = "HEAP_TRACK";
            break;
        case K2OS_SEG_ATTR_TYPE_USER:
            pStockStr = "USER_VIRTALLOC";
            break;
        case K2OS_SEG_ATTR_TYPE_DEVMAP:
            pStockStr = "DEVICEMAP";
            break;
        case K2OS_SEG_ATTR_TYPE_PHYSBUF:
            pStockStr = "PHYSBUF";
            break;
        case K2OS_SEG_ATTR_TYPE_DLX_PAGE:
            pStockStr = "DLX_PAGE";
            break;
        case K2OS_SEG_ATTR_TYPE_SEG_SLAB:
            pStockStr = "SEG_SLAB";
            break;

        default:
            K2_ASSERT(0);
            break;
        }

        if (pStockStr != NULL)
        {
            K2ASC_CopyLen(apRetSymName, pStockStr, aRetSymNameBufLen - 1);
        }

        apRetSymName[aRetSymNameBufLen - 1] = 0;

        KernObj_Release(&pSeg->Hdr);
    }
    else
    {
        if (aRetSymNameBufLen > 0)
            apRetSymName[0] = 0;
    }

    return symAddr;
}
