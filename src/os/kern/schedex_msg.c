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

BOOL 
KernSchedEx_MsgSend(
    K2OSKERN_OBJ_MAILBOX *  apMailbox,
    K2OSKERN_OBJ_MSG *      apMsg,
    K2OS_MSGIO const *      apMsgIo,
    K2STAT *                apRetStat
)
{
    //  K2OSKERN_Debug("SCHED:MsgSend(%08X)\n", apMsg);

    if (apMsg->mState != KernMsgState_Ready)
    {
        *apRetStat = K2STAT_ERROR_IN_USE;
        return FALSE;
    }

    K2_ASSERT(FALSE != apMsg->CompletionEvent.mIsSignalled);

    if (apMailbox->mBlocked)
    {
        *apRetStat = K2STAT_ERROR_CLOSED;
        return FALSE;
    }

    *apRetStat = K2STAT_NO_ERROR;

    //
    // msg takes reference on mailbox
    // mailbox takes a reference on msg
    //
    KernObj_AddRef(&apMailbox->Hdr);
    KernObj_AddRef(&apMsg->Hdr);

    apMsg->mpMailbox = apMailbox;
    K2LIST_AddAtTail(&apMailbox->PendingMsgList, &apMsg->MailboxListLink);
    apMsg->mState = KernMsgState_Pending;
    if (apMsgIo->mOpCode & K2OS_MSGOPCODE_HAS_RESPONSE)
    {
        apMsg->mRequestId = ++apMailbox->mLastRequestSeq;
        if (apMsg->mRequestId == 0)
            apMsg->mRequestId = ++apMailbox->mLastRequestSeq;
    }
    else
        apMsg->mRequestId = 0;
    K2MEM_Copy(&apMsg->Io, apMsgIo, sizeof(K2OS_MSGIO));

    return KernSchedEx_SemInc(&apMailbox->Semaphore, 1);
}

BOOL KernSched_Exec_MsgSend(void)
{
    K2_ASSERT(gData.Sched.mpActiveItem->mSchedItemType == KernSchedItem_MsgSend);

    return KernSchedEx_MsgSend(
        gData.Sched.mpActiveItem->Args.MsgSend.mpIn_Mailbox,
        gData.Sched.mpActiveItem->Args.MsgSend.mpIn_Msg,
        gData.Sched.mpActiveItem->Args.MsgSend.mpIn_Io,
        &gData.Sched.mpActiveItem->mSchedCallResult);
}

BOOL KernSched_Exec_MsgAbort(void)
{
    K2OSKERN_OBJ_MSG *      pMsg;
    K2OSKERN_OBJ_MAILBOX *  pMailbox;
    BOOL                    changedSomething;
    BOOL                    doClear;

    K2_ASSERT(gData.Sched.mpActiveItem->mSchedItemType == KernSchedItem_MsgAbort);

    pMsg = gData.Sched.mpActiveItem->Args.MsgAbort.mpIn_Msg;
    doClear = gData.Sched.mpActiveItem->Args.MsgAbort.mIn_Clear;

//    K2OSKERN_Debug("SCHED:MsgAbort(%08X)\n", pMsg);

    if (pMsg->mState == KernMsgState_Completed)
    {
        K2_ASSERT(pMsg->CompletionEvent.mIsSignalled);
        gData.Sched.mpActiveItem->Args.MsgAbort.mpOut_MailboxToRelease = NULL;
        gData.Sched.mpActiveItem->Args.MsgAbort.mpOut_MsgToRelease = NULL;
        gData.Sched.mpActiveItem->mSchedCallResult = K2STAT_COMPLETED;
        return FALSE;
    }

    if (pMsg->mState == KernMsgState_Ready)
    {
        K2_ASSERT(pMsg->CompletionEvent.mIsSignalled);
        gData.Sched.mpActiveItem->Args.MsgAbort.mpOut_MailboxToRelease = NULL;
        gData.Sched.mpActiveItem->Args.MsgAbort.mpOut_MsgToRelease = NULL;
        gData.Sched.mpActiveItem->mSchedCallResult = K2STAT_ERROR_NOT_IN_USE;
        return FALSE;
    }

    pMailbox = pMsg->mpMailbox;
    K2_ASSERT(pMailbox != NULL);

    gData.Sched.mpActiveItem->Args.MsgAbort.mpOut_MailboxToRelease = pMailbox;
    gData.Sched.mpActiveItem->Args.MsgAbort.mpOut_MsgToRelease = pMsg;
    gData.Sched.mpActiveItem->mSchedCallResult = K2STAT_NO_ERROR;

    if (pMsg->mState == KernMsgState_Pending)
    {
        K2LIST_Remove(&pMailbox->PendingMsgList, &pMsg->MailboxListLink);
        K2_ASSERT(pMailbox->Semaphore.mCurCount > 0);
        pMailbox->Semaphore.mCurCount--;
    }
    else
    {
        K2_ASSERT(pMsg->mState == KernMsgState_InSvc);
        K2LIST_Remove(&pMailbox->InSvcMsgList, &pMsg->MailboxListLink);
    }

    pMsg->mpMailbox = NULL;
    pMsg->Io.mStatus = K2STAT_ERROR_ABANDONED;
    pMsg->mState = KernMsgState_Completed;

    changedSomething = KernSchedEx_EventChange(&pMsg->CompletionEvent, TRUE);

    if (doClear)
    {
        pMsg->mRequestId = 0;
        pMsg->mState = KernMsgState_Ready;
        KernSchedEx_EventChange(&pMsg->CompletionEvent, FALSE);
    }

    return changedSomething;
}

BOOL KernSched_Exec_MsgReadResp(void)
{
    K2OSKERN_OBJ_MSG *  pMsg;

    K2_ASSERT(gData.Sched.mpActiveItem->mSchedItemType == KernSchedItem_MsgReadResp);

    pMsg = gData.Sched.mpActiveItem->Args.MsgReadResp.mpIn_Msg;

//    K2OSKERN_Debug("SCHED:MsgReadResp(%08X)\n", pMsg);

    if (pMsg->mState != KernMsgState_Completed)
    {
        if (pMsg->mState == KernMsgState_Ready)
            gData.Sched.mpActiveItem->mSchedCallResult = K2STAT_ERROR_NOT_IN_USE;
        else
            gData.Sched.mpActiveItem->mSchedCallResult = K2STAT_ERROR_IN_USE;
        return FALSE;
    }

    K2_ASSERT(pMsg->CompletionEvent.mIsSignalled);

    K2MEM_Copy(gData.Sched.mpActiveItem->Args.MsgReadResp.mpIn_MsgIoOutBuf, &pMsg->Io, sizeof(K2OS_MSGIO));
   
    gData.Sched.mpActiveItem->mSchedCallResult = K2STAT_NO_ERROR;

    if (gData.Sched.mpActiveItem->Args.MsgReadResp.mIn_Clear)
    {
        pMsg->mRequestId = 0;
        pMsg->mState = KernMsgState_Ready;
        if (KernSchedEx_EventChange(&pMsg->CompletionEvent, FALSE))
            return TRUE;
    }

    return FALSE;
}
