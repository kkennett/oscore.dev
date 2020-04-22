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

K2OS_TOKEN K2_CALLCONV_CALLERCLEANS K2OS_MailslotCreate(K2OS_TOKEN aTokName, UINT32 aMailboxCount, K2OS_TOKEN const *apTokMailboxes, BOOL aInitBlocked)
{
    K2STAT                  stat;
    K2STAT                  stat2;
    UINT32                  ix;
    K2OSKERN_OBJ_NAME *     pNameObj;
    K2OSKERN_OBJ_HEADER *   pObjHdr;
    K2OSKERN_OBJ_MAILBOX ** ppMailbox;
    K2OSKERN_OBJ_MAILSLOT * pMailslotObj;
    K2OS_TOKEN *            pTokens;
    K2OS_TOKEN              tokMailslot;
    BOOL                    ok;

    if ((aTokName == NULL) ||
        (aMailboxCount == 0) ||
        (apTokMailboxes == NULL))
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_BAD_ARGUMENT);
        return NULL;
    }

    tokMailslot = NULL;

    pNameObj = NULL;
    stat = KernTok_TranslateToAddRefObjs(1, &aTokName, (K2OSKERN_OBJ_HEADER **)&pNameObj);
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

    do {
        pTokens = (K2OS_TOKEN *)K2OS_HeapAlloc(aMailboxCount * sizeof(K2OS_TOKEN));
        if (pTokens == NULL)
        {
            stat = K2OS_ThreadGetStatus();
            break;
        }

        K2MEM_Copy(pTokens, apTokMailboxes, aMailboxCount * sizeof(K2OS_TOKEN));

        do {
            ppMailbox = (K2OSKERN_OBJ_MAILBOX **)K2OS_HeapAlloc(sizeof(K2OSKERN_OBJ_MAILBOX *) * aMailboxCount);
            if (ppMailbox == NULL)
            {
                stat = K2OS_ThreadGetStatus();
                break;
            }

            K2MEM_Zero(ppMailbox, sizeof(K2OSKERN_OBJ_MAILBOX *) * aMailboxCount);

            do {
                stat = KernTok_TranslateToAddRefObjs(aMailboxCount, pTokens, (K2OSKERN_OBJ_HEADER **)ppMailbox);
                if (K2STAT_IS_ERROR(stat))
                    break;

                do {
                    for (ix = 0; ix < aMailboxCount; ix++)
                    {
                        if (ppMailbox[ix]->Hdr.mObjType != K2OS_Obj_Mailbox)
                            break;
                    }
                    if (ix < aMailboxCount)
                    {
                        stat = K2STAT_ERROR_BAD_ARGUMENT;
                        break;
                    }

                    pMailslotObj = (K2OSKERN_OBJ_MAILSLOT *)K2OS_HeapAlloc(sizeof(K2OSKERN_OBJ_MAILSLOT));
                    if (pMailslotObj == NULL)
                    {
                        stat = K2OS_ThreadGetStatus();
                        break;
                    }

                    stat = KernMailslot_Create(pMailslotObj, pNameObj, aMailboxCount, ppMailbox, aInitBlocked);
                    if (K2STAT_IS_ERROR(stat))
                    {
                        K2OS_HeapFree(pMailslotObj);
                        break;
                    }

                    pObjHdr = &pMailslotObj->Hdr;
                    stat = KernTok_CreateNoAddRef(1, &pObjHdr, &tokMailslot);
                    if (K2STAT_IS_ERROR(stat))
                    {
                        KernObj_Release(&pMailslotObj->Hdr);
                    }

                } while (0);

                for (ix = 0; ix < aMailboxCount; ix++)
                {
                    stat2 = KernObj_Release(&ppMailbox[ix]->Hdr);
                    K2_ASSERT(!K2STAT_IS_ERROR(stat2));
                    ppMailbox[ix] = NULL;
                }

            } while (0);

            ok = K2OS_HeapFree(ppMailbox);
            K2_ASSERT(ok);

        } while (0);

        ok = K2OS_HeapFree(pTokens);
        K2_ASSERT(ok);

    } while (0);

    KernObj_Release(&pNameObj->Hdr);

    if (K2STAT_IS_ERROR(stat))
    {
        K2OS_ThreadSetStatus(stat);
        return NULL;
    }

    return tokMailslot;
}

BOOL K2_CALLCONV_CALLERCLEANS K2OS_MailslotSetBlock(K2OS_TOKEN aTokMailslot, BOOL aBlock)
{
    K2STAT                  stat;
    K2STAT                  stat2;
    K2OSKERN_OBJ_MAILSLOT * pMailslotObj;

    if (aTokMailslot == NULL)
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_BAD_TOKEN);
        return FALSE;
    }

    stat = KernTok_TranslateToAddRefObjs(1, &aTokMailslot, (K2OSKERN_OBJ_HEADER **)&pMailslotObj);
    if (!K2STAT_IS_ERROR(stat))
    {
        if (pMailslotObj->Hdr.mObjType == K2OS_Obj_Mailslot)
            stat = KernMailslot_SetBlock(pMailslotObj, aBlock);
        else
            stat = K2STAT_ERROR_BAD_TOKEN;
        stat2 = KernObj_Release(&pMailslotObj->Hdr);
        K2_ASSERT(!K2STAT_IS_ERROR(stat2));
    }

    if (K2STAT_IS_ERROR(stat))
    {
        K2OS_ThreadSetStatus(stat);
        return FALSE;
    }

    return TRUE;
}

