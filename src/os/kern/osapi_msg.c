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

BOOL K2_CALLCONV_CALLERCLEANS K2OS_MsgCreate(UINT32 aMsgCount, K2OS_TOKEN *apRetTokMsgs)
{
    K2OSKERN_OBJ_MSG ** ppMsg;
    UINT32              ix;
    K2STAT              stat;
    K2STAT              stat2;
    K2OSKERN_OBJ_MSG *  pMsg;
    BOOL                ok;
    K2OS_TOKEN *        pTokens;

    if ((aMsgCount == 0) ||
        (apRetTokMsgs == NULL))
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_BAD_ARGUMENT);
        return FALSE;
    }

    stat = K2STAT_NO_ERROR;
    K2MEM_Zero(apRetTokMsgs, sizeof(K2OS_TOKEN) * aMsgCount);

    ppMsg = (K2OSKERN_OBJ_MSG **)K2OS_HeapAlloc(sizeof(K2OSKERN_OBJ_MSG *) * aMsgCount);
    if (ppMsg == NULL)
    {
        stat = K2OS_ThreadGetStatus();
        return FALSE;
    }

    K2MEM_Zero(ppMsg, sizeof(K2OSKERN_OBJ_MSG *) * aMsgCount);

    do {
        pTokens = (K2OS_TOKEN *)K2OS_HeapAlloc(sizeof(K2OS_TOKEN) * aMsgCount);
        if (pTokens == NULL)
        {
            stat = K2OS_ThreadGetStatus();
            break;
        }

        K2MEM_Zero(pTokens, sizeof(K2OS_TOKEN) * aMsgCount);

        do {
            for (ix = 0; ix < aMsgCount; ix++)
            {
                pMsg = (K2OSKERN_OBJ_MSG *)K2OS_HeapAlloc(sizeof(K2OSKERN_OBJ_MSG));
                if (pMsg == NULL)
                {
                    stat = K2OS_ThreadGetStatus();
                    break;
                }

                stat = KernMsg_Create(pMsg);
                if (K2STAT_IS_ERROR(stat))
                    break;

                ppMsg[ix] = pMsg;
            }
            if (ix < aMsgCount)
            {
                if (ix > 0)
                {
                    do {
                        --ix;
                        stat2 = KernObj_Release(&ppMsg[ix]->Hdr);
                        K2_ASSERT(!K2STAT_IS_ERROR(stat2));
                        ppMsg[ix] = NULL;
                    } while (ix > 0);
                }
                K2_ASSERT(K2STAT_IS_ERROR(stat));
                break;
            }

            stat = KernTok_CreateNoAddRef(aMsgCount, (K2OSKERN_OBJ_HEADER **)ppMsg, pTokens);
            if (K2STAT_IS_ERROR(stat))
            {
                for (ix = 0; ix < aMsgCount; ix++)
                {
                    stat2 = KernObj_Release(&ppMsg[ix]->Hdr);
                    K2_ASSERT(!K2STAT_IS_ERROR(stat2));
                    ppMsg[ix] = NULL;
                }
            }
            else
            {
                K2MEM_Copy(apRetTokMsgs, pTokens, sizeof(K2OS_TOKEN) * aMsgCount);
            }

        } while (0);

        ok = K2OS_HeapFree(pTokens);
        K2_ASSERT(ok);

    } while (0);

    ok = K2OS_HeapFree(ppMsg);
    K2_ASSERT(ok);

    if (K2STAT_IS_ERROR(stat))
    {
        K2OS_ThreadSetStatus(stat);
        return FALSE;
    }

    return TRUE;
}

