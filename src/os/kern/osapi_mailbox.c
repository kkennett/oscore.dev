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

K2OS_TOKEN K2_CALLCONV_CALLERCLEANS K2OS_MailboxCreate(K2OS_TOKEN aTokName, BOOL aInitBlocked)
{
    K2STAT                  stat;
    K2OSKERN_OBJ_NAME *     pNameObj;
    K2OSKERN_OBJ_HEADER *   pObjHdr;
    K2OSKERN_OBJ_MAILBOX *  pMailboxObj;
    K2OS_TOKEN              tokMailbox;

    tokMailbox = NULL;

    pNameObj = NULL;

    if (aTokName != NULL)
    {
        stat = K2OSKERN_TranslateTokensToAddRefObjs(1, &aTokName, (K2OSKERN_OBJ_HEADER **)&pNameObj);
        if (!K2STAT_IS_ERROR(stat))
        {
            if (pNameObj->Hdr.mObjType != K2OS_Obj_Name)
            {
                K2OSKERN_ReleaseObject(&pNameObj->Hdr);
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
        pMailboxObj = (K2OSKERN_OBJ_MAILBOX *)K2OS_HeapAlloc(sizeof(K2OSKERN_OBJ_MAILBOX));
        if (pMailboxObj == NULL)
        {
            stat = K2OS_ThreadGetStatus();
            break;
        }

        stat = KernMailbox_Create(pMailboxObj, pNameObj, aInitBlocked);
        if (K2STAT_IS_ERROR(stat))
        {
            K2OS_HeapFree(pMailboxObj);
            break;
        }

        pObjHdr = &pMailboxObj->Hdr;
        stat = K2OSKERN_CreateTokenNoAddRef(1, &pObjHdr, &tokMailbox);
        if (K2STAT_IS_ERROR(stat))
        {
            K2OSKERN_ReleaseObject(&pMailboxObj->Hdr);
        }

    } while (0);

    if (pNameObj != NULL)
    {
        K2OSKERN_ReleaseObject(&pNameObj->Hdr);
    }

    if (K2STAT_IS_ERROR(stat))
    {
        K2OS_ThreadSetStatus(stat);
        return NULL;
    }

    return tokMailbox;
}

BOOL K2_CALLCONV_CALLERCLEANS K2OS_MailboxSetBlock(K2OS_TOKEN aTokMailbox, BOOL aBlock)
{
    K2STAT                  stat;
    K2STAT                  stat2;
    K2OSKERN_OBJ_MAILBOX *  pMailboxObj;

    if (aTokMailbox == NULL)
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_BAD_TOKEN);
        return FALSE;
    }

    stat = K2OSKERN_TranslateTokensToAddRefObjs(1, &aTokMailbox, (K2OSKERN_OBJ_HEADER **)&pMailboxObj);
    if (!K2STAT_IS_ERROR(stat))
    {
        if (pMailboxObj->Hdr.mObjType == K2OS_Obj_Mailbox)
            stat = KernMailbox_SetBlock(pMailboxObj, aBlock);
        else
            stat = K2STAT_ERROR_BAD_TOKEN;
        stat2 = K2OSKERN_ReleaseObject(&pMailboxObj->Hdr);
        K2_ASSERT(!K2STAT_IS_ERROR(stat2));
    }

    if (K2STAT_IS_ERROR(stat))
    {
        K2OS_ThreadSetStatus(stat);
        return FALSE;
    }

    return TRUE;
}

BOOL K2_CALLCONV_CALLERCLEANS K2OS_MailboxRecv(K2OS_TOKEN aTokMailbox, K2OS_MSGIO *apRetMsgIo, UINT32 *apRetRequestId)
{
    K2STAT                  stat;
    K2STAT                  stat2;
    K2OSKERN_OBJ_MAILBOX *  pMailboxObj;
    K2OS_MSGIO              actMsgIo;
    UINT32                  actReqId;

    if (aTokMailbox == NULL)
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_BAD_TOKEN);
        return FALSE;
    }

    if ((apRetRequestId == NULL) ||
        (apRetMsgIo == NULL))
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_BAD_ARGUMENT);
        return FALSE;
    }

    K2MEM_Zero(apRetMsgIo, sizeof(K2OS_MSGIO));
    *apRetRequestId = 0;

    K2MEM_Zero(&actMsgIo, sizeof(actMsgIo));
    actReqId = 0;

    K2OSKERN_Debug("Os-MboxRecv %d\n", __LINE__);
    stat = K2OSKERN_TranslateTokensToAddRefObjs(1, &aTokMailbox, (K2OSKERN_OBJ_HEADER **)&pMailboxObj);
    K2OSKERN_Debug("Os-MboxRecv %d\n", __LINE__);

    if (!K2STAT_IS_ERROR(stat))
    {
        if (pMailboxObj->Hdr.mObjType == K2OS_Obj_Mailbox)
            stat = KernMailbox_Recv(pMailboxObj, &actMsgIo, &actReqId);
        else
            stat = K2STAT_ERROR_BAD_TOKEN;
        stat2 = K2OSKERN_ReleaseObject(&pMailboxObj->Hdr);
        K2_ASSERT(!K2STAT_IS_ERROR(stat2));
    }

    if (K2STAT_IS_ERROR(stat))
    {
        K2OS_ThreadSetStatus(stat);
        return FALSE;
    }

    *apRetMsgIo = actMsgIo;
    *apRetRequestId = actReqId;

    return TRUE;
}

BOOL K2_CALLCONV_CALLERCLEANS K2OS_MailboxRespond(K2OS_TOKEN aTokMailbox, UINT32 aRequestId, K2OS_MSGIO const *apRespIo)
{
    K2STAT                  stat;
    K2STAT                  stat2;
    K2OSKERN_OBJ_MAILBOX *  pMailboxObj;
    K2OS_MSGIO              msgIo;

    if (aTokMailbox == NULL)
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_BAD_TOKEN);
        return FALSE;
    }

    if ((aRequestId == 0) ||
        (apRespIo == NULL))
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_BAD_ARGUMENT);
        return FALSE;
    }

    K2MEM_Copy(&msgIo, apRespIo, sizeof(K2OS_MSGIO));

    stat = K2OSKERN_TranslateTokensToAddRefObjs(1, &aTokMailbox, (K2OSKERN_OBJ_HEADER **)&pMailboxObj);
    if (!K2STAT_IS_ERROR(stat))
    {
        if (pMailboxObj->Hdr.mObjType == K2OS_Obj_Mailbox)
            stat = KernMailbox_Respond(pMailboxObj, aRequestId, &msgIo);
        else
            stat = K2STAT_ERROR_BAD_TOKEN;
        stat2 = K2OSKERN_ReleaseObject(&pMailboxObj->Hdr);
        K2_ASSERT(!K2STAT_IS_ERROR(stat2));
    }

    if (K2STAT_IS_ERROR(stat))
    {
        K2OS_ThreadSetStatus(stat);
        return FALSE;
    }

    return TRUE;
}


