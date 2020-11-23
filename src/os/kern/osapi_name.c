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

K2OS_TOKEN K2_CALLCONV_CALLERCLEANS K2OS_NameDefine(char const *apName)
{
    K2STAT                  stat;
    K2OSKERN_OBJ_NAME *     pName;
    K2OSKERN_OBJ_NAME *     pActual;
    char const *            pCheckName;
    UINT32                  left;
    K2OSKERN_OBJ_HEADER *   pObjHdr;
    char                    locName[K2OS_NAME_MAX_LEN + 1];
    char *                  pNameOut;
    char                    ch;
    K2OS_TOKEN              tokName;

    if ((apName == NULL) ||
        ((*apName) == 0))
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_BAD_ARGUMENT);
        return NULL;
    }

    tokName = NULL;
    K2MEM_Zero(locName, K2OS_NAME_MAX_LEN + 1);

    left = K2OS_NAME_MAX_LEN;
    pCheckName = apName;
    pNameOut = locName;
    do {
        ch = *pCheckName;
        if (ch == 0)
            break;
        if ((ch >= 'a') && (ch <= 'z'))
            ch -= ('a' - 'A');
        if (((ch < 'A') || (ch > 'Z')) &&
            ((ch < '0') || (ch > '9')) &&
            (ch != '_') &&
            (ch != '-') &&
            (ch != '/') &&
            (ch != '{') &&
            (ch != '}'))
        {
            K2OS_ThreadSetStatus(K2STAT_ERROR_BAD_ARGUMENT);
            return NULL;
        }
        *pNameOut = ch;
        pNameOut++;
        pCheckName++;
    } while (--left);

    pName = K2OS_HeapAlloc(sizeof(K2OSKERN_OBJ_NAME));
    if (pName == NULL)
        return NULL;

    do {
        //
        // initial reference is for the token we are going to return
        //
        pActual = NULL;
        stat = KernName_Define(pName, locName, &pActual);
        if (K2STAT_IS_ERROR(stat))
        {
            K2OS_HeapFree(pName);
            break;
        }

        //
        // if we get a different object back then the name was already
        // defined and we took a reference to it
        //
        if (pActual != pName)
        {
            K2OS_HeapFree(pName);
            pName = pActual;
        }

        //
        // name object now exists - storage is only released
        // via object release
        pObjHdr = &pName->Hdr;
        stat = K2OSKERN_CreateTokenNoAddRef(1, &pObjHdr, &tokName);
        if (K2STAT_IS_ERROR(stat))
        {
            //
            // token creation failed, so we need to release the reference
            // we took for it. This should destroy the object we just created
            // unless the name already existed
            //
            KernObj_Release(&pName->Hdr);
        }

    } while (0);

    if (K2STAT_IS_ERROR(stat))
    {
        K2OS_ThreadSetStatus(stat);
        return NULL;
    }

    return tokName;
}

BOOL K2_CALLCONV_CALLERCLEANS K2OS_NameString(K2OS_TOKEN aNameToken, char *apRetName, UINT32 aBufferBytes)
{
    K2STAT              stat;
    K2OSKERN_OBJ_NAME * pNameObj;
    char                name[K2OS_NAME_MAX_LEN + 1];

    if ((aNameToken == NULL) ||
        (apRetName == NULL) ||
        (aBufferBytes < (K2OS_NAME_MAX_LEN + 1)))
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_BAD_ARGUMENT);
        return FALSE;
    }

    K2MEM_Zero(apRetName, K2OS_NAME_MAX_LEN + 1);
    K2MEM_Zero(name, K2OS_NAME_MAX_LEN + 1);

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
        K2OS_ThreadSetStatus(stat);
        return FALSE;
    }

    K2ASC_Copy(name, pNameObj->NameBuffer);

    KernObj_Release(&pNameObj->Hdr);

    K2MEM_Copy(apRetName, name, K2OS_NAME_MAX_LEN + 1);

    return TRUE;
}

K2OS_TOKEN K2_CALLCONV_CALLERCLEANS K2OS_NameAcquire(K2OS_TOKEN aObjectToken)
{
    K2STAT                  stat;
    K2OSKERN_OBJ_HEADER *   pObjHdr;
    K2OSKERN_OBJ_NAME *     pNameObj;
    K2OSKERN_OBJ_HEADER *   pNameObjHdr;
    K2OS_TOKEN              tokName;

    if (aObjectToken == NULL)
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_BAD_ARGUMENT);
        return NULL;
    }

    tokName = NULL;

    stat = K2OSKERN_TranslateTokensToAddRefObjs(1, &aObjectToken, &pObjHdr);
    if (K2STAT_IS_ERROR(stat))
    {
        K2OS_ThreadSetStatus(stat);
        return NULL;
    }

    pNameObj = pObjHdr->mpName;

    if (pNameObj == NULL)
    {
        stat = K2STAT_ERROR_NOT_EXIST;
    }
    else
    {
        //
        // try to take a reference for the new token
        //
        stat = KernObj_AddRef(&pNameObj->Hdr);
        if (!K2STAT_IS_ERROR(stat))
        {
            pNameObjHdr = &pNameObj->Hdr;
            stat = K2OSKERN_CreateTokenNoAddRef(1, &pNameObjHdr, &tokName);
            if (K2STAT_IS_ERROR(stat))
            {
                //
                // token creation failed, so we need to release the 
                // reference we took for it
                //
                KernObj_Release(&pNameObj->Hdr);
            }
        }
    }

    KernObj_Release(pObjHdr);

    if (K2STAT_IS_ERROR(stat))
    {
        K2OS_ThreadSetStatus(stat);
        return NULL;
    }

    return tokName;
}

