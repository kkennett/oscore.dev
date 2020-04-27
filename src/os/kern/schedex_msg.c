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
    K2OSKERN_OBJ_MAILSLOT * apSlot,
    K2OSKERN_OBJ_MSG *      apMsg,
    K2OS_MSGIO const *      apMsgIo,
    BOOL                    aRespReq,
    K2OSKERN_OBJ_MSG **     appRetRelMsg,
    K2STAT *                apRetStat
)
{
    BOOL                    changedSomething;
    K2OSKERN_OBJ_MAILBOX *  pMailbox;
    K2LIST_LINK *           pListLink;
    UINT32                  reqId;

    //    K2OSKERN_Debug("SCHED:MsgSend(%08X)\n", apMsg);

    changedSomething = FALSE;

    *apRetStat = K2STAT_NO_ERROR;

    if (!apMsg->Event.mIsSignalled)
    {
        *apRetStat = K2STAT_ERROR_IN_USE;
    }
    else if (apSlot->mBlocked)
    {
        *apRetStat = K2STAT_ERROR_CLOSED;
    }
    else
    {
        reqId = apSlot->mNextRequestSeq;
        apSlot->mNextRequestSeq = (reqId + K2OS_MAX_NUM_SLOT_MAILBOXES) & K2OSKERN_MAILBOX_REQUESTSEQ_MASK;
        if (apSlot->mNextRequestSeq == 0)
            apSlot->mNextRequestSeq = K2OS_MAX_NUM_SLOT_MAILBOXES;

        if (aRespReq)
        {
            apMsg->mFlags |= K2OSKERN_MSG_FLAG_RESPONSE_REQUIRED;
        }
        else
        {
            apMsg->mFlags &= ~K2OSKERN_MSG_FLAG_RESPONSE_REQUIRED;
        }

        if (apSlot->EmptyMailboxList.mNodeCount > 0)
        {
            //
            // need to use a mailbox, delivery is immediate
            //
            pListLink = apSlot->EmptyMailboxList.mpHead;
            pMailbox = K2_GET_CONTAINER(K2OSKERN_OBJ_MAILBOX, pListLink, SlotListLink);
            K2_ASSERT(pMailbox->mState == K2OSKERN_MAILBOX_EMPTY);
            K2LIST_Remove(&apSlot->EmptyMailboxList, pListLink);
            K2LIST_AddAtHead(&apSlot->FullMailboxList, pListLink);

            //
            // this has to happen here in case part of the message
            // is a pointer to the message being returned that needs
            // to be released.  this is the case in thread exit
            //
            if (!aRespReq)
                *appRetRelMsg = apMsg;

            K2MEM_Copy(&pMailbox->InSvcSendIo, apMsgIo, sizeof(K2OS_MSGIO));

            if (aRespReq)
            {
                reqId |= pMailbox->mId;

                if (KernSchedEx_EventChange(&apMsg->Event, FALSE))
                    changedSomething = TRUE;
                apMsg->mpSittingInMailbox = pMailbox;
                apMsg->mRequestId = reqId;

                pMailbox->mState = K2OSKERN_MAILBOX_IN_SERVICE_WITH_MSG;
                pMailbox->mpInSvcMsg = apMsg;
                pMailbox->mInSvcRequestId = reqId;

                //
                // do not release reference. mailbox is holding it
                //
                apMsg = NULL;
            }
            else
            {
                apMsg->mRequestId = 0;

                pMailbox->mState = K2OSKERN_MAILBOX_IN_SERVICE_NO_MSG;
                K2_ASSERT(pMailbox->mpInSvcMsg == NULL);
                K2_ASSERT(pMailbox->mInSvcRequestId == 0);
            }

            if (KernSchedEx_EventChange(&pMailbox->Event, TRUE))
                changedSomething = TRUE;
        }
        else
        {
            //
            // no emptry mailboxes. need to queue message on the slot
            //
            KernSchedEx_EventChange(&apMsg->Event, FALSE);

            K2MEM_Copy(&apMsg->Io, apMsgIo, sizeof(K2OS_MSGIO));
            apMsg->mRequestId = reqId;
            apMsg->mpPendingOnSlot = apSlot;
            K2LIST_AddAtTail(&apSlot->PendingMsgList, &apMsg->SlotPendingMsgListLink);

            //
            // do not release reference.  slot queue is holding it
            //
            apMsg = NULL;
        }
    }

    *appRetRelMsg = apMsg;

    return changedSomething;
}

