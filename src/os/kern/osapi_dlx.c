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

K2OS_TOKEN K2_CALLCONV_CALLERCLEANS K2OS_DlxLoad(K2OS_PATH_TOKEN aTokPath, char const *apRelFilePath, K2_GUID128 const *apMatchId)
{
    K2STAT                  stat;
    K2STAT                  stat2;
    K2OSKERN_DLXLOADCONTEXT loadContext;
    DLX *                   pDlx;
    K2OS_TOKEN              tokDlx;
    K2OSKERN_OBJ_HEADER *   pObjHdr;
    K2OSKERN_OBJ_HEADER *   pPathObj;

    pPathObj = NULL;

    if (aTokPath != NULL)
    {
        stat = K2OSKERN_TranslateTokensToAddRefObjs(1, &aTokPath, (K2OSKERN_OBJ_HEADER **)&pPathObj);
        if (!K2STAT_IS_ERROR(stat))
        {
            if (pPathObj->mObjType != K2OS_Obj_Path)
            {
                stat = K2STAT_ERROR_BAD_ARGUMENT;
                stat2 = KernObj_Release(pPathObj);
                K2_ASSERT(!K2STAT_IS_ERROR(stat2));
            }
        }
        if (K2STAT_IS_ERROR(stat))
        {
            K2OS_ThreadSetStatus(stat);
            return NULL;
        }
    }

    do {
        loadContext.mpPathObj = pPathObj;
        loadContext.mpMatchId = apMatchId;
        loadContext.mpResult = NULL;

        tokDlx = NULL;

        stat = DLX_Acquire(apRelFilePath, &loadContext, &pDlx);
        if (!K2STAT_IS_ERROR(stat))
        {
            pObjHdr = &loadContext.mpResult->Hdr;
            stat = K2OSKERN_CreateTokenNoAddRef(1, &pObjHdr, &tokDlx);
            if (K2STAT_IS_ERROR(stat))
            {
                stat2 = KernObj_Release(&loadContext.mpResult->Hdr);
                K2_ASSERT(!K2STAT_IS_ERROR(stat2));
            }
        }
    } while (0);

    if (pPathObj != NULL)
    {
        KernObj_Release(pPathObj);
    }

    if (K2STAT_IS_ERROR(stat))
    {
        K2OS_ThreadSetStatus(stat);
    }

    return tokDlx;
}

K2OS_FILE_TOKEN K2_CALLCONV_CALLERCLEANS K2OS_DlxAcquireFile(K2OS_TOKEN aTokDlx)
{
    K2STAT                  stat;
    K2STAT                  stat2;
    K2OSKERN_OBJ_DLX *      pDlxObj;
    K2OS_FILE_TOKEN         tokResult;
    K2OSKERN_OBJ_HEADER *   pFileObjHdr;

    if (aTokDlx == NULL)
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_BAD_ARGUMENT);
        return FALSE;
    }

    tokResult = NULL;

    stat = K2OSKERN_TranslateTokensToAddRefObjs(1, &aTokDlx, (K2OSKERN_OBJ_HEADER **)&pDlxObj);
    if (!K2STAT_IS_ERROR(stat))
    {
        if (pDlxObj->Hdr.mObjType == K2OS_Obj_DLX)
        {
            if (pDlxObj->mTokFile != NULL)
            {
                stat = K2OSKERN_TranslateTokensToAddRefObjs(1, &pDlxObj->mTokFile, &pFileObjHdr);
                if (!K2STAT_IS_ERROR(stat))
                {
                    K2_ASSERT(pFileObjHdr->mObjType == K2OS_Obj_File);
                    stat = K2OSKERN_CreateTokenNoAddRef(1, &pFileObjHdr, &tokResult);
                    if (K2STAT_IS_ERROR(stat))
                    {
                        stat2 = KernObj_Release(pFileObjHdr);
                        K2_ASSERT(!K2STAT_IS_ERROR(stat2));
                    }
                }
            }
        }
        else
            stat = K2STAT_ERROR_BAD_ARGUMENT;

        stat2 = KernObj_Release(&pDlxObj->Hdr);
        K2_ASSERT(!K2STAT_IS_ERROR(stat2));
    }

    if (K2STAT_IS_ERROR(stat))
    {
        K2OS_ThreadSetStatus(stat);
        return NULL;
    }

    return tokResult;
}

BOOL K2_CALLCONV_CALLERCLEANS K2OS_DlxGetId(K2OS_TOKEN aTokDlx, K2_GUID128 * apRetId)
{
    K2STAT              stat;
    K2STAT              stat2;
    K2OSKERN_OBJ_DLX *  pDlxObj;

    if ((aTokDlx == NULL) ||
        (apRetId == NULL))
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_BAD_ARGUMENT);
        return FALSE;
    }

    stat = K2OSKERN_TranslateTokensToAddRefObjs(1, &aTokDlx, (K2OSKERN_OBJ_HEADER **)&pDlxObj);
    if (!K2STAT_IS_ERROR(stat))
    {
        if (pDlxObj->Hdr.mObjType == K2OS_Obj_DLX)
            stat = DLX_GetIdent(pDlxObj->mpDlx, NULL, 0, apRetId);
        else
            stat = K2STAT_ERROR_BAD_ARGUMENT;

        stat2 = KernObj_Release(&pDlxObj->Hdr);
        K2_ASSERT(!K2STAT_IS_ERROR(stat2));
    }

    if (K2STAT_IS_ERROR(stat))
    {
        K2OS_ThreadSetStatus(stat);
        return FALSE;
    }

    return TRUE;
}

