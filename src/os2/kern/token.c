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

K2STAT  
KernTok_CreateNoAddRef(
    K2OSKERN_OBJ_PROCESS *  apProc,
    UINT32                  aObjCount,
    K2OSKERN_OBJ_HEADER **  appObjHdr,
    K2OS_TOKEN *            apRetTokens
)
{
    K2OSKERN_TOKEN_PAGE *   pNewTokenPage;
    K2OSKERN_TOKEN_PAGE **  ppNewTokenPageList;
    K2OSKERN_TOKEN_PAGE **  ppOldTokenPageList;
    UINT32                  newTokenPageListLen;
    BOOL                    ok;
    UINT32                  value;
    K2OSKERN_TOKEN *        pToken;
    K2LIST_LINK *           pListLink;
    K2OSKERN_TOKEN_PAGE *   pFreeTokenPage;
    UINT32                  disp;

    K2_ASSERT(appObjHdr != NULL);
    K2_ASSERT(aObjCount > 0);
    K2_ASSERT(apRetTokens != NULL);
    K2_ASSERT(aObjCount <= K2OSKERN_TOKENS_PER_PAGE);

    K2MEM_Zero(apRetTokens, aObjCount * sizeof(K2OS_TOKEN));

    //
    // make sure we can get the right # of tokens
    //
    pNewTokenPage = NULL;
    ppNewTokenPageList = NULL;
    newTokenPageListLen = 0;
    ppOldTokenPageList = NULL;
    do {
        disp = K2OSKERN_SeqLock(&apProc->TokSeqLock);

        if (aObjCount <= apProc->TokFreeList.mNodeCount)
        {
            //
            // do not need to increase the token list size
            //
            break;
        }

        //
        // if we have already allocated resources for more tokens then
        // insert them and keep going
        //
        if ((pNewTokenPage != NULL) && (newTokenPageListLen == (apProc->mTokPageCount + 1)))
        {
            //
            // expand the token list here. we are guaranteed to have enough tokens
            // for the request after this
            //
            K2_ASSERT(ppNewTokenPageList != NULL);
            K2MEM_Copy(ppNewTokenPageList, apProc->mppTokPages, sizeof(K2OSKERN_TOKEN_PAGE *) * apProc->mTokPageCount);
            ppNewTokenPageList[apProc->mTokPageCount] = pNewTokenPage;
            for (value = 1; value < K2OSKERN_TOKENS_PER_PAGE; value++)
            {
                K2LIST_AddAtTail(&apProc->TokFreeList, &pNewTokenPage->Tokens[value].FreeLink);
            }
            pNewTokenPage = NULL;
            ppOldTokenPageList = apProc->mppTokPages;
            apProc->mppTokPages = ppNewTokenPageList;
            apProc->mTokPageCount++;
            break;
        }

        //
        // we need more tokens and don't have resources for them (yet)
        //

        K2OSKERN_SeqUnlock(&apProc->TokSeqLock, disp);

        if (pNewTokenPage != NULL)
        {
            //
            // somebody beat us to it to expand the token list but we still
            // do not have enough.  we need to expand it again
            //
            KernHeap_Free(ppNewTokenPageList);
            ppNewTokenPageList = NULL;


            K2OS_VirtPagesDecommit((UINT32)pNewTokenPage, 1);
            K2OS_VirtPagesFree((UINT32)pNewTokenPage);


            pNewTokenPage = NULL;
        }

        newTokenPageListLen = apProc->mTokPageCount + 1;

        ppNewTokenPageList = (K2OSKERN_TOKEN_PAGE **)KernHeap_Alloc(sizeof(K2OSKERN_TOKEN_PAGE *) * newTokenPageListLen);



        K2OS_VirtPagesAlloc((UINT32 *)&pNewTokenPage, 1, K2OS_MEMPAGE_ATTR_READWRITE);



        pNewTokenPage->Hdr.mInUseCount = 0;
        pNewTokenPage->Hdr.mPageIndex = newTokenPageListLen - 1;

    } while (1);

    //
    // cannot fail from this point forward
    //

    K2_ASSERT(aObjCount <= apProc->TokFreeList.mNodeCount);

    do {
        pListLink = apProc->TokFreeList.mpHead;
        K2LIST_Remove(&apProc->TokFreeList, pListLink);
        pToken = K2_GET_CONTAINER(K2OSKERN_TOKEN, pListLink, FreeLink);

        //
        // get page of free token we just took
        //
        pFreeTokenPage = (K2OSKERN_TOKEN_PAGE *)(((UINT32)pToken) & ~K2_VA32_MEMPAGE_OFFSET_MASK);

        //
        // create the token value. low bit is ALWAYS set in a valid token
        // and ALWAYS clear in an invalid token
        //
        value = apProc->mTokSalt | 1;
        apProc->mTokSalt = (apProc->mTokSalt - K2OSKERN_TOKEN_SALT_DEC) & K2OSKERN_TOKEN_SALT_MASK;

        value |= pFreeTokenPage->Hdr.mPageIndex * K2OSKERN_TOKENS_PER_PAGE * 2;
        value |= ((UINT32)(pToken - ((K2OSKERN_TOKEN *)pFreeTokenPage))) * 2;
        pToken->InUse.mTokValue = value;

        pFreeTokenPage->Hdr.mInUseCount++;

        pToken->InUse.mpObjHdr = *appObjHdr;
        K2_ASSERT(!(pToken->InUse.mpObjHdr->mObjFlags & K2OSKERN_OBJ_FLAG_EMBEDDED));
        appObjHdr++;

        *apRetTokens = (K2OS_TOKEN)pToken->InUse.mTokValue;
        apRetTokens++;
    } while (--aObjCount);

    K2OSKERN_SeqUnlock(&apProc->TokSeqLock, disp);

    if (ppOldTokenPageList != NULL)
    {
        KernHeap_Free(ppOldTokenPageList);
    }

    if (pNewTokenPage != NULL)
    {
        //
        // it turned out we didn't need to expand the token list
        //
        ok = K2OS_VirtPagesDecommit((UINT32)pNewTokenPage, 1);
        K2_ASSERT(ok);
        ok = K2OS_VirtPagesFree((UINT32)pNewTokenPage);
        K2_ASSERT(ok);
    }

    return K2STAT_NO_ERROR;
}