BOOL K2_CALLCONV_CALLERCLEANS K2OS_MsgSend(K2OS_TOKEN aTokMailslotName, K2OS_TOKEN aTokMsg, K2OS_MSGIO const *apMsgIo, BOOL aResponseRequired)
{
    K2STAT                  stat;
    K2STAT                  stat2;
    K2OSKERN_OBJ_MAILSLOT * pMailslot;
    K2OSKERN_OBJ_MSG *      pMsg;
    K2OS_MSGIO              msgIo;

    if ((aTokMailslotName == NULL) ||
        (aTokMsg == NULL) ||
        (apMsgIo == NULL))
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_BAD_ARGUMENT);
        return FALSE;
    }

    stat = KernName_TokenToAddRefObject(aTokMailslotName, (K2OSKERN_OBJ_HEADER **)&pMailslot);
    if (!K2STAT_IS_ERROR(stat))
    {
        if (pMailslot->Hdr.mObjType != K2OS_Obj_Mailslot)
        {
            stat = KernObj_Release(&pMailslot->Hdr);
            K2_ASSERT(!K2STAT_IS_ERROR(stat));
            stat = K2STAT_ERROR_BAD_TOKEN;
        }
    }
    if (K2STAT_IS_ERROR(stat))
    {
        K2OS_ThreadSetStatus(stat);
        return FALSE;
    }

    K2MEM_Copy(&msgIo, apMsgIo, sizeof(K2OS_MSGIO));

    stat = KernTok_TranslateToAddRefObjs(1, &aTokMsg, (K2OSKERN_OBJ_HEADER **)&pMsg);
    if (!K2STAT_IS_ERROR(stat))
    {
        if (pMsg->Hdr.mObjType == K2OS_Obj_Msg)
            stat = KernMsg_Send(pMailslot, pMsg, &msgIo, aResponseRequired);
        else
            stat = K2STAT_ERROR_BAD_TOKEN;
        stat2 = KernObj_Release(&pMsg->Hdr);
        K2_ASSERT(!K2STAT_IS_ERROR(stat2));
    }

    KernObj_Release(&pMailslot->Hdr);

    if (K2STAT_IS_ERROR(stat))
    {
        K2OS_ThreadSetStatus(stat);
        return FALSE;
    }

    return TRUE;
}

BOOL K2_CALLCONV_CALLERCLEANS K2OS_MsgAbort(K2OS_TOKEN aTokMsg)
{
    K2STAT          stat;
    K2STAT          stat2;
    K2OSKERN_OBJ_MSG * pMsg;

    if (aTokMsg == NULL)
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_BAD_TOKEN);
        return FALSE;
    }

    stat = KernTok_TranslateToAddRefObjs(1, &aTokMsg, (K2OSKERN_OBJ_HEADER **)&pMsg);
    if (!K2STAT_IS_ERROR(stat))
    {
        if (pMsg->Hdr.mObjType == K2OS_Obj_Msg)
            stat = KernMsg_Abort(pMsg);
        else
            stat = K2STAT_ERROR_BAD_TOKEN;
        stat2 = KernObj_Release(&pMsg->Hdr);
        K2_ASSERT(!K2STAT_IS_ERROR(stat2));
    }

    if (K2STAT_IS_ERROR(stat))
    {
        K2OS_ThreadSetStatus(stat);
        return FALSE;
    }

    return TRUE;
}

BOOL K2_CALLCONV_CALLERCLEANS K2OS_MsgReadResponse(K2OS_TOKEN aTokMsg, K2OS_MSGIO *apRetRespIo, BOOL aClear)
{
    K2STAT              stat;
    K2STAT              stat2;
    K2OSKERN_OBJ_MSG *  pMsg;
    K2OS_MSGIO          actResp;

    if ((aTokMsg == NULL) || (apRetRespIo == NULL))
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_BAD_TOKEN);
        return FALSE;
    }

    K2MEM_Zero(apRetRespIo, sizeof(K2OS_MSGIO));
    K2MEM_Zero(&actResp, sizeof(K2OS_MSGIO));

    stat = KernTok_TranslateToAddRefObjs(1, &aTokMsg, (K2OSKERN_OBJ_HEADER **)&pMsg);
    if (!K2STAT_IS_ERROR(stat))
    {
        if (pMsg->Hdr.mObjType == K2OS_Obj_Msg)
            stat = KernMsg_ReadResponse(pMsg, &actResp, aClear);
        else
            stat = K2STAT_ERROR_BAD_TOKEN;
        stat2 = KernObj_Release(&pMsg->Hdr);
        K2_ASSERT(!K2STAT_IS_ERROR(stat2));
    }

    if (K2STAT_IS_ERROR(stat))
    {
        K2OS_ThreadSetStatus(stat);
        return FALSE;
    }

    *apRetRespIo = actResp;

    return TRUE;
}

