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

static K2OS_CRITSEC sgDlx_Sec;

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

    stat = K2DLXSUPP_GetInfo(apDlx, NULL, NULL, segInfo, &pageAddr);
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
            apDlxObj->SegObj[ix].mSegAndMemPageAttr = K2OSKERN_SEG_ATTR_TYPE_DLX_PART | sSegToAttr(ix);
            apDlxObj->SegObj[ix].Info.DlxPart.mpDlxObj = apDlxObj;
        }
    }

    apDlxObj->PageSeg.Hdr.mObjType = K2OS_Obj_Segment;
    apDlxObj->PageSeg.Hdr.mObjFlags = K2OSKERN_OBJ_FLAG_PERMANENT;
    apDlxObj->PageSeg.Hdr.mRefCount = 0x7FFFFFFF;
    apDlxObj->PageSeg.mpProc = gpProc0;
    apDlxObj->PageSeg.ProcSegTreeNode.mUserVal = pageAddr;
    apDlxObj->PageSeg.mPagesBytes = K2_VA32_MEMPAGE_BYTES;
    apDlxObj->PageSeg.mSegAndMemPageAttr = K2OS_MAPTYPE_KERN_DATA | K2OSKERN_SEG_ATTR_TYPE_DLX_PAGE;
    apDlxObj->PageSeg.Info.DlxPage.mpDlxObj = apDlxObj;

    apDlxObj->mState = KernDlxState_Loaded;
}

void KernDlxSupp_AtReInit(DLX *apDlx, UINT32 aModulePageLinkAddr, K2DLXSUPP_HOST_FILE *apInOutHostFile)
{
    //
    // this happens right after dlx_entry for kernel has completed
    //
    if (apDlx == gData.mpShared->LoadInfo.mpDlxCrt)
    {
        sInitBuiltInDlx(&gData.DlxObjCrt, apDlx, aModulePageLinkAddr, apInOutHostFile);
    }
    else if (apDlx == gData.mpDlxHal)
    {
        sInitBuiltInDlx(&gData.DlxObjHal, apDlx, aModulePageLinkAddr, apInOutHostFile);
    }
    else if (apDlx == gData.mpDlxKern)
    {
        sInitBuiltInDlx(&gpProc0->PrimaryModule, apDlx, aModulePageLinkAddr, apInOutHostFile);
    }
    else if (apDlx == gData.mpDlxAcpi)
    {
        sInitBuiltInDlx(&gData.DlxObjAcpi, apDlx, aModulePageLinkAddr, apInOutHostFile);
    }
    else if (apDlx == gData.mpDlxExec)
    {
        sInitBuiltInDlx(&gData.DlxObjExec, apDlx, aModulePageLinkAddr, apInOutHostFile);
    }
    else
    {
        K2_ASSERT(0);
    }
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
        stat = K2OSKERN_AddRefObject(&pSeg->Hdr);
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
        switch (pSeg->mSegAndMemPageAttr & K2OSKERN_SEG_ATTR_TYPE_MASK)
        {
        case K2OSKERN_SEG_ATTR_TYPE_DLX_PART:
            pDlxObj = pSeg->Info.DlxPart.mpDlxObj;
            K2_ASSERT(pDlxObj != NULL);
            stat = DLX_GetIdent(pDlxObj->mpDlx, dlxName, MAX_DLX_NAME_LEN, NULL);
            K2_ASSERT(!K2STAT_IS_ERROR(stat));
            K2_ASSERT(&pDlxObj->SegObj[pSeg->Info.DlxPart.mSegmentIndex] == pSeg);
            DLX_AddrToName(pDlxObj->mpDlx, aAddr, pSeg->Info.DlxPart.mSegmentIndex, apRetSymName, aRetSymNameBufLen);
            break;
        case K2OSKERN_SEG_ATTR_TYPE_PROCESS:
            pStockStr = "PROCESS_AREA";
            break;
        case K2OSKERN_SEG_ATTR_TYPE_THREAD:
            pStockStr = "THREAD_STACK";
            break;
        case K2OSKERN_SEG_ATTR_TYPE_HEAP_TRACK:
            pStockStr = "HEAP_TRACK";
            break;
        case K2OSKERN_SEG_ATTR_TYPE_SPARSE:
            pStockStr = "SPARSE_VIRTALLOC";
            break;
        case K2OSKERN_SEG_ATTR_TYPE_DEVMAP:
            pStockStr = "DEVICEMAP";
            break;
        case K2OSKERN_SEG_ATTR_TYPE_BUILTIN:
            pStockStr = "BUILTIN";
            break;
        case K2OSKERN_SEG_ATTR_TYPE_DLX_PAGE:
            pStockStr = "DLX_PAGE";
            break;
        case K2OSKERN_SEG_ATTR_TYPE_SEG_SLAB:
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

        K2OSKERN_ReleaseObject(&pSeg->Hdr);
    }
    else
    {
        if (aRetSymNameBufLen > 0)
            apRetSymName[0] = 0;
    }

    return symAddr;
}