K2STAT  
KernTok_TranslateToAddRefObjs(
    K2OSKERN_OBJ_PROCESS *  apProc,
    UINT32                  aTokenCount,
    K2OS_TOKEN const *      apTokens,
    K2OSKERN_OBJ_HEADER **  appRetObjHdrs
)
{
    K2STAT                  stat;
    UINT32                  ix;
    UINT32                  tokenPageIndex;
    UINT32                  tokValue;
    K2OSKERN_TOKEN_PAGE *   pTokenPage;
    K2OSKERN_TOKEN *        pToken;
    UINT32                  disp;

    K2_ASSERT(aTokenCount > 0);
    K2_ASSERT(apTokens != NULL);
    K2_ASSERT(appRetObjHdrs != NULL);

    K2MEM_Zero(appRetObjHdrs, aTokenCount * sizeof(K2OSKERN_OBJ_HEADER *));

    stat = K2STAT_NO_ERROR;

    disp = K2OSKERN_SeqLock(&apProc->TokSeqLock);

    for (ix = 0; ix < aTokenCount; ix++)
    {
        tokValue = (UINT32)(*apTokens);
        if (!(tokValue & 1))
        {
            stat = K2STAT_ERROR_BAD_TOKEN;
            break;
        }
        tokValue &= ~(K2OSKERN_TOKEN_SALT_MASK | 1);

        tokenPageIndex = tokValue / (2 * K2OSKERN_TOKENS_PER_PAGE);
        if (tokenPageIndex >= apProc->mTokPageCount)
        {
            stat = K2STAT_ERROR_BAD_TOKEN;
            break;
        }

        pTokenPage = apProc->mppTokPages[tokenPageIndex];
        if (pTokenPage == NULL)
        {
            stat = K2STAT_ERROR_BAD_TOKEN;
            break;
        }

        pToken = &pTokenPage->Tokens[(tokValue / 2) % K2OSKERN_TOKENS_PER_PAGE];

        if (pToken->InUse.mTokValue != (UINT32)(*apTokens))
        {
            stat = K2STAT_ERROR_BAD_TOKEN;
            break;
        }

        stat = KernObj_AddRef(pToken->InUse.mpObjHdr);
        K2_ASSERT(!K2STAT_IS_ERROR(stat));

        *appRetObjHdrs = pToken->InUse.mpObjHdr;
        appRetObjHdrs++;

        apTokens++;
    }

    K2OSKERN_SeqUnlock(&apProc->TokSeqLock, disp);

    if (ix < aTokenCount)
    {
        K2_ASSERT(K2STAT_IS_ERROR(stat));
        //
        // release objects we references here outside of rwlock
        //
        if (ix > 0)
        {
            do {
                ix--;
                appRetObjHdrs--;
                KernObj_Release(*appRetObjHdrs);
            } while (ix > 0);
        }
    }

    return stat;
}

K2STAT  
KernTok_Destroy(
    K2OSKERN_OBJ_PROCESS *  apProc,
    K2OS_TOKEN              aToken
)
{
    K2STAT                  stat;
    UINT32                  tokValue;
    UINT32                  tokenPageIndex;
    K2OSKERN_TOKEN_PAGE *   pTokenPage;
    K2OSKERN_TOKEN *        pToken;
    K2OSKERN_OBJ_HEADER *   pObjHdr;
    UINT32                  disp;

    //
    // must release the object referred to
    //
    K2_ASSERT(aToken != NULL);

    stat = K2STAT_NO_ERROR;

    disp = K2OSKERN_SeqLock(&apProc->TokSeqLock);

    do {
        tokValue = (UINT32)aToken;
        if (!(tokValue & 1))
        {
            stat = K2STAT_ERROR_BAD_TOKEN;
            break;
        }
        tokValue &= ~(K2OSKERN_TOKEN_SALT_MASK | 1);

        tokenPageIndex = tokValue / (2 * K2OSKERN_TOKENS_PER_PAGE);
        if (tokenPageIndex >= apProc->mTokPageCount)
        {
            stat = K2STAT_ERROR_BAD_TOKEN;
            break;
        }

        pTokenPage = apProc->mppTokPages[tokenPageIndex];
        if (pTokenPage == NULL)
        {
            stat = K2STAT_ERROR_BAD_TOKEN;
            break;
        }

        pToken = &pTokenPage->Tokens[(tokValue / 2) % K2OSKERN_TOKENS_PER_PAGE];
        if (pToken->InUse.mTokValue != (UINT32)aToken)
        {
            stat = K2STAT_ERROR_BAD_TOKEN;
            break;
        }

        //
        // this is the right token
        //
        pObjHdr = pToken->InUse.mpObjHdr;

        //
        // this will destroy the token value
        //
        K2LIST_AddAtTail(&apProc->TokFreeList, &pToken->FreeLink);

        pTokenPage->Hdr.mInUseCount--;

    } while (0);

    K2OSKERN_SeqUnlock(&apProc->TokSeqLock, disp);

    if (!K2STAT_IS_ERROR(stat))
    {
        stat = KernObj_Release(pObjHdr);
    }

    return stat;
}
