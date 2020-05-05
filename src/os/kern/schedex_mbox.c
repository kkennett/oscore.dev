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

BOOL KernSched_Exec_MboxBlock(void)
{
    K2OSKERN_OBJ_MAILBOX *  pMailbox;
    BOOL                    setBlock;

    K2_ASSERT(gData.Sched.mpActiveItem->mSchedItemType == KernSchedItem_MboxBlock);

    pMailbox = gData.Sched.mpActiveItem->Args.MboxBlock.mpIn_Mailbox;
    setBlock = gData.Sched.mpActiveItem->Args.MboxBlock.mIn_SetBlock;

    //    K2OSKERN_Debug("SCHED:SlotBlock(%08X,%d)\n", pMailslot, setBlock);

    pMailbox->mBlocked = setBlock;

    gData.Sched.mpActiveItem->mSchedCallResult = K2STAT_NO_ERROR;

    return FALSE;
}

BOOL KernSched_Exec_MboxRecv(void)
{
    K2OSKERN_OBJ_MSG *      pMsg;
    K2OSKERN_OBJ_MAILBOX *  pMailbox;

    K2_ASSERT(gData.Sched.mpActiveItem->mSchedItemType == KernSchedItem_MboxRecv);

    pMailbox = gData.Sched.mpActiveItem->Args.MboxRecv.mpIn_Mailbox;

//    K2OSKERN_Debug("SCHED:MboxRecv(%08X)\n", pMailbox);

    if (pMailbox->PendingMsgList.mNodeCount == 0)
    {
        gData.Sched.mpActiveItem->Args.MboxRecv.mOut_RequestId = 0;
        gData.Sched.mpActiveItem->Args.MboxRecv.mpOut_MsgToRelease = NULL;
        gData.Sched.mpActiveItem->Args.MboxRecv.mpOut_MailboxToRelease = NULL;
        gData.Sched.mpActiveItem->mSchedCallResult = K2STAT_ERROR_EMPTY;
        return FALSE;
    }

    gData.Sched.mpActiveItem->mSchedCallResult = K2STAT_NO_ERROR;

    pMsg = K2_GET_CONTAINER(K2OSKERN_OBJ_MSG, pMailbox->PendingMsgList.mpHead, MailboxListLink);
    K2_ASSERT(pMsg->mState == KernMsgState_Pending);
    K2LIST_Remove(&pMailbox->PendingMsgList, &pMsg->MailboxListLink);
    K2_ASSERT(pMailbox->Semaphore.mCurCount > 0);
    pMailbox->Semaphore.mCurCount--;

    K2MEM_Copy(
        gData.Sched.mpActiveItem->Args.MboxRecv.mpIn_MsgIoOutBuf, 
        &pMsg->Io, 
        sizeof(K2OS_MSGIO)
    );

    if (0 == (pMsg->Io.mOpCode & K2OS_MSGOPCODE_HAS_RESPONSE))
    {
        K2_ASSERT(pMsg->mRequestId == 0);
        pMsg->mState = KernMsgState_Completed;
        pMsg->mpMailbox = NULL;
        gData.Sched.mpActiveItem->Args.MboxRecv.mpOut_MsgToRelease = pMsg;
        gData.Sched.mpActiveItem->Args.MboxRecv.mpOut_MailboxToRelease = pMailbox;
        gData.Sched.mpActiveItem->Args.MboxRecv.mOut_RequestId = 0;
        gData.Sched.mpActiveItem->mSchedCallResult = K2STAT_NO_ERROR;
        return KernSchedEx_EventChange(&pMsg->CompletionEvent, TRUE);
    }

    K2_ASSERT(pMsg->mRequestId != 0);
    pMsg->mState = KernMsgState_InSvc;
    K2LIST_AddAtTail(&pMailbox->InSvcMsgList, &pMsg->MailboxListLink);

    gData.Sched.mpActiveItem->Args.MboxRecv.mpOut_MsgToRelease = NULL;
    gData.Sched.mpActiveItem->Args.MboxRecv.mpOut_MailboxToRelease = NULL;
    gData.Sched.mpActiveItem->Args.MboxRecv.mOut_RequestId = pMsg->mRequestId;
    gData.Sched.mpActiveItem->mSchedCallResult = K2STAT_NO_ERROR;

    return FALSE;
}

BOOL KernSched_Exec_MboxRespond(void)
{
    K2OSKERN_OBJ_MAILBOX *  pMailbox;
    K2OSKERN_OBJ_MSG *      pMsg;
    UINT32                  requestId;
    K2LIST_LINK *           pListLink;

    K2_ASSERT(gData.Sched.mpActiveItem->mSchedItemType == KernSchedItem_MboxRespond);

    pMailbox = gData.Sched.mpActiveItem->Args.MboxRespond.mpIn_Mailbox;
    requestId = gData.Sched.mpActiveItem->Args.MboxRespond.mIn_RequestId;

    pListLink = pMailbox->InSvcMsgList.mpHead;
    if (pListLink != NULL)
    {
        do {
            pMsg = K2_GET_CONTAINER(K2OSKERN_OBJ_MSG, pListLink, MailboxListLink);
            if (pMsg->mRequestId == requestId)
                break;
            pListLink = pListLink->mpNext;
        } while (pListLink != NULL);
    }
    if (NULL == pListLink)
    {
        gData.Sched.mpActiveItem->Args.MboxRespond.mpOut_MsgToRelease = NULL;
        gData.Sched.mpActiveItem->Args.MboxRespond.mpOut_MailboxToRelease = NULL;
        gData.Sched.mpActiveItem->mSchedCallResult = K2STAT_ERROR_NOT_FOUND;
        return FALSE;
    }

    K2LIST_Remove(&pMailbox->InSvcMsgList, &pMsg->MailboxListLink);
    K2MEM_Copy(&pMsg->Io, gData.Sched.mpActiveItem->Args.MboxRespond.mpIn_ResponseIo, sizeof(K2OS_MSGIO));
    pMsg->mpMailbox = NULL;
    pMsg->mState = KernMsgState_Completed;
    gData.Sched.mpActiveItem->mSchedCallResult = K2STAT_NO_ERROR;
    gData.Sched.mpActiveItem->Args.MboxRespond.mpOut_MsgToRelease = pMsg;
    gData.Sched.mpActiveItem->Args.MboxRespond.mpOut_MailboxToRelease = pMailbox;

    return KernSchedEx_EventChange(&pMsg->CompletionEvent, TRUE);
}