K2STAT KernDlxSupp_CritSec(BOOL aEnter)
{
    BOOL ok;
    if (aEnter)
        ok = K2OS_CritSecEnter(&sgDlx_Sec);
    else
        ok = K2OS_CritSecLeave(&sgDlx_Sec);
    if (!ok)
        return K2OS_ThreadGetStatus();
    return K2STAT_OK;
}

K2STAT KernDlxSupp_Open(char const * apFileSpec, char const *apNamePart, UINT32 aNamePartLen, void *apContext, K2DLXSUPP_OPENRESULT * apRetResult)
{
    K2STAT                      stat;
    K2STAT                      stat2;
    K2OSKERN_DLXLOADCONTEXT *   pLoadContext;
    K2OSKERN_OBJ_DLX *          pDlxObj;

    pDlxObj = (K2OSKERN_OBJ_DLX *)K2OS_HeapAlloc(sizeof(K2OSKERN_OBJ_DLX));
    if (pDlxObj == NULL)
    {
        return K2STAT_ERROR_OUT_OF_MEMORY;
    }

    do {
        pLoadContext = (K2OSKERN_DLXLOADCONTEXT *)apContext;
        K2_ASSERT(pLoadContext != NULL);

        K2MEM_Zero(pDlxObj, sizeof(K2OSKERN_OBJ_DLX));
        pDlxObj->Hdr.mObjType = K2OS_Obj_DLX;
        pDlxObj->Hdr.mObjFlags = 0;
        pDlxObj->Hdr.mRefCount = 1;
        pDlxObj->Hdr.Dispose = KernDlx_Dispose;
        K2LIST_Init(&pDlxObj->Hdr.WaitEntryPrioList);

        pDlxObj->mpLoadMatchId = pLoadContext->mpMatchId;
        pDlxObj->PageSeg.Hdr.mObjType = K2OS_Obj_Segment;
        pDlxObj->PageSeg.Hdr.mObjFlags = K2OSKERN_OBJ_FLAG_EMBEDDED;
        pDlxObj->PageSeg.Hdr.mRefCount = 1;
        pDlxObj->PageSeg.Hdr.Dispose = KernMem_SegDispose;
        pDlxObj->PageSeg.mPagesBytes = K2_VA32_MEMPAGE_BYTES;
        pDlxObj->PageSeg.mSegAndMemPageAttr = K2OS_MAPTYPE_KERN_DATA | K2OSKERN_SEG_ATTR_TYPE_DLX_PAGE;
        pDlxObj->PageSeg.Info.DlxPage.mpDlxObj = pDlxObj;
        pDlxObj->mState = KernDlxState_BeforeOpen;

        stat = KernMem_AllocMapAndCreateSegment(&pDlxObj->PageSeg);
        if (K2STAT_IS_ERROR(stat))
            break;
        pDlxObj->mState = KernDlxState_PageSegAlloc;

        do {
            K2_ASSERT(K2OS_KVA_KERN_BASE < pDlxObj->PageSeg.ProcSegTreeNode.mUserVal);

            stat = gData.mfExecOpenDlx(pLoadContext->mpPathObj, apFileSpec, &pDlxObj->mTokFile, &pDlxObj->mFileTotalSectors);
            if (K2STAT_IS_ERROR(stat))
                break;
            pDlxObj->mState = KernDlxState_Open;

            apRetResult->mHostFile = (K2DLXSUPP_HOST_FILE)pDlxObj;
            apRetResult->mFileSectorCount = pDlxObj->mFileTotalSectors;
            apRetResult->mModulePageDataAddr = apRetResult->mModulePageLinkAddr = pDlxObj->PageSeg.ProcSegTreeNode.mUserVal;

        } while (0);

        if (K2STAT_IS_ERROR(stat))
        {
            stat2 = K2OSKERN_ReleaseObject(&pDlxObj->PageSeg.Hdr);
            K2_ASSERT(!K2STAT_IS_ERROR(stat2));
        }

    } while (0);

    if (K2STAT_IS_ERROR(stat))
    {
        K2OS_HeapFree(pDlxObj);
    }

    return stat;
}