BOOL KernSched_Exec_MsgSend(void)
{
    K2_ASSERT(gData.Sched.mpActiveItem->mSchedItemType == KernSchedItem_MsgSend);

    gData.Sched.mpActiveItem->Args.MsgSend.mpRetRelMsg = NULL;

    return KernSchedEx_MsgSend(
        gData.Sched.mpActiveItem->Args.MsgSend.mpSlot,
        gData.Sched.mpActiveItem->Args.MsgSend.mpMsg,
        gData.Sched.mpActiveItem->Args.MsgSend.mpIo,
        gData.Sched.mpActiveItem->Args.MsgSend.mResponseRequired,
        &gData.Sched.mpActiveItem->Args.MsgSend.mpRetRelMsg,
        &gData.Sched.mpActiveItem->mSchedCallResult);
}

BOOL KernSched_Exec_MsgAbort(void)
{
    K2STAT                  stat;
    BOOL                    changedSomething;
    K2OSKERN_OBJ_MAILSLOT * pSlot;
    K2OSKERN_OBJ_MSG *      pMsg;
    K2OSKERN_OBJ_MAILBOX *  pMailbox;

    K2_ASSERT(gData.Sched.mpActiveItem->mSchedItemType == KernSchedItem_MsgAbort);

    pMsg = gData.Sched.mpActiveItem->Args.MsgAbort.mpMsgInOut;
    gData.Sched.mpActiveItem->Args.MsgAbort.mpMsgInOut = NULL;

//    K2OSKERN_Debug("SCHED:MsgAbort(%08X)\n", pMsg);

    changedSomething = FALSE;

    stat = K2STAT_NO_ERROR;

    pSlot = pMsg->mpPendingOnSlot;
    if (pSlot != NULL)
    {
        K2LIST_Remove(&pSlot->PendingMsgList, &pMsg->SlotPendingMsgListLink);
        pMsg->mpPendingOnSlot = NULL;
    }
    else
    {
        pMailbox = pMsg->mpSittingInMailbox;
        if (pMailbox != NULL)
        {
            K2_ASSERT(pMailbox->mState == K2OSKERN_MAILBOX_IN_SERVICE_WITH_MSG);
            pMailbox->mState = K2OSKERN_MAILBOX_IN_SERVICE_MSG_ABANDONED;
            pMailbox->mpInSvcMsg = NULL;

            pMsg->mpSittingInMailbox = NULL;
        }
        else
        {
            K2_ASSERT(pMsg->Event.mIsSignalled);
            stat = K2STAT_ERROR_NOT_IN_USE;
            //
            // do not release reference as neither slot nor mailbox had one
            //
            pMsg = NULL;
        }
    }

    if (pMsg != NULL)
    {
        K2MEM_Zero(&pMsg->Io, sizeof(K2OS_MSGIO));
        pMsg->mRequestId = 0;
        pMsg->Io.mStatus = K2STAT_ERROR_ABANDONED;

        if (KernSchedEx_EventChange(&pMsg->Event, TRUE))
            changedSomething = TRUE;
    }

    if (pMsg != NULL)
        gData.Sched.mpActiveItem->Args.MsgAbort.mpMsgInOut = pMsg;

    gData.Sched.mpActiveItem->mSchedCallResult = stat;

    return changedSomething;
}

BOOL KernSched_Exec_MsgReadResp(void)
{
    K2STAT              stat;
    K2OSKERN_OBJ_MSG *  pMsg;

    K2_ASSERT(gData.Sched.mpActiveItem->mSchedItemType == KernSchedItem_MsgReadResp);

    pMsg = gData.Sched.mpActiveItem->Args.MsgReadResp.mpMsg;

//    K2OSKERN_Debug("SCHED:MsgReadResp(%08X)\n", pMsg);

    stat = K2STAT_NO_ERROR;

    if (pMsg->mRequestId == 0)
    {
        // sanity
        K2_ASSERT(pMsg->Event.mIsSignalled);
        stat = K2STAT_ERROR_NOT_IN_USE;
    }
    else
    {
        K2MEM_Copy(gData.Sched.mpActiveItem->Args.MsgReadResp.mpIo, &pMsg->Io, sizeof(K2OS_MSGIO));
        if (gData.Sched.mpActiveItem->Args.MsgReadResp.mClear)
        {
            pMsg->mRequestId = 0;
            KernSchedEx_EventChange(&pMsg->Event, FALSE);
        }

        stat = K2STAT_NO_ERROR;
    }

    gData.Sched.mpActiveItem->mSchedCallResult = stat;

    return FALSE;
}