void * K2_CALLCONV_CALLERCLEANS K2OS_DlxFindExport(K2OS_TOKEN aTokDlx, UINT32 aDlxSeg, char const *apExportName)
{
    K2STAT              stat;
    K2STAT              stat2;
    K2OSKERN_OBJ_DLX *  pDlxObj;
    UINT32              addr;

    if ((aTokDlx == NULL) ||
        (aDlxSeg >= DlxSeg_Count) ||
        (apExportName == NULL))
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_BAD_ARGUMENT);
        return FALSE;
    }

    stat = K2OSKERN_TranslateTokensToAddRefObjs(1, &aTokDlx, (K2OSKERN_OBJ_HEADER **)&pDlxObj);
    if (!K2STAT_IS_ERROR(stat))
    {
        if (pDlxObj->Hdr.mObjType == K2OS_Obj_DLX)
            stat = DLX_FindExport(pDlxObj->mpDlx, aDlxSeg, apExportName, &addr);
        else
            stat = K2STAT_ERROR_BAD_ARGUMENT;

        stat2 = KernObj_Release(&pDlxObj->Hdr);
        K2_ASSERT(!K2STAT_IS_ERROR(stat2));
    }

    if (K2STAT_IS_ERROR(stat))
    {
        K2OS_ThreadSetStatus(stat);
        return NULL;
    }

    return (void *)addr;
}

K2OS_TOKEN K2_CALLCONV_CALLERCLEANS K2OS_DlxAcquireAddressOwner(UINT32 aAddress, UINT32 *apRetSegment)
{
    K2OSKERN_OBJ_SEGMENT *  pSeg;
    K2TREE_NODE *           pTreeNode;
    K2OSKERN_OBJ_PROCESS *  pUseProc;
    K2OSKERN_OBJ_THREAD *   pCurThread;
    BOOL                    disp;
    K2STAT                  stat;
    K2STAT                  stat2;
    K2OSKERN_OBJ_DLX *      pDlxObj;
    K2OS_TOKEN              tokDlx;
    K2OSKERN_OBJ_HEADER *   pObjHdr;

    pCurThread = K2OSKERN_CURRENT_THREAD;

    //
    // find the segment containing the address
    //
    if (aAddress >= K2OS_KVA_KERN_BASE)
    {
        pUseProc = gpProc0;
    }
    else
    {
        pUseProc = pCurThread->mpProc;
    }

    disp = K2OSKERN_SeqIntrLock(&pUseProc->SegTreeSeqLock);

    pTreeNode = K2TREE_FindOrAfter(&pUseProc->SegTree, aAddress);
    if (pTreeNode == NULL)
    {
        pTreeNode = K2TREE_LastNode(&pUseProc->SegTree);
        pTreeNode = K2TREE_PrevNode(&pUseProc->SegTree, pTreeNode);
        if (pTreeNode != NULL)
        {
            pSeg = K2_GET_CONTAINER(K2OSKERN_OBJ_SEGMENT, pTreeNode, ProcSegTreeNode);
            K2_ASSERT(pSeg->ProcSegTreeNode.mUserVal < aAddress);
        }
        else
        {
            K2_ASSERT(pUseProc->SegTree.mNodeCount == 0);
            pSeg = NULL;
        }
    }
    else
    {
        if (pTreeNode->mUserVal != aAddress)
        {
            pTreeNode = K2TREE_PrevNode(&pUseProc->SegTree, pTreeNode);
            if (pTreeNode != NULL)
            {
                pSeg = K2_GET_CONTAINER(K2OSKERN_OBJ_SEGMENT, pTreeNode, ProcSegTreeNode);
                K2_ASSERT(pSeg->ProcSegTreeNode.mUserVal < aAddress);
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
        //
        // seg will start at or before page range
        //
        if ((aAddress - pSeg->ProcSegTreeNode.mUserVal) < pSeg->mPagesBytes)
        {
            //
            // address intercepts segment range
            //
            if ((pSeg->mSegAndMemPageAttr & K2OSKERN_SEG_ATTR_TYPE_MASK) == K2OSKERN_SEG_ATTR_TYPE_DLX_PART)
            {
                //
                // this segment is part of a DLX 
                //
                pDlxObj = pSeg->Info.DlxPart.mpDlxObj;
                KernObj_AddRef(&pDlxObj->Hdr);
            }
        }
    }

    K2OSKERN_SeqIntrUnlock(&pUseProc->SegTreeSeqLock, disp);

    if (pDlxObj == NULL)
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_FOUND);
        return NULL;
    }

    if (apRetSegment)
    {
        //
        // can do this because dlx holds reference to segment, which
        // cannot disappear while the dlx is there
        //
        *apRetSegment = pSeg->Info.DlxPart.mSegmentIndex;
    }

    //
    // create token for dlx reference
    //
    tokDlx = NULL;
    pObjHdr = &pDlxObj->Hdr;
    stat = K2OSKERN_CreateTokenNoAddRef(1, &pObjHdr, &tokDlx);
    if (K2STAT_IS_ERROR(stat))
    {
        stat2 = KernObj_Release(&pDlxObj->Hdr);
        K2_ASSERT(!K2STAT_IS_ERROR(stat2));
        K2OS_ThreadSetStatus(stat);
        return NULL;
    }

    K2_ASSERT(tokDlx != NULL);

    return tokDlx;
}
