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

K2STAT KernName_Define(K2OSKERN_OBJ_NAME *apName, char const *apString, K2OSKERN_OBJ_NAME **appRetActual)
{
    K2STAT stat;
    int    sLen;

    K2_ASSERT(apName != NULL);
    K2_ASSERT(apString != NULL);

    sLen = K2ASC_Len(apString);
    if ((sLen == 0) || (sLen > K2OS_NAME_MAX_LEN))
    {
        return K2STAT_ERROR_BAD_ARGUMENT;
    }

    K2MEM_Zero(apName, sizeof(K2OSKERN_OBJ_NAME));

    stat = KernEvent_Create(&apName->Event_IsOwned, NULL, FALSE, FALSE);
    if (K2STAT_IS_ERROR(stat))
    {
        return stat;
    }

    apName->Event_IsOwned.Hdr.mObjFlags |= K2OSKERN_OBJ_FLAG_EMBEDDED;

    apName->Hdr.mObjType = K2OS_Obj_Name;
    apName->Hdr.mObjFlags = 0;
    apName->Hdr.mpName = NULL;
    apName->Hdr.mRefCount = 1;
    apName->Hdr.Dispose = KernName_Dispose;
    K2LIST_Init(&apName->Hdr.WaitEntryPrioList);

    apName->mpObject = NULL;
    K2ASC_CopyLen(apName->NameBuffer, apString, K2OS_NAME_MAX_LEN);
    apName->NameBuffer[K2OS_NAME_MAX_LEN] = 0;
    K2OS_CritSecInit(&apName->OwnerSec);

    *appRetActual = NULL;
    stat = KernObj_AddName(apName, appRetActual);
    if (K2STAT_IS_ERROR(stat) || (stat == K2STAT_ALREADY_EXISTS))
    {
        if (!K2STAT_IS_ERROR(stat))
        {
            K2_ASSERT((*appRetActual) != apName);
        }
        KernObj_Release(&apName->Event_IsOwned.Hdr);
        K2OS_CritSecDone(&apName->OwnerSec);
        K2MEM_Zero(apName, sizeof(K2OSKERN_OBJ_NAME));
    }
    else
    {
        K2_ASSERT((*appRetActual) == apName);
    }

    return stat;
}

K2STAT KernName_TokenToAddRefObject(K2OS_TOKEN aNameToken, K2OSKERN_OBJ_HEADER **appRetObj)
{
    K2STAT                  stat;
    K2OSKERN_OBJ_NAME *     pNameObj;
    BOOL                    ok;
    K2OSKERN_OBJ_HEADER *   pObjHdr;

    if ((aNameToken == NULL) ||
        (appRetObj == NULL))
    {
        return K2STAT_ERROR_BAD_ARGUMENT;
    }

    *appRetObj = NULL;

    stat = K2OSKERN_TranslateTokensToAddRefObjs(1, &aNameToken, (K2OSKERN_OBJ_HEADER **)&pNameObj);
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
        return stat;
    }

    do {
        ok = K2OS_CritSecEnter(&pNameObj->OwnerSec);
        K2_ASSERT(ok);

        pObjHdr = pNameObj->mpObject;
        if (pObjHdr != NULL)
        {
            stat = KernObj_AddRef(pObjHdr);
            K2_ASSERT(!K2STAT_IS_ERROR(stat));
        }

        ok = K2OS_CritSecLeave(&pNameObj->OwnerSec);
        K2_ASSERT(ok);

        if (pObjHdr == NULL)
        {
            stat = K2STAT_ERROR_NOT_EXIST;
        }
        else
        {
            *appRetObj = pObjHdr;
        }

    } while (0);

    KernObj_Release(&pNameObj->Hdr);

    return stat;
}

void KernName_Dispose(K2OSKERN_OBJ_HEADER *apObjHdr)
{
    BOOL check;

    K2OSKERN_OBJ_NAME *apNameObj = (K2OSKERN_OBJ_NAME *)apObjHdr;

    K2_ASSERT(apNameObj != NULL);
    K2_ASSERT(apNameObj->Hdr.mObjType == K2OS_Obj_Name);
    K2_ASSERT(apNameObj->Hdr.mRefCount == 0);
    K2_ASSERT(!(apNameObj->Hdr.mObjFlags & K2OSKERN_OBJ_FLAG_PERMANENT));

    KernObj_Release(&apNameObj->Event_IsOwned.Hdr);

    K2OS_CritSecDone(&apNameObj->OwnerSec);

    check = (0 == (apNameObj->Hdr.mObjFlags & K2OSKERN_OBJ_FLAG_EMBEDDED));

    K2MEM_Zero(apNameObj, sizeof(K2OSKERN_OBJ_NAME));

    if (check)
    {
        check = K2OS_HeapFree(apNameObj);
        K2_ASSERT(check);
    }
}

