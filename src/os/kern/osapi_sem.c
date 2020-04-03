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

K2OS_TOKEN K2_CALLCONV_CALLERCLEANS K2OS_SemaphoreCreate(K2OS_TOKEN aNameToken, UINT32 aMaxCount, UINT32 aInitCount)
{
    K2STAT                  stat;
    K2STAT                  stat2;
    K2OSKERN_OBJ_NAME *     pNameObj;
    K2OSKERN_OBJ_SEM *      pSemObj;
    K2OSKERN_OBJ_HEADER *   pObjHdr;
    K2OS_TOKEN              tokSem;

    if ((aMaxCount == 0) ||
        (aInitCount > aMaxCount))
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_BAD_ARGUMENT);
        return NULL;
    }

    tokSem = NULL;

    pNameObj = NULL;
    if (aNameToken != NULL)
    {
        stat = KernTok_TranslateToAddRefObjs(1, &aNameToken, (K2OSKERN_OBJ_HEADER **)&pNameObj);
        if (!K2STAT_IS_ERROR(stat))
        {
            if (pNameObj->Hdr.mObjType != K2OS_Obj_Name)
            {
                KernObj_Release(&pNameObj->Hdr);
                stat = K2STAT_ERROR_BAD_TOKEN;
            }
        }
        if (K2STAT_IS_ERROR(stat))
        {
            K2OS_ThreadSetStatus(stat);
            return NULL;
        }
    }

    do {
        pSemObj = (K2OSKERN_OBJ_SEM *)K2OS_HeapAlloc(sizeof(K2OSKERN_OBJ_SEM));
        if (pSemObj == NULL)
        {
            stat = K2OS_ThreadGetStatus();
            break;
        }

        stat = KernSem_Create(pSemObj, pNameObj, aMaxCount, aInitCount);
        if (K2STAT_IS_ERROR(stat))
        {
            K2OS_HeapFree(pSemObj);
            break;
        }

        //
        // creating a token will ***NOT*** add a reference
        //
        pObjHdr = &pSemObj->Hdr;
        stat = KernTok_Create(1, &pObjHdr, &tokSem);
        if (K2STAT_IS_ERROR(stat))
        {
            stat2 = KernObj_Release(&pSemObj->Hdr);
            K2_ASSERT(!K2STAT_IS_ERROR(stat2));
        }

    } while (0);

    if (aNameToken != NULL)
    {
        K2_ASSERT(pNameObj != NULL);
        KernObj_Release(&pNameObj->Hdr);
    }

    if (K2STAT_IS_ERROR(stat))
    {
        K2OS_ThreadSetStatus(stat);
        return NULL;
    }

    return tokSem;
}

BOOL K2_CALLCONV_CALLERCLEANS K2OS_SemaphoreRelease(K2OS_TOKEN aSemaphoreToken, UINT32 aRelCount, UINT32 *apRetNewCount)
{
    K2STAT              stat;
    K2STAT              stat2;
    K2OSKERN_OBJ_SEM *  pSemObj;
    UINT32              newCount;

    if ((aSemaphoreToken == NULL) ||
        (apRetNewCount == NULL))
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_BAD_TOKEN);
        return FALSE;
    }

    newCount = 0;
    *apRetNewCount = (UINT32)-1;

    stat = KernTok_TranslateToAddRefObjs(1, &aSemaphoreToken, (K2OSKERN_OBJ_HEADER **)&pSemObj);
    if (!K2STAT_IS_ERROR(stat))
    {
        if (pSemObj->Hdr.mObjType == K2OS_Obj_Semaphore)
            stat = KernSem_Release(pSemObj, aRelCount, &newCount);
        else
            stat = K2STAT_ERROR_BAD_TOKEN;
        stat2 = KernObj_Release(&pSemObj->Hdr);
        K2_ASSERT(!K2STAT_IS_ERROR(stat2));
    }

    if (K2STAT_IS_ERROR(stat))
    {
        K2OS_ThreadSetStatus(stat);
        return FALSE;
    }

    *apRetNewCount = newCount;

    return TRUE;
}

K2OS_TOKEN K2_CALLCONV_CALLERCLEANS K2OS_SemaphoreAcquireByName(K2OS_TOKEN aNameToken)
{
    return KernTok_CreateFromNamedObject(aNameToken, K2OS_Obj_Semaphore);
}