K2STAT KernDlxSupp_ReadSectors(K2DLXSUPP_HOST_FILE aHostFile, void *apBuffer, UINT32 aSectorCount)
{
    K2OSKERN_OBJ_DLX *  pDlx;
    K2STAT              stat;

    pDlx = (K2OSKERN_OBJ_DLX *)aHostFile;
    stat = gData.mfExecReadDlx(pDlx->mTokFile, apBuffer, pDlx->mCurSector, aSectorCount);
    if (!K2STAT_IS_ERROR(stat))
        pDlx->mCurSector += aSectorCount;
    return stat;
}

K2STAT KernDlxSupp_Prepare(K2DLXSUPP_HOST_FILE aHostFile, DLX_INFO * apInfo, UINT32 aInfoSize, BOOL aKeepSymbols, K2DLXSUPP_SEGALLOC * apRetAlloc)
{
    return K2STAT_ERROR_NOT_IMPL;
}

BOOL   KernDlxSupp_PreCallback(K2DLXSUPP_HOST_FILE aHostFile, BOOL aIsLoad)
{
    return FALSE;
}

K2STAT KernDlxSupp_PostCallback(K2DLXSUPP_HOST_FILE aHostFile, K2STAT aUserStatus)
{
    return K2STAT_ERROR_NOT_IMPL;
}

K2STAT KernDlxSupp_Finalize(K2DLXSUPP_HOST_FILE aHostFile, K2DLXSUPP_SEGALLOC * apUpdateAlloc)
{
    return K2STAT_ERROR_NOT_IMPL;
}

K2STAT KernDlxSupp_Purge(K2DLXSUPP_HOST_FILE aHostFile)
{
    return K2STAT_ERROR_NOT_IMPL;
}

void KernInit_Dlx(void)
{
    BOOL    ok;
    K2STAT  stat;

    if (gData.mKernInitStage == KernInitStage_Threaded)
    {
        ok = K2OS_CritSecInit(&sgDlx_Sec);
        K2_ASSERT(ok);

        gData.DlxHost.AtReInit = NULL;
        gData.DlxHost.CritSec = KernDlxSupp_CritSec;
        gData.DlxHost.Open = KernDlxSupp_Open;
        gData.DlxHost.ReadSectors = KernDlxSupp_ReadSectors;
        gData.DlxHost.Prepare = KernDlxSupp_Prepare;
        gData.DlxHost.PreCallback = KernDlxSupp_PreCallback;
        gData.DlxHost.PostCallback = KernDlxSupp_PostCallback;
        gData.DlxHost.Finalize = KernDlxSupp_Finalize;
        gData.DlxHost.Purge = KernDlxSupp_Purge;

        stat = K2DLXSUPP_Init((void *)K2OS_KVA_LOADERPAGE_BASE, &gData.DlxHost, TRUE, TRUE);
        K2_ASSERT(!K2STAT_IS_ERROR(stat));
    }
}

void KernDlx_Dispose(K2OSKERN_OBJ_HEADER *apObjHdr)
{
    K2_ASSERT(0);
}
